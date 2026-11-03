"""The latency the port adds between the game's flip and the display (docs/plans/ee/RESPONSIVENESS_PLAN.md step 1).

    python tools/perf/latency_report.py <BT2_LATENCY_LOG path>

Reads <path> (the game's thread: each shown flip, when it was flipped and handed to the window) and
<path>.presents (the presenter: each present's DXGI count, when it was handed over, presented and its copy
done on the GPU, and the presents the display showed with their refresh's time). A flip is joined to the first present handed over
after it, and that present to its display time when DXGI reported it as shown (DXGI names the last present
shown, so some presents have no time). Prints, in milliseconds, the median, 90th percentile and maximum of:
the pacer's hold (flip to handover), the presenter (handover to present), the display (present to the
refresh that showed it), and the whole (flip to that refresh). Flips 60 fps holds a field are counted apart.
Then the cadence: the intervals between pictures as a variable refresh display shows them (each present, or
its GPU work's end if later) and how many are within 1 and 2 ms of a field.
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
            count, queued, presented, *done = map(int, values)
            presents.append((queued, count, presented, max(presented, done[0]) if done else presented))
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
        _, count, presented, _ = presents[i]
        at = shown.get(count)
        ms = lambda a, b: (b - a) / 1000
        rows.append((ms(flipped, handed), ms(handed, presented), ms(presented, at) if at else None, ms(flipped, at) if at else None))
    return rows, delayed


def cadence(presents):
    """Intervals in ms between consecutive pictures on a variable refresh display, gaps over 100 ms left out."""
    on_panel = sorted(p[3] for p in presents)
    return [d for d in ((b - a) / 1000 for a, b in zip(on_panel, on_panel[1:])) if d < 100]


def summary(values):
    values = sorted(v for v in values if v is not None)
    if not values:
        return 'no samples'
    return f'median {statistics.median(values):6.2f}, p90 {values[int(len(values) * 0.9)]:6.2f}, max {values[-1]:6.2f} ({len(values)} samples)'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    flips, presents, shown = read(args.log)
    rows, delayed = join(flips, presents, shown)
    print(f'{len(rows)} flips joined to a present; {delayed} held a field by 60 fps (not joined)')
    for index, name in enumerate(('pacer hold (flip to handover)', 'presenter (handover to present)',
                                  'display (present to its refresh)', 'whole (flip to its refresh)')):
        print(f'{name:34} {summary(r[index] for r in rows)}')
    intervals = cadence(presents)
    if intervals:
        field = 1001 / 60
        within = lambda ms: sum(abs(d - field) <= ms or abs(d - 2 * field) <= ms for d in intervals) / len(intervals)
        print(f'{"cadence (picture to picture)":34} {summary(intervals)}')
        print(f'{"":34} {within(1):.1%} within 1 ms of a field or two, {within(2):.1%} within 2 ms')


if __name__ == '__main__':
    main()
