"""Validate the original 0x40C request and its current native preemption blocker."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub

def require(ok,message):
    if not ok:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)
def verify(root,native):
    captured={}
    for point in ('rpc-cri-third-before','rpc-cri-fourth-before'):
        folder=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((folder/'capture.json').read_text(encoding='utf-8'));ee=(folder/'ee-ram.bin').read_bytes()
        require(m['source_elf_sha256']==EXPECTED_SHA and hashlib.sha256(ee).hexdigest()==m['capture_sha256'],'CRI command capture identity differs')
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(ee[address:address+8]==trap and ee[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(ee,MARKER,1)==(marker,),'CRI command hook differs')
        captured[point]=ee
    before=captured['rpc-cri-third-before'];after=captured['rpc-cri-fourth-before']
    require(words(before,MARKER+24,9)==(0x355C70,0x40C,0,0x39DC80,12,0x39DD80,12,0,0x2FD1F8),'Retail 0x40C request differs')
    require(words(before,0x39DC80,3)==(0xBB80,0xFFFF,0xFFFF),'Retail 0x40C input differs')
    require(words(after,0x39DD80,3)==(0,1,0) and words(after,MARKER+28,1)==(0x422,),'Retail subsequent command/result differs')
    r=json.loads(native.read_text(encoding='utf-8'));ee=Path(str(native)+'.ee.bin').read_bytes();iop=Path(str(native)+'.iop.bin').read_bytes()
    require(r['selector']==0x77 and r['pc']==0x32F028 and r['cri_rpc_calls']==2 and r['native_iop_pc']==0x1745F0,'Native 0x40C boundary differs')
    require(r['native_iop_error']=='Higher-priority nested IOP start needs preemption','Native preemption rejection differs')
    require(r['indexed_cache_tag_reads']==256 and r['indexed_cache_operations']==256,'Native prior cache scans incomplete')
    require(words(ee,0x39DC80,3)==(0xBB80,0xFFFF,0xFFFF) and words(iop,0x1767C0,3)==(0xBB80,0xFFFF,0xFFFF),'Original handler input transfer differs')
    require(r['packet_words'][6:12]==[56,0x355C70,0x40C,12,0x39DD80,12],'Native request ABI differs')
    print('Verified actual CRI 0x40C execution stops at higher-priority StartThread; no native result/completion claimed.')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--native',required=True,type=Path);args=parser.parse_args()
    verify(Path(__file__).resolve().parents[2],args.native)
