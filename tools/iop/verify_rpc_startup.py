"""Compare observed native RPC-module startup with explicit local captures."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from inspect_irx import inspect,relocate
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
PROFILES={
 'sio2d':{'base':0x7D630,'owned':0x70000,'text':0x1B20,'result':0,'workers':[(0x1BE8,None)],'semaphores':[0x1BEC,0x1BF0],'live':[0x1BD8]},
 'mcserv':{'base':0x74430,'owned':0x50000,'text':0x1240,'result':2,'workers':[(0x36D8,0x36E0)],'semaphores':[]},
 'dbcman':{'base':0x77C30,'owned':0x60000,'text':0x22A0,'result':0,'workers':[(0x2EF8+n*4,0x2D80+n*24) for n in range(4)],'semaphores':[0x2770]},
}
def verify(profile,native,ram,metadata,iop,ee,module):
    def require(ok,message):
        if not ok:raise ValueError(message)
    def word(data,p):return struct.unpack_from('<I',data,p)[0]
    config=PROFILES[profile];base=config['base'];owned=config['owned'];point=profile+'-module-after'
    address,trap,marker=capture_trap(point)
    require(len(ram)==len(iop)==2*1024*1024 and len(ee)==32*1024*1024,'RAM extent differs')
    require(metadata['source_elf_sha256']==EXPECTED_SHA and metadata['capture_point'].startswith(point+':'),'Reference identity differs')
    require(hashlib.sha256(iop).hexdigest()==metadata['iop_capture_sha256'] and hashlib.sha256(ee).hexdigest()==metadata['capture_sha256'],'Capture hash differs')
    require(ee[address:address+len(trap)]==trap and word(ee,MARKER)==marker,'Reference trap differs')
    require(native['entry_returned'] and native['entry_result']==word(ee,0x3ACA04)==config['result'] and word(ee,0x3ACA00)>0,'Resident result differs')
    report=inspect(module)
    require(word(iop,base-24)==base and word(iop,base-32)==base+report['entry'] and word(iop,base-28)==base+report['gp'] and word(iop,base-20)==config['text'],'Reference module ownership/text differs')
    for data,placement in ((iop,base),(ram,owned)):
        image=relocate(module,report,placement)
        require(all(data[placement+a:placement+b]==image[a:b] for a,b in report['code_ranges']),'Relocated code differs')
    ids={};threads={t['id']:t for t in native['threads']}
    def record(handle,tag):
        p=((handle>>7)<<2)+8
        require(p<=len(iop)-80 and word(iop,p)==(((handle>>1)&63)<<16)|tag,'Reference kernel record differs')
        return p
    for field,queue in config['workers']:
        p=record(word(iop,base+field),0x7F01);id=word(ram,owned+field);t=threads.get(id)
        require(t is not None and t['waiting'],'Native worker ownership differs')
        require(t['entry']-owned==word(iop,p+48)-base and t['gp']-owned==word(iop,p+60)-base and t['priority']==word(iop,p+4)>>16 and t['stacksize']==word(iop,p+56) and t['attr']==word(iop,p+64),'Native worker parameters differ')
        require((word(iop,p+4)&255)==4,'Reference worker wait state differs')
        if queue is None:
            require(not t['rpc_queue'] and t['event'] and t['bits']==4 and t['mode']==1 and not t['result'],'Native Vblank wait differs')
            require(word(iop,p+20)==4 and word(iop,p+32)==4 and word(iop,p+36)&0xffff==1,'Reference Vblank wait differs')
            ep=word(iop,p+24)+8
            require(ep<=len(iop)-32 and word(iop,ep)&0xffff==0x7F03,'Reference event record differs')
            event=next((e for e in native['event_objects'] if e['id']==t['event']),None)
            require(event is not None and event['attr']==word(iop,ep+4)==2 and not event['bits']&4 and not word(iop,ep+24)&4,'Vblank event/end absence differs')
            ids[field]=id
        else:
            require(t['rpc_queue']==owned+queue and not t['event'] and word(iop,p+20)==1,'RPC worker sleep/queue differs')
            require(word(iop,base+queue)==word(iop,base+field),'Reference queue thread differs')
            ids[field]=ids[queue]=id
    semaphores={s['id']:s for s in native['semaphore_objects']}
    for field in config['semaphores']:
        p=record(word(iop,base+field),0x7F02);id=word(ram,owned+field);s=semaphores.get(id)
        require(s is not None and s['attr']==word(iop,p+4) and s['count']==word(iop,p+24) and s['maximum']==word(iop,p+28),'Native semaphore differs')
        ids[field]=id
    for offset in range(config['text'],report['memory_size'],4):
        a=word(ram,owned+offset);b=word(iop,base+offset)
        if offset in config.get('live',[]):continue
        if offset in ids:b=ids[offset]
        elif a!=b and owned<=a<owned+report['memory_size'] and base<=b<base+report['memory_size']:b=b-base+owned
        require(a==b,'Normalized module data/BSS differs at '+hex(offset))
    return {'verified':'Relocated code, normalized data/BSS and guarded worker ownership/parameters/wait state','module_sha256':report['sha256'],'entry_result':config['result'],'workers':len(config['workers']),'reference_iop_sha256':metadata['iop_capture_sha256'],'native_ram_sha256':hashlib.sha256(ram).hexdigest(),'excluded_live_words':{hex(o):{'native':word(ram,owned+o),'reference':word(iop,base+o)} for o in config.get('live',[])},'limitations':'Vblank phase/timing and excluded live counter are not validated. Scenario-specific kernel layout/base. Full IOP RAM is live; opaque options, physical timing/DMA and RPC request processing are not validated.'}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',choices=PROFILES,required=True)
    for key in ('native','reference','module','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    if not a.output.resolve().is_relative_to(root/'work/iop'):raise ValueError('Derived output must stay under ignored work/iop/')
    result=verify(a.profile,json.loads(a.native.read_text(encoding='utf-8')),Path(str(a.native)+'.ram.bin').read_bytes(),json.loads(a.reference.read_text(encoding='utf-8')),a.reference.with_name('iop-ram.bin').read_bytes(),a.reference.with_name('ee-ram.bin').read_bytes(),a.module.read_bytes())
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('Scoped '+a.profile+' startup matches normalized local reference evidence.')
if __name__=='__main__':main()
