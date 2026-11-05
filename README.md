# ColorfulTalk pico LSI

[ColorfulTalk nano](https://github.com/ukagaka-irono/ColorfulTalk-nano) を ATmega328P に載せた、
**1 チップの音声合成 LSI の設計図**（ファームウェア・回路図・部品表）です。無料（MIT ライセンス）。

ホストのマイコンや PC から、ローマ字の記号列を 1 行送るだけで話します。

```
konnichiwa,seka'i.
```

- チップは ATmega328P（Arduino Uno と同じ）。部品は水晶・コンデンサ・抵抗など 10 点ほど
- UART（9600bps）または I2C（0x2E）で記号列を送る。話し終わると `>` を返し、BUSY ピンも下がる
- 音声は PWM で出力し、RC フィルタとアンプでスピーカーを鳴らす
- 4 種の声（女声・男声・子ども・ロボット）と話速をコマンドで選べる
- シミュレータで動作を確認済み（PC 版 nano と出力がビット単位で一致、実時間で途切れない）

## 中身

| 場所 | 内容 |
|---|---|
| [firmware/](firmware/) | ファームウェア（`main.c`、`Makefile`）。`firmware/nano/` は nano 1.0.0 のソース |
| [hardware/schematic.svg](hardware/schematic.svg) | 回路図 |
| [hardware/bom.md](hardware/bom.md) | 部品表 |
| [docs/spec.md](docs/spec.md) | 仕様書（ピン配置、UART・I2C のコマンド、確認したこと） |
| [docs/building.md](docs/building.md) | ビルド・ヒューズ・書き込みの手順 |
| [docs/koe-roman.md](docs/koe-roman.md) | ローマ字記号列の書き方 |
| [tests/](tests/) | シミュレータ（avr8js）による試験 |

## すぐ試す

```bash
make -C firmware
```

```bash
make -C firmware fuses flash PROGRAMMER=usbasp
```

9600bps 8N1 で接続して `konnichiwa,seka'i.` と改行を送ると話します。詳しくは [docs/building.md](docs/building.md)。

## 試験

ファームウェアを avr8js で ATmega328P として動かし、UART から操作して PWM の値・返事・BUSY を確かめます。

```bash
AVR_GCC=/path/to/avr-gcc/bin AVR8JS=/path/to/avr8js-folder NANO_CTALK=/path/to/nano/ctalk tests/check.sh
```

I2C はシミュレータが対応していないため未確認です。実機での確認結果があれば Issue で教えてください。

## ライセンス

MIT License。詳細は [LICENSE](LICENSE)。
