"""The service log's stubs (rpc_log.stub_words) run on the logging stub's small interpreter."""
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import context_block as cb  # noqa: E402
import rpc_log  # noqa: E402
from test_log_stub import RAM, RING, SLOT, Machine, checksum  # noqa: E402


class RpcMachine(Machine):
    def step(self, pc):
        w = self.word(pc)
        op, rs, rt, rd, fn = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, w & 63
        if op == 0 and fn == 0x24:                                       # and
            self.count += 1
            self.gpr[rd] = self.gpr[rs] & self.gpr[rt]
            return None
        if op == 0x3F:                                                   # sd
            self.count += 1
            imm = w & 0xFFFF
            ea = (self.gpr[rs] + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF
            struct.pack_into('<Q', self.mem, ea - SLOT, self.gpr[rt] & 0xFFFFFFFFFFFFFFFF)
            return None
        return super().step(pc)

    def run(self, start, end):
        pc = start
        for _ in range(100000):
            if pc == end:
                return
            jump = self.step(pc)
            if jump is not None:
                assert self.step(pc + 4) is None, 'a jump in a delay slot'
                pc = jump
            else:
                pc += 4
        raise AssertionError('the stub did not return')


def record(m, number):
    at = RING - SLOT + cb.LOG_ENTRY * (number % cb.LOG_ENTRIES)
    return rpc_log.parse_entry(m.mem[at:at + cb.LOG_ENTRY])


class RpcLogTest(unittest.TestCase):
    def setUp(self):
        self.m = RpcMachine(rpc_log.stub_words(SLOT, RING))
        self.send = [0x11111111, 0x80000002, 0x3, 0xFFFFFFFF, 0x55]
        for k, word in enumerate(self.send):
            self.m.put(RAM + 4 * k, word)

    def test_request_records_the_call_and_keeps_its_arguments(self):
        m = self.m
        g = m.gpr
        g[4:12] = [0x3B1230, 0x10, 1, RAM, 18, 0x2000, 0x40, 0x123456]        # a0..t3; 18 bytes: four whole words
        g[17], g[29] = 0xABCD, RAM + 0x80 + 192
        m.run(SLOT, rpc_log.CALL + 8)
        self.assertEqual(g[4:12], [0x3B1230, 0x10, 1, RAM, 18, 0x2000, 0x40, 0x123456])
        self.assertEqual(g[29], RAM + 0x80)                                    # the entry's two words ran
        self.assertEqual(m.word(RAM + 0x80 + 48), 0xABCD)
        r = record(m, 1)
        self.assertEqual((r['number'], r['reply'], r['function'], r['client'], r['mode'], r['size'], r['address']),
                         (1, False, 0x10, 0x3B1230, 1, 18, RAM))
        self.assertEqual(r['checksum'], checksum(self.send[:4]))
        self.assertEqual(r['checksum'], rpc_log.checksum(struct.pack('<5I', *self.send)[:18]))
        self.assertGreater(r['count'], 1000)

    def test_reply_records_the_receive_buffer(self):
        m = self.m
        g = m.gpr
        packet = RAM + 0x40
        m.put(packet + 44, 12)
        g[16], g[17], g[20], g[22], g[30], g[31], g[2] = packet, 0x3B1230, RAM + 4, 0x80001363, 0, 0x102EE8, 0x99
        m.put(SLOT + cb.DATA + cb.COUNT, 5)
        m.run(SLOT + rpc_log.REPLY_CODE, rpc_log.DONE + 4)
        self.assertEqual((g[2], g[31], g[16], g[17]), (0, 0x102EE8, packet, 0x3B1230))   # v0 = 0 as the game sets it
        r = record(m, 6)
        self.assertEqual((r['number'], r['reply'], r['function'], r['client'], r['size'], r['address']),
                         (6, True, 0x80001363, 0x3B1230, 12, RAM + 4))
        self.assertEqual(r['checksum'], checksum(self.send[1:4]))

    def test_empty_buffer_and_patches(self):
        m = self.m
        m.gpr[4:9] = [0x3B1230, 2, 0, 0, 0]
        m.gpr[29] = RAM + 0x80 + 192
        m.run(SLOT, rpc_log.CALL + 8)
        self.assertEqual(record(m, 1)['checksum'], 0)
        p = rpc_log.patches(SLOT)
        self.assertEqual(p[rpc_log.CALL], (rpc_log.ENTRY_WORDS, (0x08000000 | (SLOT >> 2), 0)))
        self.assertEqual(p[rpc_log.DONE][1], (0x08000000 | ((SLOT + rpc_log.REPLY_CODE) >> 2),))


if __name__ == '__main__':
    unittest.main()
