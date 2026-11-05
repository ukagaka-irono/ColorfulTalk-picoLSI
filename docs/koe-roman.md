# ローマ字記号列（ColorfulTalk nano / pico LSI）

8bit マイコンで扱いやすいよう、ASCII だけで書く記号列です。
[カナ記号列](https://github.com/ukagaka-irono/ColorfulTalk/blob/main/docs/spec/koe-kana.md) と同じ規則でアクセントや区切りを書きます。

```
konnichiwa,seka'i.
kyo'uwa/i'i/teNkidesu.
hoNtouni/so'udesuka?
```

## 音

大文字・小文字は区別しません（`N` だけは例外で、常に「ん」）。空白・改行は無視します。

| 行 | 書き方 |
|---|---|
| 母音 | a i u e o |
| か・が | ka ki ku ke ko / ga gi gu ge go |
| さ・ざ | sa shi su se so（si も可） / za ji zu ze zo（zi も可） |
| た・だ | ta chi tsu te to（ti, tu も可） / da de do。ぢ・づ は di・du（ji・zu と同じ音） |
| な | na ni nu ne no |
| は・ば・ぱ | ha hi fu he ho（hu も可） / ba bi bu be bo / pa pi pu pe po |
| ま | ma mi mu me mo |
| や・わ | ya yu yo / wa、wo（「お」と同じ音） |
| ら | ra ri ru re ro |
| 拗音 | kya kyu kyo、gya…、sha shu sho（sya… も可）、ja ju jo（jya… も可）、cha chu cho（tya… も可）、nya…、hya…、bya…、pya…、mya…、rya… |
| 外来音 | fa fi fe fo fyu、thi（てぃ）、dhi（でぃ）、twu（とぅ）、dwu（どぅ）、wi we、ye、tsa tsi tse tso、va vi vu ve vo（ば行と同じ音） |
| ん | 下の表 |
| っ | 次の子音字を重ねる（`kk` `tt` `ss` `pp` など）。`tch` も可。単独で書くときは `q` |
| ー | `-`。直前の母音を伸ばす |

### 「ん」の書き方

| 書き方 | 読み | 例 |
|---|---|---|
| `n` ＋ 母音・`y` | な行・にゃ行 | `kani` = かに、`nyanko` = にゃんこ |
| `n` ＋ それ以外の子音、または区切り・終わり | ん | `kanpai` = かんぱい、`hon.` = ほん |
| `nn` ＋ 母音・`y` | ん ＋ な行 | `konnichiwa` = こんにちわ、`minna` = みんな |
| `nn` ＋ それ以外 | ん | `kannpai` = かんぱい |
| `N`（大文字） | 常に ん | `hoNya` = ほんや、`geNiN` = げんいん、`kaNi` = かんい |

「ん」の直後に母音や や行が来るとき（ほんや、げんいん）は、大文字の `N` を使ってください。

## アクセント・区切り

| 記号 | 意味 |
|---|---|
| `'` | アクセント核。直前のモーラの後で声が下がる |
| `/` | アクセント句の区切り（間を空けない） |
| `,` | 短い間 |
| `.` `!` | 文末 |
| `?` | 疑問の文末 |
| `_` | 直後のモーラを無声化（`de_su` など） |

アクセントの付き方（東京方言）と自動の無声化は、カナ記号列と同じです。

## 制限とエラー

| 項目 | 値 |
|---|---|
| 1 つのアクセント句 | 120 モーラまで |
| 記号列全体 | 254 モーラまで |
| エラーの位置 | 先頭からのバイト数（0 始まり） |

エラーコードはカナ記号列と同じです（`CT_ERR_CHAR` `CT_ERR_ACCENT` `CT_ERR_LONG` `CT_ERR_DEVOICE` `CT_ERR_EMPTY` `CT_ERR_TOOLONG`）。
