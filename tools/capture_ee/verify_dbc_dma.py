"""Verify explicitly redirected diagnostic transport and the native owned copy."""
import argparse,hashlib,json,struct
from pathlib import Path
from capture_ee import EXPECTED_SHA
from capture_dbc_dma import BASE,SCRATCH,SUBMIT,POLL,submit_hook,poll_hook
from verify_sound_rpc import require,words

def verify(root,native=None):
    folder=root/'work/pcsx2-capture/rpc-dbc-setup-before-dbc-dma'
    m=json.loads((folder/'dbc-dma.json').read_text(encoding='utf-8'));i=(folder/'dbc-dma-iop.bin').read_bytes();e=(folder/'dbc-dma-ee.bin').read_bytes()
    baseline=json.loads((root/'work/pcsx2-capture/heap-second-after/capture.json').read_text(encoding='utf-8'))
    require(m['source_elf_sha256']==EXPECTED_SHA and all(m[k]==baseline[k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')),'DMA capture environment differs')
    require(hashlib.sha256(i).hexdigest()==m['iop_sha256'] and hashlib.sha256(e).hexdigest()==m['ee_sha256'],'DMA capture hashes differ')
    source=i[SCRATCH+64:SCRATCH+192]
    require(m['descriptor']==[BASE+0x2778,0x3A27C0,128,0] and m['count']==1 and m['polls']==1 and m['poll_return']==0xFFFFFFFF and m['token']>0,'DMA submit/poll differs')
    require(m['diagnostic_source_redirect']==SCRATCH+64 and hashlib.sha256(source).hexdigest()==m['source_sha256'],'DMA diagnostic source differs')
    require(e[0x3A27C0:0x3A2840]==source and words(i,SCRATCH+192,1)==(0xFFFFFFFF,),'Actual transport readback differs')
    for address,blob in ((SUBMIT,submit_hook(m['targets'][0])),(POLL,poll_hook(m['targets'][1]))):require(i[address:address+len(blob)]==blob,'DMA hook differs')
    if native:
        r=json.loads(native.read_text(encoding='utf-8'));ram=Path(str(native)+'.iop.bin').read_bytes()
        require(r['dbc_rpc_calls']>=2 and r['dbc_dma_submissions']==1 and r['dbc_dma_polls']==1,'Native DMA lifecycle differs')
        require(r['dbc_dma_ee_words']==r['dbc_dma_payload_words'],'Native owned source/destination differ')
    print('Actual redirected diagnostic SIF transfer/readback and completion poll verified'+('; native owned 128-byte copy and second RPC cleanup verified.' if native else '.'))
    print('Diagnostic source is an immutable copy; live retail controller-buffer bytes and scheduling are not equated.')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path.cwd());p.add_argument('--native',type=Path);a=p.parse_args();verify(a.root,a.native)
