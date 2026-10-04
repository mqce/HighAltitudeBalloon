# ESP32-C3 SuperMini + MAX-M10S GPS

PlatformIO で ESP32-C3 SuperMini から [スイッチサイエンス MAX-M10S 搭載 GNSS モジュール](https://www.switch-science.com/products/10437)（SSCI-10437）の緯度経度を取得する。

https://learn.sparkfun.com/tutorials/gnss-receiver-breakout---max-m10s-qwiic-hookup-guide

## 注意

- **外付けアンテナ必須**（未接続だと Fix しない）
- 電源は **3.3V**（5V 不可）
- 基板の I2C プルアップジャンパは初期状態でオープン。短い配線ならモジュール内蔵プルアップで足りることが多い。`chk fail` が増えるときはジャンパを閉じて 2.2 kΩ を有効にする

## ESP32-C3 SuperMini ピン（I2C）

| 役割 | GPIO |
|------|------|
| SDA | GPIO6 |
| SCL | GPIO7 |

GPIO8 はオンボード LED、GPIO9 は BOOT のストラップピンなので使わない。USB モニタは USB CDC 経由。

## 配線

| MAX-M10S ボード | ESP32-C3 SuperMini |
|-----------------|-------------------|
| SDA | GPIO6 |
| SCL | GPIO7 |
| 3V3 | 3.3V |
| GND | GND |

Qwiic ケーブルでも同じ4本（黒=GND、赤=3.3V、青=SDA、黄=SCL）。

I2C アドレス: **0x42**。クロック: **100 kHz**。

起動時に RAM だけへ次を設定する。Flash と BBR には書かないので、電源を切るとモジュール側の設定は元に戻る。

- I2C 出力は UBX のみ
- dynamic model は airborne <1g（2D Fix は出さない）
- 測位周期 1 Hz の `NAV-PVT`

## ビルド・書き込み

```bash
pio run -t upload
pio device monitor
```

## ピン変更

`src/main.cpp` 先頭の `GPS_SDA_PIN` / `GPS_SCL_PIN` / `GPS_I2C_HZ` を編集。プルアップを入れたあとは `GPS_I2C_HZ` を 400000 に上げられる。

## トラブルシュート

| 症状 | 意味 |
|------|------|
| `MAX-M10S not found` | SDA/SCL の取り違え、3.3V、アドレス 0x42 |
| `No GNSS solution yet` | I2C は応答したが `NAV-PVT` がまだ来ない |
| `NO FIX` かつ `Sats: 0` | **I2C OK・衛星未受信**（アンテナ／見通し） |
| `FIX` かつ `type:3 ok:1` | 3D Fix。`hAcc` / `vAcc` は精度の目安（m） |
| `STALE` | 新しい `NAV-PVT` が 2.5 秒以上来ていない |

`Sats: 0` のとき:

1. SMA アンテナがしっかり締まっているか
2. **アクティブアンテナ**推奨（ボードは SMA 中心ピンに約 3.3V 給電あり）
3. 空が大きく見える屋外（軒下・窓際は弱い）
4. ボードの **PPS LED** が点滅するか（Fix すると 1Hz 点滅）
5. コールドスタートは数分かかることがある（ただし衛星数がずっと 0 ならアンテナ側）
