import pathlib
import unittest
import yaml

ROOT = pathlib.Path(__file__).resolve().parents[1]


class RecoveryIntegrationTest(unittest.TestCase):
    def test_ci_covers_every_target_once(self):
        targets = yaml.safe_load((ROOT / "build.yaml").read_text())["include"]
        names = [t.get("artifact-name", t["shield"]) for t in targets]
        workflow = yaml.safe_load((ROOT / ".github/workflows/bt-recovery.yml").read_text())
        matrix = workflow["jobs"]["build"]["strategy"]["matrix"]["target"]
        self.assertCountEqual(names, matrix)
        self.assertEqual(len(matrix), len(set(matrix)))
        self.assertEqual(len(names), 16)

    def test_sleep_settings_preserved(self):
        for side in ("L", "R"):
            conf = (ROOT / f"boards/shields/MKB/MKB_{side}_Base.conf").read_text()
            self.assertIn("CONFIG_ZMK_SLEEP=y", conf)
            self.assertIn("CONFIG_ZMK_IDLE_TIMEOUT=30000", conf)
            self.assertIn("CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=1800000", conf)


if __name__ == "__main__":
    unittest.main()
