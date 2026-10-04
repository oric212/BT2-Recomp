"""Validate scoped MCMAN resident data against an explicit local capture.
No timing, card access or authentication equivalence is inferred.
"""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from inspect_irx import inspect,relocate
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
from capture_ee import EXPECTED_SHA,MARKER,capture_trap

def verify(native,ram,metadata,iop,ee,module):
    def require(ok,message):
        if not ok:raise ValueError(message)
    def word(data,p):return struct.unpack_from('<I',data,p)[0]
    address,trap,marker=capture_trap('mcman-module-after')
    require(len(ram)==len(iop)==2*1024*1024 and len(ee)==32*1024*1024,'RAM extent differs')
    require(metadata['source_elf_sha256']==EXPECTED_SHA and metadata['capture_point'].startswith('mcman-module-after:'),'Reference identity differs')
    require(hashlib.sha256(iop).hexdigest()==metadata['iop_capture_sha256'] and hashlib.sha256(ee).hexdigest()==metadata['capture_sha256'],'Capture hash differs')
    require(ee[address:address+len(trap)]==trap and word(ee,MARKER)==marker,'Reference trap differs')
    require(native['entry_returned'] and native['entry_result']==word(ee,0x3ACA04)==2 and word(ee,0x3ACA00)>0,'Resident removable result differs')
    report=inspect(module);base=0x4D030;owned=0x20000
    require(word(iop,base-24)==base and word(iop,base-32)==base+report['entry'] and word(iop,base-28)==base+report['gp'],'Module ownership differs')
    for data,placement in ((iop,base),(ram,owned)):
        image=relocate(module,report,placement)
        require(all(data[placement+a:placement+b]==image[a:b] for a,b in report['code_ranges']),'Relocated code differs')
    ids={}
    semaphores={s['id']:s for s in native['semaphore_objects']}
    for offset in (0x13F10,0x25228):
        handle=word(iop,base+offset);p=((handle>>7)<<2)+8
        require(p<=len(iop)-32 and word(iop,p)==(((handle>>1)&63)<<16)|0x7F02,'Reference semaphore ownership differs')
        id=word(ram,owned+offset);s=semaphores.get(id)
        require(s is not None and s['attr']==word(iop,p+4)==1 and s['count']==word(iop,p+24)==1 and s['maximum']==word(iop,p+28)==1,'Priority semaphore differs')
        ids[offset]=id
    require(word(iop,base-20)==0x11C50,'Reference text extent differs')
    for offset in range(0x11C50,report['memory_size'],4):
        a=word(ram,owned+offset);b=word(iop,base+offset)
        if offset in ids:b=ids[offset]
        elif a!=b and owned<=a<owned+report['memory_size'] and base<=b<base+report['memory_size']:
            # Reached startup also stores module-local pointers into BSS.
            # Compare exact module-relative values; do not normalize equal scalar words.
            b=b-base+owned
        require(a==b,'Normalized MCMAN data/BSS differs at '+hex(offset))
    require(native['mapped_ranges_entered']>=13 and len(native['threads'])==1 and native['threads'][0]['waiting'],'Native callback/worker progress differs')
    return {'verified':'Relocated code and full initialized data/BSS with observed module-local address values and two owned priority semaphores normalized','entry_result':2,'module_sha256':report['sha256'],'reference_iop_sha256':metadata['iop_capture_sha256'],'native_ram_sha256':hashlib.sha256(ram).hexdigest(),'limitations':'Reference IOP is live; kernel layout/base are scenario-specific. Opaque options, scheduling, authentication and card transfers are not equated.'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('native','reference','module','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    if not a.output.resolve().is_relative_to(root/'work/iop'):raise ValueError('Derived output must stay under ignored work/iop/')
    result=verify(json.loads(a.native.read_text(encoding='utf-8')),Path(str(a.native)+'.ram.bin').read_bytes(),json.loads(a.reference.read_text(encoding='utf-8')),a.reference.with_name('iop-ram.bin').read_bytes(),a.reference.with_name('ee-ram.bin').read_bytes(),a.module.read_bytes())
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('Scoped native MCMAN resident effects match normalized local reference evidence.')
if __name__=='__main__':main()
