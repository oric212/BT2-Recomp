"""Compare observed native RPC-module startup with explicit local captures."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from inspect_irx import inspect,relocate
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'capture_ee'))
from capture_ee import EXPECTED_SHA,MARKER,capture_trap
PROFILES={
 'cri-adxi':{'base':0xB7F30,'owned':0x168000,'text':0xC670,'result':2,'workers':[(0xDE18,None),(0xDE1C,0xED80)],'semaphores':[]},
 'sounds':{'base':0xA3430,'owned':0x150000,'text':0x23C0,'result':0,'workers':[],'semaphores':[],'queue_worker':(0x284,0x1F1578)},
 'cdvdstm':{'base':0x9E730,'owned':0x188000,'text':0x4280,'result':2,'workers':[],'semaphores':[0x4784],'events':[0x4788]},
 'modsesq2':{'base':0x9A930,'owned':0x180000,'text':0x3B00,'result':0,'workers':[],'semaphores':[]},
 'modhsyn':{'base':0x8A330,'owned':0xB0000,'text':0xBE60,'result':0,'workers':[],'semaphores':[],'uninitialized_tail':bytes.fromhex('0d40')},
 'sdrdrv':{'base':0x88630,'owned':0xA0000,'text':0x1330,'result':2,'workers':[(0x14C8,0x15C0)],'semaphores':[]},
 'libsd':{'base':0x82C30,'owned':0x90000,'text':0x4890,'result':0,'workers':[],'semaphores':[]},
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
    if 'queue_worker' in config:
        entry_offset,rq=config['queue_worker']
        candidates=[t for t in native['threads'] if t['entry']==owned+entry_offset]
        require(len(candidates)==1,'Native stack RPC worker differs');t=candidates[0]
        p=record(word(iop,rq),0x7F01)
        require(t['waiting'] and t['rpc_queue'] and not t['event'] and word(iop,p+20)==1 and word(iop,p+4)&255==4,'Stack RPC worker wait differs')
        require(t['entry']==word(iop,p+48)-base+owned and t['gp']==word(iop,p+60)-base+owned and t['priority']==word(iop,p+4)>>16 and t['stacksize']==word(iop,p+56) and t['attr']==word(iop,p+64),'Stack RPC worker parameters differ')
        nq=t['rpc_queue'];rs=word(iop,p+52);ns=t['stack'];size=t['stacksize']
        require(nq-ns==rq-rs and 0<=nq-ns<size-24,'Stack RPC queue placement differs')
        require(word(ram,nq)==t['id'] and word(iop,rq)>0,'Stack RPC queue owner differs')
        def normalize(value):
            if base<=value<base+report['memory_size']:return value-base+owned
            if rs<=value<rs+size:return value-rs+ns
            return value
        for off in (4,8,12,16):require(word(ram,nq+off)==normalize(word(iop,rq+off)),'Stack RPC queue state differs')
        nd=word(ram,nq+8);rd=word(iop,rq+8)
        require(ns<=nd<ns+size-68 and rs<=rd<rs+size-68,'Stack RPC server ownership differs')
        for off in (0,4,8,16,20,56,60,64):require(word(ram,nd+off)==normalize(word(iop,rd+off)),'Stack RPC server fields differ')
    semaphores={s['id']:s for s in native['semaphore_objects']}
    for field in config['semaphores']:
        p=record(word(iop,base+field),0x7F02);id=word(ram,owned+field);s=semaphores.get(id)
        require(s is not None and s['attr']==word(iop,p+4) and s['count']==word(iop,p+24) and s['maximum']==word(iop,p+28),'Native semaphore differs')
        ids[field]=id
    for field in config.get('events',[]):
        p=record(word(iop,base+field),0x7F03);id=word(ram,owned+field)
        event=next((e for e in native['event_objects'] if e['id']==id),None)
        require(event is not None and event['attr']==word(iop,p+4) and event['option']==word(iop,p+8) and event['bits']==word(iop,p+24),'Native external event differs')
        ids[field]=id
    for offset in range(config['text'],report['memory_size'],4):
        if offset+4>report['memory_size']:
            size=report['memory_size']-offset
            tail=iop[base+offset:base+offset+size]
            if 'uninitialized_tail' in config:
                require(tail==config['uninitialized_tail'] and ram[owned+offset:owned+offset+size]==bytes(size),'Guarded untouched allocator tail differs')
            else:require(ram[owned+offset:owned+offset+size]==tail,'Module data tail differs')
            continue
        a=word(ram,owned+offset);b=word(iop,base+offset)
        if offset in config.get('live',[]):continue
        if offset in ids:b=ids[offset]
        elif a!=b and owned<=a<owned+report['memory_size'] and base<=b<base+report['memory_size']:b=b-base+owned
        require(a==b,'Normalized module data/BSS differs at '+hex(offset))
    return {'verified':'Relocated code, normalized data/BSS and guarded worker ownership/parameters/wait state','module_sha256':report['sha256'],'entry_result':config['result'],'workers':len(config['workers'])+int('queue_worker' in config),'reference_iop_sha256':metadata['iop_capture_sha256'],'native_ram_sha256':hashlib.sha256(ram).hexdigest(),'excluded_uninitialized_tail_bytes':len(config.get('uninitialized_tail',b'')),'excluded_live_words':{hex(o):{'native':word(ram,owned+o),'reference':word(iop,base+o)} for o in config.get('live',[])},'limitations':'Vblank phase/timing, excluded live counter and reported untouched allocator-tail bytes are not validated. Scenario-specific kernel layout/base. Full IOP RAM is live; opaque options, physical timing/DMA and RPC request processing are not validated.'}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--profile',choices=PROFILES,required=True)
    for key in ('native','reference','module','output'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[2]
    if not a.output.resolve().is_relative_to(root/'work/iop'):raise ValueError('Derived output must stay under ignored work/iop/')
    result=verify(a.profile,json.loads(a.native.read_text(encoding='utf-8')),Path(str(a.native)+'.ram.bin').read_bytes(),json.loads(a.reference.read_text(encoding='utf-8')),a.reference.with_name('iop-ram.bin').read_bytes(),a.reference.with_name('ee-ram.bin').read_bytes(),a.module.read_bytes())
    a.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('Scoped '+a.profile+' startup matches normalized local reference evidence.')
if __name__=='__main__':main()
