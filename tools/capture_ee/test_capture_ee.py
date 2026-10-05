import struct
import unittest
from capture_ee import export_rva, instrument, TRAP, HANDLER, MARKER, capture_trap
from capture_ee import vif_startup_stub,FRESH_BIND_STUB,EXPECTED_SHA
from capture_ee import graph_csr_stub,graph_startup_stub,texture_stub,TEXTURE_POINTS
from unittest.mock import patch


class CaptureTests(unittest.TestCase):
    def test_second_slot_capture_preserves_first_call(self):
        blob=texture_stub('rpc-dbc-port1-before');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        early=4+(w[3]&65535)
        self.assertEqual(w[3]&0xFFFF0000,0x13200000)
        self.assertEqual(w[early:],(0x3C1F0031,0x37FF3650,0x08000000|(0x332970>>2),0x0000582D))
        self.assertEqual((w[15]&0x3FFFFFF)<<2,FRESH_BIND_STUB+60)

    def test_later_card_dma_boundary_does_not_hide_earlier_failure(self):
        from verify_sound_rpc import later_mc_dma_boundary
        r={'dbc_rpc_binds':4,'dbc_rpc_calls':4,'mc_rpc_binds':1,'mc_rpc_calls':1,'mc_query_requests':0,'native_iop_error':'Unresolved IOP import: dmacman:32','native_iop_pc':0x10E30,'selector':0x77,'pc':0x32F028,'packet_words':[0]*6+[41,0x3A6F80,1,48]}
        self.assertTrue(later_mc_dma_boundary(r))
        for key,value in [('mc_rpc_calls',0),('mc_query_requests',1),('native_iop_pc',0x10E28),('native_iop_error','Unresolved IOP import: mcman:53'),('packet_words',[])]:
            self.assertFalse(later_mc_dma_boundary(dict(r,**{key:value})))

    def test_card_slice_hook_calls_original_before_reading_programmed_registers(self):
        from capture_sio2_dma import hook
        w=struct.unpack('<'+str(len(hook(0x12340))//4)+'I',hook(0x12340))
        self.assertEqual(w[5:8],(0x8FB90010,0,0xAF190014))
        self.assertEqual(w[11:15],(0x0C000000|(0x12340>>2),0,0x3C18001D,0xAF020018))
        self.assertEqual((w[16],w[20],w[24]),(0x8F391540,0x8F391544,0x8F391548))
        self.assertEqual(w[-2:],(0x1000FFFF,0))

    def test_reverse_dma_hooks_preserve_actual_calls_and_negative_poll(self):
        from capture_dbc_dma import submit_hook,poll_hook,SUBMIT,POLL
        target=0x12340
        w=struct.unpack('<'+str(len(submit_hook(target))//4)+'I',submit_hook(target))
        self.assertEqual(w[-8],0x0C000000|(target>>2))
        self.assertEqual(w[-11:-8],(0x3C19001D,0x37390040,0xAC990000))
        p=struct.unpack('<'+str(len(poll_hook(target))//4)+'I',poll_hook(target))
        self.assertEqual(p[3],0x0C000000|(target>>2))
        self.assertEqual(12+(p[11]&65535),17)
        self.assertEqual(p[15],0x03E00008)

    def test_later_dma_boundary_does_not_hide_earlier_service_failures(self):
        from verify_sound_rpc import later_dbc_dma_boundary
        r={'dbc_rpc_binds':4,'dbc_rpc_calls':1,'native_iop_error':'Unresolved IOP import: sifman:7','native_iop_pc':0x621DC,'selector':0x77,'pc':0x32F028,'packet_words':[0]*6+[36,0x3A03C0,0x80001304,144]}
        self.assertTrue(later_dbc_dma_boundary(r))
        for key,value in [('dbc_rpc_calls',0),('native_iop_error','Unresolved IOP import: libsd:4'),('native_iop_pc',0x621D8),('packet_words',[])]:
            self.assertFalse(later_dbc_dma_boundary(dict(r,**{key:value})))

    def test_dbc_hooks_publish_real_returns_without_unrelated_mmio(self):
        for point in ('rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after'):
            address,trap,marker=capture_trap(point)
            self.assertEqual(address,TEXTURE_POINTS[point][0])
            self.assertEqual(marker,TEXTURE_POINTS[point][1])
            stub=texture_stub(point);w=struct.unpack('<'+str(len(stub)//4)+'I',stub)
            self.assertEqual(w[2:8],(0xFF020008,0xAF1D0010,0xAF040018,0xAF05001C,0xAF060020,0xAF070024))
            self.assertEqual((w[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+40)

    def test_sound_console_stubs_publish_actual_arguments(self):
        for point in ('sound-bind-after','sound-init-before','sound-init-after','deci2-after'):
            blob=texture_stub(point);words=struct.unpack('<'+str(len(blob)//4)+'I',blob)
            self.assertEqual(words[2:8],(0xFF020008,0xAF1D0010,0xAF040018,0xAF05001C,0xAF060020,0xAF070024))
            self.assertEqual((words[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+40)
        for point in ('sound-ee-thread-before','sound-ee-thread-after'):
            blob=texture_stub(point);words=struct.unpack('<'+str(len(blob)//4)+'I',blob)
            self.assertEqual(words[8:10],(0x40196000,0xAF190028))
            self.assertEqual((words[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+48)


    def test_sound_status_hook_preserves_earlier_thread_start(self):
        blob=texture_stub('sound-ee-status-after');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        self.assertEqual(w[0],0x24180003)
        early=2+(w[1]&65535)
        self.assertEqual(w[1]&0xFFFF0000,0x16380000)
        self.assertEqual(w[early:early+3],(0x0040802D,0x06000003,0x8FA30000))
        self.assertEqual((w[early+3]&0x3FFFFFF)<<2,0x32FDFC)
        self.assertEqual((w[early+5]&0x3FFFFFF)<<2,0x32FDEC)

    def test_texture_completion_skips_busy_reads(self):
        words=struct.unpack('<'+str(len(texture_stub('texture-complete'))//4)+'I',texture_stub('texture-complete'))
        self.assertEqual(words[0],0x30580100)
        early=2+(words[1]&65535)
        self.assertEqual(words[early],0x30420100)
        self.assertEqual((words[early+1]&0x3FFFFFF)<<2,0x28B9A0)
        for point,(address,marker) in TEXTURE_POINTS.items():
            self.assertEqual(capture_trap(point)[0],address)
            self.assertEqual(capture_trap(point)[2],marker)

    def test_second_csr_hook_preserves_the_first_call(self):
        words=struct.unpack('<'+str(len(graph_csr_stub())//4)+'I',graph_csr_stub())
        self.assertEqual(words[:2],(0x3C1801FF,0x3718ECF0))
        early=3+(words[2]&65535)
        self.assertEqual(words[early],0xA6120002)
        self.assertEqual((words[early+1]&0x3FFFFFF)<<2,0x30FE58)
        self.assertEqual(capture_trap('graph-csr-after')[0],0x30FE54)
    def test_vif_hook_has_a_file_backed_stub_segment(self):
        data=bytearray(116+4096);struct.pack_into('<I',data,28,52);struct.pack_into('<HH',data,42,32,2)
        struct.pack_into('<8I',data,52,1,116,0x100000,0x100000,4096,4096,5,4096)
        struct.pack_into('<8I',data,84,1,0,0,0,0,0,5,4096)
        metadata={'sha256':EXPECTED_SHA,'load_segments':[{'virtual_address':0x100000,'memory_size':4096,'file_size':4096,'flags':5,'file_offset':116}]}
        with patch('capture_ee.inspect',return_value=metadata):prepared,offset=instrument(bytes(data),'vif-startup-after')
        program=struct.unpack_from('<8I',prepared,84);stub=vif_startup_stub()
        self.assertEqual(program[2:6],(FRESH_BIND_STUB,FRESH_BIND_STUB,len(stub),len(stub)))
        self.assertEqual(prepared[program[1]:program[1]+program[4]],stub)
        self.assertEqual(prepared[offset:offset+8],capture_trap('vif-startup-after')[1])
        words=struct.unpack('<'+str(len(stub)//4)+'I',stub)
        self.assertIn(0x4842E000,words);self.assertEqual((words[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+(len(words)-2)*4)
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
