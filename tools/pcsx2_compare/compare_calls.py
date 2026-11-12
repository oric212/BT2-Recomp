"""Replay a session's captured calls in the port and compare with PCSX2 (docs/plans/validation/PCSX2_COMPARISON_PLAN.md).

For each call a session captured before and after (session.py), the probe runs the recompiled function on the
"before" RAM, registers and VU0 memory (BT2_REPLAY_CALL) and the result is compared with PCSX2's "after":

- **RAM:** every word that differs, outside the stub segment and the callee's stack (STACK bytes below sp). A
  differing word the port wrote is a finding; one only PCSX2 changed is listed as other activity during the call
  (interrupt handlers, other threads) unless the port should have written it too, which the listing lets one judge.
- **Registers:** v0, v1, f0 and the registers a function must keep (s0 to s7, gp, sp, fp).

Usage: python tools/pcsx2_compare/compare_calls.py --disc <iso> --session <name> [--site NAME] [--config Release]
Writes work/pcsx2-compare/<session>/compare.json; exits 1 if any call differs.
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(ROOT / 'tools/ps2recomp'))
import context_block as cb  # noqa: E402
from run_startup_probe import REPORT, command  # noqa: E402

STACK = 0x10000
KEPT = {'s0': 16, 's1': 17, 's2': 18, 's3': 19, 's4': 20, 's5': 21, 's6': 22, 's7': 23, 'gp': 28, 'sp': 29, 'fp': 30}


def ranges(addresses, gap=16):
    out = []
    for a in addresses:
        if out and a <= out[-1][1] + gap:
            out[-1][1] = a
        else:
            out.append([a, a])
    return out


def replay(disc, config, target, before, out):
    env = dict(os.environ, BT2_REPLAY_CALL='|'.join([str(before / 'ee-ram.bin'), str(before / 'registers.bin'),
                                                     f'{target:x}', str(out), str(before / 'vu-mem.bin')]))
    for name in ('BT2_START_BATTLE', 'BT2_START_EPISODE', 'BT2_START_SCREEN', 'BT2_PAD_SCRIPT', 'BT2_GPU_GS', 'BT2_VIEWER'):
        env.pop(name, None)
    cmd = command(ROOT, disc, config)
    cmd[REPORT] = str(out.with_suffix('.report.json'))
    done = subprocess.run(cmd, cwd=ROOT, env=env, capture_output=True, text=True, errors='replace')
    if done.returncode != 0 or 'Replayed call: v0 ' not in done.stdout:
        return (done.stderr or done.stdout)[-2000:]
    return None


def compare(start, pcsx2, port, excluded):
    """Word addresses that differ between PCSX2's and the port's RAM, and the words each changed from the start."""
    s, p, q = (np.frombuffer(x, dtype='<u4') for x in (start, pcsx2, port))
    keep = np.ones(len(s), dtype=bool)
    for low, high in excluded:
        keep[low // 4:high // 4] = False
    differ = np.flatnonzero((p != q) & keep) * 4
    port_wrote = (q != s) & keep
    pcsx2_wrote = (p != s) & keep
    return differ, port_wrote, pcsx2_wrote


def registers(block):
    values = cb.parse(block)
    named = {'v0': values['gpr'][2], 'v1': values['gpr'][3], 'f0': values['fpr'][0]}
    named.update({k: values['gpr'][r] & 0xFFFFFFFFFFFFFFFF for k, r in KEPT.items()})
    return named


def check(disc, config, session, site, call):
    folder = ROOT / 'work/pcsx2-compare' / session
    before, after = folder / f'{site["name"]}-{call}-before', folder / f'{site["name"]}-{call}-after'
    out = folder / f'{site["name"]}-{call}-port.bin'
    error = replay(disc, config, int(site['target'], 16), before, out)
    if error:
        return {'call': call, 'site': site['name'], 'result': 'replay failed', 'error': error}
    start, pcsx2, port = (before / 'ee-ram.bin').read_bytes(), (after / 'ee-ram.bin').read_bytes(), out.read_bytes()
    sp = cb.parse((before / 'registers.bin').read_bytes())['gpr'][29] & 0xFFFFFFFF
    segment = json.loads((folder / 'session.json').read_text())['segment']
    excluded = [(segment[0], segment[0] + segment[1]), (sp - STACK, sp)]
    differ, port_wrote, pcsx2_wrote = compare(start, pcsx2, port, excluded)
    findings, activity = [], []
    for low, high in ranges(differ.tolist()):
        entry = {'range': f'{low:#010x}-{high + 3:#010x}', 'pcsx2': pcsx2[low:low + 16].hex(), 'port': port[low:low + 16].hex()}
        (findings if port_wrote[low // 4:high // 4 + 1].any() else activity).append(entry)
    theirs = registers((after / 'registers.bin').read_bytes())
    ours = registers(Path(str(out) + '.registers').read_bytes())
    register_differences = {k: [f'{theirs[k]:#x}', f'{ours[k]:#x}'] for k in theirs if theirs[k] != ours[k]}
    out.unlink()
    Path(str(out) + '.registers').unlink()
    return {'call': call, 'site': site['name'], 'result': 'equal' if not findings and not register_differences else 'DIFFERENT',
            'port_words_written': int(port_wrote.sum()), 'pcsx2_words_written': int(pcsx2_wrote.sum()),
            'findings': findings, 'registers': register_differences, 'other_activity': activity}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--disc', type=Path, required=True)
    parser.add_argument('--session', required=True)
    parser.add_argument('--site', help='Only this site.')
    parser.add_argument('--config', default='Release')
    args = parser.parse_args()
    folder = ROOT / 'work/pcsx2-compare' / args.session
    session = json.loads((folder / 'session.json').read_text())
    sites = {s['name']: s for s in session['sites']}
    pairs = sorted({(c['site'], c['call']) for c in session['checkpoints'] if c['when'] == 'after'})
    results = []
    for name, call in pairs:
        if args.site and name != args.site:
            continue
        result = check(args.disc.resolve(), args.config, args.session, sites[name], call)
        results.append(result)
        line = f'{name} call {call}: {result["result"]}'
        if 'port_words_written' in result:
            line += (f' (port wrote {result["port_words_written"]} words, PCSX2 {result["pcsx2_words_written"]};'
                     f' {len(result["findings"])} differing ranges the port wrote, {len(result["other_activity"])} only PCSX2 changed)')
        print(line, flush=True)
        for entry in result.get('findings', [])[:20]:
            print(f'  {entry["range"]}  PCSX2 {entry["pcsx2"]}  port {entry["port"]}')
        for k, (theirs, ours) in result.get('registers', {}).items():
            print(f'  {k}: PCSX2 {theirs}, port {ours}')
        if result.get('error'):
            print('  ' + result['error'].strip().replace('\n', '\n  '))
    (folder / 'compare.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    return 0 if results and all(r['result'] == 'equal' for r in results) else 1


if __name__ == '__main__':
    sys.exit(main())
