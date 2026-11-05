/* ColorfulTalk nano 内部定義  SPDX-License-Identifier: MIT */
#ifndef CTN_INTERNAL_H
#define CTN_INTERNAL_H

#include <stdint.h>
#include "colorful_talk_nano.h"

/* 表は ROM に置く。AVR では PROGMEM から専用命令で読む */
#ifdef __AVR__
#  include <avr/pgmspace.h>
#  define CTN_ROM PROGMEM
#  define RD8(p)  pgm_read_byte(p)
#  define RD16(p) ((int16_t)pgm_read_word(p))
#else
#  define CTN_ROM
#  define RD8(p)  (*(const uint8_t*)(p))
#  define RD16(p) (*(const int16_t*)(p))
#endif

#define CTN_FRAME   32     /* 1 フレームのサンプル数（4ms） */
#define CTN_PULSE   32     /* 声帯音源 1 周期の点数 */

/* 声（ROM） */
typedef struct {
    uint16_t f0;           /* 基準 F0 [Hz] */
    uint16_t fscale;       /* 声道倍率 Q8 */
    uint8_t  range;        /* 抑揚の幅 ×1/64 */
    uint8_t  gain;         /* 出力ゲイン ×1/64 */
    uint8_t  te;           /* 声門が閉じる位置（0〜31） */
    uint8_t  jitter;       /* 周期ゆらぎ */
    uint8_t  breath;       /* 母音の気息音 0〜255 */
    uint8_t  pulse[CTN_PULSE]; /* 声門体積流 1 周期 0〜255 */
} CtnVoice;

/* 摩擦用の共振器の形 */
enum { FR_S = 0, FR_SH, FR_HY, FR_F, FR_LAB, FR_ALV, FR_VELF, FR_VELB, FR_COUNT };

extern const int16_t  CTN_COS[129] CTN_ROM;      /* cos(πi/128)   Q14（31.25Hz 刻み） */
extern const int16_t  CTN_EXP[65] CTN_ROM;       /* exp(-π·16i/fs) Q14（16Hz 刻み） */
extern const int16_t  CTN_FRIC[FR_COUNT][3] CTN_ROM; /* a, b, c  Q13（ピークのゲイン 1） */
extern const CtnVoice CTN_VOICES[CTN_VOICE_COUNT] CTN_ROM;

#endif
