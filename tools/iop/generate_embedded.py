"""Validate and translate the first user-local EE embedded IRX; outputs stay private."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from inspect_irx import inspect,relocate
from generate_native import emit

def validate(data,ee,iop,metadata):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
    from capture_ee import EXPECTED_SHA,MARKER,capture_trap
    if metadata['source_elf_sha256']!=EXPECTED_SHA or not metadata['capture_point'].startswith('module-buffer-after:'):
        raise ValueError('Embedded capture identity differs')
    if len(ee)!=32*1024*1024 or len(iop)!=2*1024*1024 or hashlib.sha256(ee).hexdigest()!=metadata['capture_sha256'] or hashlib.sha256(iop).hexdigest()!=metadata['iop_capture_sha256']:
        raise ValueError('Embedded capture RAM differs')
    address,trap,marker=capture_trap('module-buffer-after')
    if ee[address:address+len(trap)]!=trap or struct.unpack_from('<I',ee,MARKER)[0]!=marker or struct.unpack_from('<3I',ee,MARKER+8)!=(38,0,0x1FFECD0):
        raise ValueError('Embedded return differs')
    if data!=ee[0x369F90:0x369F90+864] or data!=iop[0xCE500:0xCE860]:raise ValueError('Embedded provenance differs')
    r=inspect(data)
    if r['sha256']!='83cf61fe583ee0846ef3a34c7b5dfdfa7fc292f84a4d5fbb7f058e4b0bedc755' or r['imports']!=[{'library':'cdvdman','version':257,'ordinal':50,'address':84}]:raise ValueError('Embedded IRX identity/import differs')
    image=relocate(data,r,0xCE930)
    if iop[0xCE930:0xCE970]!=image[:64] or struct.unpack_from('<2i',ee,0x3ACA00)!=(38,1):raise ValueError('Embedded code/result differs')
    info=struct.unpack_from('<12I',iop,0xCE900)
    if info[1:10]!=(0xCE9A0,0x101,38,0xCE930,0xD69B0,0xCE930,0x70,0x20,0):raise ValueError('Embedded loader metadata differs')
    # Result 1 causes unlink/free in modload. Metadata/code are residual evidence,
    # not a resident module claim. The linked import itself is not compared raw.
    return r,image

def validate_second(data,r,ee,iop,metadata):
    """Main reloads the same buffer after the first archive load; the original placed it at 0x196100."""
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
    from capture_ee import EXPECTED_SHA,MARKER,capture_trap
    if metadata['source_elf_sha256']!=EXPECTED_SHA or not metadata['capture_point'].startswith('rpc-display-init-after:'):
        raise ValueError('Second embedded capture identity differs')
    if len(ee)!=32*1024*1024 or len(iop)!=2*1024*1024 or hashlib.sha256(ee).hexdigest()!=metadata['capture_sha256'] or hashlib.sha256(iop).hexdigest()!=metadata['iop_capture_sha256']:
        raise ValueError('Second embedded capture RAM differs')
    address,trap,marker=capture_trap('rpc-display-init-after')
    if ee[address:address+len(trap)]!=trap or struct.unpack_from('<I',ee,MARKER)[0]!=marker:raise ValueError('Second embedded observer differs')
    if data!=iop[0x196100:0x196100+864] or struct.unpack_from('<I',ee,0x3AC8C0)[0]!=0x196100:raise ValueError('Second embedded placement differs')
    image=relocate(data,r,0x196530)
    if iop[0x196530:0x196570]!=image[:64] or struct.unpack_from('<2i',ee,0x3ACA00)!=(39,1):raise ValueError('Second embedded code/result differs')
    info=struct.unpack_from('<12I',iop,0x196500)
    if info[1:10]!=(0x1965A0,0x101,39,0x196530,0x19E5B0,0x196530,0x70,0x20,0):raise ValueError('Second embedded loader metadata differs')
    return image

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--disc',type=Path,required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[2];elf=(root/'rom/SLUS_214.41').read_bytes()
    sys.path.insert(0,str(root/'tools/capture_ee'));from capture_ee import EXPECTED_SHA
    if hashlib.sha256(elf).hexdigest()!=EXPECTED_SHA:raise ValueError('Retail executable identity differs')
    phoff=struct.unpack_from('<I',elf,28)[0];size,count=struct.unpack_from('<HH',elf,42)
    segments=[struct.unpack_from('<8I',elf,phoff+n*size) for n in range(count)]
    found=[p for p in segments if p[0]==1 and p[2]<=0x369F90 and 0x369F90+864<=p[2]+p[4]]
    if len(found)!=1:raise ValueError('Embedded source is outside unique ELF file segment')
    p=found[0];offset=p[1]+0x369F90-p[2];data=elf[offset:offset+864]
    folder=root/'work/pcsx2-capture/module-buffer-after';metadata=json.loads((folder/'capture.json').read_text(encoding='utf-8'))
    from inspect_image import inspect as inventory,disc_image
    boot=(root/'rom/IRX/IOPRP300.IMG').read_bytes()
    if boot!=disc_image(args.disc,b'IOPRP300.IMG;1'):raise ValueError('Boot image differs from local disc')
    with args.disc.open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=metadata['disc_sha256']:raise ValueError('Capture disc differs')
    row=next(r for r in inventory(boot)['entries'] if r['name']=='CDVDMAN');cd=boot[row['offset']:row['offset']+row['size']]
    off=struct.unpack_from('<I',cd,28)[0];stride,count=struct.unpack_from('<HH',cd,42)
    headers=[struct.unpack_from('<8I',cd,off+n*stride) for n in range(count)]
    mh=next(h for h in headers if h[0]==0x70000080)
    if struct.unpack_from('<H',cd,mh[1]+24)[0]!=0x225:raise ValueError('Scoped CDVDMAN version differs')
    r,image=validate(data,(folder/'ee-ram.bin').read_bytes(),(folder/'iop-ram.bin').read_bytes(),metadata)
    second_folder=root/'work/pcsx2-capture/rpc-display-init-after-absent-cards'
    second=validate_second(data,r,(second_folder/'ee-ram.bin').read_bytes(),(second_folder/'iop-ram.bin').read_bytes(),
        json.loads((second_folder/'capture.json').read_text(encoding='utf-8')))
    source=emit(image,0xCE930,r['code_ranges'],r['imports'],{0},symbol='bt2_iop_embedded')
    source+=emit(second,0x196530,r['code_ranges'],r['imports'],{0},symbol='bt2_iop_embedded_second')
    out=root/'work/iop/embedded_native.cpp';out.write_text(source,encoding='utf-8')
    out.with_suffix('.h').write_text('#pragma once\ninline constexpr std::array<unsigned char,864> iop_embedded_file={'+','.join(map(str,data))+'};\ninline constexpr std::array<unsigned char,144> iop_embedded_image={'+','.join(map(str,image))+'};\ninline constexpr std::array<unsigned char,144> iop_embedded_second_image={'+','.join(map(str,second))+'};\n',encoding='utf-8')
    out.with_suffix('.json').write_text(json.dumps({'inspection':r,'capture':metadata,'source_elf_sha256':EXPECTED_SHA,'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'limitations':'Entry/code and nonresident reference result; no general buffer loader or CDVD startup emulation.'},indent=2)+'\n',encoding='utf-8')
    print('Validated local embedded IRX and generated ignored entry code.')
if __name__=='__main__':main()
