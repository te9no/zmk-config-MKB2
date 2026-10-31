# MeKaBu Bluetooth recovery

専用ブランチ: `feat/bt-recovery`。Anantaで使用した復旧機構を、
MeKaBuの左central・右peripheral構成へ適用しています。

## 対象と動作

- ZMK: `e5c9b6915b56801193e359dd9bad4a167ce0d1b8`
- 復旧モジュール: `7e76381d90856a42e739cebf66752afebbe78748`
- スキャン／広告開始失敗に対し、1・2・4・8・16秒の制限付き再試行。
- PCとの切断と子機との切断を区別し、通常の再接続はZMKに任せます。
- ボンディング削除、定期再起動、Bluetoothスタックの再初期化は行いません。
- 左右とも既存の30秒アイドル・30分DeepSleep設定を維持します。
  Sleep時には復旧処理を止め、復帰後に猶予を設けます。
- 接続が成立したまま通信だけ停止する問題やMCU停止は検出対象外です。

## ビルド

`Bluetooth recovery`の専用Actionsがbuild.yamlの全16ターゲットをビルドします。
左右15構成では`bt-recovery` snippetを追加し、settings_resetでは有効にしません。
専用ブランチでは通常ビルドを停止し、復旧処理なしの成果物との混同を防ぎます。
各UF2とJSONにソース・依存コミットとSHA256を記録します。

ローカルでは、config/west.ymlを別のwestワークスペースのmanifest/west.ymlへ
コピーしてwest init/update後、次を実行します。

```sh
python3 /path/to/mekabu-bt-recovery/scripts/build-bt-recovery.py MKB_L_MODULE_TB
python3 /path/to/mekabu-bt-recovery/scripts/build-bt-recovery.py MKB_R_MODULE_TBv4
```

ブリッジパッチは指定ZMKコミットのみへ明示的に適用します。
別のZMKへ無条件で適用しないでください。通常のwest buildだけでは
ブリッジ適用とsnippet追加を代行しません。

## 検証範囲

CIで復旧モジュールの状態機械と実際のZMK所有関数を使ったモック試験を実行します。
生成設定の役割、復旧有効／無効、UF2のヘッダとソースがcleanであることも検証します。

MeKaBu実機の長時間放置、USB／電池駆動、DeepSleepからの復帰、
親機とPC／子機の再接続、電池消費は未確認です。
Anantaについてのユーザー報告は改善の参考情報であり、
MeKaBuの改善や原因の一致を証明するものではありません。
公開リリースや実機への書き込みはこの変更では行いません。
