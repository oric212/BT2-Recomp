"""One AI call: the port against PCSX2 (docs/plans/ee/AI_VERIFICATION_PLAN.md).

Takes the PCSX2 captures of one call before and after it (capture_ai_call.py), runs the port's recompiled AI
(0x1214F8) on the "before" RAM with the captured registers (the probe's BT2_REPLAY_CALL), and compares the RAM it
leaves and v0 with the "after" capture. Not compared: the stack below sp (the AI's own frame, whose contents depend
on registers not captured) and the capture stub's segment. Differences are printed as address ranges, with what
PCSX2 changed during the call in the same places, so the AI's writes are told apart from other activity in PCSX2.

Usage: python tools/capture_ee/compare_ai_call.py --disc <iso> --call <N> [--config Release]
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/ps2recomp'))
from run_startup_probe import REPORT, command  # noqa: E402

FUNCTIONS = {'input': 0x1214F8, 'think': 0x11E988} # capture_ai_call.py's sites.
STUB = (0x01D00000, 0x400) # The capture stub's segment (capture_ai_call.py).


def ranges(addresses):
    out = []
    for a in addresses:
        if out and a <= out[-1][1] + 16:
            out[-1][1] = a
        else:
            out.append([a, a])
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--call', type=int, required=True)
    parser.add_argument('--config', default='Release')
    parser.add_argument('--site', choices=tuple(FUNCTIONS), default='input')
    args = parser.parse_args()
    base = ROOT / 'work/pcsx2-capture'
    before_dir, after_dir = base / f'ai-{args.site}-before-{args.call}', base / f'ai-{args.site}-after-{args.call}'
    before = json.loads((before_dir / 'capture.json').read_text())
    after = json.loads((after_dir / 'capture.json').read_text())
    # a1 is the caller's sp (the call's delay slot sets it, after the 'before' stub has published the registers); a0
    # does not survive the call, so the 'after' capture is matched by the count, sp and gp.
    for key in ('calls_counted', 'sp', 'gp'):
        if before[key] != after[key]:
            raise SystemExit(f'The two captures are not the same call: {key} {before[key]:#x} against {after[key]:#x}')
    out = ROOT / 'work/logs' / f'ai-{args.site}-{args.call}-port.bin'
    env = dict(os.environ, BT2_REPLAY_CALL='|'.join([str(before_dir / 'ee-ram.bin'), f'{FUNCTIONS[args.site]:x}', f"{before['a0']:x}", f"{before['sp']:x}",
                                                     f"{before['sp']:x}", f"{before['gp']:x}", str(out)]))
    for name in ('BT2_START_BATTLE', 'BT2_PAD_SCRIPT', 'BT2_GPU_GS', 'BT2_VIEWER'):
        env.pop(name, None)
    cmd = command(ROOT, args.disc.resolve(), args.config)
    cmd[REPORT] = str(ROOT / 'work/logs/ai-replay-report.json')
    done = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
    line = next((l for l in done.stdout.splitlines() if l.startswith('Replayed call: v0 ')), None)
    if done.returncode != 0 or line is None:
        raise SystemExit('The replay failed:\n' + (done.stderr or done.stdout)[-3000:])
    port_v0 = int(line.split()[-1], 16)
    start = (before_dir / 'ee-ram.bin').read_bytes()
    pcsx2 = (after_dir / 'ee-ram.bin').read_bytes()
    port = out.read_bytes()
    sp = before['sp']
    excluded = lambda a: STUB[0] <= a < STUB[0] + STUB[1] or sp - 0x2000 <= a < sp
    differ = [a for a in range(0, len(pcsx2), 4) if pcsx2[a:a + 4] != port[a:a + 4] and not excluded(a)]
    pcsx2_changed = {a for a in range(0, len(pcsx2), 4) if pcsx2[a:a + 4] != start[a:a + 4]}
    port_changed = [a for a in range(0, len(port), 4) if port[a:a + 4] != start[a:a + 4] and not excluded(a)]
    print(f'Call {args.call}: side {before["a0"]}, v0 PCSX2 {after["v0"]:#010x}, port {port_v0:#010x}'
          f' ({"equal" if after["v0"] == port_v0 else "DIFFERENT"})')
    print(f'Words the port\'s AI changed (outside its stack frame): {len(port_changed)}; '
          f'words PCSX2 changed during the call: {len([a for a in pcsx2_changed if not excluded(a)])}')
    print(f'Words different after the call: {len(differ)}')
    for low, high in ranges(differ)[:40]:
        by_ai = any(low <= a <= high for a in port_changed)
        print(f'  {low:#010x}-{high + 3:#010x}  {"the port wrote here" if by_ai else "only PCSX2 changed it (other activity)"}'
              f'  PCSX2 {pcsx2[low:low + 8].hex()}  port {port[low:low + 8].hex()}')
    return 0 if after['v0'] == port_v0 and not [r for r in ranges(differ) if any(r[0] <= a <= r[1] for a in port_changed)] else 1


if __name__ == '__main__':
    sys.exit(main())
