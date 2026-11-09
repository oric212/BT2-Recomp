import struct, tempfile, unittest
from pathlib import Path
from card_image import CardImage, MAGIC
from import_save import BadSave, write_save

PAGE, PER_CLUSTER, CLUSTERS, ALLOC = 512, 2, 64, 8
STAMP_A, STAMP_B = bytes(range(8)), bytes(range(8, 16))
END = 0xFFFFFFFF


def entry(mode, length, cluster, name):
    raw = struct.pack('<HxxI8sI4x8s', mode, length, STAMP_A, cluster, STAMP_B)
    return (raw + bytes(0x40 - len(raw)) + name.encode().ljust(32, b'\0')).ljust(512, b'\0')


def card(spare=False, chain=None, content=bytes(range(256)) * 3, length=None):
    """A small card: the root (allocatable clusters 0-1) holds one save folder (2-3) with one 768-byte file (4)."""
    clusters = [bytes(PAGE * PER_CLUSTER)] * CLUSTERS
    sb = bytearray(PAGE * PER_CLUSTER)
    sb[:len(MAGIC)] = MAGIC
    struct.pack_into('<HH', sb, 0x28, PAGE, PER_CLUSTER)
    struct.pack_into('<IIII', sb, 0x30, CLUSTERS, ALLOC, CLUSTERS, 0)
    struct.pack_into('<I', sb, 0x50, 2)  # The one indirect FAT cluster.
    clusters[0] = bytes(sb)
    clusters[2] = struct.pack('<I', 3).ljust(PAGE * PER_CLUSTER, b'\0')
    fat = chain or {0: 1, 1: END, 2: 3, 3: END, 4: END}
    table = [0] * (PAGE * PER_CLUSTER // 4)
    for n, nxt in fat.items(): table[n] = 0x80000000 | nxt
    clusters[3] = struct.pack(f'<{len(table)}I', *table)
    root = entry(0x8427, 3, 0, '.') + entry(0x8427, 0, 0, '..') + entry(0x8427, 3, 2, 'BATEST-00000SAVE')
    folder = entry(0x8427, 3, 2, '.') + entry(0x8427, 0, 0, '..') + entry(0x8497, length or len(content), 4, 'data')
    clusters[ALLOC + 0], clusters[ALLOC + 1] = root[:1024], root[1024:].ljust(1024, b'\0')
    clusters[ALLOC + 2], clusters[ALLOC + 3] = folder[:1024], folder[1024:].ljust(1024, b'\0')
    clusters[ALLOC + 4] = content.ljust(1024, b'\0')
    pages = b''.join(clusters)
    if spare: pages = b''.join(pages[i:i + PAGE] + b'\xee' * 16 for i in range(0, len(pages), PAGE))
    return pages


class CardImageTest(unittest.TestCase):
    def test_saves_are_listed_and_read_with_and_without_spare_bytes(self):
        for spare in (False, True):
            image = CardImage(card(spare))
            self.assertEqual([e['name'] for e in image.saves()], ['BATEST-00000SAVE'])
            folder, _, mode, created, modified, files = image.save('BATEST-00000SAVE')
            self.assertEqual((folder, mode, created, modified), ('BATEST-00000SAVE', 0x8427, STAMP_A, STAMP_B))
            self.assertEqual(files, [('data', 0x8497, STAMP_A, STAMP_B, bytes(range(256)) * 3)])

    def test_a_save_lands_on_a_folder_card_as_imported_ones_do(self):
        with tempfile.TemporaryDirectory() as folder:
            target = write_save(folder, CardImage(card()).save('BATEST-00000SAVE'))
            self.assertEqual((target / 'data').read_bytes(), bytes(range(256)) * 3)
            self.assertEqual((target / 'entries.txt').read_text().splitlines()[1], f'data 8497 {STAMP_A.hex()} {STAMP_B.hex()}')

    def test_damage_is_refused_by_what_is_wrong(self):
        with self.assertRaisesRegex(BadSave, 'superblock'): CardImage(b'not a card' * 100)
        with self.assertRaisesRegex(BadSave, 'not the size'): CardImage(card()[:-1])
        with self.assertRaisesRegex(BadSave, 'loops'): CardImage(card(chain={0: 0, 2: 3, 3: END, 4: END})).saves()
        with self.assertRaisesRegex(BadSave, 'no save'): CardImage(card()).save('OTHER')
        with self.assertRaisesRegex(BadSave, 'ends before'): CardImage(card(length=2000)).save('BATEST-00000SAVE')


if __name__ == '__main__':
    unittest.main()
