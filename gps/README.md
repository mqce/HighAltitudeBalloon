# ESP32-C3 SuperMini + MAX-M10S GPS

PlatformIO で ESP32-C3 SuperMini から [スイッチサイエンス MAX-M10S 搭載 GNSS モジュール](https://www.switch-science.com/products/10437)（SSCI-10437）の緯度経度を取得する。

https://learn.sparkfun.com/tutorials/gnss-receiver-breakout---max-m10s-qwiic-hookup-guide

## 注意

- **外付けアンテナ必須**（未接続だと Fix しない）
- 電源は **3.3V**（5V 不可）
- ボードの UART シルクは TX→ESP32 RX、RX→ESP32 TX でクロス接続

## ESP32-C3 SuperMini ピン（UART）

| シルク | GPIO | 役割 |
|--------|------|------|
| RX | GPIO20 | UART 受信 |
| TX | GPIO21 | UART 送信 |

USB モニタは USB CDC 経由のため、GPIO20/21 を GPS 用に使用可能。

## 配線

| MAX-M10S ボード | ESP32-C3 SuperMini |
|-----------------|-------------------|
| TX | GPIO20 (RX) |
| RX | GPIO21 (TX) |
| 3V3 | 3.3V |
| GND | GND |

UART ボーレート: **9600**（u-blox MAX-M10S 既定）

SparkFun 資料では 38400 と書かれている場合がある。`chk ok` が増えないときは `GPS_BAUD` を 38400 に変更して再試行。

## ビルド・書き込み

```bash
pio run -t upload
pio device monitor
```

## ピン変更

`src/main.cpp` 先頭の `GPS_RX_PIN` / `GPS_TX_PIN` / `GPS_BAUD` を編集。

## トラブルシュート

| 症状 | 意味 |
|------|------|
| `No GPS data` | 配線・ボーレート不良 |
| `chk ok` 増加、`Sats: 0` | **UART OK・衛星未受信**（アンテナ／見通し） |
| `chk fail` 増加 | ノイズ・ボーレート不一致 |

`Sats: 0` のとき:

1. SMA アンテナがしっかり締まっているか
2. **アクティブアンテナ**推奨（ボードは SMA 中心ピンに約 3.3V 給電あり）
3. 空が大きく見える屋外（軒下・窓際は弱い）
4. ボードの **PPS LED** が点滅するか（Fix すると 1Hz 点滅）
5. コールドスタートは数分かかることがある（ただし衛星数がずっと 0 ならアンテナ側）
