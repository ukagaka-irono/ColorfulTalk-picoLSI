/*
 * ColorfulTalk nano — 8bit CPU 向けの極小の日本語音声合成エンジン
 *
 * 入力: ローマ字記号列（ASCII, docs/koe-roman.md）。話し終わるまで呼び出し側が保持すること
 * 出力: 8kHz。1 サンプルずつ取り出す（16bit 符号付き、または PWM 用の 8bit 符号なし）
 * RAM: 静的な作業領域 1 つ（約 150 バイト）。表はすべて ROM（AVR では PROGMEM）
 * SPDX-License-Identifier: MIT
 */
#ifndef COLORFUL_TALK_NANO_H
#define COLORFUL_TALK_NANO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CTN_VERSION     "1.0.0"
#define CTN_SAMPLE_RATE 8000

/* 声 */
enum { CTN_VOICE_F = 0, CTN_VOICE_M = 1, CTN_VOICE_C = 2, CTN_VOICE_R = 3, CTN_VOICE_COUNT = 4 };

/* エラーコード（ColorfulTalk シリーズ共通の値） */
#ifndef CT_OK
enum {
    CT_OK = 0, CT_ERR_NULL = 1, CT_ERR_UTF8 = 2, CT_ERR_CHAR = 3, CT_ERR_ACCENT = 4, CT_ERR_LONG = 5,
    CT_ERR_DEVOICE = 6, CT_ERR_EMPTY = 7, CT_ERR_PARAM = 8, CT_ERR_TOOLONG = 9
};
#endif

/*
 * 話し始める。記号列を先頭から検査し、誤りがあればエラーコードを返す（位置は ctn_error_pos）。
 * voice: CTN_VOICE_*、speed: 50〜200（100 = 標準）
 */
int8_t   ctn_start(const char* koe, uint8_t voice, uint8_t speed);
int16_t  ctn_sample(void);      /* 1 サンプル。話し終わったら 0 */
uint8_t  ctn_sample8(void);     /* 8bit 符号なし（中心 128, ctn_sample の 1/128）。PWM にそのまま使える */
uint8_t  ctn_busy(void);        /* 話している間 1 */
void     ctn_stop(void);
int16_t  ctn_error_pos(void);   /* 直近の ctn_start のエラー位置（バイト単位）。なければ -1 */

#ifdef __cplusplus
}
#endif

#endif
