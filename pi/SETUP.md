# SSH で Pi Zero W をセットアップする

対象は Raspberry Pi Zero W 1.1。Windows PC から、モニターもキーボードも使わずに気球送信プログラムまで入れる。配線と送るデータの意味は [README.md](README.md)。

| 項目 | 値 |
|------|-----|
| ボード | Raspberry Pi Zero W 1.1 |
| ホスト名 | `zero` |
| SSH | `labobo@zero.local` |
| ユーザー | `labobo` |

Imager のホスト名には `zero` と入れる。`zero.local` の `.local` は mDNS が付ける名前なので、ホスト名に含めない。Zero W の Wi-Fi は 2.4 GHz だけなので、5 GHz 専用の SSID には繋がらない。

## 1. OS を書き込む

[Raspberry Pi Imager](https://www.raspberrypi.com/software/) で microSD（8 GB 以上）に書き込む。

1. デバイスは Raspberry Pi Zero W を選ぶ。
2. OS は **Raspberry Pi OS Lite**（デスクトップ無し、32-bit）にする。Zero W 1.1 は 32-bit しか動かない。
3. 書き込む前の OS カスタマイズで次を入れる。画面が出ない Imager では、書き込みボタンの前の歯車、または Ctrl+Shift+X。
   - ホスト名: `zero`
   - ユーザー名: `labobo` と、そのパスワード
   - SSH を有効にする（パスワード認証）
   - Wi-Fi の SSID、パスワード、無線の国コード `JP`
   - タイムゾーンは `Asia/Tokyo`
4. 書き込みが終わったら SD カードを Zero W に挿す。

Wi-Fi と SSH はこの画面で設定する。boot パーティションに `wpa_supplicant.conf` を置いても、現行の Raspberry Pi OS は読まない。

## 2. SSH で入る

電源は **PWR IN** と書かれた micro USB に入れる。初回起動はパーティション拡張が入るので、ACT LED の点滅が落ち着くまで 2〜5 分見る。

PowerShell:

```powershell
ssh labobo@zero.local
```

初回はホスト鍵の確認が出るので `yes` と入力する。パスワードは Imager で `labobo` に設定したもので、入力中は表示されない。

`zero.local` が見つからないときは、ルータの DHCP クライアント一覧で Zero W の IPv4 アドレスを見て、そのアドレスに対して `ssh` する。

```powershell
ssh labobo@192.168.1.50
```

## 3. I2C と UART を有効にする

ログインしたあと、次をまとめて実行する。

```bash
sudo apt update
sudo apt install -y python3-venv python3-pip i2c-tools

sudo raspi-config nonint do_i2c 0
sudo raspi-config nonint do_serial 2

if [ -f /boot/firmware/config.txt ]; then
  BOOT=/boot/firmware
else
  BOOT=/boot
fi
grep -q '^dtoverlay=disable-bt' "$BOOT/config.txt" || echo 'dtoverlay=disable-bt' | sudo tee -a "$BOOT/config.txt"

# Imager の初回設定が console=serial0 を残すことがある。LoRa 用 UART と競合するので消す。
sudo sed -i -E 's/console=serial0,[0-9]+ //g; s/console=ttyAMA0,[0-9]+ //g' "$BOOT/cmdline.txt"

sudo reboot
```

`do_i2c 0` は I2C を有効にする。`do_serial 2` は GPIO の UART を有効にし、シリアルログインは無効にする。`dtoverlay=disable-bt` は Zero W で PL011 を GPIO14/15 に出し、115200 bps を安定させる。Bluetooth は使えなくなる。`cmdline.txt` から `console=serial0` を消すのは、ログインシェルが UART を掴んだままにならないようにするためである。

再起動で SSH は切れる。もう一度入る。

```powershell
ssh labobo@zero.local
```

```bash
ls -l /dev/serial0
if [ -f /boot/firmware/cmdline.txt ]; then
  CMDLINE=/boot/firmware/cmdline.txt
else
  CMDLINE=/boot/cmdline.txt
fi
grep console=serial "$CMDLINE" || echo "serial console is off"
i2cdetect -y 1
```

`/dev/serial0` のリンク先は `ttyAMA0` である。`ttyS0` のときは `dtoverlay=disable-bt` が config.txt に入っているか確認して再起動する。`console=serial` の行は残っていないこと。

MAX-M10S を繋いだ状態の `i2cdetect` では、アドレス `42` が出る。`--` のままなら SDA/SCL、3.3V、GND を見る。

## 4. プログラムを送って起動する

PowerShell でリポジトリに移動し、Pi へコピーする。clone の場所が違えば `cd` のパスを合わせる。

```powershell
cd C:\repos\balloon
ssh labobo@zero.local "rm -rf ~/pi"
scp -r .\pi labobo@zero.local:~/pi
```

再び `ssh labobo@zero.local` して、配置とサービス登録をする。Windows からコピーしたユニットファイルは改行が CR+LF になるので、登録前に取り除く。

```bash
sudo mkdir -p /opt/balloon
sudo rm -rf /opt/balloon/pi
sudo mv ~/pi /opt/balloon/pi
sudo python3 -m venv /opt/balloon/venv
sudo /opt/balloon/venv/bin/pip install -r /opt/balloon/pi/requirements.txt
sudo sed -i 's/\r$//' /opt/balloon/pi/balloon.service
sudo cp /opt/balloon/pi/balloon.service /etc/systemd/system/balloon.service
sudo systemctl daemon-reload
sudo systemctl enable --now balloon.service
sudo journalctl -u balloon.service -f
```

ログの見方は [README.md](README.md) の「期待するログ」。`journalctl` の追従は Ctrl+C で止まる。サービスは動いたままである。

止めるとき、または設定を変えたあとに入れ直すとき:

```bash
sudo systemctl stop balloon.service
sudo systemctl start balloon.service
```
