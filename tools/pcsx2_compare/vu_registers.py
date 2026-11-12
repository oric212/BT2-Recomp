"""VU1's registers read out of the PCSX2 process (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 6).

The game keeps values in VU1's registers from one VIF1 chain to the next, and the EE cannot read them, so a replay of
a chain needs them from PCSX2 itself. PCSX2 keeps both VUs' state in an array of its VURegs structure: VF[32]
(16 bytes each) first, then VI[32] at 16 bytes each (VI[16] to VI[31] hold the control registers: 18 the clipping
flags, 20 R, 21 I, 22 Q), then ACC. The array is not exported. VU0's entry is found by its VF registers, which the
checkpoint stub publishes (the VFS part of the register block), in the executable's writable sections; VU1's entry
follows at the distance between the two entries' data memory pointers (VU0's at VUmem + 0x1000, VU1's at
VUmem + 0x6000, the same field of each), and must hold VF0 = (0, 0, 0, 1).
"""
import struct

SIZE = 0x430                           # VF[32], VI[32], ACC: what a replay loads.
VF, VI, ACC = 0x000, 0x200, 0x400
CLIP, R, I, Q = 18, 20, 21, 22          # Control registers among the VIs.
VU0_DATA, VU1_DATA = 0x1000, 0x6000     # Offsets in the VUmem block.
VF0 = struct.pack('<4f', 0, 0, 0, 1)
CHUNK = 1 << 24


def writable_sections(image):
    """(RVA, size) of the PE image's writable sections."""
    pe, = struct.unpack_from('<I', image, 0x3C)
    count, = struct.unpack_from('<H', image, pe + 6)
    optional, = struct.unpack_from('<H', image, pe + 20)
    table = pe + 24 + optional
    out = []
    for k in range(count):
        size, rva = struct.unpack_from('<II', image, table + 40 * k + 8)
        flags, = struct.unpack_from('<I', image, table + 40 * k + 36)
        if flags & 0x80000000:
            out.append((rva, size))
    return out


def find(read, start, size, pattern):
    """The first address in [start, start + size) where `pattern` lies, read in chunks that overlap by its length."""
    at = start
    while at < start + size:
        length = min(CHUNK + len(pattern), start + size - at)
        found = read(at, length).find(pattern)
        if found >= 0:
            return at + found
        at += CHUNK
    return None


def locate(read, base, image, vumem, vu0_vf):
    """The address of VU1's VURegs entry. `read(address, size)` reads the PCSX2 process; `base` is its image base,
    `vumem` the VUmem block's address and `vu0_vf` VU0's 32 VF registers as the stub published them."""
    vu0 = None
    for rva, size in writable_sections(image):
        vu0 = find(read, base + rva, size, vu0_vf)
        if vu0 is not None:
            break
    if vu0 is None:
        raise LookupError("VU0's registers were not found in PCSX2's writable sections.")
    window = read(vu0, 0x4000)
    pointer = window.find(struct.pack('<Q', vumem + VU0_DATA))
    second = window.find(struct.pack('<Q', vumem + VU1_DATA))
    if pointer < 0 or second < 0 or second <= pointer or pointer % 8:
        raise LookupError("The VUs' data memory pointers were not found after VU0's registers.")
    vu1 = vu0 + second - pointer
    if read(vu1, 16) != VF0:
        raise LookupError("The entry found for VU1 does not hold VF0 = (0, 0, 0, 1).")
    return vu1
