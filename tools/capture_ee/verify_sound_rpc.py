"""Compare the bounded SDRDRV bind/request against independent local evidence."""
import argparse,hashlib,json,struct,sys
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def completed_directory_diagnostics(r):
    names=['ZS2US_1.AFS','ZS2US_2.AFS','ZS2USOP.SFD','ZS2USED.SFD','LOGO_O.SFD']
    expected=['DVCI: "'+name+'" found.\n' for name in names]+['DVCI: Total 5 files.\n']
    return (r.get('console_messages')==len(r.get('console_messages_text',[])) and r.get('console_messages_text',[])[:6]==expected and r.get('disc_rpc_calls')==6 and r.get('disc_dma_submissions')==6 and r.get('ncmd_read_calls',0)>=1 and r.get('disc_read_calls')==r.get('ncmd_read_calls') and r.get('native_iop_error')=='')

def require(condition,message):
    if not condition:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)
def capture(root,point,baseline):
    f=root/'work/pcsx2-capture'/point;m=json.loads((f/'capture.json').read_text(encoding='utf-8'));ee=(f/'ee-ram.bin').read_bytes();iop=(f/'iop-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point);stub=texture_stub(point)
    require(len(ee)==32*1024*1024 and len(iop)==2*1024*1024,'Sound capture lengths differ')
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'] and hashlib.sha256(iop).hexdigest()==m['iop_capture_sha256'],'Sound capture identity/hash differs')
    require(all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')) and m['capture_point'].startswith(point+':'),'Sound capture environment differs')
    require(ee[address:address+len(trap)]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Sound capture hook/publication differs')
    return ee,iop

def later_dbc_dma_boundary(r):
    """Recognize only the later, pending reverse-DMA failure, never a sound failure."""
    return (r.get('dbc_rpc_binds')==4 and r.get('dbc_rpc_calls')==1
        and r.get('native_iop_error')=='Unresolved IOP import: sifman:7'
        and r.get('native_iop_pc')==0x621DC and r.get('selector')==0x77 and r.get('pc')==0x32F028
        and r.get('packet_words',[])[6:10]==[36,0x3A03C0,0x80001304,144])

def queued_sound_bind_completed(r):
    """Recognize the owned earlier bind/retained child, independent of later boundary."""
    return (r.get('queued_sound_submissions')==1 and r.get('queued_sound_completions')==1
        and r.get('sound_thread_waiting')==1 and r.get('sound_dispatch_bind_id')==29
        and r.get('sound_dispatch_wait_ra')==0x332874 and r.get('sound_dispatch_wait_sp')==0x1FFECB0
        and r.get('ee_sound_probe_attempted')==0 and r.get('ee_sound_probe_waits')==0
        and r.get('ee_sound_probe_parent_pc')==0 and r.get('ee_sound_bind_returns')==1
        and r.get('pending_ee_bind')==[0]*5 and r.get('native_iop_error')==''
        and r.get('ee_sound_context')==[3,0,0x32EB88,0x38BDF0,0x3A6C80,0x333098,0,0x10001]
        and r.get('ee_sound_probe_frontier')==[0x32EB88,0x38BDF0,0x3A6C80,0x333098,0x3A6D00,0x10001]
        and r.get('sound_thread_words',[])[7:9]==[1,0]
        and len(r.get('sound_iop_context',[]))==5
        and r['sound_iop_context']==[0xA1304,0xA0F24,0xA94E0,0x1E7588,0])

def completed_card_query(r):
    return (r.get('mc_query_requests')==1 and r.get('mc_dma_submissions')==1 and r.get('mc_dma_polls')==1
        and r.get('timed_waits')==5 and r.get('timed_wakes')==5 and r.get('timed_dispatches')==1
        and r.get('logical_time_us')==1900 and r.get('card_alarm_fires')==4 and r.get('card_alarm_sets')==5
        and r.get('sio2_card_fifo_packets')==5 and r.get('sio2_card_transfer_counts')==[6,6]
        and ((r.get('pending_ee_bind')==[0x2000E,0xA1CA0,0x80000704,0x1FF400,2]
            and not r.get('queued_sound_submissions',0) and not r.get('queued_sound_completions',0))
            or queued_sound_bind_completed(r)))

def later_mc_dma_boundary(r):
    return (r.get('dbc_rpc_binds')==4 and r.get('dbc_rpc_calls')==4 and r.get('mc_rpc_binds')==1
        and r.get('mc_rpc_calls')==1 and r.get('mc_query_requests')==0
        and ((r.get('native_iop_error')=='Unresolved IOP import: dmacman:32' and r.get('native_iop_pc')==0x10E30)
            or (r.get('native_iop_error')=='Unresolved IOP import: thbase:33' and r.get('native_iop_pc')==0x31B40
                and r.get('sio2_card_transfer_counts')==[6,6] and r.get('native_iop_import_arguments',[None])[0]==300))
        and r.get('selector')==0x77 and r.get('pc')==0x32F028
        and r.get('packet_words',[])[6:10]==[41,0x3A6F80,1,48])

def later_iop_boundary(r):return completed_card_query(r) or later_dbc_dma_boundary(r) or later_mc_dma_boundary(r)

def verify(root,native=None):
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    bind,_=capture(root,'sound-bind-after',baseline);before,_=capture(root,'sound-init-before',baseline);after,iop=capture(root,'sound-init-after',baseline)
    require(words(bind,MARKER+8,4)==(0,0,0x1FFECF0,0),'Sound bind return differs')
    require(words(bind,0x3A5C80,10)==(0,27,7,0,0,0x8A260,0,0,0,0x89C10),'Sound bind client differs')
    require(words(before,MARKER+16,3)==(0x1FFECD0,0,1) and words(before,MARKER+28,1)==(0xE620,),'Sound request caller differs')
    require(words(after,MARKER+8,4)==(0,0,0x1FFECD0,0) and words(after,0x3A5C40,4)==(0,0,0,0),'Sound response differs')
    require(words(after,0x3A5C80,10)==(0,28,7,0,0,0x8A260,0x38BDF0,0,0x3A5C40,0x89C10),'Sound completed client differs')
    require(words(after,0x3A97E0,6)==(0xE620,64,0x3A5C40,16,1,0x89C10),'Sound request fields differ')
    sys.path.insert(0,str(root/'tools/iop'));from inspect_irx import inspect,relocate
    module=(root/'rom/IRX/SDRDRV.IRX').read_bytes();report=inspect(module);refbase=0x88630;owned=0xA0000
    image=relocate(module,report,refbase)
    require(all(iop[refbase+a:refbase+b]==image[a:b] for a,b in report['code_ranges']),'Reference SDRDRV code differs')
    handle=words(iop,refbase+0x14CC,1)[0];p=((handle>>7)<<2)+8
    require(p<=len(iop)-72 and words(iop,p,1)==((((handle>>1)&63)<<16)|0x7F01,),'Sound-created thread ownership differs')
    expected=(words(iop,p+48,1)[0]-refbase,words(iop,p+60,1)[0]-refbase,words(iop,p+56,1)[0],words(iop,p+4,1)[0]>>16,words(iop,p+64,1)[0])
    require(expected==(0xFF0,report['gp'],0x800,24,0x2000000) and words(iop,p+4,1)[0]&255==1,'Sound-created started helper thread parameters/state differ')
    second,second_iop=capture(root,'sound-service-bind-after',baseline)
    require(words(second,MARKER+8,4)==(0,0,0x1FFED20,0),'SOUNDS bind return differs')
    client=words(second,0x6351C0,10)
    require(client[:5]==(0,29,7,0,0) and client[6:9]==(0,0,0) and client[5] and client[9],'SOUNDS bind client differs')
    require(words(second,0x3A97DC,2)==(0x6351C0,0x02000004) and words(second_iop,client[9],1)==(0x02000004,),'Retail SOUNDS registration differs')
    thread_before,_=capture(root,'sound-ee-thread-before',baseline)
    thread,_=capture(root,'sound-ee-thread-after',baseline)
    require(words(thread,MARKER+8,2)==(3,0) and words(thread,0x3681D4,1)==(3,),'Retail EE StartThread return differs')
    require(words(thread_before,MARKER+40,1)==(0x70030C11,) and words(thread,MARKER+40,1)==(0x70030C11,),'Retail thread CP0 Status differs')
    console,_=capture(root,'deci2-after',baseline)
    require(words(console,MARKER+8,2)==(0xFFFFFFFF,0xFFFFFFFF) and words(console,MARKER+24,1)==(16,),'DECI2 return/subfunction differs')
    param=words(console,MARKER+28,1)[0];text=words(console,param,1)[0];end=console.find(b'\0',text,text+4096)
    require(end>=text,'DECI2 text is not terminated')
    require(console[text:end].decode('ascii'),'Empty DECI2 diagnostic')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'));ram=Path(str(native)+'.iop.bin').read_bytes()
        require((r.get('sound_thread_starts')==1 or (r['target']==0x32FDA0 and r['source_pc']==0x315210)) and (r['console_messages']==0 or completed_directory_diagnostics(r)),'Native checked EE thread-start state differs')
        require(r['sound_rpc_binds']==1 and r['sound_rpc_calls']==1 and (not r['native_iop_error'] or later_iop_boundary(r)),'Native sound lifecycle differs')
        require(r['sound_client_words'][5]==owned+0x1C30 and r['sound_client_words'][9]==owned+0x15E0,'Native sound server mapping differs')
        t=r['sound_thread_words'];require(len(t)==9 and (t[1]-owned,t[2]-owned,t[3],t[4],t[5])==expected and (t[7:]==[0,1] or (completed_card_query(r) and t[7:]==[1,0])),'Native sound-created started helper thread differs')
        require(words(ram,owned+0x14CC,1)==(t[0],) and words(ram,owned+0x14E0,4)==words(after,0x3A5C40,4),'Native handler thread/result differs')
        image=relocate(module,report,owned)
        require(all(ram[owned+a:owned+b]==image[a:b] for a,b in report['code_ranges']),'Native SDRDRV code differs')
        for offset in (12,28,32,36,40,44,48,52):require(words(ram,owned+0x15E0+offset,1)==words(iop,refbase+0x15E0+offset,1),'Native request metadata differs')
        require(words(ram,owned+0x15D0,1)==(owned+0x15E0,) and words(ram,owned+0x15C4,1)==(0,) and words(ram,owned+0x15CC,1)==(0,),'Native queue completion differs')
    print('SDRDRV bind, first command/result and started helper thread verified'+(' against native execution.' if native else '.'))
    print('Shared live RPC reply, undefined allocator tail, ready/running scheduling equivalence, queue timing and audio playback are not equated.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
