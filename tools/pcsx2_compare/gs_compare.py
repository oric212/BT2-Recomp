"""GS memory and the drawn picture of the port against PCSX2 (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 7).

A session takes PCSX2's save state at a call (the frame's VIF1 kick, `--save-state`) and at a later checkpoint by
which PCSX2's GS has drawn everything the call sent (the frame's end; one just after the call can find PCSX2's GS
still drawing). The port replays the call (BT2_REPLAY_CALL) from PCSX2's RAM, VU and VIF1 state, with PCSX2's GS
memory and registers loaded first (gs_memory.setup_packets), and writes its GS memory afterwards. The two GS memories
are compared by region: the frame buffer and the Z buffer the later state's FRAME_1 and ZBUF_1 name, and the rest
(textures, palettes, work buffers); the frame's pixels by how much they differ (paraLLEl-GS does not round every
interpolation as PCSX2's software renderer does). The two pictures and their difference go to
work/frames/pcsx2-compare/<session>-<label>-*.png.

    python tools/pcsx2_compare/gs_compare.py --disc <iso> --session <name> --call @102ED8-300-before
        --settled @102F08-300-before [--function 28b090]
"""
import argparse
import json
import os
import subprocess
import sys
import zipfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'tools/ps2recomp'))
import gs_memory as gm  # noqa: E402
import ram_store  # noqa: E402
from run_startup_probe import REPORT, command  # noqa: E402

PAGE_WORDS = 2048
FRAME_HEIGHT = 448


def replay(disc, config, function, call, out):
    """GS memory (words) after the port ran `function` from the checkpoint `call` with its GS state."""
    setup = out.with_suffix('.gif')
    setup.write_bytes(gm.setup_packets(zipfile.ZipFile(call / 'state.p2s').read('GS.bin')))
    ram = ram_store.materialize(call, out.with_suffix('.in.bin'))
    parts = [ram, call / 'registers.bin', f'{function:x}', out, call / 'vu-mem.bin', call / 'hardware.bin',
             call / 'vu1-regs.bin', setup]
    env = dict(os.environ, BT2_REPLAY_CALL='|'.join(str(p) for p in parts), BT2_NATIVE_PASSES='none')
    for name in ('BT2_START_BATTLE', 'BT2_START_EPISODE', 'BT2_START_SCREEN', 'BT2_PAD_SCRIPT', 'BT2_VIEWER',
                 'BT2_SUPER_SAMPLING', 'BT2_HD_TEXTURES', 'BT2_SIXTY_FPS'):
        env.pop(name, None)                                   # One sample per pixel, the game's own textures.
    cmd = command(ROOT, disc, config)
    cmd[REPORT] = str(out.with_suffix('.report.json'))
    done = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
    if ram != call / 'ee-ram.bin':
        ram.unlink()
    gs = Path(str(out) + '.gs')
    if done.returncode != 0 or not gs.exists():
        raise RuntimeError((done.stderr or done.stdout)[-2000:])
    words = np.frombuffer(gs.read_bytes(), '<u4').copy()
    for path in (out, Path(str(out) + '.registers'), Path(str(out) + '.vu1'), gs, setup, out.with_suffix('.report.json')):
        path.unlink(missing_ok=True)
    return words


def regions(contexts):
    """{name: page range} of the frame and Z buffers FRAME_1 and ZBUF_1 name (448 lines), and the rest."""
    fbp, fbw, _ = gm.frame_register(contexts[0]['FRAME'])
    zbp, _, _ = gm.frame_register(contexts[0]['ZBUF'])
    pages = (FRAME_HEIGHT + 31) // 32 * fbw
    named = {'frame': range(fbp, fbp + pages), 'Z buffer': range(zbp, zbp + pages)}
    used = set().union(*named.values())
    named['the rest'] = [k for k in range(512) if k not in used]
    return named, (fbp, fbw)


def compare(before, pcsx2, port, contexts):
    named, (fbp, fbw) = regions(contexts)
    b, p, q = (x.reshape(512, PAGE_WORDS) for x in (before, pcsx2, port))
    result = {}
    for name, pages in named.items():
        pages = list(pages)
        result[name] = {'pcsx2_changed': int((b[pages] != p[pages]).sum()), 'port_changed': int((b[pages] != q[pages]).sum()),
                        'differ': int((p[pages] != q[pages]).sum())}
    a = gm.ct32_picture(pcsx2.tobytes(), fbp, fbw, 64 * fbw, FRAME_HEIGHT).astype(int)
    c = gm.ct32_picture(port.tobytes(), fbp, fbw, 64 * fbw, FRAME_HEIGHT).astype(int)
    d = np.abs(a - c).max(axis=2)
    result['pixels'] = {'total': int(d.size), 'equal': int((d == 0).sum()), 'by 1': int((d == 1).sum()),
                        'by 2': int((d == 2).sum()), 'by 3 to 16': int(((d > 2) & (d <= 16)).sum()),
                        'by more than 16': int((d > 16).sum()), 'largest': int(d.max())}
    return result, a, c, d


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--session', required=True)
    parser.add_argument('--call', required=True, help='The checkpoint before the call, with a save state.')
    parser.add_argument('--settled', required=True, help="A later checkpoint with a save state, PCSX2's GS done.")
    parser.add_argument('--function', default='28b090', help='The function the call runs (hex).')
    parser.add_argument('--config', default='Release')
    args = parser.parse_args()
    folder = ROOT / 'work/pcsx2-compare' / args.session
    call, settled = folder / args.call, folder / args.settled
    port = replay(args.disc.resolve(), args.config, int(args.function, 16), call, folder / 'gs-port.bin')
    before = np.frombuffer(gm.read_state(call / 'state.p2s')[0], '<u4')
    pcsx2_bytes, contexts = gm.read_state(settled / 'state.p2s')
    result, a, c, d = compare(before, np.frombuffer(pcsx2_bytes, '<u4'), port, contexts)
    for name in ('frame', 'Z buffer', 'the rest'):
        r = result[name]
        print(f"{name:9s} PCSX2 changed {r['pcsx2_changed']:7d} words, the port {r['port_changed']:7d}; {r['differ']:7d} differ")
    print('frame pixels: ' + ', '.join(f'{k} {v}' for k, v in result['pixels'].items()))
    from PIL import Image
    out = ROOT / 'work/frames/pcsx2-compare'
    out.mkdir(parents=True, exist_ok=True)
    label = f'{args.session}-{args.call.split("-before")[0].lstrip("@")}'
    for name, picture in (('pcsx2', a), ('port', c)):
        rgba = picture.astype(np.uint8).copy()
        rgba[..., 3] = 255
        Image.fromarray(rgba).save(out / f'{label}-{name}.png')
    Image.fromarray(np.clip(d * 32, 0, 255).astype(np.uint8)).save(out / f'{label}-difference.png')
    (folder / 'gs-compare.json').write_text(json.dumps(result, indent=1), encoding='utf-8')


if __name__ == '__main__':
    main()
