# HighAltitudeBalloon

成層圏気球向けのLoRa送受信プログラム群です。

| ディレクトリ | 内容 |
|---|---|
| [gps](gps/README.md) | MAX-M10S から緯度経度を取得 ESP32|
| [lora](lora/README.md) | TLM922S-P01A の LoRa P2P 通信 ESP32|
| [pi](pi/README.md) | Raspberry Pi Zero W 1.1 で GNSS を LoRa 送信 |
| [ground](ground/README.md) | 地上局 ESP32 受信ファーム + Node.js サーバー / ブラウザ UI |

# 地上側
ESP32に繋いだLoRaで受信する。
ESP32はUSBでPCに繋ぎ、Node.jsのローカルサーバー経由でブラウザに地図・高度グラフ・生データを表示する。

## ディレクトリ
[/ground](ground/README.md)

## コマンド
```bash
cd ground/firmware
pio run -t upload

cd ../server
npm install
npm start -- --port COM5
```
ブラウザで http://127.0.0.1:8080/ を開く。

# 気球側
RaspberryPiをベースに、GPSとLoRaを搭載。
GPSで取得した時刻、座標、高度をLoRaで送信しつづける。
