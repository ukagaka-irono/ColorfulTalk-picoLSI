/*
 * ColorfulTalk pico LSI ファームウェア（ATmega328P, 16MHz）
 *
 * ・UART（9600bps 8N1）または I2C スレーブ（0x2E）でローマ字記号列を 1 行受け取り、話す
 * ・音声は Timer2 の高速 PWM（62.5kHz）で PD3/OC2B から出す。Timer1 の割り込みで 8kHz ごとに 1 サンプル
 * ・メインループが ColorfulTalk nano で合成してリングバッファを満たし、割り込みが取り出す
 * ・PD2 は話している間 High（BUSY）
 * SPDX-License-Identifier: MIT
 */
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <string.h>
#include "colorful_talk_nano.h"

#define PICO_VERSION "1.0.0"
#define LINE_MAX     128          /* 1 行の長さ（終端を含む） */
#define RB_SIZE      64           /* 出力のリングバッファ（2 のべき） */
#define PREFILL      32           /* これだけ溜めてから鳴らし始める */
#define I2C_ADDR     0x2E

/* ---- 受信（UART と I2C で共通の 1 行） ------------------------------------- */
static volatile char    rx_line[LINE_MAX];
static volatile uint8_t rx_len;
static volatile uint8_t rx_ready;          /* 1 行そろった（メインループが取るまで次は受けない） */
static volatile uint8_t rx_overflow;

static char speak_buf[LINE_MAX];           /* 話している記号列（nano が話しながら読む） */

/* ---- 出力 ------------------------------------------------------------------- */
static volatile uint8_t rb[RB_SIZE];
static volatile uint8_t rb_head, rb_tail;
static volatile uint8_t playing;           /* 割り込みがバッファから取り出す */
static volatile uint8_t underruns;

static uint8_t voice = CTN_VOICE_F, speed = 100;
static volatile uint8_t status_err;        /* I2C で読める直近のエラーコード */

/* ---- UART -------------------------------------------------------------------- */
static void uart_init(void)
{
    UBRR0H = 0;
    UBRR0L = 103;                              /* 16MHz / (16 × 9600) − 1 */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);    /* 8N1 */
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0))) {}
    UDR0 = (uint8_t)c;
}

static void uart_puts(const char* s)
{
    while (*s) uart_putc(*s++);
}

static void uart_putu(uint16_t v)
{
    char b[6];
    uint8_t n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) uart_putc(b[--n]);
}

/* 受信した 1 文字。改行で 1 行の終わり */
static void rx_char(char c)
{
    if (rx_ready) return;                      /* 前の行がまだ処理されていない: 捨てる */
    if (c == '\r' || c == '\n' || c == 0) {
        if (rx_len) { rx_line[rx_len] = 0; rx_ready = 1; }
        return;
    }
    if (rx_len < LINE_MAX - 1) rx_line[rx_len++] = c;
    else rx_overflow = 1;
}

ISR(USART_RX_vect)
{
    rx_char((char)UDR0);
}

/* ---- I2C スレーブ ----------------------------------------------------------------- */
static void i2c_init(void)
{
    TWAR = (uint8_t)(I2C_ADDR << 1);
    TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
}

ISR(TWI_vect)
{
    switch (TWSR & 0xF8) {
    case 0x60: case 0x68:                       /* 自分宛ての書き込み開始 */
        break;
    case 0x80:                                  /* データを受信 */
        rx_char((char)TWDR);
        break;
    case 0xA0:                                  /* 停止: 1 行の終わり */
        rx_char('\n');
        break;
    case 0xA8: case 0xB8:                       /* 読み出し: 状態 1 バイト目、エラーコード 2 バイト目 */
        TWDR = (uint8_t)((TWSR & 0xF8) == 0xA8 ? (playing || ctn_busy() ? 1 : rx_ready ? 2 : 0) : status_err);
        break;
    default:
        break;
    }
    TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
}

/* ---- 音声出力 ------------------------------------------------------------------- */
static void audio_init(void)
{
    DDRD |= (1 << PD3) | (1 << PD2);           /* PWM 出力と BUSY */
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);   /* 高速 PWM, OC2B */
    TCCR2B = (1 << CS20);                      /* 分周なし: 16MHz / 256 = 62.5kHz */
    OCR2B = 128;
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS10);       /* CTC, 分周なし */
    OCR1A = 1999;                              /* 16MHz / 2000 = 8kHz */
    TIMSK1 = (1 << OCIE1A);
}

ISR(TIMER1_COMPA_vect)
{
    if (!playing) return;
    if (rb_head != rb_tail) {
        OCR2B = rb[rb_tail];
        rb_tail = (uint8_t)((rb_tail + 1) & (RB_SIZE - 1));
    } else {
        GPIOR0 = ++underruns;                  /* 合成が間に合わなかった（GPIOR0 は試験用） */
    }
}

static uint8_t rb_free(void)
{
    return (uint8_t)((rb_tail - rb_head - 1) & (RB_SIZE - 1));
}

/* nano で合成してバッファを満たす。話し終わって全部出したら 1 */
static uint8_t pump(void)
{
    while (rb_free() && ctn_busy()) {
        uint8_t s = ctn_sample8();
        if (!ctn_busy()) break;
        rb[rb_head] = s;
        rb_head = (uint8_t)((rb_head + 1) & (RB_SIZE - 1));
        if (!playing && ((rb_head - rb_tail) & (RB_SIZE - 1)) >= PREFILL) playing = 1;
    }
    if (!ctn_busy()) {
        playing = 1;                           /* 短い発話でも残りを出す */
        if (rb_head == rb_tail) return 1;
    }
    return 0;
}

static void speak_end(void)
{
    playing = 0;
    PORTD &= (uint8_t)~(1 << PD2);            /* BUSY を先に下げてから無音に戻す */
    OCR2B = 128;
}

/* ---- コマンド ------------------------------------------------------------------- */
static uint8_t parse_u8(const char* s, uint16_t* v)
{
    uint16_t x = 0;
    while (*s == ' ') s++;
    if (*s < '0' || *s > '9') return 0;
    while (*s >= '0' && *s <= '9') { x = (uint16_t)(x * 10 + (*s - '0')); if (x > 999) return 0; s++; }
    *v = x;
    return 1;
}

static void reply_error(uint8_t code, int16_t pos)
{
    status_err = code;
    uart_putc('E');
    uart_putu(code);
    uart_putc(',');
    uart_putu(pos < 0 ? 0 : (uint16_t)pos);
    uart_puts("\r\n");
}

/* 1 行を処理する。話し始めたら 1 */
static uint8_t handle_line(const char* line)
{
    uint16_t v;
    if (line[0] == '#') {
        switch (line[1]) {
        case 'V': case 'v':
            if (parse_u8(line + 2, &v) && v < CTN_VOICE_COUNT) voice = (uint8_t)v;
            else reply_error(CT_ERR_PARAM, 0);
            break;
        case 'S': case 's':
            if (parse_u8(line + 2, &v) && v >= 50 && v <= 200) speed = (uint8_t)v;
            else reply_error(CT_ERR_PARAM, 0);
            break;
        case 'X': case 'x':
            break;                             /* 話していなければ何もしない */
        case '?':
            uart_puts("ColorfulTalk pico " PICO_VERSION "\r\n");
            break;
        default:
            reply_error(CT_ERR_CHAR, 1);
            break;
        }
        uart_putc('>');
        return 0;
    }
    memcpy(speak_buf, line, LINE_MAX);
    {
        int8_t err = ctn_start(speak_buf, voice, speed);
        if (err) {
            reply_error((uint8_t)err, ctn_error_pos());
            uart_putc('>');
            return 0;
        }
    }
    status_err = 0;
    rb_head = rb_tail = 0;
    PORTD |= (1 << PD2);
    return 1;
}

int main(void)
{
    char line[LINE_MAX];
    uint8_t speaking = 0;

    audio_init();
    uart_init();
    i2c_init();
    sei();
    uart_putc('>');

    for (;;) {
        if (speaking && pump()) {
            speak_end();
            speaking = 0;
            uart_putc('>');
        }
        if (rx_ready) {
            uint8_t ov;
            cli();
            memcpy(line, (const char*)rx_line, LINE_MAX);
            ov = rx_overflow;
            if (speaking && !(line[0] == '#' && (line[1] == 'X' || line[1] == 'x'))) {
                sei();                         /* 話している間は次の行を待たせる（#X だけはすぐ処理） */
                continue;
            }
            rx_len = 0;
            rx_ready = 0;
            rx_overflow = 0;
            sei();
            if (ov) { reply_error(CT_ERR_TOOLONG, LINE_MAX - 1); uart_putc('>'); continue; }
            if (speaking) {                    /* #X */
                ctn_stop();
                speak_end();
                speaking = 0;
                uart_putc('>');
                continue;
            }
            speaking = handle_line(line);
        }
    }
}
