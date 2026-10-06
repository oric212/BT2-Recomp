"""Reject partial or mixed queued/diagnostic sound completion evidence."""
import unittest
from verify_sound_rpc import queued_sound_bind_completed,completed_card_query

class QueuedSoundEvidenceTests(unittest.TestCase):
    def profile(self):
        return dict(queued_sound_submissions=1,queued_sound_completions=1,sound_thread_waiting=1,
            sound_dispatch_bind_id=29,sound_dispatch_wait_ra=0x332874,sound_dispatch_wait_sp=0x1FFECB0,
            ee_sound_probe_attempted=0,ee_sound_probe_waits=0,ee_sound_probe_parent_pc=0,
            ee_sound_bind_returns=1,pending_ee_bind=[0]*5,native_iop_error='',
            ee_sound_context=[3,0,0x32EB88,0x38BDF0,0x3A6C80,0x333098,0,0x10001],
            ee_sound_probe_frontier=[0x32EB88,0x38BDF0,0x3A6C80,0x333098,0x3A6D00,0x10001],
            sound_thread_words=[0]*7+[1,0],sound_iop_context=[0xA1304,0xA0F24,0xA94E0,0x1E7588,0])
    def test_complete_owned_profile(self):
        self.assertTrue(queued_sound_bind_completed(self.profile()))
    def test_each_required_field_is_required(self):
        for key in self.profile():
            with self.subTest(key=key):
                r=self.profile();del r[key];self.assertFalse(queued_sound_bind_completed(r))
    def test_partial_duplicate_and_diagnostic_states_rejected(self):
        for key,value in [('queued_sound_submissions',2),('queued_sound_completions',0),
                ('sound_dispatch_bind_id',30),('sound_dispatch_wait_ra',0x32EAF8),
                ('sound_dispatch_wait_sp',0x1FFEFB0),('ee_sound_probe_attempted',1),
                ('ee_sound_probe_waits',1),('ee_sound_bind_returns',0),
                ('pending_ee_bind',[0x2000E,0xA1CA0,0x80000704,0x1FF400,2]),
                ('native_iop_error','failure'),('sound_thread_words',[0]*7+[0,1])]:
            with self.subTest(key=key):
                r=self.profile();r[key]=value;self.assertFalse(queued_sound_bind_completed(r))
    def test_child_cpu_and_frontier_each_word(self):
        for key in ['ee_sound_context','ee_sound_probe_frontier','sound_iop_context']:
            for index in range(len(self.profile()[key])):
                r=self.profile();r[key][index]^=1
                with self.subTest(key=key,index=index):self.assertFalse(queued_sound_bind_completed(r))
    def test_missing_pending_bind_alone_does_not_complete_card_query(self):
        r=dict(mc_query_requests=1,mc_dma_submissions=1,mc_dma_polls=1,timed_waits=5,
            timed_wakes=5,timed_dispatches=1,logical_time_us=1900,card_alarm_fires=4,
            card_alarm_sets=5,sio2_card_fifo_packets=5,sio2_card_transfer_counts=[6,6],pending_ee_bind=[0]*5)
        self.assertFalse(completed_card_query(r));r.update(self.profile());self.assertTrue(completed_card_query(r))
        r['queued_sound_completions']=0;self.assertFalse(completed_card_query(r))

if __name__=='__main__':unittest.main()
