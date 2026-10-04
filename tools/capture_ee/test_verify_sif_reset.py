import hashlib
import struct
import unittest
from unittest.mock import patch
import verify_sif_reset as verifier
from capture_ee import MARKER, capture_trap

class SifResetEvidenceTests(unittest.TestCase):
    def fixture(self,point):
        # Entirely authored RAM fixture; no kernel or executable bytes.
        ee=bytearray(32*1024*1024)
        marker=capture_trap(point)[2]
        struct.pack_into('<III',ee,MARKER,marker,0 if point=='sif-stop-after' else 0x19600,0x1FFEC80)
        struct.pack_into('<I',ee,0x14EAC,0x80006380)
        return ee

    def check(self,ee,point):
        with patch.object(verifier,'KERNEL_STOP_SHA',hashlib.sha256(bytes(44)).hexdigest()):
            return verifier.validate_publication(ee,point)

    def test_stop_and_query_publications(self):
        for point,result in (('sif-stop-after',0),('sif-reset-subaddr',0x19600)):
            self.assertEqual(self.check(self.fixture(point),point),struct.pack('<I',result))

    def test_altered_publication_table_or_kernel_rejected(self):
        for address in (MARKER,MARKER+4,MARKER+8,0x14EAC,0x6380):
            ee=self.fixture('sif-stop-after');ee[address]^=1
            with self.subTest(address=hex(address)),self.assertRaises(ValueError):self.check(ee,'sif-stop-after')

    def test_truncated_capture_rejected(self):
        with self.assertRaises(ValueError):self.check(self.fixture('sif-stop-after')[:-1],'sif-stop-after')


class SmflagEvidenceTests(unittest.TestCase):
    def fixture(self,point):
        ee=bytearray(32*1024*1024)
        marker=capture_trap(point)[2]
        struct.pack_into('<4I',ee,MARKER,marker,0 if point=='reset-smflag-before' else 0x30000,0x1FFEC80,0x30000)
        return ee

    def test_before_and_after_publications(self):
        for point in ('reset-smflag-before','reset-smflag-after'):
            verifier.validate_smflag_publication(self.fixture(point),point)

    def test_wrong_marker_result_stack_or_flag_rejected(self):
        for offset in (0,4,8,12):
            ee=self.fixture('reset-smflag-after');ee[MARKER+offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                verifier.validate_smflag_publication(ee,'reset-smflag-after')

    def test_truncated_capture_rejected(self):
        with self.assertRaises(ValueError):
            verifier.validate_smflag_publication(self.fixture('reset-smflag-before')[:-1],'reset-smflag-before')
