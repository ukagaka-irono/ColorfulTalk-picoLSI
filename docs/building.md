# ファームウェアのビルドと書き込み

## 必要なもの

- avr-gcc と avr-libc（例: [ZakKemble/avr-gcc-build](https://github.com/ZakKemble/avr-gcc-build) の配布物、Microchip の AVR 8-bit Toolchain、Linux の `gcc-avr` パッケージ）
- avrdude
- ISP 書き込み器（USBasp、AVRISP mkII、Arduino as ISP など）

## ビルド

```bash
make -C firmware
```

`firmware/pico-lsi.hex` ができます。ビルド済みの hex は GitHub のリリースに添付します。

## ヒューズ（最初に 1 回だけ）

外部水晶 16MHz、ブラウンアウト検出 2.7V の設定です。

| ヒューズ | 値 |
|---|---|
| low | `0xFF` |
| high | `0xD9` |
| extended | `0xFD`（avrdude の版によっては `0x05` と表示されます） |

```bash
make -C firmware fuses PROGRAMMER=usbasp
```

## 書き込み

```bash
make -C firmware flash PROGRAMMER=usbasp
```

Arduino as ISP を使う場合は `PROGRAMMER=stk500v1 PORT=COM3`（Windows）などを指定し、avrdude に `-b 19200` が必要なことがあります。

## 動作の確認

USB シリアル変換器などで 9600bps 8N1 に接続し、ターミナルソフトから送ります。

```
>                      ← 起動
#?
ColorfulTalk pico 1.0.0
>
konnichiwa,seka'i.     ← 話す
>                      ← 話し終わり
```

Arduino をホストにする例:

```c
Serial.begin(9600);
Serial.print("konnichiwa,seka'i.\n");
while (Serial.read() != '>') {}   // 話し終わりを待つ
```

## Arduino Uno で試す

Arduino Uno の ATmega328P（16MHz）にもそのまま書き込めます。音声は D3 ピンから出ます（RC フィルタを付けてアンプへ）。
ただしブートローダーは消えるため、Arduino として使い直すにはブートローダーを書き込み直してください。
