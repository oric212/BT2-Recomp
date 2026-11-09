import sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import package
class PackageTests(unittest.TestCase):
    def fake(self,folder):
        build,crt=folder/'build',folder/'crt'
        for name in package.PROGRAMS:(build/name).parent.mkdir(parents=True,exist_ok=True);(build/name).write_bytes(b'x')
        (build/'assets/struggle-buttons/xbox').mkdir(parents=True);(build/'assets/logo.png').write_bytes(b'x');(build/'assets/struggle-buttons/xbox/a.dds').write_bytes(b'x')
        crt.mkdir()
        for name in package.CRT:(crt/name).write_bytes(b'x')
        return build,crt
    def test_layout(self):
        with tempfile.TemporaryDirectory() as folder:
            targets=[target for _,target in package.plan(*self.fake(Path(folder)))]
            for name in ('bt2-game.exe','bt2-launcher.exe','bt2-entry-probe.exe','vcruntime140.dll','assets/struggle-buttons/xbox/a.dds',
                         'README.txt','ONLINE-SETUP.txt','LICENSE.txt','data/PUT YOUR GAME HERE.txt'):
                self.assertIn(name,targets)
            self.assertEqual(len(targets),len(set(targets)))
            self.assertNotIn('assets/logo.png',targets) # The game's title logo is not the project's to ship.
            # Nothing of the console or the game: no work folder, captures, discs or debug files; no player files.
            self.assertFalse([t for t in targets if t.startswith(('user/','saves/','work/')) or t.endswith(('.iso','.pdb','.bin'))])
    def test_game_and_console_content_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            build,crt=self.fake(Path(folder))
            for name in ('game.iso','SLUS_214.41','bt2-game.pdb','kernel-snapshot.bin'):
                (build/'assets'/name).write_bytes(b'x')
                with self.assertRaises(SystemExit):package.plan(build,crt)
                (build/'assets'/name).unlink()
    def test_missing_program_is_named(self):
        with tempfile.TemporaryDirectory() as folder:
            build,crt=self.fake(Path(folder))
            (build/'bt2-entry-probe.exe').unlink()
            with self.assertRaises(SystemExit) as stop:package.plan(build,crt)
            self.assertIn('bt2-entry-probe.exe',str(stop.exception))
if __name__=='__main__':unittest.main()
