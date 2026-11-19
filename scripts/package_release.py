"""Validate CI provenance before packaging all firmware for publication."""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import zipfile


def package(source, output, matrix, commit, tag):
    targets = matrix['include']
    assert targets, 'Empty build matrix'
    binaries = sorted(source.rglob('*.uf2'))
    assert len(binaries) == len(targets), 'Missing or extra firmware'
    names = set()
    remaining = targets.copy()
    for binary in binaries:
        assert binary.name not in names, 'Duplicate artifact name'
        names.add(binary.name)
        meta = json.loads(binary.with_suffix('.uf2.json').read_text())
        assert meta['artifact']['name'] == binary.name
        assert hashlib.sha256(binary.read_bytes()).hexdigest() == meta['artifact']['sha256']
        assert meta['inputs']['source']['commit'] == commit, 'Wrong source commit'
        assert meta['inputs']['source']['dirty'] is False, 'Dirty source'
        keys = ('board', 'shield', 'snippet', 'artifact-name')
        matches = [t for t in remaining if all(t.get(k, '') == meta['target'].get(k, '') for k in keys)]
        assert len(matches) == 1, 'Unexpected or duplicate target'
        remaining.remove(matches[0])
    assert not remaining
    assert re.fullmatch(r'zmk-0\.4-[A-Za-z0-9][A-Za-z0-9._-]*', tag)
    output.mkdir(parents=True, exist_ok=False)
    checksums = []
    for binary in binaries:
        for path in (binary, binary.with_suffix('.uf2.json')):
            shutil.copy2(path, output / path.name)
            checksums.append(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}')
    archive = f'MeKaBu-{tag}-all.zip'
    notes = f'''# MeKaBu {tag}

ZeroShift 対応版の左右UF2を同梱しています。

左手には `MKB-ZeroShift-left.uf2`、右手には `MKB-ZeroShift-right.uf2` を書き込みます。
左側を PC に接続して[共通の設定画面](https://te9no.github.io/zmk-feature-zeroshift/)を開き、
対象側のモジュールを選んで保存し、その側を再起動してください。

CI でビルド対象とファイル数、ソースコミット、SHA-256 を検証しています。配布バイナリの実機動作は未確認です。

ソース: `{commit}`。ZIPには来歴JSONとSHA256SUMSを同梱しています。

## 同梱UF2

'''
    notes += '\n'.join(f'- `{name}`' for name in sorted(names)) + '\n'
    (output / 'README.md').write_text(notes)
    (output / 'SHA256SUMS').write_text('\n'.join(checksums) + '\n')
    with zipfile.ZipFile(output / archive, 'w', zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(output.iterdir()):
            if path.name != archive:
                bundle.write(path, path.name)
    print(f'Validated and packaged {len(binaries)} targets')


if __name__ == '__main__':
    package(Path(sys.argv[1]), Path(sys.argv[2]), json.loads(os.environ['MATRIX']),
            os.environ['GITHUB_SHA'], os.environ['TAG'])
