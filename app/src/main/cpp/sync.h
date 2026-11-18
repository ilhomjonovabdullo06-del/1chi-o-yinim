// Boks PvP: xost -> mehmon holat (snapshot) va mehmon -> xost kirish (input) paketlari.
// Pack/unpack va yumshatish (blend) shu yerda: Android'ga bog'liq emas, Linux'da sinaladi.
// Xost o'yinni yuritadi (haqiqat manbai); mehmon faqat holatni oladi va chizadi, o'z joystiklarini yuboradi.
#ifndef SYNC_H
#define SYNC_H

#include <stdint.h>
#include "game.h"

#define SYNC_VER 2          // 2: himoya charchog'i (gscale + fatigue bayrog'i)
#define SYNC_K_SNAP 1       // xost -> mehmon, sekundiga ~30 marta
#define SYNC_K_INPUT 2      // mehmon -> xost, har kadrda
#define SYNC_SNAP_LEN 48
#define SYNC_INPUT_LEN 8

typedef struct {
    int mstate;                         // o'yin holati (main.c dagi S_INTRO/S_FIGHT/S_BREAK/S_END)
    int rnd, pw, ew, lastres, finalres, isko;   // xost nuqtai nazaridan
    int ready_host, ready_guest;        // "keyingi raund / qayta o'ynash" tayyorligi
    float rtime, stimer;
} MatchInfo;

int  sync_pack_snapshot(uint8_t *b, const Game *g, const MatchInfo *m);                  // uzunlikni qaytaradi
int  sync_unpack_snapshot(const uint8_t *b, int len, Game *g, MatchInfo *m);             // 1 = yaroqli
int  sync_pack_input(uint8_t *b, const Input *in, int two_stick, int ready);
int  sync_unpack_input(const uint8_t *b, int len, Input *in, int *two_stick, int *ready);
// Mehmon: chiziladigan holat r ni kelgan holat t ga qarab yumshatadi (k = 0..1). Katta sakrash (yangi raund) darhol o'tadi.
void sync_blend(Game *r, const Game *t, float k);

#endif
