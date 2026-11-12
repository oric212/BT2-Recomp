"""The checkpoint stub and the register block it publishes (docs/plans/validation/PCSX2_COMPARISON_PLAN.md).

A checkpoint is a call site (`jal target` and its delay slot) in an instrumented copy of the executable. The call
is replaced by a jump to a stub in an added segment that runs the delay slot, counts the call, and at the calls the
host asks for publishes the full register state, marks the checkpoint and spins until the host clears the mark;
then it makes the call and, if asked, publishes and waits again after it returns. `t8` and `t9` are the stub's own.

Each site has a slot of SLOT bytes: the code, then at DATA its words (offsets below) and the register block at
BLOCK. The block's layout is shared with the probe's replay (runtime/probe/call_replay.cpp).
"""
import struct

SLOT = 0x2000
# The stub's data, from the slot's start: a page of its own, as PCSX2 write-protects the pages of code it has
# compiled (to catch self-modifying code) and the host's writes there fail (WinError 998, first session).
DATA = 0x1000
COUNT, SKIP, WANT_AFTER, ARMED, MARK = 0x0, 0x4, 0x8, 0xC, 0x10
BLOCK = 0x100                      # The register block, from DATA.
MARK_BEFORE, MARK_AFTER = 1, 2
NEVER = 0x7FFFFFFF                 # SKIP that captures no more calls.

# The register block (BLOCK_SIZE bytes, little-endian).
GPRS, FPRS, HILO = 0x000, 0x200, 0x280               # 32 x 128 bits; 32 x 32 bits; HI, LO, HI1, LO1 x 64 bits
SA, FCR31, VU_STATUS, VU_MAC, VU_CLIP, VU_R, VU_I, VU_Q = range(0x2A0, 0x2C0, 4)
VIS, VFS, BLOCK_SIZE = 0x2C0, 0x300, 0x500           # 16 x 32 bits; 32 x 128 bits
T8, T9, RA = 24, 25, 31

# Instructions that change the flow and so cannot be moved out of a delay slot.
_BRANCH_OPCODES = {1, 2, 3, 4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17}


def _i(opcode, rs, rt, immediate):
    return (opcode << 26) | (rs << 21) | (rt << 16) | (immediate & 0xFFFF)


def _store(opcode, rt, offset):
    return _i(opcode, T8, rt, offset)


def check_delay_slot(word):
    """The delay slot runs before the stub's publication: refuse a branch or a use of the stub's registers or ra."""
    opcode, rs, rt, rd, funct = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, word & 63
    if opcode in _BRANCH_OPCODES or (opcode == 0 and funct in (8, 9)) or (opcode in (0x11, 0x12) and rs == 8):
        raise ValueError(f'The delay slot {word:#010x} is a branch.')
    registers = {rs, rt, rd} if opcode in (0, 0x1C) else {rs, rt}       # rd only in R-type and MMI words
    if registers & {T8, T9, RA}:
        raise ValueError(f'The delay slot {word:#010x} may use t8, t9 or ra.')


def publish(mark):
    """Store every register into the block at DATA + BLOCK (t8 holds DATA's address), mark, and wait."""
    b = BLOCK
    words = [_store(0x1F, r, b + GPRS + 16 * r) for r in range(32)]                      # sq
    words += [_store(0x39, f, b + FPRS + 4 * f) for f in range(32)]                       # swc1
    for k, move in enumerate((0x00000010, 0x00000012, 0x70000010, 0x70000012)):           # mfhi, mflo, mfhi1, mflo1
        words += [move | (T9 << 11), _store(0x3F, T9, b + HILO + 8 * k)]                  # sd
    words += [0x00000028 | (T9 << 11), _store(0x2B, T9, b + SA)]                          # mfsa, sw
    words += [0x44400000 | (T9 << 16) | (31 << 11), _store(0x2B, T9, b + FCR31)]          # cfc1 $31
    for control, offset in ((16, VU_STATUS), (17, VU_MAC), (18, VU_CLIP), (20, VU_R), (21, VU_I), (22, VU_Q)):
        words += [0x48400000 | (T9 << 16) | (control << 11), _store(0x2B, T9, b + offset)]  # cfc2
    for vi in range(16):
        words += [0x48400000 | (T9 << 16) | (vi << 11), _store(0x2B, T9, b + VIS + 4 * vi)]
    words += [_store(0x3E, vf, b + VFS + 16 * vf) for vf in range(32)]                   # sqc2
    words += [_i(0x0D, 0, T9, mark), _store(0x2B, T9, MARK),                              # ori t9,zero,mark; sw
              _store(0x23, T9, MARK), _i(5, T9, 0, -2), 0]                                # lw; bne t9,zero,-2; nop
    return words


def stub_words(slot, target, delay, resume, first_skip, want_after):
    """The stub at address `slot` for `jal target` with its delay slot `delay`, returning to `resume`."""
    check_delay_slot(delay)
    data = slot + DATA
    base = [_i(0x0F, 0, T8, data >> 16), _i(0x0D, T8, T8, data & 0xFFFF)]                 # lui, ori t8
    words = [delay] + base
    words += [_store(0x23, T9, COUNT), _i(0x09, T9, T9, 1), _store(0x2B, T9, COUNT)]       # count += 1
    words += [_store(0x2B, 0, ARMED), _store(0x23, T9, SKIP)]                              # armed = 0; t9 = skip
    words += [_i(4, T9, 0, 4), 0,                                                          # beq t9,zero,capture
              _i(0x09, T9, T9, -1), _i(4, 0, 0, 0), _store(0x2B, T9, SKIP)]             # skip -= 1; b call
    branch_to_call = len(words) - 2
    words += [_store(0x23, T9, WANT_AFTER), _store(0x2B, T9, ARMED)]                       # capture: armed = want
    words += publish(MARK_BEFORE)
    call = len(words)
    words[branch_to_call] = _i(4, 0, 0, call - (branch_to_call + 1))
    words += [0x0C000000 | ((target >> 2) & 0x3FFFFFF), 0] + base                           # jal target; nop
    words += [_store(0x23, T9, ARMED), _i(4, T9, 0, 0), 0]                               # beq t9,zero,resume
    branch_to_resume = len(words) - 2
    words += publish(MARK_AFTER)
    words[branch_to_resume] = _i(4, T9, 0, len(words) - (branch_to_resume + 1))
    words += [0x08000000 | ((resume >> 2) & 0x3FFFFFF), 0]                                  # j resume; nop
    if len(words) * 4 > DATA:
        raise ValueError('The stub is longer than its code area.')
    image = bytearray(SLOT)
    struct.pack_into(f'<{len(words)}I', image, 0, *words)
    struct.pack_into('<3I', image, DATA, 0, first_skip, 1 if want_after else 0)
    return bytes(image)


def parse(block):
    """The register block as a dict of named values (for reports)."""
    if len(block) != BLOCK_SIZE:
        raise ValueError('Expected a register block of 0x500 bytes.')
    gpr = [int.from_bytes(block[GPRS + 16 * r:GPRS + 16 * r + 16], 'little') for r in range(32)]
    hi, lo, hi1, lo1 = struct.unpack_from('<4Q', block, HILO)
    return {'gpr': gpr, 'fpr': list(struct.unpack_from('<32I', block, FPRS)), 'hi': hi, 'lo': lo, 'hi1': hi1, 'lo1': lo1,
            'sa': struct.unpack_from('<I', block, SA)[0], 'fcr31': struct.unpack_from('<I', block, FCR31)[0],
            'vi': list(struct.unpack_from('<16I', block, VIS)),
            'vf': [block[VFS + 16 * v:VFS + 16 * v + 16].hex() for v in range(32)]}


def from_registers(**registers):
    """A block with only the named GPRs set (low 64 bits), for captures that published only some of them."""
    names = {'a0': 4, 'a1': 5, 'a2': 6, 'a3': 7, 'sp': 29, 'gp': 28, 'ra': 31, 'v0': 2}
    block = bytearray(BLOCK_SIZE)
    struct.pack_into('<f', block, VU_R, 1.0)
    struct.pack_into('<4f', block, VFS, 0.0, 0.0, 0.0, 1.0)                                  # VF0 = (0, 0, 0, 1)
    for name, value in registers.items():
        struct.pack_into('<Q', block, GPRS + 16 * names[name], value & 0xFFFFFFFFFFFFFFFF)
    return bytes(block)
