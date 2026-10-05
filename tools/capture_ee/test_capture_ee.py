import struct
import unittest
from capture_ee import export_rva, instrument, TRAP, HANDLER, MARKER, capture_trap


class CaptureTests(unittest.TestCase):
    def test_loadfile_traps_do_not_overlap_the_caller(self):
        for point,address,marker in [('loadfile-bind-after',0x337264,0xB737),('loadfile-version-after',0x3372A8,0xB738),('loadfile-module-before',0x337BDC,0xB739),('loadfile-module-after',0x337BE4,0xB73A)]:
            actual,trap,value=capture_trap(point);words=struct.unpack('<8I',trap)
            self.assertEqual((actual,value),(address,marker))
            self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
            self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)
            self.assertLessEqual(address+len(trap),0x337C28)

    def test_authored_trap_encodings(self):
        words = struct.unpack('<6I', TRAP)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), MARKER)
        self.assertEqual(words[3], (0x2b << 26) | (24 << 21) | (25 << 16))
        self.assertEqual((words[4] & 0x03ffffff) << 2, HANDLER + 16)
        self.assertEqual(words[5], 0)

    def test_bind_capture_preserves_request_and_publishes_caller_boundary(self):
        address, trap, marker = capture_trap('sif-bind-after')
        words = struct.unpack('<8I', trap)
        self.assertEqual(address, 0x33284C)
        self.assertEqual(marker, 0xB70F)
        self.assertEqual(words[2:4], (0xAF020004, 0xAF1D0008))
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)
        self.assertEqual(words[7], 0)

    def test_rpc_call_semaphore_capture_boundary(self):
        address, trap, marker = capture_trap('rpc-call-sema')
        words = struct.unpack('<8I', trap)
        self.assertEqual((address, marker), (0x332AD0, 0xB710))
        self.assertEqual(words[2:4], (0xAF020004, 0xAF1D0008))
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)

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

    def test_dma_status_trap_reads_reached_register(self):
        address, trap, marker = capture_trap('dma-status')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x3319CC)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x1000E010)
        self.assertEqual(words[2], 0x8F190000)
        self.assertEqual(words[5], 0xAF190004)
        self.assertEqual(marker, 0xB709)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)

    def test_dma5_control_trap_reads_reached_register(self):
        address, trap, marker = capture_trap('dma5-control')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x3319EC)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x1000C000)
        self.assertEqual(words[2], 0x8F190000)
        self.assertEqual(marker, 0xB70A)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)

    def test_sif_dma_traps_publish_result_and_stack(self):
        for point, expected_address, expected_marker in (
                ('sif-dma-before', 0x331D44, 0xB70C), ('sif-dma-after', 0x331D4C, 0xB70D)):
            address, trap, marker = capture_trap(point)
            words = struct.unpack('<8I', trap)
            self.assertEqual((address, marker), (expected_address, expected_marker))
            self.assertEqual(words[2:4], (0xAF020004, 0xAF1D0008))
            self.assertEqual((words[6] & 0x03FFFFFF) << 2, address + 24)
            self.assertEqual(words[7], 0)

    def test_sif_rpc_query_trap(self):
        address, trap, marker = capture_trap('sif-rpc-state')
        words = struct.unpack('<8I', trap)
        self.assertEqual((address, marker), (0x332110, 0xB70E))
        self.assertEqual(words[2:4], (0xAF020004, 0xAF040008))
        self.assertEqual((words[6] & 0x03FFFFFF) << 2, address + 24)

    def test_sif_trap_publishes_return_and_selector(self):
        address, trap, marker = capture_trap('sif-command')
        words = struct.unpack('<8I', trap)
        self.assertEqual(address, 0x331A34)
        self.assertEqual(words[2:4], (0xAF020004, 0xAF040008))
        self.assertEqual(marker, 0xB70B)
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)
        self.assertEqual(words[7], 0)


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


class OsdReadbackCaptureTests(unittest.TestCase):
    def test_trap_after_getter_before_restore(self):
        address, trap, marker = capture_trap('osd-readback')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338620)
        self.assertEqual(marker, 0xB708)
        self.assertEqual(words[2], 0x8FB90004)
        self.assertEqual(words[4], 0x8FB90000)
        self.assertEqual(words[3], 0xAF190004)
        self.assertEqual(words[5], 0xAF190008)
        self.assertEqual((words[8] & 0x3ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)


class McmanReturnCaptureTests(unittest.TestCase):
    def test_unique_caller_return_preserves_module_result_publication(self):
        address,trap,marker=capture_trap('mcman-module-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x100434,0xB73B))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class HeapBindReturnCaptureTests(unittest.TestCase):
    def test_unique_return_preserves_result_and_stack(self):
        address,trap,marker=capture_trap('fileio-bind-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x1005A4,0xB747))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class HeapAllocationReturnCaptureTests(unittest.TestCase):
    def test_original_allocation_return_is_published(self):
        address,trap,marker=capture_trap('heap-alloc-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x3205D4,0xB748))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class HeapCopyReturnCaptureTests(unittest.TestCase):
    def test_trap_follows_the_completed_poll_loop(self):
        address,trap,marker=capture_trap('heap-copy-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x320620,0xB749))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)

class ModuleBufferReturnCaptureTests(unittest.TestCase):
    def test_unique_buffer_load_return(self):
        address,trap,marker=capture_trap('module-buffer-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x320630,0xB74A))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)

class HeapFreeReturnCaptureTests(unittest.TestCase):
    def test_unique_owned_free_return(self):
        address,trap,marker=capture_trap('heap-free-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x320650,0xB74B))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)

class DmacStartupCaptureTests(unittest.TestCase):
    def test_unique_setup_return(self):
        address,trap,marker=capture_trap('dmac-startup-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x1005B4,0xB74C))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)
