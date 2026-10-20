import tempfile
import unittest
from pathlib import Path

import texture_name_check


class TextureNameCheckTest(unittest.TestCase):
    def test_names_found_by_folder_and_texture_hash(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            names = d / 'names.txt'
            names.write_text('# uploaded_pixels 10\n1a-2b-00001a53 3\n1a-ff-00001a53 1\n9c-00000000 2\n', encoding='utf-8')
            pack = d / 'pack' / 'replacements'
            (pack / 'Stages').mkdir(parents=True)
            (pack / 'Stages' / '1a-2b-00001a53.dds').write_bytes(b'')
            (pack / '9c-00000000.png').write_bytes(b'')
            (pack / 'notes.txt').write_bytes(b'')
            drawn, notes = texture_name_check.read_names(names)
            self.assertEqual(drawn, {'1a-2b-00001a53': 3, '1a-ff-00001a53': 1, '9c-00000000': 2})
            self.assertEqual(notes, ['uploaded_pixels 10'])
            index = texture_name_check.pack_index(d / 'pack')
            self.assertEqual(index, {'1a-2b-00001a53': 'Stages', '9c-00000000': '.'})
            found, by_texture = texture_name_check.check(drawn, index)
            self.assertEqual(found, {'1a-2b-00001a53': 'Stages', '9c-00000000': '.'})
            self.assertEqual(by_texture, 3)  # The second name differs in its palette only.


if __name__ == '__main__':
    unittest.main()
