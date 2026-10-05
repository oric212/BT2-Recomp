"""Inspect the new IOP allocation evidence without granting native ownership."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def verify(root):
    folder=root/'work/pcsx2-capture/heap-second-after';m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
    baseline=json.loads((folder.parent/'ee-pools-after/capture.json').read_text(encoding='utf-8'))
    address,trap,marker=capture_trap('heap-second-after');stub=texture_stub('heap-second-after')
    if len(ee)!=32*1024*1024 or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or m['source_elf_sha256']!=EXPECTED_SHA or any(m[k]!=baseline[k] for k in ['pcsx2_sha256','bios_sha256','disc_sha256']) or not m['capture_point'].startswith('heap-second-after:'):raise ValueError('Second heap capture identity differs')
    if ee[address:address+len(trap)]!=trap or ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]!=stub or struct.unpack_from('<I',ee,MARKER)[0]!=marker or struct.unpack_from('<4I',ee,MARKER+8)!=(0xCE500,0,0x1FFED30,0):raise ValueError('Second heap return/hook differs')
    if struct.unpack_from('<I',ee,0x3AC8C0)[0]!=0x81800 or struct.unpack_from('<I',ee,0x3AC880)[0]!=0xCE500 or struct.unpack_from('<10I',ee,0x3AC840)!=(0,26,7,0,0,0x48480,0x38BDF0,0,0,0x48438):raise ValueError('Second heap request/client/result differs')
    generated=json.loads((root/'work/iop/fileio_boot_native.json').read_text(encoding='utf-8'))
    start,end=0xCE500,0xCE500+0x81800
    if not start<=generated['base']<end:raise ValueError('Expected native FILEIO ownership conflict absent; inspect layout again')
    print(f'Second retail heap allocation [{start:#x}, {end:#x}) overlaps translated native FILEIO at {generated["base"]:#x}. Native allocation remains blocked.')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());a=p.parse_args();verify(a.root)
