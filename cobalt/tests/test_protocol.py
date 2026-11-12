import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from protocol import cc, effect, extended, fixed_route_field, source_depth_field, modulation


class ProtocolTests(unittest.TestCase):
    def test_reference_effect_transactions(self):
        # Independently transcribed from the supplied binary's param_midi branches.
        self.assertEqual(effect(1, 0, 25), bytes.fromhex("B0 63 22 B0 62 01 B0 26 19 B0 06 00"))
        self.assertEqual(effect(3, 6, 127, 16), bytes.fromhex("BF 63 22 BF 62 03 BF 26 7F BF 06 06"))

    def test_documented_matrix_boundary_slots(self):
        self.assertEqual(modulation(1, 2, 3, 65), bytes.fromhex("B0 58 40 B0 64 02 B0 6C 03 B0 58 41"))
        self.assertEqual(modulation(8, 0, 127, 0, 2), bytes.fromhex("B1 5F 40 B1 6B 00 B1 73 7F B1 5F 00"))

    def test_rejects_invalid_data_instead_of_wrapping(self):
        for args in ((34,1,0,-1,1), (34,1,0,128,1), (34,1,0,1,0), (34,1,0,1,17), (128,1,0,1,1)):
            with self.assertRaises(ValueError): extended(*args)
        for args in ((0,0,1), (4,0,1), (1,7,1)):
            with self.assertRaises(ValueError): effect(*args)

    def test_all_supported_effect_fields_remain_complete_transactions(self):
        for slot in range(1,4):
            for field in range(7):
                for channel in range(1,17):
                    data=effect(slot,field,0,channel)
                    self.assertEqual(len(data),12)
                    self.assertEqual(list(data[1::3]),[99,98,38,6])
                    self.assertEqual(set(data[0::3]),{0xAF+channel})
                    self.assertTrue(all(x<128 for i,x in enumerate(data) if i%3))

    def test_fixed_route_group_reference(self):
        self.assertEqual(source_depth_field(7,64),bytes.fromhex("B0 63 20 B0 62 0C B0 26 40 B0 06 07"))
        self.assertEqual(fixed_route_field(3,64),bytes.fromhex("B0 63 20 B0 62 0D B0 26 40 B0 06 03"))


if __name__ == "__main__":
    unittest.main()
