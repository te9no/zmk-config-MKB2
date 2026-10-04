import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

spec = importlib.util.spec_from_file_location('release', Path(__file__).resolve().parents[1] / 'scripts/package_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def run_package(self, fault=None):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source = root / 'input'
            source.mkdir()
            target = {'board': 'xiao_ble//zmk', 'shield': 'test', 'artifact-name': 'test'}
            data = b'firmware fixture'
            (source / 'test.uf2').write_bytes(data)
            meta = {'artifact': {'name': 'test.uf2', 'sha256': hashlib.sha256(data).hexdigest()},
                    'inputs': {'source': {'commit': 'abc', 'dirty': False}}, 'target': target}
            if fault == 'hash':
                meta['artifact']['sha256'] = 'wrong'
            if fault == 'commit':
                meta['inputs']['source']['commit'] = 'old'
            if fault == 'dirty':
                meta['inputs']['source']['dirty'] = True
            (source / 'test.uf2.json').write_text(json.dumps(meta))
            matrix = {'include': [target, target] if fault == 'missing' else [target]}
            release.package(source, root / 'out', matrix, 'abc', 'zmk-0.4-test')
            with zipfile.ZipFile(root / 'out/MeKaBu-zmk-0.4-test-all.zip') as archive:
                self.assertIsNone(archive.testzip())
                self.assertEqual(set(archive.namelist()), {'test.uf2', 'test.uf2.json', 'README.md', 'SHA256SUMS'})

    def test_complete_archive(self):
        self.run_package()

    def test_reject_invalid_inputs(self):
        for fault in ('hash', 'commit', 'dirty', 'missing'):
            with self.subTest(fault=fault), self.assertRaises(AssertionError):
                self.run_package(fault)
