# MeKaBu ZeroShift の保存・再起動試験

Renode 1.17.0 で nRF52840 を2台起動し、Web UI と同じ Studio クライアントから左側に接続します。各側でオフを含む9通りを順に保存し、保存直後の起動値、反対側の設定、対象側だけをリセットした後の保存値と起動値を検査します。最後に左をエンコーダ、右を1uキーに設定し、左右を順に再起動して設定の独立性を検査します。

実機用と同じ ZeroShift、設定ストレージ、モジュール初期化、左右転送処理を使います。Renode ではホストと左右間の接続に仮想 UART を使い、USB、BLE、表示を無効にします。SAADC は固定値、RZT と Pinnacle は初期化に必要な I²C 応答を模擬します。PMW3610 と ADS1220 について確認するのはファームウェアが返す起動状態までです。センサー実測値、HID 入力、消費電力、無線接続は実機で確認してください。

以下はワークスペースのルートから実行します。Node.js 22 と Renode 1.17.0 を使用します。

```sh
ZMK_CONFIG_ROOT=work/mekabu-volume-id ZMK_BUILD_MATRIX=build-zeroshift-sim.yaml ZMK_CONFIG_NAME=mekabu ZMK_FIRMWARE_TEST_PURPOSE=renode-zeroshift-v1 CMAKE_BUILD_PARALLEL_LEVEL=4 ./just.sh --profile mekabu-zeroshift build-parallel MKB-ZeroShift-sim 2
(cd config/zmk-feature-zeroshift/studio-web && npm ci && npm run build)
export ZEROSHIFT_SHARED_RPC="$PWD/config/zmk-feature-zeroshift/studio-web/dist/rpc.mjs"
python3 -B work/mekabu-volume-id/tests/renode/test_studio.py --renode .zmk-workspace/tools/renode-1.17.0/renode --firmware-dir firmware/testing/mekabu/renode-zeroshift-v1 --log /tmp/mkb-renode.log
```
