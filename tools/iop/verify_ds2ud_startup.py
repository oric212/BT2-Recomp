"""Validate scoped DS2U_D resident/worker effects and direct local poll evidence."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from inspect_irx import inspect,relocate
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
from capture_pad_poll import hook,HOOK,CODE,SCRATCH

def verify(native,ram,metadata,iop,ee,module,poll,polldata):
    def require(ok,message):
        if not ok:raise ValueError(message)
    def word(d,p):return struct.unpack_from('<I',d,p)[0]
    base=0x80430;owned=0x80000;address,trap,marker=capture_trap('ds2ud-module-after')
    require(len(ram)==len(iop)==2*1024*1024 and len(ee)==32*1024*1024,'RAM extent differs')
    require(metadata['source_elf_sha256']==EXPECTED_SHA and metadata['capture_point'].startswith('ds2ud-module-after:'),'Reference identity differs')
    require(hashlib.sha256(iop).hexdigest()==metadata['iop_capture_sha256'] and hashlib.sha256(ee).hexdigest()==metadata['capture_sha256'],'Capture hashes differ')
    require(ee[address:address+len(trap)]==trap and word(ee,MARKER)==marker,'Caller trap differs')
    require(native['entry_returned'] and native['entry_result']==word(ee,0x3ACA04)==0,'Resident return differs')
    report=inspect(module)
    require(word(iop,base-24)==base and word(iop,base-32)==base and word(iop,base-28)==base+report['gp'] and word(iop,base-20)==0x21C0,'Reference module differs')
    for data,placement in [(iop,base),(ram,owned)]:
        image=relocate(module,report,placement)
        require(all(data[placement+a:placement+b]==image[a:b] for a,b in report['code_ranges']),'Relocated code differs')
    threads={t['id']:t for t in native['threads']}
    for field,connected in [(0x22C4,True),(0x2480,False)]:
        handle=word(iop,base+field);p=((handle>>7)<<2)+8;t=threads[word(ram,owned+field)]
        require(p<=len(iop)-80 and word(iop,p)==(((handle>>1)&63)<<16)|0x7F01,'Reference worker record differs')
        require(t['entry']-owned==word(iop,p+48)-base==0x1DC and t['gp']-owned==word(iop,p+60)-base==report['gp'] and t['priority']==46 and t['stacksize']==word(iop,p+56)==0x800 and t['attr']==word(iop,p+64)==0x2000000,'Worker parameters differ')
        if connected:
            require(t['waiting'] and t['event'] and t['bits']==word(iop,p+32)==1 and t['mode']==word(iop,p+36)&0xffff==1 and word(iop,p+20)==4 and word(iop,p+4)&255==4,'Start-phase wait differs')
        else:require(not t['waiting'] and t['pc']==0 and word(iop,p+4)==16,'Disconnected worker is not dormant')
    require(len(polldata)==2*1024*1024 and poll['disc_sha256']==metadata['disc_sha256'],'Poll extent/disc identity differs')
    count=poll['read_count'];require(count==10,'Both initial ports must be captured')
    require(poll['source_sha256']==EXPECTED_SHA and hashlib.sha256(polldata).hexdigest()==poll['iop_sha256'],'Poll identity/hash differs')
    require(polldata[CODE:CODE+len(hook(count))]==hook(count) and poll['hook_sha256']==hashlib.sha256(hook(count)).hexdigest(),'Poll hook differs')
    require(polldata[HOOK:HOOK+8]==struct.pack('<2I',0x08000000|(CODE>>2),0) and word(polldata,SCRATCH)==count,'Poll publication differs')
    reply=list(polldata[SCRATCH+4:SCRATCH+4+count]);require(reply==poll['reply']==[255,65,90,255,255]+[255]*5,'Observed poll replies differ')
    require(poll['registers']==dict(zip(['control','command_status','port_status','fifo_status','irq_status'],struct.unpack_from('<5I',polldata,SCRATCH+0x20)))=={'control':0x3BD,'command_status':0x2D100,'port_status':15,'fifo_status':0,'irq_status':0},'Poll registers differ')
    return {'verified':'Resident return, relocated code, owned worker parameters/start wait/dormant state and direct first two poll evidence','module_sha256':report['sha256'],'limitations':'Live DS2U_D data/BSS evolves under Vblank and is not equated. Input is scoped to initial digital/no-buttons port 0 and disconnected port 1; physical timing, later controller commands and gameplay are not validated.'}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('native','reference','module','poll','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    if not a.output.resolve().is_relative_to(root/'work/iop'):raise ValueError('Output must stay in ignored work/iop')
    result=verify(json.loads(a.native.read_text()),Path(str(a.native)+'.ram.bin').read_bytes(),json.loads(a.reference.read_text()),a.reference.with_name('iop-ram.bin').read_bytes(),a.reference.with_name('ee-ram.bin').read_bytes(),a.module.read_bytes(),json.loads(a.poll.read_text()),a.poll.with_name('pad-poll-iop.bin').read_bytes())
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('Scoped DS2U_D startup and both initial polls verified.')
if __name__=='__main__':main()
