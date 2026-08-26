import collections
import json
import pathlib
import re
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[1]
VIAL_DEFINITION = REPOSITORY_ROOT / "config" / "vial.json"
MATRIX_TRANSFORM = REPOSITORY_ROOT / "boards" / "shields" / "MKB" / "MKB.dtsi"


class MkbVialDefinitionTest(unittest.TestCase):
    def setUp(self):
        self.definition = json.loads(VIAL_DEFINITION.read_text(encoding="utf-8"))

    def test_identity_matches_usb_configuration(self):
        self.assertEqual(self.definition["name"], "MeKaBu")
        self.assertEqual(self.definition["vendorId"], "0x1D50")
        self.assertEqual(self.definition["productId"], "0x615E")

    def test_layout_covers_every_matrix_transform_position_once(self):
        transform_source = MATRIX_TRANSFORM.read_text(encoding="utf-8")
        transform_positions = [
            f"{int(row)},{int(col)}"
            for row, col in re.findall(r"RC\(\s*(\d+)\s*,\s*(\d+)\s*\)", transform_source)
        ]

        layout_positions = [
            item
            for row in self.definition["layouts"]["keymap"]
            for item in row
            if isinstance(item, str)
        ]

        self.assertEqual(len(transform_positions), 58)
        self.assertEqual(collections.Counter(layout_positions), collections.Counter(transform_positions))
        self.assertEqual(len(layout_positions), len(set(layout_positions)))

    def test_positions_fit_declared_matrix(self):
        rows = self.definition["matrix"]["rows"]
        cols = self.definition["matrix"]["cols"]
        self.assertEqual((rows, cols), (7, 12))

        for layout_row in self.definition["layouts"]["keymap"]:
            for item in layout_row:
                if not isinstance(item, str):
                    continue
                row, col = (int(value) for value in item.split(","))
                self.assertIn(row, range(rows))
                self.assertIn(col, range(cols))

    def test_zmk_custom_keycodes_match_firmware_user_range(self):
        expected = [
            "ZMK_BT_CLR",
            "ZMK_BT_SEL_0",
            "ZMK_BT_SEL_1",
            "ZMK_BT_SEL_2",
            "ZMK_BT_SEL_3",
            "ZMK_BT_SEL_4",
            "ZMK_BT_CLR_ALL",
            "ZMK_BT_NXT",
            "ZMK_BT_PRV",
            "ZMK_STUDIO_UNLOCK",
            "MKB_LAYOUT_SHIFT",
            "ZMK_CAPS_WORD",
            "ZMK_KEY_REPEAT",
            "ZMK_OUT_TOG",
            "ZMK_OUT_USB",
            "ZMK_OUT_BLE",
            "ZMK_SYS_RESET",
        ]
        self.assertEqual([item["name"] for item in self.definition["customKeycodes"]], expected)


if __name__ == "__main__":
    unittest.main()
