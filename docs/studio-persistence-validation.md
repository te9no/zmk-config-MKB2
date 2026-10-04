# Studioのマクロ割り当て・再起動後コンボの検証

対象: zmk-0.4のMKB Central（mkb-dya-studio-v2 snippetを使う構成）。

## 修正

- runtime_macro.dtsiをCentralへ追加し、StudioからRuntime Macroのスロットを割り当て可能にする。
- Custom Settingsの読み込み完了イベントでコンボの動作用キャッシュを再構築する。
  初期キーイベントで読み込み前の内容がキャッシュされた場合も再同期する。
- 保存形式、保存済み設定、通常のキー割り当ては変更しない。
- Shift付きキーは既存のHID修飾ビット対応を維持する。UIの症状は未再現のため変更しない。

## ビルド・実機確認結果

2026-10-04: 左右TB構成をUSB経由で書き込み。ユーザーが左右TBの基本動作、
Studioでのマクロ割り当て・実行、電源再投入後にStudioで再保存せずコンボが動作することを確認。
これはユーザーによる実機確認結果であり、以下の全試験を実施したという意味ではない。
Shift付きキー、キーを押したままの起動、設定読み込み時のCDCログは未確認。

- 左: `MKB_L_MODULE_TB.uf2` SHA256 `8b36e28eb892fc117b362a064abac511f3961e5463b252c004a2f6cd85f0abbc`
- 右: `MKB_R_MODULE_TBv4.uf2` SHA256 `523b0970bc318ba310ef3b4da38bfb8a698f32703870b5b930a0c2284a20023e`
- 右ビルド: `just.sh --profile mkb-studio-persistence build-fast MKB_R_MODULE_TBv4` 成功。
- 左のCDC復帰とCOM3のオープンを確認。採取期間のログ出力はなく、起動ログの検証は未完了。

2026-10-04: ホスト回帰テスト3件成功。`just.sh --profile mkb-studio-persistence
build-fast MKB_L_MODULE --pristine=always`で左7構成が成功。
ログ: `.zmk-workspace/profiles/mkb-studio-persistence/logs/build-parallel-20261004-005734/`。
JOYの生成DTSに`rmacro`、リンクマップに設定読込完了イベントの購読と補正関数を確認。
回帰テストは実コードのコールバックに保存状態・イベントのテスト用代替を接続したものです。
実ファームの電源断・保存復元を再現した試験ではありません。

## 検証手順（未確認の追加ケースを含む）

1. StudioでShift+1などを割り当て、保存、再接続して設定と入力を確認する。
   出力される記号はOS配列・Layout Shift設定にも依存する。
2. マクロを作成・保存し、Runtime Macroの該当スロットをキーへ割り当てて実行する。
3. コンボを保存し、通常起動とキーを押したままの起動を各々試す。
   電池とUSBを両方外す電源断も含める。
4. 起動後にStudioで再保存せずコンボを試す。CDCの
   `Runtime combo cache refreshed after settings load`を確認する。
5. 動かなければ新規Studioセッションから読み直し、保存データと実行キャッシュを区別する。

電源再投入後の動作は確認済みだが、コンボの起動順問題が元の症状の原因だったかは未確定。
上流リポジトリへの変更・PR作成は行わない。
