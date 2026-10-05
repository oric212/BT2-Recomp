"""Validate the first disc-search bind boundary, without granting file lookup success."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA,MARKER,FRESH_BIND_STUB,capture_trap,texture_stub
from verify_mc_timed import completed_card_query

def require(condition,message):
    if not condition:raise ValueError(message)
def words(data,address,count):return struct.unpack_from('<'+str(count)+'I',data,address)

def disc_bind_boundary(r):
    return (r.get('disc_bind_semaphores')==1 and r.get('selector')==0x77 and r.get('pc')==0x32F028
        and r.get('target')==0 and r.get('native_iop_error')==''
        and r.get('packet_words',[])[6:9]==[42,0x3A8780,0x80000597])

def verify(root,native):
    baseline=json.loads((root/'work/pcsx2-capture/rpc-mc-callback-before-absent-cards/capture.json').read_text(encoding='utf-8'))
    observations={}
    for point in ('rpc-disc-init-after','rpc-disc-bind-sema','rpc-disc-bind-after'):
        f=root/'work/pcsx2-capture'/(point+'-absent-cards')
        m=json.loads((f/'capture.json').read_text(encoding='utf-8'));b=(f/'ee-ram.bin').read_bytes();i=(f/'iop-ram.bin').read_bytes()
        require(m['capture_point'].startswith(point+':') and m['source_elf_sha256']==EXPECTED_SHA and len(b)==32*1024*1024 and len(i)==2*1024*1024,'Disc capture identity/size differs')
        require(hashlib.sha256(b).hexdigest()==m['capture_sha256'] and hashlib.sha256(i).hexdigest()==m['iop_capture_sha256'],'Disc capture hashes differ')
        require(all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'Disc environment differs')
        ini=(f/'profile/PCSX2/inis/PCSX2.ini').read_text(encoding='utf-8')
        require('Slot1_Enable = false' in ini and 'Slot2_Enable = false' in ini,'Disc capture card profile differs')
        address,trap,marker=capture_trap(point);stub=texture_stub(point)
        require(b[address:address+len(trap)]==trap and b[FRESH_BIND_STUB:FRESH_BIND_STUB+len(stub)]==stub and words(b,MARKER,1)==(marker,),'Disc hook/publication differs')
        observations[point]=b
    b=observations['rpc-disc-bind-sema'];sp=words(b,MARKER+16,1)[0]
    require(sp==0x1FFEB10 and words(b,MARKER+8,2)==(10,0),'Actual disc semaphore return differs')
    descriptor=words(b,sp,6)
    require((descriptor[1],descriptor[2],descriptor[5])==(1,0,0x3838D8),'Actual disc semaphore input differs')
    b=observations['rpc-disc-bind-after']
    require(words(b,MARKER+8,2)==(0,0) and words(b,0x3A8780,10)==(0,42,10,0,0,0x441C8,0,0,0,0x433D0),'Actual disc-search bind client differs')
    r=json.loads(native.read_text(encoding='utf-8'))
    require(completed_card_query(r) and r.get('disc_bind_semaphores')==1,'Native prior query/disc semaphore lifecycle differs')
    require(disc_bind_boundary(r),'Expected owned unsupported disc-search submission')
    print('Actual disc initialization/semaphore/bind evidence verified. Native search request remains unsupported; no file or sector success claimed.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path,default=Path('work/bt2-generation/entry-stop.json'));a=p.parse_args();verify(a.root,a.native)
