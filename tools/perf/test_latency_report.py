import sys, tempfile, unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).parent))
import latency_report


class LatencyReportTest(unittest.TestCase):
    def test_join(self):
        with tempfile.TemporaryDirectory() as folder:
            log = Path(folder) / 'latency.csv'
            # Flip 1: flipped at 0, handed over at 10 ms, presented at 11 ms, shown at 20 ms. Flip 2 is held by 60 fps.
            # Flip 3 has a present the display never reported as shown.
            log.write_text('flip,1,0,10000,0\nflip,2,30000,40000,1\nflip,3,60000,70000,0\n', encoding='utf-8')
            Path(str(log) + '.presents').write_text('present,1,10100,11000\nshown,1,20000\npresent,2,70100,71000\n', encoding='utf-8')
            rows, delayed = latency_report.join(*latency_report.read(log))
            self.assertEqual(delayed, 1)
            self.assertEqual(rows[0], (10.0, 1.0, 9.0, 20.0))
            self.assertEqual(rows[1][:2], (10.0, 1.0))
            self.assertIsNone(rows[1][3])


if __name__ == '__main__':
    unittest.main()
