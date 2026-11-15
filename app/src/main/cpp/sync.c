#include "sync.h"
#include <math.h>
#include <string.h>

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static void p16(uint8_t *b, int v) { b[0] = (uint8_t)(v & 255); b[1] = (uint8_t)((v >> 8) & 255); }
static int g16(const uint8_t *b) { return (int16_t)(uint16_t)(b[0] | (b[1] << 8)); }
static int gu16(const uint8_t *b) { return b[0] | (b[1] << 8); }
static int q16(float v, float scale) { return (int)lrintf(clampf(v * scale, -32768.f, 32767.f)); }
static int8_t q8(float v) { return (int8_t)lrintf(clampf(v, -1.f, 1.f) * 127.f); }

int sync_pack_snapshot(uint8_t *b, const Game *g, const MatchInfo *m) {
    b[0] = SYNC_VER; b[1] = (uint8_t)m->mstate; b[2] = (uint8_t)m->rnd; b[3] = (uint8_t)m->pw; b[4] = (uint8_t)m->ew;
    b[5] = (uint8_t)m->lastres; b[6] = (uint8_t)m->finalres;
    b[7] = (uint8_t)((m->isko ? 1 : 0) | (m->ready_host ? 2 : 0) | (m->ready_guest ? 4 : 0) |
                     (g->f[0].flash > 0 ? 8 : 0) | (g->f[1].flash > 0 ? 16 : 0));
    p16(b + 8, (int)lrintf(clampf(m->rtime * 100.f, 0, 65535.f)));
    p16(b + 10, (int)lrintf(clampf(m->stimer * 100.f, 0, 65535.f)));
    int o = 12;
    for (int i = 0; i < 2; i++) {
        const Fighter *f = &g->f[i];
        p16(b + o, q16(f->x, 256.f)); p16(b + o + 2, q16(f->y, 256.f));
        p16(b + o + 4, (int)lrintf(clampf(f->hp, 0, 300.f) * 200.f));
        o += 6;
        for (int k = 0; k < 2; k++) {
            p16(b + o, q16(f->glove[k][0], 256.f)); p16(b + o + 2, q16(f->glove[k][1], 256.f));
            b[o + 4] = (uint8_t)lrintf(clampf(f->guard[k], 0, 1.f) * 255.f);
            o += 5;
        }
    }
    return o;
}

int sync_unpack_snapshot(const uint8_t *b, int len, Game *g, MatchInfo *m) {
    if (len != SYNC_SNAP_LEN || b[0] != SYNC_VER) return 0;
    m->mstate = b[1]; m->rnd = b[2]; m->pw = b[3]; m->ew = b[4]; m->lastres = b[5]; m->finalres = b[6];
    m->isko = b[7] & 1; m->ready_host = (b[7] >> 1) & 1; m->ready_guest = (b[7] >> 2) & 1;
    g->f[0].flash = (b[7] & 8) ? 0.15f : 0.f; g->f[1].flash = (b[7] & 16) ? 0.15f : 0.f;
    m->rtime = gu16(b + 8) / 100.f; m->stimer = gu16(b + 10) / 100.f;
    int o = 12;
    for (int i = 0; i < 2; i++) {
        Fighter *f = &g->f[i];
        f->x = g16(b + o) / 256.f; f->y = g16(b + o + 2) / 256.f; f->hp = gu16(b + o + 4) / 200.f;
        o += 6;
        for (int k = 0; k < 2; k++) {
            f->glove[k][0] = g16(b + o) / 256.f; f->glove[k][1] = g16(b + o + 2) / 256.f;
            f->guard[k] = b[o + 4] / 255.f;
            o += 5;
        }
    }
    return 1;
}

int sync_pack_input(uint8_t *b, const Input *in, int two_stick, int ready) {
    b[0] = SYNC_VER; b[1] = (uint8_t)((two_stick ? 1 : 0) | (ready ? 2 : 0));
    b[2] = (uint8_t)q8(in->mvx); b[3] = (uint8_t)q8(in->mvy);
    b[4] = (uint8_t)q8(in->arm[0][0]); b[5] = (uint8_t)q8(in->arm[0][1]);
    b[6] = (uint8_t)q8(in->arm[1][0]); b[7] = (uint8_t)q8(in->arm[1][1]);
    return SYNC_INPUT_LEN;
}
int sync_unpack_input(const uint8_t *b, int len, Input *in, int *two_stick, int *ready) {
    if (len != SYNC_INPUT_LEN || b[0] != SYNC_VER) return 0;
    *two_stick = b[1] & 1; *ready = (b[1] >> 1) & 1;
    in->mvx = (int8_t)b[2] / 127.f; in->mvy = (int8_t)b[3] / 127.f;
    in->arm[0][0] = (int8_t)b[4] / 127.f; in->arm[0][1] = (int8_t)b[5] / 127.f;
    in->arm[1][0] = (int8_t)b[6] / 127.f; in->arm[1][1] = (int8_t)b[7] / 127.f;
    return 1;
}

void sync_blend(Game *r, const Game *t, float k) {
    k = clampf(k, 0, 1);
    for (int i = 0; i < 2; i++) {
        Fighter *a = &r->f[i]; const Fighter *b = &t->f[i];
        if (fabsf(b->x - a->x) + fabsf(b->y - a->y) > 12.f) { a->x = b->x; a->y = b->y; }   // yangi raund: sakrash
        else { a->x += (b->x - a->x) * k; a->y += (b->y - a->y) * k; }
        a->hp += (b->hp - a->hp) * k;
        a->flash = b->flash;
        for (int j = 0; j < 2; j++) {
            a->glove[j][0] += (b->glove[j][0] - a->glove[j][0]) * k;
            a->glove[j][1] += (b->glove[j][1] - a->glove[j][1]) * k;
            a->guard[j] += (b->guard[j] - a->guard[j]) * k;
        }
    }
}
