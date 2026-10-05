"""Negative completion evidence and capture instrumentation bounds."""
import unittest
from verify_sound_rpc import completed_card_query
from capture_iop_delay import hook as delay_hook
from capture_card_alarm import hook as alarm_hook
from capture_sio2_flow import count_hook,fifo_hook

class TimedEvidenceTests(unittest.TestCase):
    def test_incomplete_and_missing_bind_are_not_completion(self):
        profile={'mc_query_requests':1,'mc_dma_submissions':1,'mc_dma_polls':1,'timed_waits':5,'timed_wakes':5,'timed_dispatches':1,'logical_time_us':1900,'card_alarm_fires':4,'card_alarm_sets':5,'sio2_card_fifo_packets':5,'sio2_card_transfer_counts':[6,6],'pending_ee_bind':[0x2000E,0xA1CA0,0x80000704,0x1FF400,2]}
        self.assertTrue(completed_card_query(profile))
        for key in ('mc_dma_polls','timed_wakes','pending_ee_bind','card_alarm_fires'):
            bad=dict(profile);bad.pop(key);self.assertFalse(completed_card_query(bad))
    def test_observers_stay_in_owned_code_windows(self):
        self.assertLessEqual(len(count_hook(0x1000)),0x80)
        self.assertLessEqual(len(fifo_hook()),0x80)
        self.assertLessEqual(len(delay_hook(0x1000)),0x300)
        self.assertLessEqual(len(alarm_hook(0x1000)),0x300)

if __name__=='__main__':unittest.main()
