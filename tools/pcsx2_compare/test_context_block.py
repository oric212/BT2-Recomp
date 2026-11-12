"""The checkpoint stub run on a small interpreter of the instructions it uses (context_block.py)."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import context_block as cb  # noqa: E402

SLOT, TARGET, RESUME = 0x01D00000, 0x0011E988, 0x00102BC0
DELAY = 0x03A0282D                       # daddu a1, sp, zero


class Machine:
    """Runs a stub; the target 'returns' v0 = its call count; a mark is answered by the host at once."""

    def __init__(self, image, host):
        self.mem = bytearray(image)
        self.gpr = [0] * 32
        self.gpr[29], self.gpr[4] = 0x01FFF000, 2
        self.hi, self.lo, self.hi1, self.lo1, self.sa = 0x11, 0x22, 0x33, 0x44, 0
        self.fpr = [0x3F800000 + k for k in range(32)]
        self.vi = list(range(100, 116))
        self.vf = [bytes([v]) * 16 for v in range(32)]
        self.host, self.calls = host, 0

    def word(self, address):
        return struct.unpack_from('<I', self.mem, address - SLOT)[0]

    def store(self, address, data):
        self.mem[address - SLOT:address - SLOT + len(data)] = data
        if address == SLOT + cb.DATA + cb.MARK and data != bytes(4):
            self.host(self, struct.unpack('<I', data)[0])

    def run(self, body=TARGET, stop=RESUME):
        pc, after = SLOT, None
        for _ in range(100000):
            if pc == stop:
                return
            if pc == body:
                self.calls += 1
                self.gpr[2], self.gpr[24], self.gpr[25] = self.calls, 0xDEAD, 0xBEEF
                pc = self.gpr[31]
                continue
            w = self.word(pc)
            op, rs, rt, rd, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, w & 0xFFFF
            simm = imm - 0x10000 if imm & 0x8000 else imm
            ea = (self.gpr[rs] + simm) & 0xFFFFFFFF
            jump = None
            if w == 0:
                pass
            elif op == 0x0F:
                self.gpr[rt] = imm << 16
            elif op == 0x0D:
                self.gpr[rt] = self.gpr[rs] | imm
            elif op == 0x09:
                self.gpr[rt] = (self.gpr[rs] + simm) & 0xFFFFFFFF
            elif op == 0x23:
                self.gpr[rt] = struct.unpack_from('<I', self.mem, ea - SLOT)[0]
            elif op == 0x1E:
                self.gpr[rt] = int.from_bytes(self.mem[ea - SLOT:ea - SLOT + 16], 'little')
            elif op == 0x31:
                self.fpr[rt] = struct.unpack_from('<I', self.mem, ea - SLOT)[0]
            elif op == 0 and w & 63 == 8:
                jump = self.gpr[rs]
            elif op == 0x2B:
                self.store(ea, struct.pack('<I', self.gpr[rt] & 0xFFFFFFFF))
            elif op == 0x3F:
                self.store(ea, struct.pack('<Q', self.gpr[rt] & 0xFFFFFFFFFFFFFFFF))
            elif op == 0x1F:
                self.store(ea, self.gpr[rt].to_bytes(16, 'little'))
            elif op == 0x39:
                self.store(ea, struct.pack('<I', self.fpr[rt]))
            elif op == 0x3E:
                self.store(ea, self.vf[rt])
            elif op in (4, 5):
                if (self.gpr[rs] == self.gpr[rt]) == (op == 4):
                    jump = pc + 4 + 4 * simm
            elif op in (2, 3):
                jump = (w & 0x3FFFFFF) << 2
                if op == 3:
                    self.gpr[31] = pc + 8
            elif op == 0 and w & 63 == 0x2D:
                self.gpr[rd] = self.gpr[rs] + self.gpr[rt]
            elif op == 0 and w & 63 in (0x10, 0x12, 0x28):
                self.gpr[rd] = {0x10: self.hi, 0x12: self.lo, 0x28: self.sa}[w & 63]
            elif op == 0x1C and w & 63 in (0x10, 0x12):
                self.gpr[rd] = self.hi1 if w & 63 == 0x10 else self.lo1
            elif op == 0x11 and rs == 2:
                self.gpr[rt] = 0x5A5A
            elif op == 0x12 and rs == 2:
                self.gpr[rt] = self.vi[rd] if rd < 16 else 0x1000 + rd
            else:
                raise AssertionError(f'unexpected instruction {w:#010x}')
            self.gpr[0] = 0
            if jump is not None:
                pc += 4
                continue_at = jump
                # The delay slot.
                saved = self.word(pc)
                if saved:
                    self.gpr_step(saved)
                pc = continue_at
            else:
                pc += 4
        raise AssertionError('the stub did not return')

    def gpr_step(self, w):
        op, rs, rt = w >> 26, (w >> 21) & 31, (w >> 16) & 31
        imm = w & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm
        if op == 0x2B:
            self.store((self.gpr[rs] + simm) & 0xFFFFFFFF, struct.pack('<I', self.gpr[rt] & 0xFFFFFFFF))
        else:
            raise AssertionError(f'unexpected delay slot {w:#010x}')


def run_calls(first_skip, want_after, calls, next_skip=1):
    image = cb.stub_words(SLOT, TARGET, DELAY, RESUME, first_skip, want_after)
    seen = []

    def host(machine, mark):
        base = SLOT + cb.DATA - SLOT
        block = bytes(machine.mem[base + cb.BLOCK:base + cb.BLOCK + cb.BLOCK_SIZE])
        count = struct.unpack_from('<I', machine.mem, base + cb.COUNT)[0]
        seen.append((count, mark, cb.parse(block)))
        if mark == cb.MARK_BEFORE:
            struct.pack_into('<I', machine.mem, base + cb.SKIP, next_skip)
        struct.pack_into('<I', machine.mem, base + cb.MARK, 0)

    machine = Machine(image, host)
    for _ in range(calls):
        machine.gpr[31] = 0x12345678
        machine.run()
        machine.gpr[24] = machine.gpr[25] = 0
    return machine, seen


class StubTests(unittest.TestCase):
    def test_counts_skips_and_publishes_before_and_after(self):
        machine, seen = run_calls(first_skip=2, want_after=True, calls=7, next_skip=1)
        self.assertEqual(machine.calls, 7)
        self.assertEqual([(c, m) for c, m, _ in seen], [(3, 1), (3, 2), (5, 1), (5, 2), (7, 1), (7, 2)])
        before, after = seen[0][2], seen[1][2]
        self.assertEqual(before['gpr'][5], 0x01FFF000)          # the delay slot ran before the publication
        self.assertEqual(before['gpr'][4], 2)
        self.assertEqual(before['gpr'][24], SLOT + cb.DATA)     # t8 is the stub's
        self.assertEqual(after['gpr'][2], 3)                     # v0 of the third call
        self.assertEqual((before['hi'], before['lo'], before['hi1'], before['lo1']), (0x11, 0x22, 0x33, 0x44))
        self.assertEqual(before['fpr'][7], 0x3F800007)
        self.assertEqual(before['fcr31'], 0x5A5A)
        self.assertEqual(before['vi'][15], 115)
        self.assertEqual(before['vf'][9], '09' * 16)

    def test_the_host_changes_arguments_and_results(self):
        image = cb.stub_words(SLOT, TARGET, DELAY, RESUME, 0, True)
        seen = []

        def host(machine, mark):
            base = cb.DATA + cb.BLOCK
            seen.append((mark, machine.gpr[4], machine.calls))
            register = 4 if mark == cb.MARK_BEFORE else 2       # a0 before the call, v0 after it
            machine.mem[base + cb.GPRS + 16 * register:base + cb.GPRS + 16 * register + 8] = struct.pack('<Q', 0x77)
            struct.pack_into('<I', machine.mem, cb.DATA + cb.SKIP, cb.NEVER)
            struct.pack_into('<I', machine.mem, cb.DATA + cb.MARK, 0)

        machine = Machine(image, host)
        machine.gpr[31] = 0x12345678
        machine.run()
        self.assertEqual([m for m, _, _ in seen], [1, 2])
        self.assertEqual(machine.gpr[4] & 0xFFFFFFFF, 0x77)            # the call saw the changed argument
        self.assertEqual(machine.gpr[2] & 0xFFFFFFFF, 0x77)            # and the caller the changed result

    def test_function_entry(self):
        # The function's first words (addiu sp,sp,-16; daddu a2,a0,zero) run in the stub, then target + 8; with
        # `after` the function returns into the stub, which returns to the caller's ra.
        entry = [0x27BDFFF0, 0x0080302D]
        for want_after, marks in ((True, [1, 2]), (False, [1])):
            image = cb.stub_words(SLOT, TARGET, 0, TARGET + 8, 0, want_after, entry=entry)
            seen = []

            def host(machine, mark):
                seen.append(mark)
                struct.pack_into('<I', machine.mem, cb.DATA + cb.SKIP, cb.NEVER)
                struct.pack_into('<I', machine.mem, cb.DATA + cb.MARK, 0)

            machine = Machine(image, host)
            machine.gpr[31] = 0x00400000
            machine.run(body=TARGET + 8, stop=0x00400000)
            self.assertEqual(seen, marks)
            self.assertEqual((machine.calls, machine.gpr[29], machine.gpr[6]), (1, 0x01FFF000 - 16, 2))
            self.assertEqual(machine.gpr[31], 0x00400000)
        with self.assertRaises(ValueError):
            cb.stub_words(SLOT, TARGET, 0, TARGET + 8, 0, True, entry=[0x10000003, 0])   # a branch

    def test_before_only(self):
        _, seen = run_calls(first_skip=0, want_after=False, calls=3, next_skip=cb.NEVER)
        self.assertEqual([(c, m) for c, m, _ in seen], [(1, 1)])

    def test_refuses_delay_slots_it_cannot_move(self):
        for word in (0x10000003, 0x03E00008, 0x0300202D, 0x27BF0000):   # b, jr ra, daddu a0,t8,zero, addiu ra,sp,0
            with self.assertRaises(ValueError):
                cb.check_delay_slot(word)
        cb.check_delay_slot(DELAY)

    def test_layout(self):
        self.assertEqual((cb.SA, cb.FCR31, cb.VU_Q), (0x2A0, 0x2A4, 0x2BC))
        self.assertLessEqual(cb.DATA + cb.BLOCK + cb.BLOCK_SIZE, cb.SLOT)
        block = cb.from_registers(a0=2, sp=0x01FFF000)
        self.assertEqual(cb.parse(block)['gpr'][29], 0x01FFF000)


if __name__ == '__main__':
    unittest.main()
