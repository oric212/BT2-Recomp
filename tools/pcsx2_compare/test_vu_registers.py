import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import vu_registers as vr  # noqa: E402


def image_with_sections(sections):
    """A PE header with the given (rva, size, flags) sections, enough for writable_sections."""
    pe = 0x80
    data = bytearray(0x400)
    struct.pack_into('<I', data, 0x3C, pe)
    data[pe:pe + 4] = b'PE\0\0'
    struct.pack_into('<H', data, pe + 6, len(sections))
    struct.pack_into('<H', data, pe + 20, 0xF0)
    table = pe + 24 + 0xF0
    for k, (rva, size, flags) in enumerate(sections):
        struct.pack_into('<II', data, table + 40 * k + 8, size, rva)
        struct.pack_into('<I', data, table + 40 * k + 36, flags)
    return bytes(data)


class VuRegistersTest(unittest.TestCase):
    def test_locates_vu1_after_vu0(self):
        base, vumem, entry = 0x140000000, 0x7FF000000000, 0x900
        memory = bytearray(0x10000)
        vu0_vf = bytes(range(256)) * 2
        vu0 = 0x2000
        memory[vu0:vu0 + 512] = vu0_vf
        struct.pack_into('<Q', memory, vu0 + 0x480, vumem + vr.VU0_DATA)
        memory[vu0 + entry:vu0 + entry + 16] = vr.VF0
        struct.pack_into('<Q', memory, vu0 + entry + 0x480, vumem + vr.VU1_DATA)
        image = image_with_sections([(0x1000, 0x1000, 0x60000020), (0x2000, 0x4000, 0xC0000040)])

        def read(address, size):
            at = address - base
            return bytes(memory[at:at + size])

        self.assertEqual(vr.locate(read, base, image, vumem, vu0_vf), base + vu0 + entry)

    def test_refuses_without_vf0(self):
        base, vumem = 0x140000000, 0x7FF000000000
        memory = bytearray(0x10000)
        vu0_vf = b'\x01' * 512
        memory[0x2000:0x2200] = vu0_vf
        struct.pack_into('<Q', memory, 0x2480, vumem + vr.VU0_DATA)
        struct.pack_into('<Q', memory, 0x2D80, vumem + vr.VU1_DATA)
        image = image_with_sections([(0x2000, 0x4000, 0xC0000040)])
        with self.assertRaises(LookupError):
            vr.locate(lambda a, s: bytes(memory[a - base:a - base + s]), base, image, vumem, vu0_vf)


if __name__ == '__main__':
    unittest.main()
