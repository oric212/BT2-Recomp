import struct,unittest
from capture_cri_thread import start_hook,wait_hook,dma_hook,adma_hook
class CriThreadHookTests(unittest.TestCase):
    def test_original_start_is_called_once_and_parent_can_resume(self):
        for terminal in (True,False):
            w=struct.unpack('<16I',start_hook(0x12340,terminal))
            self.assertEqual(sum(x>>26==3 for x in w),1)
            self.assertEqual(w[6],0x0C000000|(0x12340>>2))
            self.assertEqual(w[-4:],(0x24190001,0xAF190000,0x1000FFFF,0) if terminal else (0x8F1F000C,0,0x03E00008,0))
    def test_wait_keeps_original_kernel_target_and_delay(self):
        w=struct.unpack('<7I',wait_hook(0x12340));self.assertEqual(w[-2:],(0x08000000|(0x12340>>2),0x24000008))
    def test_dma_observer_reads_original_control_once(self):
        w=struct.unpack('<14I',dma_hook());self.assertEqual(w[1:4],(0x8E620000,0,0xAF020034))
        self.assertEqual(w.count(0x8E620000),1)
    def test_adma_terminal_only_records_requested_value(self):
        w=struct.unpack('<12I',adma_hook());self.assertEqual(w[1:3],(0xAF020034,0xAF100038))
        self.assertNotIn(0xA6020000,w)
