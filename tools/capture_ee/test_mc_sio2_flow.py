"""Observers must retain real DMA execution and bounded scratch ownership."""
import struct,unittest
from capture_sio2_flow import hook,send_hook,control_hook
from verify_sound_rpc import later_mc_dma_boundary

def words(b):return struct.unpack('<'+str(len(b)//4)+'I',b)
class CardFlowTests(unittest.TestCase):
    def test_start_observer_tail_calls_actual_api(self):
        target=0x75B4;w=words(send_hook(target))
        self.assertEqual(w[-2:],(0x08000000|(target>>2),0))
        for branch in (1,7):
            self.assertEqual(branch+1+(w[branch]&0xFFFF),len(w)-2)
        self.assertIn(0x2F230040,w) # Bound trace writes to 64 owned records.
    def test_control_observer_stores_device_register_and_bounds_trace(self):
        w=words(control_hook());self.assertEqual(w[:3],(0x3C02BF80,0x34428268,0xAC440000))
        self.assertEqual(7+1+(w[7]&0xFFFF),len(w)-2)
        self.assertIn(0x2F230020,w) # No overlap with the send observer region.
        self.assertEqual(words(hook(0))[-2:],(0x1000FFFF,0))
    def test_later_boundary_requires_completed_transfer_pair_count(self):
        r={'dbc_rpc_binds':4,'dbc_rpc_calls':4,'mc_rpc_binds':1,'mc_rpc_calls':1,'mc_query_requests':0,
           'native_iop_error':'Unresolved IOP import: thbase:33','native_iop_pc':0x31B40,
           'selector':0x77,'pc':0x32F028,'packet_words':[0]*6+[41,0x3A6F80,1,48],'sio2_card_transfer_counts':[6,6],'native_iop_import_arguments':[300,1,1,0]}
        self.assertTrue(later_mc_dma_boundary(r));r['sio2_card_transfer_counts']=[6,5];self.assertFalse(later_mc_dma_boundary(r))
if __name__=='__main__':unittest.main()
