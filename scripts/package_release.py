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

ZMK 0.4開発版の全構成プレリリースです。安定版mainを置き換えるものではありません。

## ダウンロード

Assetsの **{archive}** が全構成の一括ZIPです。UF2単体も選べます。
`MKB_L_`は左、`MKB_R_`は右です。末尾のモジュール名を確認してください。
**TBv3/TBv4および左右を取り違えないでください。**
「Source code」は書き込み用ファームウェアではありません。

## 書き込み

設定を控え、以前のファームを確保してから、片側ずつUSB接続してください。
リセットを素早く2回押し、現れたUF2ドライブへ該当UF2をコピーします。
自動再起動を待ってもう片側も更新します。DYA Studioは左のStudio用ポートへ接続します。
**settings_resetは保存設定・ペアリング消去用です。通常更新には使用しないでください。**

## 検証範囲

全構成をCIビルドし、成果物の個数・対象・ソースcommit・SHA256を検証しました。
2026-10-04のローカルビルドでユーザーが左右TB（右TBv4）の動作、マクロ割り当て、
電源再投入後のコンボ動作を確認しました。CI配布バイナリ自体の実機検証、他構成、
Shift付きキー割り当てとキーを押したままの起動は未確認です。
左LPPSにはStudio用snippetが含まれず、今回のStudio機能追加の対象外です。
修正はRuntime Macroの公開と設定読込後のコンボキャッシュ更新です。
元のコンボ不具合の根本原因は未確定です。

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
