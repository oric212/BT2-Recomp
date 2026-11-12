"""The game's own input delay in PCSX2, from two logging sessions (docs/plans/validation/PCSX2_COMPARISON_PLAN.md,
step 9).

Both sessions log the fight loop's pad update (`@102EE0`, word 1: player 1's buttons in the pad record) and its frame
end (`@102F08`: the frame counter, the display buffer, checksums of the two display-list buffers the frames alternate
between, sums 1 and 3, and of VU1's data memory, sum 2); the second presses a button at a pad update (session.py `--log ...,press=`). Fight frame k is the k-th call
of each. The tool finds the first frame whose pad update returned the press, checks that both runs were alike
before it (PCSX2 repeats itself), and reports the first frame end whose display lists and whose VU1 memory differ,
in frames and in fields: the frame ends follow a VBlank, so the fields from the start of the read's field (the
frame end before the press) to a frame end are the cycles between them over a field's (294,912,000 / 59.94).

The port's side comes from two probe runs with
`BT2_EE_CALL_SUMS=<file>:102ee8,102ec8:3b0c2c:430000-457000:1100c000-11010000:530000-557000`
(a played fight, `BT2_START_BATTLE_PLAYER=1`, the press by the pad script): the call after the pad update gives the
buttons, the loop's first call the state after the frame end before it, and the probe's VBlank count the fields.

Usage: python tools/pcsx2_compare/input_delay.py --without <session> --with <session>
       python tools/pcsx2_compare/input_delay.py --port-without <file> --port-with <file>
"""
import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIELD = 294_912_000 * 1001 / 60_000            # EE cycles a field (NTSC).
PAD, END = '@102EE0', '@102F08'


def cycles(later, earlier):
    """Cycles from one record to a later one (COP0 Count wraps every 2**32 cycles, about 14.5 s)."""
    return (later - earlier) % (1 << 32)


def load(session):
    folder = ROOT / 'work/pcsx2-compare' / session
    return (json.loads((folder / f'{PAD}.log.json').read_text(encoding='utf-8')),
            json.loads((folder / f'{END}.log.json').read_text(encoding='utf-8')))


def load_port(path):
    """A probe run's call sums as the sessions' (pad, end) records: fight frame k's pad update and its frame end
    (the loop's first call after the pad update, which follows the frame end)."""
    pad, end = [], []
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        source, field, _flips, word, lists, vu1, other_lists = line.split()
        record = {'cycles': int(int(field) * FIELD) % (1 << 32), 'word1': int(word), 'sum1': int(lists), 'sum2': int(vu1),
                  'sum3': int(other_lists)}
        if source == '102ee8':
            pad.append(record)
        elif len(end) < len(pad):
            end.append(record)
    return pad[:len(end)], end


def lists(record):
    return record['sum1'], record['sum3']


def vu1(record):
    return record['sum2']


def analyse(without, with_):
    """The delays as a dict (the arguments: (pad, end) record lists of each session)."""
    (pad_a, end_a), (pad_b, end_b) = without, with_
    frames = min(len(pad_a), len(pad_b), len(end_a), len(end_b))
    press = next((k for k in range(frames) if pad_b[k]['word1'] != pad_a[k]['word1']), None)
    if press is None:
        raise ValueError('The press does not show in the pad record of the second session.')
    before = [k for k in range(press) if (lists(end_a[k]), vu1(end_a[k])) != (lists(end_b[k]), vu1(end_b[k]))]
    out = {'press_frame': press + 1, 'buttons': hex(pad_b[press]['word1']),
           'alike_before': not before, 'first_unlike_before': before[0] + 1 if before else None}
    start = end_b[press - 1]['cycles'] if press else None      # The read's field begins at the frame end before.

    def first(key):
        k = next((k for k in range(press, frames) if key(end_a[k]) != key(end_b[k])), None)
        if k is None:
            return None
        fields = cycles(end_b[k]['cycles'], start) / FIELD if start is not None else None
        return {'frame': k + 1, 'frames_after_read': k - press, 'fields_after_read_field_start': round(fields, 2)}

    out['display_lists'] = first(lists)
    out['vu1_memory'] = first(vu1)
    spans = [cycles(end_b[k]['cycles'], end_b[k - 1]['cycles']) / FIELD for k in range(1, frames)]
    out['fields_per_frame'] = {'min': round(min(spans), 2), 'max': round(max(spans), 2)} if spans else None
    out['read_after_field_start'] = round(cycles(pad_b[press]['cycles'], start) / FIELD, 3) if start is not None else None
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--without', help='The session without the press.')
    parser.add_argument('--with', dest='with_', help='The session with the press.')
    parser.add_argument('--port-without', help="A probe run's call sums without the press.")
    parser.add_argument('--port-with', help="A probe run's call sums with the press.")
    args = parser.parse_args()
    if args.without and args.with_:
        result = analyse(load(args.without), load(args.with_))
    elif args.port_without and args.port_with:
        result = analyse(load_port(args.port_without), load_port(args.port_with))
    else:
        parser.error('Give --without and --with, or --port-without and --port-with.')
    print(json.dumps(result, indent=2))
    return 0 if result['alike_before'] else 1


if __name__ == '__main__':
    sys.exit(main())
