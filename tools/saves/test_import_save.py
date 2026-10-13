import struct, tempfile, unittest, zlib
from pathlib import Path
from import_save import BadSave, RC4_START, read_cbs, unscramble, write_save

STAMP_A, STAMP_B = bytes(range(8)), bytes(range(8, 16))


def container(files, folder=b'BATEST-00000SAVE', title=b'A test save', packed_is_file_length=False, damage=None):
    """A synthetic save in the container's layout (the scrambling is its own inverse)."""
    body = b''.join(STAMP_A + STAMP_B + struct.pack('<IHHII', len(content), mode, 0, 0, 0) + name.ljust(32, b'\0') + content for name, mode, content in files)
    packed = unscramble(zlib.compress(body))
    header_length = 92 + 36
    header = b'CFU\0' + struct.pack('<II', 8000, header_length) + struct.pack('<II', len(body), len(packed) + (header_length if packed_is_file_length else 0))
    header += folder.ljust(32, b'\0') + STAMP_A + STAMP_B + struct.pack('<IIIIII', 0, 0, 0x8427, 0, 0, 0) + title.ljust(36, b'\0')
    data = bytearray(header + packed)
    if damage: damage(data)
    return bytes(data)


class ImportSave(unittest.TestCase):
    FILES = [(b'icon.sys', 0x8497, b'I' * 964), (b'data', 0x8497, bytes(range(256)) * 3)]

    def test_the_starting_permutation_is_a_permutation(self):
        self.assertEqual(sorted(RC4_START), list(range(256)))
        self.assertEqual(unscramble(unscramble(b'any bytes at all')), b'any bytes at all')

    def test_a_save_is_read_with_its_files_modes_and_times(self):
        for whole in (False, True):  # The packed length names the body or the whole file.
            folder, title, mode, created, modified, files = read_cbs(container(self.FILES, packed_is_file_length=whole))
            self.assertEqual((folder, title, mode, created, modified), ('BATEST-00000SAVE', 'A test save', 0x8427, STAMP_A, STAMP_B))
            self.assertEqual([(f[0], f[1], f[2], f[3], f[4]) for f in files],
                             [('icon.sys', 0x8497, STAMP_A, STAMP_B, b'I' * 964), ('data', 0x8497, STAMP_A, STAMP_B, bytes(range(256)) * 3)])

    def test_damaged_saves_are_refused(self):
        with self.assertRaisesRegex(BadSave, 'signature'): read_cbs(b'XFU\0' + bytes(200))
        with self.assertRaisesRegex(BadSave, 'header length'): read_cbs(container(self.FILES, damage=lambda d: d.__setitem__(slice(8, 12), struct.pack('<I', 40))))
        with self.assertRaisesRegex(BadSave, 'not as long'): read_cbs(container(self.FILES) + b'extra')
        with self.assertRaisesRegex(BadSave, 'does not unpack'): read_cbs(container(self.FILES, damage=lambda d: d.__setitem__(-20, d[-20] ^ 0xFF)))
        with self.assertRaisesRegex(BadSave, 'unpacked body'): read_cbs(container(self.FILES, damage=lambda d: d.__setitem__(slice(12, 16), struct.pack('<I', 7))))
        with self.assertRaisesRegex(BadSave, 'cannot be stored'): read_cbs(container([(b'..', 0x8497, b'x')]))
        with self.assertRaisesRegex(BadSave, 'cannot be stored'): read_cbs(container(self.FILES, folder=b'a/b'))

    def test_a_save_is_written_as_a_folder_and_never_over_another(self):
        with tempfile.TemporaryDirectory() as card:
            target = write_save(card, read_cbs(container(self.FILES)))
            self.assertEqual(target, Path(card) / 'BATEST-00000SAVE')
            self.assertEqual((target / 'icon.sys').read_bytes(), b'I' * 964)
            self.assertEqual((target / 'data').read_bytes(), bytes(range(256)) * 3)
            self.assertEqual((target / 'entries.txt').read_text().splitlines(),
                             ['. 8427 0001020304050607 08090a0b0c0d0e0f', 'icon.sys 8497 0001020304050607 08090a0b0c0d0e0f', 'data 8497 0001020304050607 08090a0b0c0d0e0f'])
            with self.assertRaisesRegex(BadSave, 'exists already'): write_save(card, read_cbs(container(self.FILES)))


if __name__ == '__main__':
    unittest.main()
