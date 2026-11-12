"""The logging stub (context_block.log_stub_words) run on a small interpreter of the instructions it uses."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import context_block as cb  # noqa: E402

SLOT, TARGET, RESUME = 0x01D00000, 0x0028BDA0, 0x00102EE8
RING = SLOT + cb.SLOT                    # The ring right after the slot.
RAM = RING + cb.LOG_ENTRIES * cb.LOG_ENTRY   # Words and checksum ranges for the test.
DELAY = 0x0000282D                       # daddu a1, zero, zero
SIZE = RAM + 0x100 - SLOT


class Machine:
    def __init__(self, image):
        self.mem = bytearray(SIZE)
        self.mem[:len(image)] = image
        self.gpr = [0] * 32
        self.gpr[4:8] = [0x1111, 0x2222, 0x3333, 0x4444]
        self.count, self.calls = 1000, 0

    def word(self, address):
        return struct.unpack_from('<I', self.mem, address - SLOT)[0]

    def put(self, address, value):
        struct.pack_into('<I', self.mem, address - SLOT, value & 0xFFFFFFFF)

    def step(self, pc):
        """Runs the word at pc; returns the jump target or None."""
        w = self.word(pc)
        self.count += 1
        op, rs, rt, rd, sh, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 63
        imm = w & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        g = self.gpr
        ea = (g[rs] + simm) & 0xFFFFFFFF
        jump = None
        if w == 0:
            pass
        elif op == 0x0F:
            g[rt] = imm << 16
        elif op == 0x0D:
            g[rt] = g[rs] | imm
        elif op == 0x0C:
            g[rt] = g[rs] & imm
        elif op == 0x09:
            g[rt] = (g[rs] + simm) & 0xFFFFFFFF
        elif op == 0x23:
            g[rt] = self.word(ea)
        elif op == 0x2B:
            self.put(ea, g[rt])
        elif op == 0x1F:
            self.mem[ea - SLOT:ea - SLOT + 16] = g[rt].to_bytes(16, 'little')
        elif op == 0x1E:
            g[rt] = int.from_bytes(self.mem[ea - SLOT:ea - SLOT + 16], 'little')
        elif op == 0x10 and rs == 0 and rd == 9:
            g[rt] = self.count
        elif op in (4, 5):
            if (g[rs] == g[rt]) == (op == 4):
                jump = pc + 4 + 4 * simm
        elif op in (2, 3):
            jump = (w & 0x3FFFFFF) << 2
            if op == 3:
                g[31] = pc + 8
        elif op == 0 and fn == 0x2D:
            g[rd] = (g[rs] + g[rt]) & 0xFFFFFFFF
        elif op == 0 and fn == 0x21:
            g[rd] = (g[rs] + g[rt]) & 0xFFFFFFFF
        elif op == 0 and fn == 0x23:
            g[rd] = (g[rs] - g[rt]) & 0xFFFFFFFF
        elif op == 0 and fn == 0x2B:
            g[rd] = int(g[rs] < g[rt])
        elif op == 0 and fn == 0x25:
            g[rd] = g[rs] | g[rt]
        elif op == 0 and fn == 0x00:
            g[rd] = (g[rt] << sh) & 0xFFFFFFFF
        elif op == 0 and fn == 0x02:
            g[rd] = g[rt] >> sh
        else:
            raise AssertionError(f'unexpected instruction {w:#010x} at {pc:#x}')
        g[0] = 0
        return jump

    def call(self):
        pc = SLOT
        for _ in range(100000):
            if pc == RESUME:
                return
            if pc == TARGET:
                self.calls += 1
                self.gpr[2], self.gpr[4] = 0x77, 0xAAAA          # a result kept; an argument register spoiled
                pc = self.gpr[31]
                continue
            jump = self.step(pc)
            if jump is not None:
                assert self.step(pc + 4) is None, 'a jump in a delay slot'
                pc = jump
            else:
                pc += 4
        raise AssertionError('the stub did not return')


def checksum(words):
    total = 0
    for w in words:
        total = (total + w) & 0xFFFFFFFF
        total = ((total << 1) | (total >> 31)) & 0xFFFFFFFF
    return total


class LogStubTest(unittest.TestCase):
    def test_records_presses_and_keeps_registers(self):
        button = RAM
        words = (RAM + 4, RAM + 8)
        sums = ((RAM + 0x10, RAM + 0x30), (RAM + 0x40, RAM + 0x40), (RAM + 0x10, RAM + 0x14))
        image = cb.log_stub_words(SLOT, TARGET, DELAY, RESUME, RING, words, sums, press=(3, 2, button, 0x4000))
        m = Machine(image)
        for k in range(8):
            m.put(RAM + 0x10 + 4 * k, 0x80000001 * (k + 1))
        m.put(words[0], 0xCAFE)
        presses = []
        for call in range(1, 7):
            m.put(button, 0x0010)                                  # The game's own buttons, rewritten each call.
            m.gpr[4:8] = [0x1111, 0x2222, 0x3333, 0x4444]
            m.put(words[1], call * 7)
            m.call()
            presses.append(m.word(button))
            self.assertEqual(m.gpr[2], 0x77)
            self.assertEqual(m.gpr[4:8], [0xAAAA, 0, 0x3333, 0x4444])   # a0 as the call left it, a1 from the delay slot
            entry = cb.parse_log_entry(m.mem[RING - SLOT + 32 * call:RING - SLOT + 32 * call + 32])
            number, count, w1, w2, s1, s2, s3 = entry
            self.assertEqual((number, w1, w2, s2), (call, 0xCAFE, call * 7, 0))
            self.assertEqual(s1, checksum([0x80000001 * (k + 1) & 0xFFFFFFFF for k in range(8)]))
            self.assertEqual(s3, checksum([0x80000001]))
            self.assertGreater(count, 1000)
        self.assertEqual(m.calls, 6)
        self.assertEqual(presses, [0x10, 0x10, 0x4010, 0x4010, 0x10, 0x10])   # Calls 3 and 4.

    def test_ring_wraps(self):
        image = cb.log_stub_words(SLOT, TARGET, DELAY, RESUME, RING, (RAM, RAM), ((RAM, RAM),) * 3)
        m = Machine(image)
        m.put(SLOT + cb.DATA + cb.COUNT, cb.LOG_ENTRIES - 1)
        m.call()
        m.call()
        self.assertEqual(m.word(RING), cb.LOG_ENTRIES)            # Call 4096 at entry 0, 4097 at entry 1.
        self.assertEqual(m.word(RING + 32), cb.LOG_ENTRIES + 1)

    def test_set_stub_changes_one_call(self):
        target = 0x00101A60
        entry = [0x27BDFFF0, 0x0080182D]                       # addiu sp, sp, -0x10; daddu v1, a0, zero
        image = cb.set_stub_words(SLOT, target, entry, 2, {4: 0, 7: 1})
        m = Machine(image)
        seen = []
        for call in range(1, 4):
            m.gpr[4], m.gpr[7], m.gpr[29] = 9, 5, 0x1000
            pc = SLOT
            while pc != target + 8:
                jump = m.step(pc)
                if jump is not None:
                    self.assertIsNone(m.step(pc + 4))
                    pc = jump
                else:
                    pc += 4
            seen.append((m.gpr[4], m.gpr[7], m.gpr[3], m.gpr[29]))
        self.assertEqual(seen, [(9, 5, 9, 0xFF0), (0, 1, 0, 0xFF0), (9, 5, 9, 0xFF0)])

    def test_rejects_unaligned_ranges(self):
        with self.assertRaises(ValueError):
            cb.log_stub_words(SLOT, TARGET, DELAY, RESUME, RING, (RAM, RAM), ((RAM + 2, RAM + 8), (RAM, RAM), (RAM, RAM)))


if __name__ == '__main__':
    unittest.main()
