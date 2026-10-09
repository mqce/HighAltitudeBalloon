# Raspberry Pi Zero 気球送信

Raspberry Pi Zero で MAX-M10S の測位結果を TLM922S-P01A から LoRa 送信する。有効な 3D Fix かつ時刻が確定しているときだけ、5 秒おきに次の 1 行を送る。

```text
20261009T085005Z,35.681236,139.767125,1234.5
```

時刻は GNSS の UTC、緯度経度は度（小数 6 桁）、高度は平均海面高度のメートル（小数 1 桁）。未 Fix のときは送らず、状態だけをログする。

地上側は [lora](../lora/README.md) の `ground` のままでよい。受信テキストをそのまま表示する。周波数、SF、帯域幅、出力を気球側と揃える。

## ハードウェア

| 項目 | 内容 |
|------|------|
| 計算機 | Raspberry Pi Zero / Zero W / Zero 2 W |
| GNSS | スイッチサイエンス MAX-M10S（SSCI-10437） |
| LoRa | TLM922S-P01A |
| GNSS 接続 | I2C 100 kHz、アドレス 0x42 |
| LoRa 接続 | UART 115200 8N1 |

外付けアンテナが無いと Fix しない。GNSS と LoRa の電源はどちらも 3.3V（5V 不可）。

### 配線

| Raspberry Pi | MAX-M10S |
|--------------|----------|
| GPIO2 SDA（物理ピン 3） | SDA |
| GPIO3 SCL（物理ピン 5） | SCL |
| 3.3V | 3V3 |
| GND | GND |

| Raspberry Pi | TLM922S-P01A |
|--------------|--------------|
| 3.3V | VDD |
| GND | GND |
| GND | MD0 (Pin1) |
| GPIO14 TXD（物理ピン 8） | UART_RX (Pin21) |
| GPIO15 RXD（物理ピン 10） | UART_TX (Pin22) |

MD0 は GND（LOW）のままにする。

### 無線パラメータ

[lora/include/pins.h](../lora/include/pins.h) の初期値と同じ。

| 項目 | 値 |
|------|-----|
| 周波数 | 922500000 Hz |
| SF | 7 |
| 帯域幅 | 125 kHz |
| 出力 | 14 dBm |
| sync | 12 |
| CRC | on |
| 送信間隔 | 5 秒 |

変更するときは [balloon.py](balloon.py) 先頭の定数を編集し、地上側も同じ値にする。

GNSS は RAM だけに設定する。I2C 出力は UBX のみ、dynamic model は airborne &lt;1g、測位周期は 1 Hz。電源を切るとモジュール側の設定は戻る。

## Raspberry Pi OS の準備

I2C を有効にし、シリアルはハードウェアだけ有効にしてログインシェルは無効にする。

```bash
sudo raspi-config
```

- Interface Options → I2C → Enable
- Interface Options → Serial Port → ログインシェルは No、シリアルハードウェアは Yes

Zero W と Zero 2 W は、Bluetooth が使う UART と GPIO14/15 が入れ替わる。115200 bps を安定させるため、PL011 を GPIO に出す。Bookworm では `/boot/firmware/config.txt`、それ以前は `/boot/config.txt` に次を追加する。

```text
dtoverlay=disable-bt
```

再起動後、`/dev/serial0` が GPIO14/15 の UART、`/dev/i2c-1` が I2C になる。`/boot/firmware/cmdline.txt` に `console=serial0` が残っていないことを確認する。

## インストールと実行

リポジトリを `/opt/balloon` に置く場合。

```bash
sudo mkdir -p /opt/balloon
sudo cp -a /path/to/balloon/pi /opt/balloon/pi
sudo python3 -m venv /opt/balloon/venv
sudo /opt/balloon/venv/bin/pip install -r /opt/balloon/pi/requirements.txt
```

手元でログを見る:

```bash
sudo /opt/balloon/venv/bin/python -u /opt/balloon/pi/balloon.py
```

起動時から動かす:

```bash
sudo cp /opt/balloon/pi/balloon.service /etc/systemd/system/balloon.service
sudo systemctl daemon-reload
sudo systemctl enable --now balloon.service
journalctl -u balloon.service -f
```

Windows からコピーした `balloon.service` は改行が CR+LF になり、systemd が読めないことがある。そのときは次を実行する。

```bash
sudo sed -i 's/\r$//' /etc/systemd/system/balloon.service
sudo systemctl daemon-reload
```

ユニットは root で動く。パスを変えたときは `balloon.service` の `WorkingDirectory` と `ExecStart` も合わせる。

## 期待するログ

```text
MAX-M10S ready (UBX, airborne <1g, RAM only)
=== balloon (TX) ===
P2P configure OK
NO FIX type=0 ok=0 sats=0 age=120 ms
TX: 20261009T085005Z,35.681236,139.767125,1234.5 ... OK
```

`NO FIX`、`STALE`、`NO TIME` のときは送信しない。PVT が 2.5 秒より古いときが `STALE`。

## トラブルシュート

| 症状 | 確認 |
|------|------|
| `LoRa UART begin failed` | `/dev/serial0`、シリアルコンソール無効、配線 |
| `P2P configure failed` で応答なし | TX/RX の交差、3.3V、GND、MD0=LOW、RST_M0=HIGH |
| `MAX-M10S not found` | `i2cdetect -y 1` で `42` が出るか。SDA/SCL、3.3V、GND |
| `NO FIX` かつ `sats=0` | アンテナと空の見通し。コールドスタートは数分かかることがある |
| 地上で文字が化ける | 周波数、SF、帯域幅、sync、CRC が両端で一致しているか |
