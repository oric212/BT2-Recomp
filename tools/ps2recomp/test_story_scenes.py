import struct,unittest
import story_scenes


def section(tag,body,extra=0):
    return tag+struct.pack('<3I',16,len(body),extra)+body


def script(blocks):
    """A synthetic GSCF file: an empty header section, the code blocks, an empty data section."""
    code=b''.join(section(b'GSAC',struct.pack(f'<{len(words)}I',*words),number)+b'EOFC'+struct.pack('<3I',16,0,0)
                  for number,words in blocks)
    body=section(b'GSHD',b'')+section(b'GSCD',code,1)+section(b'GSTD',b'')
    return b'GSCF'+struct.pack('<3I',16,len(body),1)+body


class StorySceneTests(unittest.TestCase):
    def test_instructions_take_their_argument_words(self):
        words=struct.pack('<6I',0x0FA10301,0x0A,0x20A,0x30A,0x2AF90001,0)
        self.assertEqual(story_scenes.instructions(words),[(4001,[(0,10),(2,10),(3,10)]),(11001,[])])

    def test_scene_blocks(self):
        data=script([(0xFFFFFFFF,[0x3A990101,0x0A,0,0]),(1000,[0x03F40101,0x210A])])
        blocks=story_scenes.scene(data)
        self.assertEqual(blocks[0xFFFFFFFF],[(15001,[(0,10)])])
        self.assertEqual(blocks[1000],[(1012,[(0x21,10)])])
        with self.assertRaises(ValueError):story_scenes.scene(b'XXXX'+data[4:])

    def test_survey_follows_the_next_scene(self):
        class Files:
            def __init__(self,files):self.files=files
            def entry(self,n):
                if n not in self.files:raise ValueError('missing')
                return self.files[n]
        files={33:script([(1000,[0x03F40101,0x280A])]),40:script([(1000,[0x2AF90001])]),36:script([])}
        parts=story_scenes.survey(Files(files),[2])
        self.assertEqual(parts[0]['scenes'],[33,40])
        self.assertEqual(parts[0]['commands'],{1012:1,11001:1})
        self.assertEqual(parts[1]['first_scene'],36)


if __name__=='__main__':unittest.main()
