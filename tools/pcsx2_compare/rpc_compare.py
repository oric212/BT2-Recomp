"""The game's service calls in PCSX2 and in the port, compared per client
(docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 8).

PCSX2's side is a session's rpc.log.json (session.py --rpc-log), the port's a BT2_RPC_LOG file; both hold every
request to sceSifCallRpc and every synchronous reply with its buffer's checksum. The two programs do not take the
same time, so the calls are compared per client (one per service: the game's client structures lie at the same
addresses in both) in their order, not by when they came: the function number, the sizes and the checksums of the
send and receive buffers. For each client the first difference is listed with its neighbours. A run that went
further than the other leaves calls over; they are counted, not judged.
"""
import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rpc_log  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]


def pcsx2_records(path):
    out = []
    for entry in json.loads(path.read_text(encoding='utf-8')):
        reply = bool(entry['mode'] & rpc_log.REPLY)
        out.append({'kind': 'R' if reply else 'Q', 'function': entry['function'],
                    'client': entry['client'], 'mode': entry['mode'] & ~rpc_log.REPLY, 'size': entry['size'],
                    'address': entry['address'], 'checksum': entry['checksum'], 'when': entry['count']})
    return out


def port_records(path):
    """The port's records, and {client: service ID} from its binds (B lines)."""
    out, services = [], {}
    for line in path.read_text(encoding='utf-8').splitlines():
        kind, function, client, mode, size, address, total, field = line.split()
        if kind == 'B':
            services[int(client, 16)] = int(function, 16)
            continue
        out.append({'kind': kind, 'function': int(function, 16), 'client': int(client, 16), 'mode': int(mode, 16),
                    'size': int(size, 16), 'address': int(address, 16), 'checksum': int(total, 16), 'when': int(field)})
    return out, services


KEY = ('kind', 'function', 'mode', 'size', 'address', 'checksum')


def describe(r):
    return (f"{r['kind']} fn {r['function']:#x} mode {r['mode']} size {r['size']:#x} at {r['address']:#x} "
            f"sum {r['checksum']:08x}")


def compare(pcsx2, port):
    """{client: {calls, equal, first difference or None, left over}} in client order."""
    by = defaultdict(lambda: ([], []))
    for side, records in ((0, pcsx2), (1, port)):
        for r in records:
            by[r['client']][side].append(r)
    result = {}
    for client in sorted(by):
        a, b = by[client]
        n = min(len(a), len(b))
        first = next((k for k in range(n) if tuple(a[k][f] for f in KEY) != tuple(b[k][f] for f in KEY)), None)
        differ = sum(1 for k in range(n) if tuple(a[k][f] for f in KEY) != tuple(b[k][f] for f in KEY))
        result[client] = {'pcsx2': len(a), 'port': len(b), 'compared': n, 'differ': differ,
                          'equal': n if first is None else first, 'first_difference': first,
                          'context': None if first is None else
                          [(describe(a[k]), describe(b[k])) for k in range(max(0, first - 2), min(n, first + 3))]}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    parser.add_argument('--session', required=True, help='The PCSX2 session with an rpc.log.json.')
    parser.add_argument('--port', type=Path, required=True, help='The port\'s BT2_RPC_LOG file.')
    parser.add_argument('--out', type=Path, help='Write the result as JSON.')
    args = parser.parse_args()
    pcsx2 = pcsx2_records(ROOT / 'work/pcsx2-compare' / args.session / 'rpc.log.json')
    port, services = port_records(args.port)
    result = compare(pcsx2, port)
    print(f'{len(pcsx2)} records in PCSX2, {len(port)} in the port')
    for client, r in result.items():
        r['service'] = services.get(client)
        state = ('equal' if r['first_difference'] is None
                 else f"{r['differ']} differ, the first at {r['first_difference']}")
        service = f"service {r['service']:#x}" if r['service'] is not None else 'service not bound in the port'
        print(f"client {client:#x} ({service}): PCSX2 {r['pcsx2']}, port {r['port']}, compared {r['compared']}: {state}")
        for left, right in r['context'] or []:
            print(f'    PCSX2 {left}\n    port  {right}')
    if args.out:
        args.out.write_text(json.dumps({f'{c:#x}': r for c, r in result.items()}, indent=1), encoding='utf-8')
    return 0 if all(r['first_difference'] is None for r in result.values()) else 1


if __name__ == '__main__':
    sys.exit(main())
