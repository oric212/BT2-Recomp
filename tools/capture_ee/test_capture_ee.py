import struct
import unittest
from capture_ee import export_rva, instrument, TRAP, HANDLER, MARKER


class CaptureTests(unittest.TestCase):
    def test_authored_trap_encodings(self):
        words = struct.unpack('<6I', TRAP)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), MARKER)
        self.assertEqual(words[3], (0x2b << 26) | (24 << 21) | (25 << 16))
        self.assertEqual((words[4] & 0x03ffffff) << 2, HANDLER + 16)
        self.assertEqual(words[5], 0)

    def test_wrong_elf_rejected(self):
        with self.assertRaises(ValueError):
            instrument(bytes(52))

    def test_pe_export_lookup_and_missing_symbol(self):
        data = bytearray(1024)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 0x3c, 0x80)
        data[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<HH', data, 0x84, 0x8664, 1)
        struct.pack_into('<H', data, 0x94, 240)
        struct.pack_into('<H', data, 0x98, 0x20b)
        struct.pack_into('<II', data, 0x98+112, 0x1000, 40)
        struct.pack_into('<IIII', data, 0x98+240+8, 512, 0x1000, 512, 512)
        struct.pack_into('<6I', data, 512+16, 1, 1, 1, 0x1040, 0x1044, 0x1048)
        struct.pack_into('<I', data, 576, 0x2000)
        struct.pack_into('<I', data, 580, 0x1050)
        struct.pack_into('<H', data, 584, 0)
        data[592:598] = b'EEmem\0'
        self.assertEqual(export_rva(data, 'EEmem'), 0x2000)
        with self.assertRaises(ValueError):
            export_rva(data, 'missing')
        struct.pack_into('<I', data, 576, 0x1000)
        with self.assertRaises(ValueError):
            export_rva(data, 'EEmem')

    def test_truncated_pe_rejected(self):
        for data in (b'', b'MZ', b'MZ' + bytes(100)):
            with self.assertRaises(ValueError):
                export_rva(data, 'EEmem')
