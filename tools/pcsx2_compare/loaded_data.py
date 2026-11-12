"""EE RAM of the port against PCSX2's at the same event (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 5).

PCSX2's side is a comparison session's checkpoint (session.py), the port's the RAM the probe writes before the same
call (BT2_EE_CALL_RAM=<file>:<hex call address>:<n>). Both programs reach the event by their own paths and timing, so
the tool lists where the two differ, as ranges of words merged across small gaps, and names for each range the known
region it lies in (config/pcsx2_compare_regions.json: the regions that differ by timing or by the session's own
instrumentation, each with its reason). A range outside every known region is a finding.

    python tools/pcsx2_compare/loaded_data.py --pcsx2 work/pcsx2-compare/<session>/<checkpoint> --port <ram file>
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ram_store  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
REGIONS = ROOT / 'config' / 'pcsx2_compare_regions.json'
RAM_SIZE = 0x2000000


def load_regions(path: Path = REGIONS):
    data = json.loads(path.read_text(encoding='utf-8'))
    return [(int(r['start'], 16), int(r['end'], 16), r['name'], r['reason']) for r in data['regions']]


def session_patches(checkpoint: Path):
    """The words the session's instrumented executable changed: each call site's jal (4 bytes), each function entry
    a site takes or sets registers at (its first two words), from session.json next to the checkpoint."""
    path = Path(checkpoint).parent / 'session.json'
    if not path.exists():
        return []
    out = []
    for site in json.loads(path.read_text(encoding='utf-8'))['sites']:
        call = int(site['call'], 16)
        size = 8 if site.get('entry') or call == int(site['target'], 16) else 4
        out.append((call, call + size, 'session patches', 'The session sends these calls to its stubs.'))
    return out


def differing_ranges(a: bytes, b: bytes, gap: int = 64):
    """[start, end) byte ranges where the words of a and b differ; ranges closer than gap bytes are merged."""
    if len(a) != len(b) or len(a) % 4:
        raise ValueError('The two RAMs differ in size')
    words = np.flatnonzero(np.frombuffer(a, dtype='<u4') != np.frombuffer(b, dtype='<u4'))
    if words.size == 0:
        return []
    starts = words * 4
    breaks = np.flatnonzero(np.diff(starts) > gap)
    first = np.concatenate(([0], breaks + 1))
    last = np.concatenate((breaks, [words.size - 1]))
    return [(int(starts[f]), int(starts[l]) + 4, int(l - f + 1)) for f, l in zip(first, last)]


def classify(ranges, regions):
    """Each range with the known region holding it, or None; a range across a region's edge is split there."""
    out = []
    for start, end, words in ranges:
        pieces = []
        at = start
        for r_start, r_end, name, _ in sorted(regions):
            if r_end <= at or r_start >= end:
                continue
            if r_start > at:
                pieces.append((at, r_start, None))
            pieces.append((max(at, r_start), min(end, r_end), name))
            at = min(end, r_end)
            if at >= end:
                break
        if at < end:
            pieces.append((at, end, None))
        for p_start, p_end, name in pieces:
            out.append({'start': p_start, 'end': p_end, 'region': name,
                        'words': words if len(pieces) == 1 else None})
    return out


def compare(pcsx2: bytes, port: bytes, regions, gap: int = 64):
    ranges = classify(differing_ranges(pcsx2, port, gap), regions)
    known = {}
    for r in ranges:
        if r['region'] is not None:
            entry = known.setdefault(r['region'], {'ranges': 0, 'bytes': 0})
            entry['ranges'] += 1
            entry['bytes'] += r['end'] - r['start']
    findings = [r for r in ranges if r['region'] is None]
    return {'ranges': len(ranges), 'known': known, 'findings': findings}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--pcsx2', type=Path, required=True, help='A session checkpoint folder.')
    parser.add_argument('--port', type=Path, required=True, help='The RAM BT2_EE_CALL_RAM wrote.')
    parser.add_argument('--regions', type=Path, default=REGIONS)
    parser.add_argument('--gap', type=int, default=64, help='Merge differing words closer than this many bytes.')
    parser.add_argument('--out', type=Path, help='Write the full result as JSON.')
    parser.add_argument('--show', type=int, default=40, help='Findings printed.')
    args = parser.parse_args()
    pcsx2 = ram_store.read(args.pcsx2)
    port = args.port.read_bytes()
    if len(pcsx2) != RAM_SIZE or len(port) != RAM_SIZE:
        sys.exit('Both RAMs must be 32 MB')
    result = compare(pcsx2, port, load_regions(args.regions) + session_patches(args.pcsx2), args.gap)
    print(f"{result['ranges']} differing ranges")
    for name, entry in sorted(result['known'].items()):
        print(f"  known {name}: {entry['ranges']} ranges, {entry['bytes']} bytes")
    findings = result['findings']
    print(f"{len(findings)} outside the known regions, {sum(f['end'] - f['start'] for f in findings)} bytes")
    for f in findings[:args.show]:
        print(f"  {f['start']:07X}-{f['end']:07X} ({f['end'] - f['start']} bytes)")
    if args.out:
        args.out.write_text(json.dumps(result, indent=1), encoding='utf-8')


if __name__ == '__main__':
    main()
