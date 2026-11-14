// Boks: o'yin mantig'i (Android'ga bog'liq emas, Linux'da sinash mumkin)
// Dunyo birliklari: ring markazi (0,0), yarim o'lchami RING_HALF. Ekranda 1 birlik = H/100 piksel.
#ifndef GAME_H
#define GAME_H

#define RING_HALF 40.f

// Qo'l joystigi AVATAR sistemasida: arm[k][0] = yon (o'ng +), arm[k][1] = oldinga (raqib tomon +).
// k=0 chap qo'l, k=1 o'ng qo'l. mvx/mvy dunyo sistemasida (-1..1).
typedef struct { float mvx, mvy; float arm[2][2]; } Input;

typedef struct {
    float x, y, hp, face, flash;
    float fx, fy, qx, qy;       // oldinga va o'ngga birlik vektorlar
    float glove[2][2];          // qo'lqop siljishi yelkaga nisbatan: {oldinga, yon}
    float prel[2][2];           // oldingi kadrdagi dunyo siljishi (tezlik uchun)
    float guard[2];             // himoya darajasi 0..1 (orqa yarim doira)
    float vt[2];                // qo'lqopning raqib tomon tezligi
    float hitcd[2];
    float sx[2], sy[2];         // yelka (dunyo)
    float gx[2], gy[2];         // qo'lqop (dunyo)
} Fighter;

typedef struct { int state, hand, seen, strikes; float timer, cd, block; } AI;

typedef struct { Fighter f[2]; AI ai; float t; int hits[2]; int blocks[2]; float walk_t[2]; int walking[2]; } Game;

void game_reset(Game *g);
void game_step(Game *g, float dt, const Input in[2]);
// 1-jangchini (dushman) boshqaradigan sun'iy intellekt. diff: 0 oddiy, 1 tez, 2 himoya.
void ai_input(Game *g, float dt, int diff, Input *out);
int  ai_blocking(const Game *g, int diff);
// 2 joystikli boshqaruv: faqat qo'l joystiklaridan yurishni chiqaradi (in->mvx/mvy to'ldiriladi,
// yurish paytida qo'llar himoya holatiga o'tkaziladi). Har kadrda game_step'dan oldin chaqiriladi.
void game_two_stick(Game *g, int who, float dt, Input *in);

#endif
