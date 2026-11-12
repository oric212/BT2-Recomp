"""The checkpoint stub and the register block it publishes (docs/plans/validation/PCSX2_COMPARISON_PLAN.md).

A checkpoint is a call site (`jal target` and its delay slot) or a function's entry in an instrumented copy of the
executable. The call (or the function's first two words) is replaced by a jump to a stub in an added segment that
runs the delay slot, counts the call, and at the calls the host asks for publishes the full register state, marks
the checkpoint and spins until the host clears the mark, then loads the arguments back (the host may have changed
them); then it makes the call and, if asked, publishes and waits again after it returns, and loads the results
back. `t8` and `t9` are the stub's own.

Each site has a slot of SLOT bytes: the code, then at DATA its words (offsets below) and the register block at
BLOCK. The block's layout is shared with the probe's replay (runtime/probe/call_replay.cpp).
"""
import struct

SLOT = 0x2000
# The stub's data, from the slot's start: a page of its own, as PCSX2 write-protects the pages of code it has
# compiled (to catch self-modifying code) and the host's writes there fail (WinError 998, first session).
DATA = 0x1000
COUNT, SKIP, WANT_AFTER, ARMED, MARK, SAVED_RA = 0x0, 0x4, 0x8, 0xC, 0x10, 0x14
BLOCK = 0x100                      # The register block, from DATA.
MARK_BEFORE, MARK_AFTER = 1, 2
NEVER = 0x7FFFFFFF                 # SKIP that captures no more calls.

# The register block (BLOCK_SIZE bytes, little-endian).
GPRS, FPRS, HILO = 0x000, 0x200, 0x280               # 32 x 128 bits; 32 x 32 bits; HI, LO, HI1, LO1 x 64 bits
SA, FCR31, VU_STATUS, VU_MAC, VU_CLIP, VU_R, VU_I, VU_Q = range(0x2A0, 0x2C0, 4)
VIS, VFS, BLOCK_SIZE = 0x2C0, 0x300, 0x500           # 16 x 32 bits; 32 x 128 bits
T8, T9, RA = 24, 25, 31
# After the block (step 6, VU1 and the GIF stream): the hardware words a replay of a VIF1 kick needs, read by the
# EE as the game would. VIF1's registers 0x10003C00 to 0x10003D70 (STAT, FBRST, ERR, MARK, CYCLE, MODE, NUM, MASK,
# CODE, ITOPS, BASE, OFST, TOPS, ITOP, TOP, a reserved word, R0 to R3, C0 to C3), then GIF_STAT (PATH3's mask),
# then VIF1's DMA channel (D1_CHCR, D1_MADR, D1_QWC, D1_TADR: whether a chain is still running).
HARDWARE = BLOCK + BLOCK_SIZE
HARDWARE_WORDS = [0x10003C00 + 0x10 * k for k in range(24)] + [0x10003020] + [0x10009000 + 0x10 * k for k in range(4)]
HARDWARE_SIZE = 4 * len(HARDWARE_WORDS)

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


# Registers the stub loads back from the block after a wait, so the host can change them: a call's arguments
# (a0 to a3, t0 to t3, f12 to f19) before it, its results (v0, v1, f0) after it.
ARGUMENTS = ([4, 5, 6, 7, 8, 9, 10, 11], list(range(12, 20)))
RESULTS = ([2, 3], [0])


def publish(mark, reload):
    """Store every register into the block at DATA + BLOCK (t8 holds DATA's address), mark, wait, and load the
    `reload` registers back."""
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
    for k, address in enumerate(HARDWARE_WORDS):                                          # lui t9; lw t9; sw
        high = (address + 0x8000) >> 16                                                   # lw's offset is signed
        words += [_i(0x0F, 0, T9, high), _i(0x23, T9, T9, address & 0xFFFF), _store(0x2B, T9, HARDWARE + 4 * k)]
    words += [_i(0x0D, 0, T9, mark), _store(0x2B, T9, MARK),                              # ori t9,zero,mark; sw
              _store(0x23, T9, MARK), _i(5, T9, 0, -2), 0]                                # lw; bne t9,zero,-2; nop
    gprs, fprs = reload
    words += [_store(0x1E, r, b + GPRS + 16 * r) for r in gprs]                           # lq
    words += [_store(0x31, f, b + FPRS + 4 * f) for f in fprs]                            # lwc1
    return words


def check_entry_words(words):
    """A function's first two words run in the stub: refuse branches and uses of t8 and t9 (ra is the caller's
    return address there, as it would be)."""
    for word in words:
        opcode, rs, rt, rd, funct = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, word & 63
        if opcode in _BRANCH_OPCODES or (opcode == 0 and funct in (8, 9)) or (opcode in (0x11, 0x12) and rs == 8):
            raise ValueError(f"The function's word {word:#010x} is a branch.")
        if ({rs, rt, rd} if opcode in (0, 0x1C) else {rs, rt}) & {T8, T9}:
            raise ValueError(f"The function's word {word:#010x} may use t8 or t9.")


def stub_words(slot, target, delay, resume, first_skip, want_after, entry=None):
    """The stub at address `slot`. A call site: `jal target` with its delay slot `delay`, returning to `resume`.
    A function's entry (`entry`, its first two words): the stub runs them and jumps to target + 8; to take the
    state after the call it keeps the caller's ra (at SAVED_RA) and has the function return into the stub."""
    data = slot + DATA
    base = [_i(0x0F, 0, T8, data >> 16), _i(0x0D, T8, T8, data & 0xFFFF)]                 # lui, ori t8
    if entry is None:
        check_delay_slot(delay)
        words = [delay] + base
    else:
        check_entry_words(entry)
        words = list(base)
    words += [_store(0x23, T9, COUNT), _i(0x09, T9, T9, 1), _store(0x2B, T9, COUNT)]       # count += 1
    words += [_store(0x2B, 0, ARMED), _store(0x23, T9, SKIP)]                              # armed = 0; t9 = skip
    words += [_i(4, T9, 0, 4), 0,                                                          # beq t9,zero,capture
              _i(0x09, T9, T9, -1), _i(4, 0, 0, 0), _store(0x2B, T9, SKIP)]             # skip -= 1; b call
    branch_to_call = len(words) - 2
    words += [_store(0x23, T9, WANT_AFTER), _store(0x2B, T9, ARMED)]                       # capture: armed = want
    words += publish(MARK_BEFORE, ARGUMENTS)
    call = len(words)
    words[branch_to_call] = _i(4, 0, 0, call - (branch_to_call + 1))
    if entry is None:
        words += [0x0C000000 | ((target >> 2) & 0x3FFFFFF), 0] + base                       # jal target; nop
        words += [_store(0x23, T9, ARMED), _i(4, T9, 0, 0), 0]                           # beq t9,zero,resume
        branch_to_resume = len(words) - 2
        words += publish(MARK_AFTER, RESULTS)
        words[branch_to_resume] = _i(4, T9, 0, len(words) - (branch_to_resume + 1))
        words += [0x08000000 | ((resume >> 2) & 0x3FFFFFF), 0]                              # j resume; nop
    else:
        words += [_store(0x23, T9, ARMED), _i(4, T9, 0, 0), 0]                           # beq t9,zero,body
        branch_to_body = len(words) - 2
        words += [_store(0x2B, RA, SAVED_RA), 0x0C000000, 0]                               # keep ra; jal body; nop
        jal_body = len(words) - 2
        words += base + publish(MARK_AFTER, RESULTS)
        words += [_store(0x23, RA, SAVED_RA), 0x03E00008, 0]                               # lw ra; jr ra; nop
        body = len(words)
        words[branch_to_body] = _i(4, T9, 0, body - (branch_to_body + 1))
        words[jal_body] = 0x0C000000 | (((slot + 4 * body) >> 2) & 0x3FFFFFF)
        words += list(entry) + [0x08000000 | (((target + 8) >> 2) & 0x3FFFFFF), 0]        # the two words; j target+8
    if len(words) * 4 > DATA:
        raise ValueError('The stub is longer than its code area.')
    image = bytearray(SLOT)
    struct.pack_into(f'<{len(words)}I', image, 0, *words)
    struct.pack_into('<3I', image, DATA, 0, first_skip, 1 if want_after else 0)
    return bytes(image)


# A logging site (step 9, the input delay): after each call the stub writes one LOG_ENTRY-byte record to a ring
# and goes on without waiting, so PCSX2's timing stays the game's (a checkpoint's wait lets VBlanks pass). The
# record: the call's number, COP0 Count (the EE's cycle counter), two words read from RAM, three checksums of RAM
# ranges (each word added and the sum rotated left by one), then a zero. The stub can also press a button: from call
# PRESS_FIRST for PRESS_HOLD calls it ORs PRESS_VALUE into the word at PRESS_ADDR after the call (the pad record
# the game reads its buttons from). Its data (offsets from DATA) as below; the ring lies outside the slot.
LOG_RING, LOG_WORDS, LOG_SUMS, LOG_PRESS, LOG_SAVE = 0x20, 0x24, 0x2C, 0x44, 0x80
LOG_ENTRY, LOG_ENTRIES, LOG_CHECKSUMS = 32, 4096, 3
A0, A1, A2, A3 = 4, 5, 6, 7


def _r(rs, rt, rd, funct, shamt=0):
    return (rs << 21) | (rt << 16) | (rd << 11) | (shamt << 6) | funct


def log_stub_words(slot, target, delay, resume, ring, words, sums, press=None):
    """The logging stub at `slot` for the call site `jal target` (delay slot `delay`, returning to `resume`).
    `words`: two RAM addresses; `sums`: three (start, end) ranges, end exclusive, 4-byte aligned (an empty range
    gives 0); `press`: (first call, calls held, address, value) or None."""
    check_delay_slot(delay)
    data = slot + DATA
    code, labels, fixups = [], {}, []

    def branch(opcode, rs, rt, label):                       # beq/bne to a label, with a nop in the delay slot.
        fixups.append((len(code), label))
        code.extend([_i(opcode, rs, rt, 0), 0])

    def load_word(rt, address):                              # rt = the word at address (t8 = DATA).
        code.append(_store(0x23, rt, address))

    code += [delay, 0x0C000000 | ((target >> 2) & 0x3FFFFFF), 0]                          # the call itself
    code += [_i(0x0F, 0, T8, data >> 16), _i(0x0D, T8, T8, data & 0xFFFF)]                # t8 = DATA
    code += [_store(0x1F, r, LOG_SAVE + 16 * (r - A0)) for r in (A0, A1, A2, A3)]          # sq a0..a3
    code += [_store(0x23, T9, COUNT), _i(0x09, T9, T9, 1), _store(0x2B, T9, COUNT)]        # count += 1
    # The press: (count - first) < hold, unsigned.
    load_word(A0, LOG_PRESS)
    code.append(_r(T9, A0, A0, 0x23))                                                      # subu a0, t9, a0
    load_word(A1, LOG_PRESS + 4)
    code.append(_r(A0, A1, A0, 0x2B))                                                      # sltu a0, a0, a1
    branch(4, A0, 0, 'record')
    load_word(A0, LOG_PRESS + 8)
    load_word(A1, LOG_PRESS + 12)
    code += [_i(0x23, A0, A2, 0), _r(A2, A1, A2, 0x25), _i(0x2B, A0, A2, 0)]               # lw; or; sw
    labels['record'] = len(code)
    # a3 = the ring's entry for this call.
    code += [_i(0x0C, T9, A0, LOG_ENTRIES - 1), _r(0, A0, A0, 0x00, 5)]                    # andi; sll 5
    load_word(A1, LOG_RING)
    code.append(_r(A0, A1, A3, 0x21))                                                      # addu a3, a0, a1
    code += [0x40000000 | (A0 << 16) | (9 << 11), _i(0x2B, A3, A0, 4)]                    # mfc0 a0, Count; sw
    for k in range(2):
        load_word(A0, LOG_WORDS + 4 * k)
        code += [_i(0x23, A0, A0, 0), _i(0x2B, A3, A0, 8 + 4 * k)]                         # lw a0, 0(a0); sw
    for k in range(LOG_CHECKSUMS):
        load_word(A0, LOG_SUMS + 8 * k)
        load_word(A1, LOG_SUMS + 8 * k + 4)
        code.append(_r(0, 0, A2, 0x25))                                                    # or a2, zero, zero
        labels[f'loop{k}'] = len(code)
        branch(4, A0, A1, f'done{k}')
        code += [_i(0x23, A0, T9, 0), _r(A2, T9, A2, 0x21),                                # lw t9; addu a2
                 _r(0, A2, T9, 0x00, 1), _r(0, A2, A2, 0x02, 31), _r(A2, T9, A2, 0x25),    # rotate left by one
                 _i(0x09, A0, A0, 4)]                                                      # addiu a0, 4
        branch(4, 0, 0, f'loop{k}')
        labels[f'done{k}'] = len(code)
        code.append(_i(0x2B, A3, A2, 16 + 4 * k))                                  # sw a2
    code += [_store(0x23, T9, COUNT), _i(0x2B, A3, T9, 0)]                                 # the number last
    code += [_store(0x1E, r, LOG_SAVE + 16 * (r - A0)) for r in (A0, A1, A2, A3)]          # lq a0..a3
    code += [0x08000000 | ((resume >> 2) & 0x3FFFFFF), 0]                                   # j resume; nop
    for at, label in fixups:
        code[at] |= (labels[label] - (at + 1)) & 0xFFFF
    if len(code) * 4 > DATA:
        raise ValueError('The stub is longer than its code area.')
    for start, end in sums:
        if start % 4 or end % 4 or end < start:
            raise ValueError(f'A checksum range must be 4-byte aligned: {start:#x}-{end:#x}')
    image = bytearray(SLOT)
    struct.pack_into(f'<{len(code)}I', image, 0, *code)
    first, hold, address, value = press or (0, 0, data, 0)
    struct.pack_into('<I2I6I4I', image, DATA + LOG_RING, ring, *words, *[v for s in sums for v in s],
                     first, hold, address, value)
    return bytes(image)


SET_CALL, SET_VALUES = 0x20, 0x24     # The setting stub's data, from DATA: the call, then one word per register.


def set_stub_words(slot, target, entry, call, registers):
    """A stub at a function's entry that, at its `call`-th call, sets the given registers ({number: 32-bit value},
    loaded sign-extended as LW does) before the function runs, without waiting for the host. `entry`: the
    function's first two words, run in the stub after the change."""
    check_entry_words(entry)
    if not 1 <= call < 0x8000:
        raise ValueError('The call to change is 1 to 32767.')
    data = slot + DATA
    code = [_i(0x0F, 0, T8, data >> 16), _i(0x0D, T8, T8, data & 0xFFFF)]
    code += [_store(0x23, T9, COUNT), _i(0x09, T9, T9, 1), _store(0x2B, T9, COUNT)]
    code += [_i(0x09, T9, T9, -call), _i(5, T9, 0, len(registers) + 1), 0]               # bne t9,zero,skip
    code += [_store(0x23, r, SET_VALUES + 4 * k) for k, r in enumerate(registers)]
    code += list(entry) + [0x08000000 | (((target + 8) >> 2) & 0x3FFFFFF), 0]
    image = bytearray(SLOT)
    struct.pack_into(f'<{len(code)}I', image, 0, *code)
    struct.pack_into(f'<I{len(registers)}I', image, DATA + SET_CALL, call, *[v & 0xFFFFFFFF for v in registers.values()])
    return bytes(image)


def parse_log_entry(entry):
    """One ring record: (call number, Count, word 1, word 2, checksum 1, checksum 2, checksum 3)."""
    return struct.unpack_from('<7I', entry)


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
