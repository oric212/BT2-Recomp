import struct
import unittest
from capture_ee import export_rva, instrument, TRAP, HANDLER, MARKER, capture_trap


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

    def test_timer_trap_reads_exact_mmio_and_publishes_payload(self):
        address, trap, marker = capture_trap('timer3-mode')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338a1c)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x10001810)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(words[5], 0xaf190004)
        self.assertEqual(marker, 0xb703)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)

    def test_timer2_trap_reads_reached_register(self):
        address, trap, marker = capture_trap('timer2-mode')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338c58)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x10001010)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(words[5], 0xaf190004)
        self.assertEqual(marker, 0xb704)
        self.assertEqual(words[6], 0x3419b704)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)

    def test_intc_trap_reads_mask_before_enable_syscall(self):
        address, trap, marker = capture_trap('intc-mask')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x32e9a4)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x1000f010)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(marker, 0xb705)
        self.assertEqual(words[6], 0x3419b705)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)

    def test_timer2_readback_trap_at_second_read(self):
        address, trap, marker = capture_trap('timer2-readback')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338dd8)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x10001010)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(words[5], 0xaf190004)
        self.assertEqual(marker, 0xb706)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)


class OsdCaptureTests(unittest.TestCase):
    def test_osd_trap_reads_returned_stack_word_before_setter(self):
        address, trap, marker = capture_trap('osd-config')
        words = struct.unpack('<8I', trap)
        self.assertEqual(address, 0x3385F4)
        self.assertEqual(words[0], (0x23 << 26) | (29 << 21) | (25 << 16))
        self.assertEqual(((words[1] & 0xffff) << 16) | (words[2] & 0xffff), MARKER)
        self.assertEqual(words[3], 0xAF190004)
        self.assertEqual(marker, 0xB707)
        self.assertEqual(words[5], 0xAF190000)
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)
        self.assertEqual(words[7], 0)
