"""Compare local memory-module and heap-free returns with the bounded native run."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from capture_ee import MARKER,capture_trap

def verify(root,native=None):
    sys.path.insert(0,str(root/'tools/iop'))
    from generate_embedded import validate
    folder=root/'work/pcsx2-capture/module-buffer-after'
    metadata=json.loads((folder/'capture.json').read_text(encoding='utf-8'))
    ee=(folder/'ee-ram.bin').read_bytes();iop=(folder/'iop-ram.bin').read_bytes()
    data=(root/'work/bt2-generation/guest-memory.bin').read_bytes()[0x369F90:0x369F90+864]
    validate(data,ee,iop,metadata)
    free=folder.parent/'heap-free-after';fm=json.loads((free/'capture.json').read_text(encoding='utf-8'));fe=(free/'ee-ram.bin').read_bytes()
    if any(metadata[k]!=fm[k] for k in ('source_elf_sha256','disc_sha256','bios_sha256','pcsx2_sha256')) or hashlib.sha256(fe).hexdigest()!=fm['capture_sha256']:
        raise ValueError('Heap-free capture environment/RAM differs')
    address,trap,marker=capture_trap('heap-free-after')
    if fe[address:address+len(trap)]!=trap or struct.unpack_from('<I',fe,MARKER)[0]!=marker or struct.unpack_from('<3I',fe,MARKER+8)!=(0,0,0x1FFECD0):raise ValueError('Heap-free return/stack differs')
    if struct.unpack_from('<I',fe,0x3AC8C0)[0]!=0xCE500 or struct.unpack_from('<I',fe,0x3AC880)[0]:raise ValueError('Heap-free input/result differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'));ram=Path(str(native)+'.iop.bin').read_bytes()
        if r['native_module_loads']<14 or r['heap_frees']!=1 or r['native_iop_error'] or r['native_iop_steps']<30902 or (r['heap_calls']==1 and ram[0xCE930:0xCE970]!=iop[0xCE930:0xCE970]):raise ValueError('Native buffer completion/free differs')
        if r.get('dmac_startup_writes')==3:
            dm=folder.parent/'dmac-startup-after';de=(dm/'ee-ram.bin').read_bytes();meta=json.loads((dm/'capture.json').read_text(encoding='utf-8'))
            address,trap,marker=capture_trap('dmac-startup-after')
            if any(metadata[k]!=meta[k] for k in ('source_elf_sha256','disc_sha256','bios_sha256','pcsx2_sha256')) or hashlib.sha256(de).hexdigest()!=meta['capture_sha256'] or de[address:address+len(trap)]!=trap or struct.unpack_from('<I',de,MARKER)[0]!=marker or struct.unpack_from('<I',de,MARKER+16)[0]!=0x1FFED40:
                raise ValueError('DMAC caller capture differs')
            if r['dmac_control']!=1 or r['dmac_priority']!=0x83FF0002:raise ValueError('Native DMAC setup differs')
    print('Embedded code/nonresident result and heap-free return verified'+(' against native execution.' if native else '.'))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
