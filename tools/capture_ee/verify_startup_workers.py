"""Compare the diagnostic helper5 boundary; no priority admission or first wait claimed."""
import argparse,hashlib,json
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_sound_rpc import queued_sound_bind_completed,require,words

def verify(root,native):
    point='rpc-helper5-critical-before';folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
    m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point);stub=texture_stub(point)
    require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],'Worker capture identity differs')
    require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'Worker observer differs')
    require(words(ee,MARKER+24,2)==(5,1) and words(ee,MARKER+16,1)==(0x34E370,) and words(ee,MARKER+48,1)==(0x38BDF0,),'Original critical request/context differs')
    r=json.loads(native.read_text(encoding='utf-8'))
    require(queued_sound_bind_completed(r) and not r['native_iop_error'],'Diagnostic lost queued sound ownership')
    require(r['selector']==0x29 and r['pc']==0x32EAF8 and r['arguments']==[5,1] and r['sp']==0x34E370,'Native worker boundary differs')
    require(r['startup_worker_dispatches']==1 and r['startup_worker_status_queries']==1 and r['startup_worker_waits']==0
        and r['startup_dispatch_parent_pc']==0x32EAF8 and r['main_thread_priority']==2,'Diagnostic dispatch scope differs')
    require(r['startup_worker_state'][1]==[5,0x2EDF48,1,0,16,0x38BDF0,0x34E370,0x2EDC54],'Native worker continuation differs')
    print('Verified diagnostic helper5 execution to its captured critical priority request. Priority admission, return and helper waits remain unimplemented.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',type=Path,required=True);a=p.parse_args();verify(Path(__file__).resolve().parents[2],a.native)
