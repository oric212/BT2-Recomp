import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import run_gate
class GateTests(unittest.TestCase):
    def test_every_phase_verifier_exists_and_is_not_run_on_the_full_report(self):
        full=run_gate.full_run_verifiers()
        for name in run_gate.phase_only():
            self.assertTrue((run_gate.ROOT/'tools/capture_ee'/f'verify_{name}.py').is_file(),name)
            self.assertNotIn(name,full)
        self.assertTrue(full)
        for name in run_gate.NOT_IN_GATE:
            self.assertTrue((run_gate.ROOT/'tools/capture_ee'/f'verify_{name}.py').is_file(),name)
            self.assertNotIn(name,full)
    def test_verifier_arguments(self):
        disc=Path('disc.iso');report=Path('report.json')
        self.assertEqual(run_gate.verifier_command('timer2',report,disc)[2:],['--native','report.json'])
        self.assertEqual(run_gate.verifier_command('disc_read',report,disc)[2:],['--native','report.json','--disc','disc.iso','--workspace','0x1D4B00'])
        self.assertEqual(run_gate.verifier_command('ee_sound_dispatch',report,disc)[-1],'--queued')
if __name__=='__main__':unittest.main()
