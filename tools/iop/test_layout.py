import hashlib,json,tempfile,unittest
from pathlib import Path
from verify_layout import verify
from contextlib import redirect_stdout
from io import StringIO

class LayoutTests(unittest.TestCase):
    def fixture(self,root):
        folder=root/'work/iop';folder.mkdir(parents=True)
        for n in range(14):
            p=folder/f'module{n}_native';source='// synthetic\n';data=bytes(4)
            p.with_suffix('.cpp').write_text(source,encoding='utf-8');p.with_suffix('.bin').write_bytes(data)
            p.with_suffix('.json').write_text(json.dumps({'base':0x1000*(n+1),'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'image_sha256':hashlib.sha256(data).hexdigest()}),encoding='utf-8')
        return folder
    def test_conflicting_heap_stack_module_and_out_of_ram(self):
        with tempfile.TemporaryDirectory() as d,redirect_stdout(StringIO()):
            root=Path(d);folder=self.fixture(root);self.assertEqual(len(verify(root)),16)
            p=folder/'module0_native.json';original=json.loads(p.read_text())
            for base in [0x2000,0xCE500,0x1E0000,0x200000]:
                changed={**original,'base':base};p.write_text(json.dumps(changed))
                with self.assertRaises(ValueError):verify(root)
    def test_provenance_and_missing_image(self):
        with tempfile.TemporaryDirectory() as d,redirect_stdout(StringIO()):
            root=Path(d);folder=self.fixture(root);p=folder/'module0_native.cpp';p.write_text('// altered\n')
            with self.assertRaises(ValueError):verify(root)
            p.write_text('// synthetic\n');(folder/'module0_native.bin').unlink()
            with self.assertRaises(ValueError):verify(root)
