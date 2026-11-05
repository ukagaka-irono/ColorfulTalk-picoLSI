/*
 * ColorfulTalk nano — 8bit CPU 向けの極小の日本語音声合成エンジン
 *
 * ・記号列（ローマ字）は配列に展開せず、話しながら少しずつ読む
 * ・1 モーラずつ音素の区間（最大 6 個）を作り、4ms ごとに目標値へ 1 次の追従で近づける
 * ・合成器はカスケード 3 共振器（F1〜F3）＋摩擦用 1 共振器。16bit の整数演算
 * SPDX-License-Identifier: MIT
 */
#include <string.h>
#include "ctn_internal.h"

/* ---- 音素 ---------------------------------------------------------------- */
enum { C_NONE = 0, C_K, C_G, C_S, C_SH, C_Z, C_J, C_T, C_D, C_TS, C_CH, C_N, C_H, C_HY, C_F, C_B, C_P, C_M, C_R };
enum { G_NONE = 0, G_Y, G_W };
enum { V_NONE = 0, V_A, V_I, V_U, V_E, V_O };
enum { M_NORMAL = 0, M_N, M_Q, M_LONG };
enum { PE_NONE = 0, PE_COMMA, PE_PERIOD, PE_QUESTION };
enum { S_SIL = 0, S_VOWEL, S_DEVV, S_CLOS, S_VBAR, S_BURST, S_ASP, S_FRIC, S_VFRIC, S_NASAL, S_FLAP, S_GLIDE };
enum { T_END = 0, T_MORA, T_N, T_Q, T_LONG, T_ACC, T_SEP, T_DEV, T_SKIP, T_ERR };

typedef struct { uint8_t type, cons, glide, vowel; } Mora;

/* ローマ字表: 3 文字までの綴り, 子音 | 半母音 << 5, 母音 */
typedef struct { char s[4]; uint8_t cg, v; } Roma;
#define R(s, c, g, v) { s, (uint8_t)((c) | ((g) << 5)), v }
static const Roma ROMA[] CTN_ROM = {
    R("a",C_NONE,0,V_A), R("i",C_NONE,0,V_I), R("u",C_NONE,0,V_U), R("e",C_NONE,0,V_E), R("o",C_NONE,0,V_O),
    R("ka",C_K,0,V_A), R("ki",C_K,0,V_I), R("ku",C_K,0,V_U), R("ke",C_K,0,V_E), R("ko",C_K,0,V_O),
    R("kya",C_K,G_Y,V_A), R("kyu",C_K,G_Y,V_U), R("kyo",C_K,G_Y,V_O),
    R("ga",C_G,0,V_A), R("gi",C_G,0,V_I), R("gu",C_G,0,V_U), R("ge",C_G,0,V_E), R("go",C_G,0,V_O),
    R("gya",C_G,G_Y,V_A), R("gyu",C_G,G_Y,V_U), R("gyo",C_G,G_Y,V_O),
    R("sa",C_S,0,V_A), R("si",C_SH,0,V_I), R("su",C_S,0,V_U), R("se",C_S,0,V_E), R("so",C_S,0,V_O),
    R("shi",C_SH,0,V_I), R("sha",C_SH,0,V_A), R("shu",C_SH,0,V_U), R("sho",C_SH,0,V_O), R("she",C_SH,0,V_E),
    R("sya",C_SH,0,V_A), R("syu",C_SH,0,V_U), R("syo",C_SH,0,V_O),
    R("za",C_Z,0,V_A), R("zi",C_J,0,V_I), R("zu",C_Z,0,V_U), R("ze",C_Z,0,V_E), R("zo",C_Z,0,V_O),
    R("ji",C_J,0,V_I), R("ja",C_J,0,V_A), R("ju",C_J,0,V_U), R("jo",C_J,0,V_O), R("je",C_J,0,V_E),
    R("jya",C_J,0,V_A), R("jyu",C_J,0,V_U), R("jyo",C_J,0,V_O),
    R("ta",C_T,0,V_A), R("ti",C_CH,0,V_I), R("tu",C_TS,0,V_U), R("te",C_T,0,V_E), R("to",C_T,0,V_O),
    R("chi",C_CH,0,V_I), R("cha",C_CH,0,V_A), R("chu",C_CH,0,V_U), R("cho",C_CH,0,V_O), R("che",C_CH,0,V_E),
    R("tya",C_CH,0,V_A), R("tyu",C_CH,0,V_U), R("tyo",C_CH,0,V_O),
    R("tsu",C_TS,0,V_U), R("tsa",C_TS,0,V_A), R("tsi",C_TS,0,V_I), R("tse",C_TS,0,V_E), R("tso",C_TS,0,V_O),
    R("thi",C_T,0,V_I), R("twu",C_T,0,V_U),
    R("da",C_D,0,V_A), R("di",C_J,0,V_I), R("du",C_Z,0,V_U), R("de",C_D,0,V_E), R("do",C_D,0,V_O),
    R("dhi",C_D,0,V_I), R("dwu",C_D,0,V_U),
    R("na",C_N,0,V_A), R("ni",C_N,0,V_I), R("nu",C_N,0,V_U), R("ne",C_N,0,V_E), R("no",C_N,0,V_O),
    R("nya",C_N,G_Y,V_A), R("nyu",C_N,G_Y,V_U), R("nyo",C_N,G_Y,V_O),
    R("ha",C_H,0,V_A), R("hi",C_HY,0,V_I), R("hu",C_F,0,V_U), R("he",C_H,0,V_E), R("ho",C_H,0,V_O),
    R("hya",C_HY,0,V_A), R("hyu",C_HY,0,V_U), R("hyo",C_HY,0,V_O),
    R("fu",C_F,0,V_U), R("fa",C_F,0,V_A), R("fi",C_F,0,V_I), R("fe",C_F,0,V_E), R("fo",C_F,0,V_O), R("fyu",C_F,G_Y,V_U),
    R("ba",C_B,0,V_A), R("bi",C_B,0,V_I), R("bu",C_B,0,V_U), R("be",C_B,0,V_E), R("bo",C_B,0,V_O),
    R("bya",C_B,G_Y,V_A), R("byu",C_B,G_Y,V_U), R("byo",C_B,G_Y,V_O),
    R("va",C_B,0,V_A), R("vi",C_B,0,V_I), R("vu",C_B,0,V_U), R("ve",C_B,0,V_E), R("vo",C_B,0,V_O),
    R("pa",C_P,0,V_A), R("pi",C_P,0,V_I), R("pu",C_P,0,V_U), R("pe",C_P,0,V_E), R("po",C_P,0,V_O),
    R("pya",C_P,G_Y,V_A), R("pyu",C_P,G_Y,V_U), R("pyo",C_P,G_Y,V_O),
    R("ma",C_M,0,V_A), R("mi",C_M,0,V_I), R("mu",C_M,0,V_U), R("me",C_M,0,V_E), R("mo",C_M,0,V_O),
    R("mya",C_M,G_Y,V_A), R("myu",C_M,G_Y,V_U), R("myo",C_M,G_Y,V_O),
    R("ya",C_NONE,G_Y,V_A), R("yu",C_NONE,G_Y,V_U), R("yo",C_NONE,G_Y,V_O), R("ye",C_NONE,G_Y,V_E),
    R("ra",C_R,0,V_A), R("ri",C_R,0,V_I), R("ru",C_R,0,V_U), R("re",C_R,0,V_E), R("ro",C_R,0,V_O),
    R("rya",C_R,G_Y,V_A), R("ryu",C_R,G_Y,V_U), R("ryo",C_R,G_Y,V_O),
    R("wa",C_NONE,G_W,V_A), R("wo",C_NONE,0,V_O), R("wi",C_NONE,G_W,V_I), R("we",C_NONE,G_W,V_E),
};
#define N_ROMA (sizeof ROMA / sizeof ROMA[0])

/* 母音のフォルマント（成人男性）: F1 F2 F3 B1 B2 B3 */
static const int16_t VF[6][6] CTN_ROM = {
    { 500, 1500, 2500, 80, 100, 140 },
    { 750, 1250, 2600, 90, 100, 140 },
    { 290, 2250, 3000, 60, 100, 150 },
    { 340, 1450, 2350, 70, 100, 140 },
    { 480, 1950, 2600, 70, 100, 150 },
    { 480,  850, 2550, 80,  90, 140 },
};

/* ---- 状態（静的に 1 つ） --------------------------------------------------- */
#define SQ 6
static struct {
    const char* p;
    const CtnVoice* vp;
    uint8_t busy, speed, done_frames, finished;
    int16_t err_pos;
    /* 句 */
    uint8_t ph_n, ph_acc, ph_end, ph_idx, prev_vowel, prev_dev, dev_next, sent_start;
    uint8_t steps;          /* 句を下げる回数（ダウンステップ） */
    uint8_t f0_set;         /* 最初のモーラで声の高さを決めた */
    uint16_t sent_frames;
    /* 区間の待ち行列 */
    uint8_t sq_kind[SQ], sq_cons[SQ], sq_vow[SQ], sq_aux[SQ], sq_len[SQ];
    uint8_t sq_n, sq_i, left;
    uint16_t f0_tgt, f0_cur;   /* Hz ×16 */
    uint8_t f0_rate;
    /* 目標値と現在値 */
    int16_t tf[3], cf[3], bw[3];
    uint8_t tav, tah, taf, cav, cah, caf, fric, frate, arate;
    /* 合成器 */
    int16_t ra[3], rb[3], rc[3], y1[3], y2[3];
    int16_t fy1, fy2, prev;
    uint16_t phase, inc, base_inc, lfsr;
    uint8_t sub;
} g;

/* ---- 字句 ------------------------------------------------------------------- */
static char lc(char c) { return (char)(c >= 'A' && c <= 'Z' ? c + 32 : c); }
static uint8_t is_v(char c) { c = lc(c); return c == 'a' || c == 'i' || c == 'u' || c == 'e' || c == 'o'; }

/* p から 1 単位を読み、使った文字数を返す */
static uint8_t tok(const char* p, Mora* m, uint8_t* type, uint8_t* sep)
{
    char c = *p, c0 = lc(c), c1, c2;
    uint8_t i, k, best = 0;
    m->type = M_NORMAL; m->cons = 0; m->glide = 0; m->vowel = 0;
    *sep = PE_NONE;
    if (!c) { *type = T_END; return 0; }
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { *type = T_SKIP; return 1; }
    if (c == '\'') { *type = T_ACC; return 1; }
    if (c == '_') { *type = T_DEV; return 1; }
    if (c == '-') { *type = T_LONG; return 1; }
    if (c == '/') { *type = T_SEP; return 1; }
    if (c == ',') { *type = T_SEP; *sep = PE_COMMA; return 1; }
    if (c == '.' || c == '!') { *type = T_SEP; *sep = PE_PERIOD; return 1; }
    if (c == '?') { *type = T_SEP; *sep = PE_QUESTION; return 1; }
    if (c == 'N') { *type = T_N; return 1; }                       /* 大文字の N は常に「ん」 */
    if (c0 == 'q') { *type = T_Q; return 1; }
    c1 = lc(p[1]);
    c2 = c1 ? lc(p[2]) : 0;
    if (c0 == 'n' && !is_v(c1) && c1 != 'y') {
        *type = T_N;
        return (uint8_t)(c1 == 'n' && !is_v(c2) && c2 != 'y' ? 2 : 1);   /* nn → ん */
    }
    if (c0 >= 'a' && c0 <= 'z' && !is_v(c0) && c0 != 'n' && (c1 == c0 || (c0 == 't' && c1 == 'c' && c2 == 'h'))) {
        *type = T_Q;                                                  /* 子音を重ねる → っ */
        return 1;
    }
    for (i = 0; i < N_ROMA; i++) {
        const Roma* r = &ROMA[i];
        uint8_t len = 0;
        for (k = 0; k < 3; k++) {
            char rc = (char)RD8(&r->s[k]);
            if (!rc) break;
            if (lc(p[k]) != rc) { len = 0; break; }
            len = (uint8_t)(k + 1);
        }
        if (k == 3 || !RD8(&r->s[k])) {
            if (len > best) {
                uint8_t cg = RD8(&r->cg);
                best = len;
                m->cons = cg & 31;
                m->glide = cg >> 5;
                m->vowel = RD8(&r->v);
            }
        }
    }
    *type = best ? T_MORA : T_ERR;
    return best;
}

/* 句（次の区切りまで）のモーラ数・アクセント核・終わり方 */
static void scan_phrase(const char* p, uint8_t* n, uint8_t* acc, uint8_t* end)
{
    Mora m;
    uint8_t t, sep, len;
    *n = 0; *acc = 0; *end = PE_PERIOD;
    for (;;) {
        len = tok(p, &m, &t, &sep);
        if (t == T_END || t == T_ERR) return;
        if (t == T_SEP) { *end = sep; return; }
        if (t == T_MORA || t == T_N || t == T_Q || t == T_LONG) (*n)++;
        if (t == T_ACC) *acc = *n;
        p += len;
    }
}

/* 記号列全体を検査する（カナ記号列と同じ規則） */
static int8_t validate(const char* s)
{
    Mora m;
    const char* p = s;
    uint8_t t, sep, len, cur = 0, acc = 0, dev = 0, last = 0xFF, total = 0;
    const char* dev_at = 0;
    for (;;) {
        len = tok(p, &m, &t, &sep);
        g.err_pos = (int16_t)(p - s);
        if (t == T_END) break;
        if (t == T_ERR) return CT_ERR_CHAR;
        if (t == T_ACC) { if (!cur || acc) return CT_ERR_ACCENT; acc = 1; }
        else if (t == T_SEP) {
            if (dev) { g.err_pos = (int16_t)(dev_at - s); return CT_ERR_DEVOICE; }
            if (sep != PE_NONE || cur) { cur = 0; acc = 0; last = 0xFF; }
        } else if (t == T_DEV) {
            if (dev) return CT_ERR_DEVOICE;
            dev = 1; dev_at = p;
        } else if (t != T_SKIP) {
            if (t == T_LONG && (!cur || last == T_Q)) return CT_ERR_LONG;
            if (dev && t != T_MORA) { g.err_pos = (int16_t)(dev_at - s); return CT_ERR_DEVOICE; }
            dev = 0;
            cur++; total++;
            last = t;
            if (cur > 120 || total == 255) return CT_ERR_TOOLONG;
        }
        p += len;
    }
    if (dev) { g.err_pos = (int16_t)(dev_at - s); return CT_ERR_DEVOICE; }
    if (!total) { g.err_pos = 0; return CT_ERR_EMPTY; }
    g.err_pos = -1;
    return CT_OK;
}

/* ---- 韻律 ------------------------------------------------------------------- */
static uint8_t voiceless(uint8_t c)
{
    return c == C_K || c == C_S || c == C_SH || c == C_T || c == C_CH || c == C_TS ||
           c == C_H || c == C_HY || c == C_F || c == C_P;
}
static uint8_t place(uint8_t c)   /* 0 なし 1 唇 2 歯茎 3 硬口蓋 4 軟口蓋 5 声門 */
{
    switch (c) {
    case C_B: case C_P: case C_M: case C_F: return 1;
    case C_S: case C_Z: case C_T: case C_D: case C_TS: case C_N: case C_R: return 2;
    case C_SH: case C_J: case C_CH: case C_HY: return 3;
    case C_K: case C_G: return 4;
    case C_H: return 5;
    default: return 0;
    }
}

static uint8_t frames(uint8_t f)   /* 話速で長さを変える */
{
    uint16_t x = (uint16_t)((f * 100u + g.speed / 2) / g.speed);
    return (uint8_t)(x < 1 ? 1 : x > 255 ? 255 : x);
}

static void push(uint8_t kind, uint8_t cons, uint8_t vow, uint8_t aux, uint8_t len)
{
    if (g.sq_n >= SQ) return;
    g.sq_kind[g.sq_n] = kind; g.sq_cons[g.sq_n] = cons; g.sq_vow[g.sq_n] = vow;
    g.sq_aux[g.sq_n] = aux; g.sq_len[g.sq_n] = frames(len);
    g.sq_n++;
}

/* 次のモーラ（区切り / をまたいで先読み。それ以外の区切りや終わりでは型 0xFF） */
static void peek(const char* p, Mora* m, uint8_t* type)
{
    uint8_t t, sep, len;
    for (;;) {
        len = tok(p, m, &t, &sep);
        if (t == T_MORA || t == T_N || t == T_Q || t == T_LONG) { *type = t; return; }
        if (t == T_END || t == T_ERR || (t == T_SEP && sep != PE_NONE)) { *type = 0xFF; return; }
        p += len;
    }
}

static uint16_t f0_tone(uint8_t high, uint8_t idx)
{
    const CtnVoice* v = g.vp;
    uint32_t f = (uint32_t)RD16(&v->f0) * 16u, r = RD8(&v->range);
    uint32_t down = (uint32_t)g.steps * 8u * r / 64u + (uint32_t)g.sent_frames / 25u;  /* % */
    if (down > 30) down = 30;
    if (high) f = f * (100u + 26u * r / 64u) / 100u;
    else if (idx > 1) f = f * (100u - 8u * r / 64u) / 100u;
    return (uint16_t)(f * (100u - down) / 100u);
}

/* 次のモーラを読んで区間を作る。読むものがなければ 0 */
static uint8_t next_mora(void)
{
    Mora m, nx;
    uint8_t t, sep, len, nxt, ci, cv, k, idx, last, high, cms, vlen, force_dev = 0;
    g.sq_n = 0;
    g.sq_i = 0;
    /* 区切りや記号を飛ばしてモーラを探す */
    for (;;) {
        len = tok(g.p, &m, &t, &sep);
        if (t == T_END) return 0;
        g.p += len;
        if (t == T_DEV) { force_dev = 1; continue; }
        if (t == T_SEP) {
            if (sep == PE_NONE) continue;
            continue;
        }
        if (t == T_MORA || t == T_N || t == T_Q || t == T_LONG) break;
    }
    if (g.ph_idx >= g.ph_n) {                       /* 新しい句 */
        scan_phrase(g.p - len, &g.ph_n, &g.ph_acc, &g.ph_end);
        g.ph_idx = 0;
    }
    idx = ++g.ph_idx;
    last = idx == g.ph_n;
    peek(g.p, &nx, &nxt);
    if (last && g.ph_end != PE_NONE) nxt = 0xFF;

    /* 声の高さの目標（東京方言: 1 型は 1 モーラ目だけ高い。それ以外は 2 モーラ目から核まで高い） */
    high = g.ph_acc == 1 ? idx == 1 : (idx >= 2 && (g.ph_acc == 0 || idx <= g.ph_acc));
    g.f0_tgt = f0_tone(high, idx);
    g.f0_rate = 56;
    if (!g.f0_set) { g.f0_cur = g.f0_tgt; g.f0_set = 1; }   /* 話し始めは目標の高さから */
    if (last && g.ph_end == PE_PERIOD) g.f0_tgt = (uint16_t)((uint32_t)g.f0_tgt * 94u / 100u);
    if (last && g.ph_end == PE_QUESTION) {
        g.f0_tgt = (uint16_t)((uint32_t)RD16(&g.vp->f0) * 16u * (100u + 45u * RD8(&g.vp->range) / 64u) / 100u);
        g.f0_rate = 40;
    }

    if (t == T_LONG) { m.type = M_LONG; m.vowel = g.prev_vowel; }
    else if (t == T_N) m.type = M_N;
    else if (t == T_Q) m.type = M_Q;

    ci = place(m.cons);
    cv = m.glide == G_Y ? V_I : m.glide == G_W ? V_U : m.vowel;
    if (m.type == M_NORMAL) {
        /* 無声化（無声子音に挟まれた i/u、文末の す・つ。アクセント核と連続は除く） */
        uint8_t dev = force_dev;
        if (!dev && (m.vowel == V_I || m.vowel == V_U) && voiceless(m.cons) && !m.glide && !g.prev_dev &&
            g.ph_acc != idx) {
            if (nxt != 0xFF) dev = (nxt == T_MORA && voiceless(nx.cons)) || nxt == T_Q;
            else if (last && g.ph_end == PE_PERIOD && m.vowel == V_U && (m.cons == C_S || m.cons == C_TS)) dev = 1;
        }
        g.prev_dev = dev;
        cms = 0;
        switch (m.cons) {
        case C_K: case C_T: case C_P:
            push(S_CLOS, m.cons, cv, 0, g.prev_vowel == 0xFE ? 4 : 8);
            push(S_BURST, m.cons, cv, 0, ci == 4 ? 2 : 1);
            k = ci == 1 ? 6 : ci == 2 ? 5 : 8;
            if (m.glide) k = (uint8_t)(k * 3 / 5);
            push(S_ASP, m.cons, cv, 0, dev ? (uint8_t)(k + 1) : k);
            cms = 9 + k;
            break;
        case C_G: case C_D: case C_B:
            push(S_VBAR, m.cons, cv, 0, 8);
            push(S_BURST, m.cons, cv, 0, ci == 4 ? 3 : 1);
            cms = 10;
            break;
        case C_TS: case C_CH:
            push(S_CLOS, m.cons, cv, 0, 9);
            push(S_BURST, m.cons, cv, 0, 1);
            push(S_FRIC, m.cons, cv, 0, m.cons == C_TS ? 10 : 11);
            cms = 21;
            break;
        case C_Z: case C_J:
            push(S_VBAR, m.cons, cv, 0, 6);
            push(S_BURST, m.cons, cv, 0, 1);
            push(S_VFRIC, m.cons, cv, 0, 11);
            cms = 18;
            break;
        case C_S: case C_SH: case C_HY: case C_F: case C_H:
            k = m.cons == C_S ? 24 : m.cons == C_SH ? 26 : m.cons == C_HY ? 20 : 19;
            push(S_FRIC, m.cons, cv, 0, dev ? (uint8_t)(k + 3) : k);
            cms = k;
            break;
        case C_N: case C_M:
            push(S_NASAL, m.cons, cv, m.cons == C_M ? 1 : 2, 16);
            cms = 16;
            break;
        case C_R:
            push(S_FLAP, m.cons, cv, 0, 5);
            cms = 5;
            break;
        default:
            break;
        }
        if (m.glide) { push(S_GLIDE, m.cons, m.vowel, m.glide, m.cons ? 9 : 12); cms += m.cons ? 9 : 12; }
        if (dev) {
            push(S_DEVV, m.cons, m.vowel, 0, 11);
        } else {
            vlen = (uint8_t)(cms > 34 ? 14 : 31 - cms / 2);
            if (vlen < 14) vlen = 14;
            vlen = (uint8_t)(vlen * (m.vowel == V_A ? 105u : m.vowel == V_I ? 90u : m.vowel == V_U ? 88u : 100u) / 100u);
            if (last) vlen = (uint8_t)(vlen * (g.ph_end == PE_NONE ? 106u : g.ph_end == PE_COMMA ? 120u : 135u) / 100u);
            if (g.ph_acc == idx) vlen = (uint8_t)(vlen * 108u / 100u);
            push(S_VOWEL, 0, m.vowel, 0, vlen);
        }
        g.prev_vowel = m.vowel;
    } else if (m.type == M_N) {
        uint8_t np = 3;                              /* 1 m, 2 n, 3 ng/語末 */
        if (nxt == T_MORA) {
            uint8_t pl = place(nx.cons);
            if (pl == 1 && nx.cons != C_F) np = 1;
            else if ((pl == 2 || pl == 3) && nx.cons != C_S && nx.cons != C_SH && nx.cons != C_HY) np = 2;
        }
        push(S_NASAL, 0, g.prev_vowel, np, nxt == 0xFF ? 26 : 24);
        g.prev_dev = 0;
    } else if (m.type == M_LONG) {
        push(S_VOWEL, 0, m.vowel, 0, last ? 32 : 25);
        g.prev_dev = 0;
    } else {                                         /* っ */
        uint8_t nc = nxt == T_MORA ? nx.cons : 0;
        if (nc == C_K || nc == C_T || nc == C_P || nc == C_TS || nc == C_CH || nc == C_G || nc == C_D || nc == C_B)
            push(S_CLOS, nc, nx.vowel, 0, 21);
        else if (nc == C_S || nc == C_SH || nc == C_HY || nc == C_F || nc == C_H)
            push(S_FRIC, nc, nx.vowel, 0, 18);
        else
            push(S_SIL, 0, g.prev_vowel, 0, 15);
        g.prev_dev = 0;
        g.prev_vowel = 0xFE;                         /* 次の閉鎖を短くする目印 */
        return 1;
    }
    if (g.prev_vowel == 0xFE) g.prev_vowel = m.vowel;

    /* 句の終わり */
    if (last) {
        if (g.ph_acc > 0) g.steps++;
        if (g.ph_end == PE_COMMA) { push(S_SIL, 0, m.vowel, 0, 55); g.steps /= 2; }
        else if (g.ph_end != PE_NONE) { push(S_SIL, 0, m.vowel, 0, 105); g.steps = 0; g.sent_frames = 0; }
    }
    return 1;
}

/* ---- 目標値 ----------------------------------------------------------------- */
static int16_t mix(int16_t base, int16_t pct, int16_t x) { return (int16_t)(base + (int32_t)pct * (x - base) / 100); }

static void targets(uint8_t kind, uint8_t cons, uint8_t vow, uint8_t aux)
{
    const int16_t* t = VF[vow <= V_O ? vow : 0];
    int16_t f1 = RD16(&t[0]), f2 = RD16(&t[1]), f3 = RD16(&t[2]);
    uint8_t pl = place(cons), k;
    uint16_t fs = (uint16_t)RD16(&g.vp->fscale);
    uint8_t vdb = vow == V_I || vow == V_U ? 200 : vow == V_A ? 200 : 195;

    g.bw[0] = RD16(&t[3]); g.bw[1] = RD16(&t[4]); g.bw[2] = RD16(&t[5]);
    if (fs > 256) g.bw[0] = (int16_t)(g.bw[0] + ((fs - 256u) * 130u >> 8));   /* 小柄な声ほど B1 を広く（V1 と同じ） */
    g.tav = 0; g.tah = 0; g.taf = 0; g.fric = 0xFF; g.frate = 64; g.arate = 128;

    /* 子音: 調音位置のローカス */
    if (kind != S_VOWEL && kind != S_SIL && kind != S_DEVV && kind != S_GLIDE && pl && pl != 5) {
        if (pl == 1)      { f2 = mix(800, 45, f2);  f3 = mix(2200, 50, f3); }
        else if (pl == 2) { f2 = mix(1700, 45, f2); f3 = mix(2650, 40, f3); }
        else if (pl == 3) { f2 = mix(2150, 30, f2); f3 = mix(2900, 30, f3); }
        else              { if (vow == V_I || vow == V_E) { f2 = 2300; f3 = 2700; }
                            else { f2 = f2 + 400 < 1400 ? 1400 : f2 + 400; f3 = f2 + 450; } }
        if (f3 < f2 + 250) f3 = f2 + 250;
        f1 = 260;
    }
    switch (kind) {
    case S_VOWEL: g.tav = vdb; g.tah = RD8(&g.vp->breath); g.frate = 56; break;
    case S_DEVV:  g.tah = 50; g.bw[0] = (int16_t)(g.bw[0] * 5 / 2);
                  if (cons == C_S || cons == C_TS) { g.fric = FR_S; g.taf = 30; }
                  else if (cons == C_SH || cons == C_CH || cons == C_HY) { g.fric = FR_SH; g.taf = 30; }
                  break;
    case S_CLOS:  g.frate = 160; g.arate = 255; break;
    case S_VBAR:  g.tav = 50; f1 = 200; g.bw[0] = 80; g.frate = 160; break;
    case S_BURST: g.arate = 255;
                  g.fric = pl == 1 ? FR_LAB : pl == 2 ? FR_ALV : (vow == V_I || vow == V_E) ? FR_VELF : FR_VELB;
                  if (cons == C_TS || cons == C_Z) g.fric = FR_S;
                  if (cons == C_CH || cons == C_J) g.fric = FR_SH;
                  g.taf = pl == 1 ? 126 : pl == 2 ? 159 : 255;
                  break;
    case S_ASP:   g.tah = 100; g.bw[0] = (int16_t)(g.bw[0] * 2);
                  if (pl == 4) { g.fric = (vow == V_I || vow == V_E) ? FR_VELF : FR_VELB; g.taf = 63; }
                  if (pl == 2) { g.fric = FR_ALV; g.taf = 32; }
                  break;
    case S_FRIC: case S_VFRIC:
        if (cons == C_S || cons == C_TS || cons == C_Z) { g.fric = FR_S; g.taf = 100; }
        else if (cons == C_SH || cons == C_CH || cons == C_J) { g.fric = FR_SH; g.taf = 89; }
        else if (cons == C_HY) { g.fric = FR_HY; g.taf = 32; g.tah = 40; }
        else if (cons == C_F) { g.fric = FR_F; g.taf = 40; g.tah = 63; }
        else { g.tah = 45; g.bw[0] = (int16_t)(g.bw[0] * 2); }   /* h */
        if (kind == S_VFRIC) { g.taf = (uint8_t)(g.taf * 5 / 8); g.tav = 63; f1 = 250; }
        g.arate = 96;
        break;
    case S_NASAL:
        g.tav = 150; f1 = 250; g.bw[0] = 100; g.bw[1] = 300; g.bw[2] = 300; g.frate = 160;
        if (aux == 1)      { f2 = mix(800, 45, RD16(&t[1])); f3 = 2130; }
        else if (aux == 2) { f2 = vow == V_I ? mix(2150, 30, RD16(&t[1])) : mix(1700, 45, RD16(&t[1])); f3 = 2600; }
        else               { f2 = 1250; f3 = 2500; }
        break;
    case S_FLAP:  g.tav = 70; f1 = 230; g.bw[0] = 100; g.frate = 128; break;
    case S_GLIDE: g.tav = 126; g.frate = 48;
                  if (aux == G_Y) { f1 = 260; f2 = 2200; f3 = 3000; } else { f1 = 320; f2 = 780; f3 = 2300; }
                  break;
    default:      g.arate = 64; break;                         /* 無音: ゆっくり減衰 */
    }
    g.tf[0] = f1; g.tf[1] = f2; g.tf[2] = f3;
    for (k = 0; k < 3; k++) {
        int32_t f = ((int32_t)g.tf[k] * fs) >> 8;
        g.tf[k] = (int16_t)(f > 3600 ? 3600 : f);
    }
}

/* ---- フレーム ----------------------------------------------------------------- */
static void coef(uint8_t k)
{
    int16_t f = g.cf[k], bw = g.bw[k];
    uint16_t fi = (uint16_t)(((uint32_t)f * 1049u) >> 15);   /* f / 31.25 */
    uint8_t bi = (uint8_t)((bw + 8) >> 4);
    int32_t e, cs;
    if (fi > 128) fi = 128;
    if (bi > 64) bi = 64;
    e = RD16(&CTN_EXP[bi]);
    cs = RD16(&CTN_COS[fi]);
    /* 係数は Q13（a は 4 近くまでになるので Q14 では 16bit を超える） */
    g.rc[k] = (int16_t)(-((e * e) >> 15));
    g.rb[k] = (int16_t)((e * cs) >> 14);
    {
        int32_t a = 8192L - g.rb[k] - g.rc[k];
        g.ra[k] = (int16_t)(a > 32767 ? 32767 : a);
    }
}

static uint8_t approach8(uint8_t c, uint8_t t, uint8_t rate)
{
    int16_t d = (int16_t)t - c;
    if (rate == 255) return t;
    return (uint8_t)(c + (int16_t)(((int32_t)d * rate + (d > 0 ? 255 : 0)) >> 8));
}

/* 4ms ごと。話し終わったら 0 */
static uint8_t frame(void)
{
    uint8_t k;
    while (!g.left) {
        if (g.sq_i < g.sq_n) {
            targets(g.sq_kind[g.sq_i], g.sq_cons[g.sq_i], g.sq_vow[g.sq_i], g.sq_aux[g.sq_i]);
            g.left = g.sq_len[g.sq_i];
            g.sq_i++;
        } else if (g.finished) {
            if (!g.done_frames) return 0;
            g.done_frames--;
            targets(S_SIL, 0, V_A, 0);
            g.left = 1;
        } else if (!next_mora()) {
            g.finished = 1;
            g.done_frames = 4;                       /* 減衰を終わらせる 16ms */
        }
    }
    g.left--;
    g.sent_frames++;

    for (k = 0; k < 3; k++) g.cf[k] = (int16_t)(g.cf[k] + (((int32_t)(g.tf[k] - g.cf[k]) * g.frate) >> 8));
    g.cav = approach8(g.cav, g.tav, g.arate);
    g.cah = approach8(g.cah, g.tah, g.arate);
    g.caf = approach8(g.caf, g.taf, g.arate == 255 ? 255 : 192);
    g.f0_cur = (uint16_t)(g.f0_cur + (((int32_t)g.f0_tgt - g.f0_cur) * g.f0_rate >> 8));
    g.base_inc = (uint16_t)(((uint32_t)g.f0_cur * 8389u) >> 14);    /* f0 × 65536 / 8000 */
    for (k = 0; k < 3; k++) coef(k);
    return 1;
}

/* ---- 公開 API ----------------------------------------------------------------- */
int8_t ctn_start(const char* koe, uint8_t voice, uint8_t speed)
{
    int8_t err;
    memset(&g, 0, sizeof g);
    g.err_pos = -1;
    if (!koe) return CT_ERR_NULL;
    if (voice >= CTN_VOICE_COUNT || speed < 50 || speed > 200) return CT_ERR_PARAM;
    err = validate(koe);
    if (err) return err;
    g.p = koe;
    g.vp = &CTN_VOICES[voice];
    g.speed = speed;
    g.prev_vowel = V_A;
    g.lfsr = 0xACE1u;
    g.f0_cur = g.f0_tgt = (uint16_t)(RD16(&g.vp->f0) * 16);
    g.cf[0] = 500; g.cf[1] = 1500; g.cf[2] = 2500;
    push(S_SIL, 0, V_A, 0, 15);                     /* 先頭の無音 60ms */
    g.busy = 1;
    return CT_OK;
}

static int16_t noise(void)
{
    uint16_t s = g.lfsr;
    int16_t a, b;
    s = (uint16_t)((s >> 1) ^ (uint16_t)(-(int16_t)(s & 1u) & 0xB400u)); a = (int8_t)(s & 0xFF);
    s = (uint16_t)((s >> 1) ^ (uint16_t)(-(int16_t)(s & 1u) & 0xB400u)); b = (int8_t)(s >> 8);
    g.lfsr = s;
    return (int16_t)((a + b) * 2);                  /* ±512 */
}

static int16_t res(uint8_t k, int16_t x)
{
    int32_t y = ((int32_t)g.ra[k] * x + (int32_t)g.rb[k] * g.y1[k] + (int32_t)g.rc[k] * g.y2[k]) >> 13;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    g.y2[k] = g.y1[k];
    g.y1[k] = (int16_t)y;
    return (int16_t)y;
}

int16_t ctn_sample(void)
{
    int16_t x, n, out;
    int32_t y, p = 0;
    uint16_t prev;
    uint8_t closed, pv;

    if (!g.busy) return 0;
    if (g.sub == 0 && !frame()) { g.busy = 0; return 0; }
    g.sub = (uint8_t)((g.sub + 1) & (CTN_FRAME - 1));

    /* 声帯音源 */
    if (!g.inc) g.inc = g.base_inc;
    prev = g.phase;
    g.phase = (uint16_t)(g.phase + g.inc);
    if (g.phase < prev) {
        uint8_t j = RD8(&g.vp->jitter);
        g.inc = (uint16_t)(g.base_inc + (int16_t)(((int32_t)g.base_inc * j * noise()) >> 17));
    }
    {   /* 声門体積流の表を線形補間で引く（周期ごとの形のばらつきを抑える） */
        uint8_t i0 = (uint8_t)(g.phase >> 11), fr = (uint8_t)(g.phase >> 3);
        uint8_t a0 = RD8(&g.vp->pulse[i0]), a1 = i0 < CTN_PULSE - 1 ? RD8(&g.vp->pulse[i0 + 1]) : 0;
        pv = (uint8_t)(a0 + (int16_t)(((int16_t)a1 - a0) * fr >> 8));
    }
    closed = (uint8_t)((g.phase >> 11) >= RD8(&g.vp->te));
    n = noise();
    x = (int16_t)(((uint16_t)pv * g.cav) >> 7);                             /* 0〜508 */
    {
        int16_t asp = (int16_t)(((int32_t)n * g.cah) >> 9);                 /* 気息音 */
        if (closed && g.cav) asp >>= 1;
        x = (int16_t)(x + asp);
    }
    /* カスケード F3 → F2 → F1 */
    x = res(2, x);
    x = res(1, x);
    x = res(0, x);
    /* 摩擦（パラレル） */
    if (g.fric != 0xFF && g.caf) {
        const int16_t* c = CTN_FRIC[g.fric];
        int16_t fn = closed && g.cav ? (int16_t)(n >> 1) : n;
        int32_t fy = ((int32_t)RD16(&c[0]) * fn + (int32_t)RD16(&c[1]) * g.fy1 + (int32_t)RD16(&c[2]) * g.fy2) >> 13;
        if (fy > 32767) fy = 32767;
        if (fy < -32768) fy = -32768;
        g.fy2 = g.fy1;
        g.fy1 = (int16_t)fy;
        p = (fy * g.caf) >> 9;
    }
    y = (int32_t)x + p;
    /* 放射特性と出力 */
    {
        int32_t r = y - g.prev;
        g.prev = (int16_t)(y > 32767 ? 32767 : y < -32768 ? -32768 : y);
        r = (r * RD8(&g.vp->gain)) >> 1;
        out = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
    return out;
}

uint8_t ctn_sample8(void)
{
    int16_t s = (int16_t)(ctn_sample() >> 7);       /* ±16384 を 8bit の全幅に */
    return (uint8_t)(s > 127 ? 255 : s < -128 ? 0 : s + 128);
}

uint8_t ctn_busy(void) { return g.busy; }
void ctn_stop(void) { g.busy = 0; }
int16_t ctn_error_pos(void) { return g.err_pos; }

