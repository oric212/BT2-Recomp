"""The native passes' shader headers match their sources (build_native_shaders.py --check)."""
import os
import pathlib
import shutil
import subprocess
import sys
import unittest

TOOL = pathlib.Path(__file__).with_name('build_native_shaders.py')


def has_glslc():
    sdk = os.environ.get('VULKAN_SDK')
    return bool(shutil.which('glslc') or (sdk and (pathlib.Path(sdk) / 'Bin' / 'glslc.exe').exists()))


class NativeShaderHeaders(unittest.TestCase):
    @unittest.skipUnless(has_glslc(), 'glslc (Vulkan SDK) not installed')
    def test_headers_current(self):
        result = subprocess.run([sys.executable, str(TOOL), '--check'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)


if __name__ == '__main__':
    unittest.main()
