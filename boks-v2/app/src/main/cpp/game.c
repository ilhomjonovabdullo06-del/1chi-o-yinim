#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define REST_F 8.f        // qo'lqopning tinch holatdagi oldinga masofasi (yelkadan)
#define REACH 14.f        // old yarim doira: qo'l cho'zilish radiusi (katta)
#define GUARD_R 3.5f      // orqa yarim doira: harakat radiusi (kichik)
#define DEAD 0.12f
#define GLOVE_K 40.f      // qo'lqop joystikka ergashish tezligi
#define VMAX 450.f
#define MOVE_SPEED 30.f
#define BODY_R 5.5f
#define GLOVE_R 2.6f
#define SHOULDER 5.f
#define HIT_V 70.f        // shundan sekin harakat zarba hisoblanmaydi

static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static void frame(Fighter *f, const Fighter *o) {
    float a = atan2f(o->y - f->y, o->x - f->x);
    f->face = a;
    f->fx = cosf(a); f->fy = sinf(a);
    f->qx = -sinf(a); f->qy = cosf(a);
    for (int k = 0; k < 2; k++) {
        float s = k ? SHOULDER : -SHOULDER;
        f->sx[k] = f->x + f->qx * s; f->sy[k] = f->y + f->qy * s;
    }
}
static void rel_vec(const Fighter *f, int k, float *rx, float *ry) {
    *rx = f->fx * f->glove[k][0] + f->qx * f->glove[k][1];
    *ry = f->fy * f->glove[k][0] + f->qy * f->glove[k][1];
}
static void glove_world(Fighter *f) {
    for (int k = 0; k < 2; k++) {
        float rx, ry; rel_vec(f, k, &rx, &ry);
        f->gx[k] = f->sx[k] + rx; f->gy[k] = f->sy[k] + ry;
    }
}

void game_reset(Game *g) {
    memset(g, 0, sizeof(*g));
    g->f[0].x = 0; g->f[0].y = 22;
    g->f[1].x = 0; g->f[1].y = -22;
    for (int i = 0; i < 2; i++) {
        g->f[i].hp = 100;
        for (int k = 0; k < 2; k++) { g->f[i].glove[k][0] = REST_F; g->f[i].glove[k][1] = 0; }
    }
    for (int i = 0; i < 2; i++) {
        frame(&g->f[i], &g->f[1 - i]);
        for (int k = 0; k < 2; k++) rel_vec(&g->f[i], k, &g->f[i].prel[k][0], &g->f[i].prel[k][1]);
        glove_world(&g->f[i]);
    }
    g->ai.cd = 1.5f;
}

// Joystik (yon, oldinga) -> qo'lqopning kerakli joyi (yelkaga nisbatan).
static void arm_target(const float in[2], float *tf, float *ts, float *guard) {
    float side = in[0], fwd = in[1], m = hypotf(side, fwd);
    if (m > 1) { side /= m; fwd /= m; m = 1; }
    *tf = REST_F; *ts = 0; *guard = 0;
    if (m < DEAD) return;
    float ds = side / m, df = fwd / m;
    if (fwd >= 0) {                       // old yarim doira: zarba tomoni
        *tf = REST_F + df * REACH * m; *ts = ds * REACH * m;
    } else {                              // orqa yarim doira: himoya tomoni
        *tf = REST_F + df * GUARD_R * m; *ts = ds * GUARD_R * m; *guard = m;
    }
}

static void resolve_hits(Game *g, float dt) {
    for (int a = 0; a < 2; a++) {
        Fighter *A = &g->f[a], *D = &g->f[1 - a];
        for (int k = 0; k < 2; k++) {
            if (A->hitcd[k] > 0) { A->hitcd[k] -= dt; continue; }
            if (A->vt[k] < HIT_V) continue;
            float s = clampf(A->vt[k] / 200.f, 0, 1);
            int blocked = 0;
            for (int j = 0; j < 2; j++) {
                if (D->guard[j] < 0.25f) continue;        // faqat himoya holatidagi qo'l bloklaydi
                float rg = 1.f + 5.f * D->guard[j];
                if (hypotf(A->gx[k] - D->gx[j], A->gy[k] - D->gy[j]) < rg + GLOVE_R) blocked = 1;
            }
            int body = hypotf(A->gx[k] - D->x, A->gy[k] - D->y) < BODY_R + GLOVE_R;
            if (!blocked && !body) continue;
            if (blocked) {
                D->hp -= 0.2f + 0.8f * s;
                g->blocks[1 - a]++;
            } else {
                D->hp -= 2.f + 6.f * s;
                D->flash = 0.15f;
                float dx = D->x - A->x, dy = D->y - A->y, d = hypotf(dx, dy) + 0.001f;
                D->x += dx / d * 2.f; D->y += dy / d * 2.f;
                g->hits[a]++;
            }
            A->glove[k][0] *= 0.7f;        // qo'lqop biroz qaytadi
            A->hitcd[k] = 0.45f;
        }
    }
}

void game_step(Game *g, float dt, const Input in[2]) {
    g->t += dt;
    // yurish
    for (int i = 0; i < 2; i++) {
        float m = hypotf(in[i].mvx, in[i].mvy), sc = m > 1 ? 1 / m : 1;
        g->f[i].x += in[i].mvx * sc * MOVE_SPEED * dt;
        g->f[i].y += in[i].mvy * sc * MOVE_SPEED * dt;
    }
    // jismlar to'qnashuvi va ring chegarasi
    Fighter *A = &g->f[0], *B = &g->f[1];
    float dx = A->x - B->x, dy = A->y - B->y, d = hypotf(dx, dy) + 0.001f, mind = 12.f;
    if (d < mind) {
        float push = (mind - d) / 2;
        A->x += dx / d * push; A->y += dy / d * push; B->x -= dx / d * push; B->y -= dy / d * push;
    }
    float lim = RING_HALF - 6.f;
    for (int i = 0; i < 2; i++) {
        g->f[i].x = clampf(g->f[i].x, -lim, lim); g->f[i].y = clampf(g->f[i].y, -lim, lim);
        g->f[i].flash -= dt;
    }
    // qo'llar
    for (int i = 0; i < 2; i++) {
        Fighter *f = &g->f[i], *o = &g->f[1 - i];
        frame(f, o);
        for (int k = 0; k < 2; k++) {
            float tf, ts, gd;
            arm_target(in[i].arm[k], &tf, &ts, &gd);
            f->guard[k] = gd;
            float k_ = clampf(GLOVE_K * dt, 0, 1);
            float sf = (tf - f->glove[k][0]) * k_, ss = (ts - f->glove[k][1]) * k_;
            float len = hypotf(sf, ss), mx = VMAX * dt;
            if (len > mx) { sf *= mx / len; ss *= mx / len; }
            f->glove[k][0] += sf; f->glove[k][1] += ss;
        }
        glove_world(f);
    }
    // qo'lqop tezligi (raqib tomon) - zarba kuchi shundan
    for (int i = 0; i < 2; i++) {
        Fighter *f = &g->f[i], *o = &g->f[1 - i];
        for (int k = 0; k < 2; k++) {
            float rx, ry; rel_vec(f, k, &rx, &ry);
            float vx = (rx - f->prel[k][0]) / (dt > 0 ? dt : 1e-3f);
            float vy = (ry - f->prel[k][1]) / (dt > 0 ? dt : 1e-3f);
            f->prel[k][0] = rx; f->prel[k][1] = ry;
            float ux = o->x - f->gx[k], uy = o->y - f->gy[k], ul = hypotf(ux, uy) + 0.001f;
            f->vt[k] = (vx * ux + vy * uy) / ul;
        }
    }
    resolve_hits(g, dt);
}

/* ---------- Sun'iy intellekt: virtual joystiklar bilan boshqaradi ---------- */
int ai_blocking(const Game *g, int diff) {
    return g->ai.block > 0 || (diff == 2 && g->ai.state == 0);
}

void ai_input(Game *g, float dt, int diff, Input *o) {
    AI *a = &g->ai;
    Fighter *e = &g->f[1], *p = &g->f[0];
    memset(o, 0, sizeof(*o));
    float dx = p->x - e->x, dy = p->y - e->y, d = hypotf(dx, dy) + 0.001f;
    float nx = dx / d, ny = dy / d;
    int fast = diff >= 1;
    a->block -= dt; a->cd -= dt;

    // o'yinchi zarba berayotganini sezsa, ba'zan bloklaydi
    float pv = p->vt[0] > p->vt[1] ? p->vt[0] : p->vt[1];
    if (pv > 100 && !a->seen) {
        a->seen = 1;
        if (a->state == 0 && a->block <= 0 && (rand() % 100) < 30) a->block = 0.5f;
    }
    if (pv < 50) a->seen = 0;

    if (a->state == 0) {
        float sp = 0;
        if (d > 26) sp = 16; else if (d < 20) sp = -12;
        float st = sinf(g->t * 1.3f) * 8;
        o->mvx = (nx * sp - ny * st) / MOVE_SPEED;
        o->mvy = (ny * sp + nx * st) / MOVE_SPEED;
        if (a->cd <= 0 && d < 28) { a->state = 1; a->timer = fast ? 0.22f : 0.35f; a->hand = rand() & 1; }
    } else if (a->state == 1) {
        a->timer -= dt;
        if (a->timer <= 0) { a->state = 2; a->timer = fast ? 0.14f : 0.22f; a->strikes++; }
    } else {
        a->timer -= dt;
        if (a->timer <= 0) {
            a->state = 0;
            a->cd = (0.7f + 1.2f * (rand() / (float)RAND_MAX)) * (fast ? 0.44f : 1.f);
        }
    }

    if (a->state == 1) {            // yelka orqaga tortiladi (ogohlantirish)
        o->arm[a->hand][1] = -0.8f;
    } else if (a->state == 2) {     // zarba: joystik oldinga
        o->arm[a->hand][1] = 1.0f;
    } else if (ai_blocking(g, diff)) {   // himoya: ikkala joystik orqa yarimda, ichkariga
        o->arm[0][0] = 0.35f; o->arm[0][1] = -0.95f;
        o->arm[1][0] = -0.35f; o->arm[1][1] = -0.95f;
    }
}

/* ---------- 2 joystikli boshqaruv ---------- */
// Ikkala joystik taxminan bir yo'nalishda (35 gradus ichida) va kamida 70% og'sa, 0.12 s dan keyin
// jangchi shu tomonga yuradi (avatar sistemasida: oldinga = raqib tomon, yon = aylanish).
// Boshlangach biroz yumshoqroq shartlar (hysteresis) ishlaydi, shunda yurish uzilib qolmaydi.
void game_two_stick(Game *g, int who, float dt, Input *in) {
    Fighter *f = &g->f[who];
    float s0 = in->arm[0][0], f0 = in->arm[0][1], s1 = in->arm[1][0], f1 = in->arm[1][1];
    float m0 = hypotf(s0, f0), m1 = hypotf(s1, f1);
    float minm = g->walking[who] ? 0.5f : 0.7f;
    float minc = g->walking[who] ? 0.70f : 0.819f;   // cos 35 gradus = 0.819
    int same = 0;
    if (m0 >= minm && m1 >= minm) same = ((s0 * s1 + f0 * f1) / (m0 * m1)) >= minc;
    if (same) g->walk_t[who] += dt; else g->walk_t[who] = 0;
    g->walking[who] = same && g->walk_t[who] >= 0.12f;
    in->mvx = in->mvy = 0;
    if (!g->walking[who]) return;
    float ds = s0 / m0 + s1 / m1, df = f0 / m0 + f1 / m1, dl = hypotf(ds, df) + 1e-3f;
    ds /= dl; df /= dl;
    in->mvx = (f->fx * df + f->qx * ds) * 0.6f;      // yurish tezligi 60%
    in->mvy = (f->fy * df + f->qy * ds) * 0.6f;
    in->arm[0][0] = 0.35f;  in->arm[0][1] = -0.95f;  // yurganda qo'llar himoyada
    in->arm[1][0] = -0.35f; in->arm[1][1] = -0.95f;
}
