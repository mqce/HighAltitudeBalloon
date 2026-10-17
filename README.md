# HighAltitudeBalloon

成層圏気球向けのLoRa送受信プログラム群です。

| ディレクトリ | 内容 |
|---|---|
| [gps](gps/README.md) | MAX-M10S から緯度経度を取得 ESP32|
| [lora](lora/README.md) | TLM922S-P01A の LoRa P2P 通信 ESP32|
| [pi](pi/README.md) | Raspberry Pi Zero W 1.1 で GNSS を LoRa 送信 |

# 地上側
ESP32に繋いだLoRaで受信する。
ESP32はUSBでPCに繋いでログを見る。

## ディレクトリ
/lora/ground

## コマンド
pio run -e ground -t upload
pio device monitor -e ground

# 気球側
RaspberryPiをベースに、GPSとLoRaを搭載。
GPSで取得した時刻、座標、高度をLoRaで送信しつづける。
