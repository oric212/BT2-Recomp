"""Compare scoped native SIO2MAN startup effects with the local reference capture.
Retail kernel record offsets below are observations of this checkpoint, not a
portable THREADMAN ABI. Opaque option values and timing are not equated.
"""
import argparse,hashlib,json,struct
from pathlib import Path
import sys
from inspect_irx import inspect,relocate
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
from capture_ee import EXPECTED_SHA,MARKER,capture_trap

def verify(native,ram,metadata,iop,ee,module):
    def require(ok,message):
        if not ok:raise ValueError(message)
    def word(data,address):return struct.unpack_from('<I',data,address)[0]
    address,trap,marker=capture_trap('loadfile-module-after')
    require(len(iop)==2*1024*1024 and len(ram)==len(iop) and len(ee)==32*1024*1024,'RAM extent differs')
    require(metadata['source_elf_sha256']==EXPECTED_SHA and metadata['capture_point'].startswith('loadfile-module-after:'),'Reference identity differs')
    require(hashlib.sha256(iop).hexdigest()==metadata['iop_capture_sha256'] and hashlib.sha256(ee).hexdigest()==metadata['capture_sha256'],'Capture hash differs')
    require(ee[address:address+len(trap)]==trap and word(ee,MARKER)==marker,'Reference trap differs')
    report=inspect(module);base=0x4BF30;owned=0x10000
    require(word(iop,base-48+24)==base and word(iop,base-48+16)==base+report['entry'] and word(iop,base-48+20)==base+report['gp'],'Module ownership differs')
    require(native['entry_returned'] and native['entry_result']==0 and struct.unpack_from('<2i',ee,0x3ACA00)[1]==0,'Module result differs')
    reference_image=relocate(module,report,base)
    require(all(iop[base+a:base+b]==reference_image[a:b] for a,b in report['code_ranges']),'Reference module code differs')
    expected=relocate(module,report,owned)
    require(all(ram[owned+a:owned+b]==expected[a:b] for a,b in report['code_ranges']),'Native code image differs')
    require(native['sio2_control_known'] and native['sio2_control']==0x3BC,'Native reset differs')
    require(len(native['threads'])==len(native['event_objects'])==len(native['semaphore_objects'])==1,'Startup object counts differ')
    t=native['threads'][0];event=native['event_objects'][0];sema=native['semaphore_objects'][0]
    def record(offset,tag):
        handle=word(iop,base+offset);p=((handle>>7)<<2)+8
        require(0<=p<=len(iop)-80,'Reference record outside RAM')
        require(word(iop,p)==(((handle>>1)&63)<<16)|tag,'Observed kernel object tag/ID differs')
        return p
    sp=record(0xFC0,0x7F02);ep=record(0xFCC,0x7F03);tp=record(0xFD0,0x7F01)
    require(sema['attr']==word(iop,sp+4)==0 and sema['count']==word(iop,sp+24)==1 and sema['maximum']==word(iop,sp+28)==64,'Semaphore state differs')
    require(event['attr']==word(iop,ep+4)==2 and event['bits']==word(iop,ep+24)==0 and word(iop,ep+8)==1,'Event state differs')
    require(t['waiting'] and t['event']==event['id'] and t['bits']==word(iop,tp+32)==0x4155 and t['mode']==(word(iop,tp+36)&65535)==1,'Thread wait differs')
    require((word(iop,tp+4)&255)==4 and word(iop,tp+20)==4 and word(iop,tp+24)==ep-8,'Reference wait ownership differs')
    require(t['priority']==word(iop,tp+4)>>16==16 and t['stacksize']==word(iop,tp+56)==0x800 and t['attr']==word(iop,tp+64)==0x02000000,'Thread parameters differ')
    require(t['entry']-owned==word(iop,tp+48)-base and t['gp']-owned==word(iop,tp+60)-base,'Thread entry/GP differs')
    require(native['dma_channels']==[{'channel':11,'priority':3,'enabled':True},{'channel':12,'priority':3,'enabled':True}],'Reached DMA setup differs')
    irq=native['interrupts'];require(len(irq)==1 and irq[0]['irq']==17 and irq[0]['enabled'] and irq[0]['mode']==1,'Native IRQ registration differs')
    ids={0xFC0:sema['id'],0xFCC:event['id'],0xFD0:t['id']}
    for offset in range(0xFB8,report['memory_size'],4):
        require(word(ram,owned+offset)==(ids[offset] if offset in ids else word(iop,base+offset)),'Normalized module BSS differs')
    return {'module_sha256':report['sha256'],'native_ram_sha256':hashlib.sha256(ram).hexdigest(),'reference_iop_sha256':metadata['iop_capture_sha256'],'entry_result':0,'thread_wait_bits':t['bits'],'verified':'Normalized module BSS, semaphore/event state and retained native thread entry/GP/priority/wait; reached reset/DMA/IRQ state checked','limitations':'Opaque options differ and are not compared. Reference kernel layout is scenario-specific; full IOP snapshot is live. Hardware reset/DMA/IRQ are public-specification checks, not captured MMIO. No LOADFILE completion, transfer, input or timing validation.'}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);p.add_argument('--reference',type=Path,required=True);p.add_argument('--module',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    root=Path(__file__).resolve().parents[2]
    if not a.output.resolve().is_relative_to(root/'work/iop'):raise ValueError('Derived validation output must stay under ignored work/iop/')
    result=verify(json.loads(a.native.read_text(encoding='utf-8')),Path(str(a.native)+'.ram.bin').read_bytes(),json.loads(a.reference.read_text(encoding='utf-8')),a.reference.with_name('iop-ram.bin').read_bytes(),a.reference.with_name('ee-ram.bin').read_bytes(),a.module.read_bytes())
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('Scoped SIO2MAN startup effects match normalized local reference evidence; hardware transfers remain unvalidated.')
if __name__=='__main__':main()
