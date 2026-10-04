"""Verify reached DMA-stop/query checkpoints; no IOP reset completion inferred."""
import hashlib
import json
import struct
from capture_ee import EXPECTED_SHA, MARKER, capture_trap

KERNEL_STOP_SHA = '6a47c4cee303ef9916bbf8229e556dcd2f84ccdd054239b4c10f3a6dadecb19e'


def validate_publication(ee, point):
    address, trap, marker = capture_trap(point)
    if len(ee) != 32 * 1024 * 1024:
        raise ValueError('Expected complete bounded EE capture.')
    result = 0 if point == 'sif-stop-after' else 0x19600
    if struct.unpack_from('<III', ee, MARKER) != (marker, result, 0x1FFEC80):
        raise ValueError('SIF stop/query publication differs.')
    if struct.unpack_from('<I', ee, 0x14EAC)[0] != 0x80006380:
        raise ValueError('SIF stop syscall table differs.')
    if hashlib.sha256(ee[0x6380:0x63AC]).hexdigest() != KERNEL_STOP_SHA:
        raise ValueError('SIF stop kernel implementation differs.')
    return struct.pack('<I',result)


def verify(path, point):
    manifest=json.loads(path.read_text(encoding='utf-8'))
    ee=path.with_name('ee-ram.bin').read_bytes()
    address,trap,marker=capture_trap(point); patch=manifest['patch']
    if (manifest['source_elf_sha256'] != EXPECTED_SHA or not manifest['capture_point'].startswith(point+':')
        or patch['address']!=address or patch['size']!=len(trap)
        or patch['authored_words']!=list(struct.unpack('<8I',trap))
        or patch['marker_address']!=MARKER or patch['marker_value']!=marker
        or hashlib.sha256(ee).hexdigest()!=manifest['capture_sha256'] or ee[address:address+len(trap)]!=trap):
        raise ValueError('SIF stop/query provenance differs.')
    return validate_publication(ee,point),manifest


def validate_smflag_publication(ee,point):
    if len(ee) != 32*1024*1024: raise ValueError('Expected bounded EE capture.')
    marker=capture_trap(point)[2]
    expected=(marker,0 if point=='reset-smflag-before' else 0x30000,0x1FFEC80,0x30000)
    if struct.unpack_from('<4I',ee,MARKER)!=expected:
        raise ValueError('Reached SMFLAG publication differs.')


def verify_smflag_pair(before,after):
    manifests=[]
    for path,point in ((before,'reset-smflag-before'),(after,'reset-smflag-after')):
        m=json.loads(path.read_text(encoding='utf-8'));ee=path.with_name('ee-ram.bin').read_bytes()
        address,trap,marker=capture_trap(point);p=m['patch']
        if (m['source_elf_sha256']!=EXPECTED_SHA or not m['capture_point'].startswith(point+':')
            or p['address']!=address or p['size']!=len(trap) or p['authored_words']!=list(struct.unpack('<12I',trap))
            or p['marker_address']!=MARKER or p['marker_value']!=marker
            or hashlib.sha256(ee).hexdigest()!=m['capture_sha256'] or ee[address:address+len(trap)]!=trap):
            raise ValueError('SMFLAG pair provenance differs.')
        validate_smflag_publication(ee,point);manifests.append(m)
    if any(manifests[0][key]!=manifests[1][key] for key in ('source_elf_sha256','pcsx2_sha256','bios_sha256')):
        raise ValueError('SMFLAG pair environments differ.')
    return struct.pack('<III',0x30000,0x30000,0x30000),{'before':manifests[0],'after':manifests[1]}
