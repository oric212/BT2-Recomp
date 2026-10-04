import struct
import unittest
from verify_rpc_bind import validate_state


class BindEvidenceTests(unittest.TestCase):
    def setUp(self):
        # Authored protocol fixture only; no retail bytes or captures.
        self.ee = bytearray(32 * 1024 * 1024)
        self.iop = bytearray(2 * 1024 * 1024)
        def put(data, address, *words):
            struct.pack_into('<' + str(len(words)) + 'I', data, address, *words)
        put(self.ee, 0x387E80, 0xB70F, 0x801, 0x1FFEC20)
        put(self.iop, 0x19600, 0, 0, 0x80000009, 0, 5, 0x203A97C0, 2, 0x3A87A8, 0x80000592)
        put(self.iop, 0x196C0, 0x19600, 0x19680, 0x3A94C0)
        put(self.ee, 0x3A94C0, 0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
            0x3A87A8, 0x80000009, 0x3F570, 0x3FB20)
        put(self.iop, 0x3F570, 0x80000592, 0x39534, 0x3FB20)
        put(self.iop, 0x3F570 + 64, 0x3F558)
        put(self.iop, 0x3F558 + 8, 0x3F570)
        put(self.ee, 0x3A87A8, 0, 2, 3, 0, 0, 0x3FB20, 0, 0, 0, 0x3F570)
        put(self.ee, 0x3A97C0, 64, 0, 0x80000009, 0, 4, 0x203A97C0, 0,
            0x3A87A8, 0x80000592)
        for command, callback in ((8, 0x3322E8), (9, 0x3326C0), (10, 0x3328E0), (12, 0x3323F8)):
            put(self.ee, 0x3A95C0 + command * 12, callback, 0x3AAFC0, 0x38BDF0)

    def test_consistent_protocol_state(self):
        validate_state(self.ee, self.iop)

    def test_wrong_reply_client_queue_and_packet_rejected(self):
        for data, address in ((self.ee, 0x3A94E4), (self.ee, 0x3A87A8),
                              (self.iop, 0x3F560), (self.iop, 0x19620),
                              (self.ee, 0x3A97D0), (self.ee, 0x3A9620)):
            with self.subTest(address=hex(address)):
                original = data[address]
                data[address] ^= 1
                with self.assertRaises(ValueError):
                    validate_state(self.ee, self.iop)
                data[address] = original

    def test_wrong_sizes_rejected(self):
        with self.assertRaises(ValueError):
            validate_state(self.ee[:-1], self.iop)


if __name__ == '__main__':
    unittest.main()
