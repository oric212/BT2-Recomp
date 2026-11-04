import sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import package
class PackageTests(unittest.TestCase):
    def fake(self,folder):
        build,generation,crt=folder/'build',folder/'gen',folder/'crt'
        for name in package.PROGRAMS:(build/name).parent.mkdir(parents=True,exist_ok=True);(build/name).write_bytes(b'x')
        (build/'assets/struggle-buttons/xbox').mkdir(parents=True);(build/'assets/logo.png').write_bytes(b'x');(build/'assets/struggle-buttons/xbox/a.dds').write_bytes(b'x')
        generation.mkdir();crt.mkdir()
        for name in package.startup_inputs():(generation/name).write_bytes(b'x')
        for name in package.CRT:(crt/name).write_bytes(b'x')
        return build,generation,crt
    def test_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            targets=[target for _,target in package.plan(*self.fake(Path(folder)))]
            for name in ('bt2-game.exe','bt2-launcher.exe','bt2-entry-probe.exe','vcruntime140.dll','assets/logo.png','assets/struggle-buttons/xbox/a.dds',
                         'work/bt2-generation/guest-memory.bin','README.txt','ONLINE-SETUP.txt','LICENSE.txt','data/PUT YOUR GAME HERE.txt'):
                self.assertIn(name,targets)
            self.assertEqual(len(targets),len(set(targets)))
            self.assertFalse([t for t in targets if t.startswith(('user/','saves/')) or t.endswith(('.iso','.pdb'))])
    def test_game_content_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            build,generation,crt=self.fake(Path(folder))
            for name in ('game.iso','SLUS_214.41','bt2-game.pdb'):
                (build/'assets'/name).write_bytes(b'x')
                with self.assertRaises(SystemExit):package.plan(build,generation,crt)
                (build/'assets'/name).unlink()
    def test_missing_input_is_named(self):
        with tempfile.TemporaryDirectory() as folder:
            build,generation,crt=self.fake(Path(folder))
            (generation/'kernel-snapshot.bin').unlink()
            with self.assertRaises(SystemExit) as stop:package.plan(build,generation,crt)
            self.assertIn('kernel-snapshot.bin',str(stop.exception))
if __name__=='__main__':unittest.main()
