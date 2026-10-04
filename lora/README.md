# LoRa P2P 実験（ESP32-C3 SuperMini + TLM922S-P01A）

成層圏気球向け LoRa 通信の初期実験用プロジェクトです。  
気球側・地上側それぞれに TLM922S-P01A を1台ずつ使い、UART 経由の P2P で簡易通信を行います。

現時点の動作は次のとおりです。

- **気球側 (`balloon`)**: 約3秒ごとに `"hello"` を送信
- **地上側 (`ground`)**: 受信して USB シリアルに表示

## ハードウェア

| 項目 | 内容 |
|------|------|
| MCU | ESP32-C3 SuperMini × 2 |
| LoRa | TLM922S-P01A × 2 |
| ホスト↔モジュール | UART 115200 8N1 |
| デバッグ出力 | USB-C（USB CDC） |

### 配線

気球側・地上側とも同じです。

| ESP32-C3 SuperMini | TLM922S-P01A |
|--------------------|--------------|
| 3V3 | VDD |
| GND | GND |
| 3V3 | RST_M0 (Pin24) |
| GND | MD0 (Pin1) |
| GPIO6 (TX) | UART_RX (Pin21) |
| GPIO7 (RX) | UART_TX (Pin22) |

- **RST_M0 (Pin24)** は active-low のリセット入力。**HIGH (3V3) で通常動作**。開放のままだとリセットがかかり UART が応答しないことがある。
- **MD0 (Pin1)** は通常動作で LOW (GND)。HIGH は Flash 書き込みモード。

### 無線パラメータ（初期値）

`platformio.ini` の `build_flags` で共通設定しています。  
送信側（`balloon`）と受信側（`ground`）で必ず同一にします。

| 項目 | 値 |
|------|-----|
| 周波数 | 922500000 Hz（922.5 MHz） |
| SF | 7（実験） / 10（飛行候補） |
| BW | 125 kHz |
| 送信出力 | 14 dBm |
| 送信間隔 | 実験: 約 3 s / 飛行想定: 5–10 s |
| ペイロード | 実験: `"hello"` / 飛行想定: 約 30–50 byte |

#### 各設定の根拠

| 項目 | 根拠 |
|------|------|
| 周波数 922.5 MHz | 日本の特定小電力 920 MHz 帯、TLM922S-P01A の技適範囲（概ね 920.6–928 MHz）内。距離性能より合法性・両端一致・混信回避が優先。 |
| SF | Spreading Factor（拡散率）。大きいほど遠距離・耐雑音向き、送信時間は伸びる。現状は近距離実験用に SF7。成層圏気球は十数〜数十 km 想定のため、高知高専の地上実験（最大約 28 km）に合わせ SF10 を第一候補とする。 |
| BW 125 kHz | Bandwidth（帯域幅）。狭いほど距離向き・遅い。920 MHz 帯 LoRa の標準的な値で、同論文でも主に 125 kHz。 |
| 送信出力 14 dBm | モジュール設定の初期値。技適の空中線電力上限内で運用すること（証明書・データシートで確認）。 |
| 送信間隔 | `"hello"` 程度なら 3 s でもエアタイムは小さく、ARIB STD-T108 の休止・1 時間合計送信時間の目安に対して余裕がある。本番は 50 byte 前後・SF10 を想定し、論文どおり 5–10 s、エアタイムおおむね 2 s 以内を目安。 |
| ペイロード | LoRa は 1 回あたり数十 byte 程度が現実的。飛行時は時刻・緯度・経度・高度・気圧に絞る。時刻は RTC 必須ではなく、GNSS から位置と同時に取得するのが本命。 |

参考:

- ARIB STD-T108（920 MHz 帯の送信時間・休止などの制限）
- 高知工業高等専門学校学術紀要: [大気圏観測のための気球搭載観測装置の開発：LoRa通信実験と気球観測計画](https://www.kochi-ct.ac.jp/files/uploads/%E5%A4%A7%E6%B0%97%E5%9C%8F%E8%A6%B3%E6%B8%AC%E3%81%AE%E3%81%9F%E3%82%81%E3%81%AE%E6%B0%97%E7%90%83%E6%90%AD%E8%BC%89%E8%A6%B3%E6%B8%AC%E8%A3%85%E7%BD%AE%E3%81%AE%E9%96%8B%E7%99%BA%EF%BC%9ALoRa%E9%80%9A%E4%BF%A1%E5%AE%9F%E9%A8%93%E3%81%A8%E6%B0%97%E7%90%83%E8%A6%B3%E6%B8%AC%E8%A8%88%E7%94%BB.pdf)

## ビルド / 書き込み

PlatformIO を使用します。気球側と地上側は別環境です。

```bash
# 気球側
pio run -e balloon -t upload
pio device monitor -e balloon

# 地上側
pio run -e ground -t upload
pio device monitor -e ground
```

## 期待するログ

**気球側**

```text
=== balloon (TX) ===
UART RX=7 TX=6
P2P configure OK
...
TX: hello ... OK
```

**地上側**

```text
=== ground (RX) ===
UART RX=7 TX=6
P2P configure OK
Waiting for packets...
RX text="hello" hex=68656c6c6f rssi=... snr=...
```

`P2P configure failed` が出る場合は、配線・電源・アンテナを確認し、モジュール電源を入れ直して再試行してください。

## ディレクトリ構成

```text
lora/
├── platformio.ini          # balloon / ground 環境定義
├── include/
│   ├── pins.h              # ピン・無線パラメータ
│   └── tlm922s.h           # TLM922S ラッパ
├── src/
│   ├── tlm922s.cpp         # ATコマンド送受信
│   ├── balloon/main.cpp    # 送信側
│   └── ground/main.cpp     # 受信側
└── README.md
```
