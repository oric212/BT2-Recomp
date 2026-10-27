"""The latency the port adds between the game's flip and the display (docs/plans/ee/RESPONSIVENESS_PLAN.md step 1).

    python tools/perf/latency_report.py <BT2_LATENCY_LOG path>

Reads <path> (the game's thread: each shown flip, when it was flipped and handed to the window) and
<path>.presents (the presenter: each present's DXGI count, when it was handed over and presented, and the
presents the display showed with their refresh's time). A flip is joined to the first present handed over
after it, and that present to its display time when DXGI reported it as shown (DXGI names the last present
shown, so some presents have no time). Prints, in milliseconds, the median, 90th percentile and maximum of:
the pacer's hold (flip to handover), the presenter (handover to present), the display (present to the
refresh that showed it), and the whole (flip to that refresh). Flips 60 fps holds a field are counted apart.
"""
import argparse, bisect, statistics
from pathlib import Path


def read(path):
    flips, presents, shown = [], [], {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        kind, *values = line.split(',')
        if kind == 'flip':
            flip, flipped, handed, delayed = map(int, values)
            flips.append((flip, flipped, handed, bool(delayed)))
    for line in Path(str(path) + '.presents').read_text(encoding='utf-8').splitlines():
        kind, *values = line.split(',')
        if kind == 'present':
            count, queued, presented = map(int, values)
            presents.append((queued, count, presented))
        elif kind == 'shown':
            count, at = map(int, values)
            shown.setdefault(count, at)
    presents.sort()
    return flips, presents, shown


def join(flips, presents, shown):
    """Per flip shown without the 60 fps hold: (hold, presenter, display, whole) in ms; None where unknown."""
    queued = [p[0] for p in presents]
    rows, delayed = [], 0
    for flip, flipped, handed, held in flips:
        if held:
            delayed += 1
            continue
        i = bisect.bisect_left(queued, handed)
        if i == len(presents) or presents[i][0] - handed > 5000:
            continue
        _, count, presented = presents[i]
        at = shown.get(count)
        ms = lambda a, b: (b - a) / 1000
        rows.append((ms(flipped, handed), ms(handed, presented), ms(presented, at) if at else None, ms(flipped, at) if at else None))
    return rows, delayed


def summary(values):
    values = sorted(v for v in values if v is not None)
    if not values:
        return 'no samples'
    return f'median {statistics.median(values):6.2f}, p90 {values[int(len(values) * 0.9)]:6.2f}, max {values[-1]:6.2f} ({len(values)} samples)'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    rows, delayed = join(*read(args.log))
    print(f'{len(rows)} flips joined to a present; {delayed} held a field by 60 fps (not joined)')
    for index, name in enumerate(('pacer hold (flip to handover)', 'presenter (handover to present)',
                                  'display (present to its refresh)', 'whole (flip to its refresh)')):
        print(f'{name:34} {summary(r[index] for r in rows)}')


if __name__ == '__main__':
    main()
