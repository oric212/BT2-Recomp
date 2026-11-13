"""The service log: every SIF RPC call the game makes, logged in PCSX2 by two patches of the game's sceSifCallRpc
(docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 8).

sceSifCallRpc (0x332970) takes the client in a0, the function number in a1, the mode in a2 (bit 0: do not wait),
the send buffer and its size in a3 and t0, the receive buffer and its size in t1 and t2. Two stubs share one slot,
its counter and one ring of LOG_ENTRY-byte records (context_block's data layout), and neither waits for the host:

- the request, at the function's entry (its first two words run in the stub);
- the reply of a call that waited, at 0x332B3C: reached only after the semaphore wait of a synchronous call that
  was sent, where s1 holds the client, s6 the function number, fp the mode, s4 the receive buffer and the command
  packet (s0) the receive size at +44.

A record: its number (written last), COP0 Count, the function number, the client, the mode (REPLY set for a reply:
function numbers use bit 31 themselves, the controller's 0x80001363), the buffer's size,
its address and the checksum of its whole words (each word added, the sum rotated left by one).
The stubs use only registers the function does not hold there (t4 to t9, v0 and v1 at the entry; at the reply
also the argument registers), so a call on any thread is logged.
"""
import struct

import context_block as cb

CALL = 0x00332970                                  # sceSifCallRpc
ENTRY_WORDS = (0x27BDFF40, 0xFFB10030)             # addiu sp, sp, -192; sd s1, 48(sp)
DONE = 0x00332B3C                                  # daddu v0, zero, zero: the synchronous call's success
DONE_WORDS = (0x0000102D, 0xDFBF00B0)              # ... and ld ra, 176(sp) at 0x332B40, the return path
REPLY = 0x80000000
REPLY_CODE = 0x800                                 # The reply stub's offset in the slot.

V0, V1, A0, A1, A2, A3, T0 = 2, 3, 4, 5, 6, 7, 8
T4, T5, T6, T7, S0, S1, S4, S6, FP = 12, 13, 14, 15, 16, 17, 20, 22, 30


def checksum(data):
    """The stubs' checksum of a buffer's whole words."""
    total = 0
    for word in struct.unpack_from(f'<{len(data) // 4}I', data):
        total = (total + word) & 0xFFFFFFFF
        total = ((total << 1) | (total >> 31)) & 0xFFFFFFFF
    return total


def _record_start(code, data):
    """t8 = data; t9 = the counter + 1, stored; v0 = the record; its Count."""
    _i, _r, _s = cb._i, cb._r, cb._store
    code += [_i(0x0F, 0, cb.T8, data >> 16), _i(0x0D, cb.T8, cb.T8, data & 0xFFFF)]
    code += [_s(0x23, cb.T9, cb.COUNT), _i(0x09, cb.T9, cb.T9, 1), _s(0x2B, cb.T9, cb.COUNT)]
    code += [_i(0x0C, cb.T9, V0, cb.LOG_ENTRIES - 1), _r(0, V0, V0, 0x00, 5), _s(0x23, V1, cb.LOG_RING),
             _r(V0, V1, V0, 0x21)]                                                   # andi; sll 5; lw; addu
    code += [0x40000000 | (V1 << 16) | (9 << 11), _i(0x2B, V0, V1, 4)]                # mfc0 v1, Count; sw


def _checksum(code, address, size):
    """t6 = the checksum of size // 4 words from address (registers), stored at +28; uses t4, t5, t7."""
    _i, _r = cb._i, cb._r
    code += [_r(address, 0, T4, 0x25), _i(0x09, 0, T7, -4), _r(size, T7, T5, 0x24), _r(T5, T4, T5, 0x21),
             _r(0, 0, T6, 0x25)]                                       # t4 = address; t5 = t4 + (size & ~3); t6 = 0
    loop = len(code)
    body = [_i(0x23, T4, T7, 0), _r(T6, T7, T6, 0x21),                 # lw t7; addu t6
            _r(0, T6, T7, 0x00, 1), _r(0, T6, T6, 0x02, 31), _r(T6, T7, T6, 0x25),    # rotate left by one
            _i(0x09, T4, T4, 4)]
    code += [_i(4, T4, T5, len(body) + 3), 0] + body                   # beq t4, t5, done (past the b and its nop)
    code += [_i(4, 0, 0, (loop - (len(code) + 1)) & 0xFFFF), 0]        # b loop
    code += [_i(0x2B, V0, T6, 28)]                                     # done: sw t6, 28(v0)


def stub_words(slot, ring):
    """The slot's image: the request stub at its start, the reply stub at REPLY_CODE, the counter and the ring's
    address in its data."""
    _i, _r = cb._i, cb._r
    data = slot + cb.DATA
    request = []
    _record_start(request, data)
    request += [_i(0x2B, V0, A1, 8), _i(0x2B, V0, A0, 12), _i(0x2B, V0, A2, 16), _i(0x2B, V0, T0, 20),
                _i(0x2B, V0, A3, 24)]
    _checksum(request, A3, T0)
    request += [_i(0x2B, V0, cb.T9, 0)]                                                   # the number last
    request += list(ENTRY_WORDS) + [0x08000000 | (((CALL + 8) >> 2) & 0x3FFFFFF), 0]
    reply = []
    _record_start(reply, data)
    reply += [_i(0x2B, V0, S6, 8), _i(0x2B, V0, S1, 12),
              _i(0x0F, 0, V1, REPLY >> 16), _r(FP, V1, V1, 0x25), _i(0x2B, V0, V1, 16),            # mode | REPLY
              _i(0x23, S0, T0, 44), _i(0x2B, V0, T0, 20),
              _i(0x2B, V0, S4, 24)]
    _checksum(reply, S4, T0)
    reply += [_i(0x2B, V0, cb.T9, 0), DONE_WORDS[0], 0x08000000 | (((DONE + 4) >> 2) & 0x3FFFFFF), 0]
    if len(request) * 4 > REPLY_CODE or REPLY_CODE + len(reply) * 4 > cb.DATA:
        raise ValueError('A stub is longer than its code area.')
    image = bytearray(cb.SLOT)
    struct.pack_into(f'<{len(request)}I', image, 0, *request)
    struct.pack_into(f'<{len(reply)}I', image, REPLY_CODE, *reply)
    struct.pack_into('<I', image, cb.DATA + cb.LOG_RING, ring)
    return bytes(image)


def patches(slot):
    """{address: (expected words, new words)}: the function's entry to the request stub, its synchronous success
    to the reply stub (the word after, ld ra, stays: it runs in the jump's delay slot and again on return)."""
    return {CALL: (ENTRY_WORDS, (0x08000000 | (slot >> 2), 0)),
            DONE: (DONE_WORDS[:1], (0x08000000 | ((slot + REPLY_CODE) >> 2),))}


FIELDS = ('number', 'count', 'function', 'client', 'mode', 'size', 'address', 'checksum')   # rpc.log.json


def unpack(entry):
    """One record's eight words (the number first, as the session's ring reader expects)."""
    return struct.unpack_from('<8I', entry)


def parse_entry(entry):
    """One record as a dict: the words of FIELDS, the mode without REPLY, and whether it is a reply."""
    record = dict(zip(FIELDS, unpack(entry)))
    record['reply'] = bool(record['mode'] & REPLY)
    record['mode'] &= ~REPLY
    return record
