import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rpc_compare as rc  # noqa: E402
import rpc_log  # noqa: E402


class RpcCompareTest(unittest.TestCase):
    def test_reads_both_logs_and_compares_per_client(self):
        with tempfile.TemporaryDirectory() as folder:
            pcsx2_file, port_file = Path(folder) / 'rpc.log.json', Path(folder) / 'port.txt'
            pcsx2_file.write_text(json.dumps([
                {'number': 1, 'count': 10, 'function': 0x10, 'client': 0x3B0000, 'mode': 0, 'size': 16, 'address': 0x400, 'checksum': 5},
                {'number': 2, 'count': 11, 'function': 0x2, 'client': 0x3C0000, 'mode': 1, 'size': 0, 'address': 0, 'checksum': 0},
                {'number': 3, 'count': 12, 'function': 0x10, 'client': 0x3B0000, 'mode': rpc_log.REPLY, 'size': 64, 'address': 0x500, 'checksum': 7},
                {'number': 4, 'count': 13, 'function': 0x3, 'client': 0x3C0000, 'mode': 1, 'size': 0, 'address': 0, 'checksum': 0},
            ]), encoding='utf-8')
            port_file.write_text('B 80000001 3c0000 0 0 0 0 39\n'        # a bind names the client's service
                                 'Q 2 3c0000 1 0 0 0 40\n'               # another client's call first: order per client only
                                 'Q 10 3b0000 0 10 400 5 41\n'
                                 'R 10 3b0000 0 40 500 8 41\n'           # a reply that differs
                                 'Q 3 3c0000 1 0 0 0 42\n'
                                 'Q 4 3c0000 1 0 0 0 43\n', encoding='utf-8')   # a call PCSX2's run did not reach
            pcsx2, (port, services) = rc.pcsx2_records(pcsx2_file), rc.port_records(port_file)
        self.assertEqual(services, {0x3C0000: 0x80000001})
        self.assertEqual((pcsx2[2]['kind'], pcsx2[2]['function']), ('R', 0x10))
        result = rc.compare(pcsx2, port)
        self.assertEqual(result[0x3C0000]['first_difference'], None)
        self.assertEqual((result[0x3C0000]['pcsx2'], result[0x3C0000]['port'], result[0x3C0000]['compared']), (2, 3, 2))
        self.assertEqual((result[0x3B0000]['first_difference'], result[0x3B0000]['differ']), (1, 1))
        self.assertIn('sum 00000007', result[0x3B0000]['context'][1][0])
        self.assertIn('sum 00000008', result[0x3B0000]['context'][1][1])


if __name__ == '__main__':
    unittest.main()
