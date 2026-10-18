# 地上局（ESP32-C3 + TLM922S-P01A + PC）

気球から LoRa で届く GNSS データを受信し、PC のブラウザで地図・高度グラフ・生データをリアルタイムに表示します。

```text
TLM922S ──UART── ESP32-C3 ──USB (JSON Lines)── Node.js サーバー ──WebSocket── ブラウザ
                                                └ logs/*.jsonl に全行保存
```

| ディレクトリ | 内容 |
|---|---|
| `firmware/` | ESP32-C3 受信ファーム（PlatformIO） |
| `server/` | Node.js ローカルサーバーとブラウザ UI |

## ハードウェア

配線は [lora/README.md](../lora/README.md) と同じです（GPIO0 = RX、GPIO1 = TX、RST_M0 = 3V3、MD0 = GND）。  
無線パラメータは `firmware/platformio.ini` の `build_flags` で設定し、気球側 [pi/balloon.py](../pi/balloon.py) と必ず一致させます。

## ファームウェア

```bash
cd ground/firmware
pio run -t upload
pio device monitor   # 動作確認用。サーバー起動前に必ず閉じる
```

USB シリアルには 1 行 1 JSON で出力します。

```text
{"type":"log","msg":"P2P configure OK"}
{"type":"rx","hex":"3230323631303039...","rssi":-87,"snr":7}
{"type":"rx_fail","reason":"radio_err_timeout"}
```

`rx_fail` の `radio_err_timeout` は 10 秒の受信窓で何も届かなかったことを示し、待機中は定期的に出ます。  
P2P 設定中の `CMD: ... OK` 行は TLM922S ドライバが直接出すため JSON ではありませんが、サーバーはこれを `log` として扱います。

## サーバー

Node.js 18.3 以上が必要です（`serialport` は Node 18 で動く v12 系に固定）。

```bash
cd ground/server
npm install
npm run ports              # COM ポート一覧
npm start -- --port COM5   # ポートは環境に合わせる
```

ブラウザで <http://127.0.0.1:8080/> を開きます。

| オプション | 環境変数 | 既定値 | 内容 |
|---|---|---|---|
| `--port` | `SERIAL_PORT` | なし（必須） | ESP32 の COM ポート |
| `--http-port` | `HTTP_PORT` | `8080` | HTTP / WebSocket ポート |
| `--host` | `HOST` | `127.0.0.1` | 待ち受けアドレス |

- COM ポートは同時に 1 プロセスしか開けません。`pio device monitor` を閉じてから起動します。
- ESP32 を抜き差ししても 3 秒おきに自動で再接続します。
- 受信した全行を PC 受信時刻 `receivedAt` 付きで `server/logs/YYYYMMDD-HHMMSS.jsonl` に保存します（起動ごとに新規ファイル）。
- 同じ LAN のスマホなどから見る場合は `--host 0.0.0.0` で起動します。認証は無いので、信頼できるネットワークでのみ使ってください。

## 画面

- **地図**: 航跡（赤線）と最新位置。地理院タイルの標準地図・淡色地図・写真を右上で切り替え。「追従」をオンにすると最新位置へ自動で移動します。
- **状態**: サーバー / シリアル接続、最終受信からの経過秒（15 秒で黄、60 秒で赤）、受信数 / 失敗数 / パース失敗数
- **最新値**: 時刻（UTC と現地時刻）、緯度、経度、高度、上昇速度、RSSI / SNR。「座標コピー」で `緯度,経度` をクリップボードへ
- **高度**: GNSS 時刻に対する高度の推移
- **生データ**: 直近 200 行。`rx`（青）、`rx_fail`（灰）、`parse_error`（赤）、`log`

ブラウザを再読み込みしても、サーバーが保持している履歴から復元されます。サーバーを再起動すると画面上の履歴は消えますが、ログファイルは残ります。

地図タイルはインターネットから取得します。オフライン運用は未対応です（タイル URL は `server/public/app.js` 先頭の `TILE_LAYERS` にまとめてあります）。

## ペイロード

気球側は次の 1 行を送ります（[pi/README.md](../pi/README.md)）。

```text
20261009T085005Z,35.681236,139.767125,1234.5
```

UTC 時刻、緯度、経度、平均海面高度 [m] です。形式が合わない、または緯度経度が範囲外の場合は `parse_error` として生データにのみ表示します。

## ディレクトリ構成

```text
ground/
├── firmware/
│   ├── platformio.ini
│   ├── include/            # pins.h, tlm922s.h（lora/ からコピー）
│   └── src/                # main.cpp（JSON Lines 出力）, tlm922s.cpp
└── server/
    ├── package.json
    ├── src/
    │   ├── index.js        # HTTP / WebSocket、起動オプション
    │   ├── serial.js       # シリアル接続・再接続・行パース
    │   ├── telemetry.js    # ペイロード解析、履歴、上昇速度
    │   └── logger.js       # JSONL ログ保存
    ├── public/             # index.html, app.js, style.css
    └── logs/               # 受信ログ（git 管理外）
```
