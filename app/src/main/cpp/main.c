// BOKS v2 - Android (NDK, NativeActivity, GLES2, OpenSL ES)
// Menyu, 3 raund x 1.5 daqiqa, qo'ng'iroq, 2 xil joystik joylashuvi, 3 xil qiyinlik.

#include <android_native_app_glue.h>
#include "game.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PI 3.14159265f
#define SEG 28
#define ROUND_SEC 90.f

typedef struct { float cx, cy, r, vx, vy; int pid; } Joy;
enum { S_MENU, S_INTRO, S_FIGHT, S_BREAK, S_END, S_HELP };

static EGLDisplay dpy = EGL_NO_DISPLAY;
static EGLSurface surf = EGL_NO_SURFACE;
static EGLContext ctx = EGL_NO_CONTEXT;
static GLuint prog;
static GLint locRes, locCol, locPos;
static int W, H, started = 0;
static float U, CX, CY, RING;

static Joy mv, jl, jr;
static Game g;
static int flip = 0;   // PvP mehmon uchun tasvirni 180 gradus aylantirish (keyingi qismlarda)

static int state = S_MENU, lay = 0, diff = 0;
static int rnd = 1, pw = 0, ew = 0, lastres = 3, finalres = 3, isko = 0;
static float stimer, rtime;
static int help_page = 0;

/* ---------- OVOZ (OpenSL ES, qo'ng'iroq sintezi) ---------- */
#define SR 22050
static int16_t bell1[SR * 2], bell3[SR * 3];
static SLObjectItf eng_o, mix_o, pl_o;
static SLEngineItf eng;
static SLPlayItf pl;
static SLAndroidSimpleBufferQueueItf bq;

static void synth(int16_t *b, int n, int strikes, float gap) {
    for (int i = 0; i < n; i++) {
        float s = 0;
        for (int k = 0; k < strikes; k++) {
            float t = i / (float)SR - k * gap;
            if (t < 0 || t > 1.6f) continue;
            float f = 740.f;
            s += expf(-3.2f * t) * sinf(2 * PI * f * t)
               + 0.6f * expf(-5.5f * t) * sinf(2 * PI * f * 2.76f * t)
               + 0.35f * expf(-9.f * t) * sinf(2 * PI * f * 5.4f * t)
               + 0.2f * expf(-14.f * t) * sinf(2 * PI * f * 8.93f * t);
            if (t < 0.004f) s += ((rand() & 1023) / 512.f - 1.f) * 0.5f;
        }
        s *= 0.35f;
        if (s > 1) s = 1;
        if (s < -1) s = -1;
        b[i] = (int16_t)(s * 30000);
    }
}
static void snd_init(void) {
    synth(bell1, SR * 2, 1, 0);
    synth(bell3, SR * 3, 3, 0.5f);
    if (slCreateEngine(&eng_o, 0, NULL, 0, NULL, NULL) != SL_RESULT_SUCCESS) return;
    (*eng_o)->Realize(eng_o, SL_BOOLEAN_FALSE);
    (*eng_o)->GetInterface(eng_o, SL_IID_ENGINE, &eng);
    (*eng)->CreateOutputMix(eng, &mix_o, 0, NULL, NULL);
    (*mix_o)->Realize(mix_o, SL_BOOLEAN_FALSE);
    SLDataLocator_AndroidSimpleBufferQueue lq = { SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2 };
    SLDataFormat_PCM fmt = { SL_DATAFORMAT_PCM, 1, SL_SAMPLINGRATE_22_05, SL_PCMSAMPLEFORMAT_FIXED_16,
                             SL_PCMSAMPLEFORMAT_FIXED_16, SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN };
    SLDataSource src = { &lq, &fmt };
    SLDataLocator_OutputMix lm = { SL_DATALOCATOR_OUTPUTMIX, mix_o };
    SLDataSink snk = { &lm, NULL };
    const SLInterfaceID ids[1] = { SL_IID_ANDROIDSIMPLEBUFFERQUEUE };
    const SLboolean req[1] = { SL_BOOLEAN_TRUE };
    if ((*eng)->CreateAudioPlayer(eng, &pl_o, &src, &snk, 1, ids, req) != SL_RESULT_SUCCESS) return;
    (*pl_o)->Realize(pl_o, SL_BOOLEAN_FALSE);
    (*pl_o)->GetInterface(pl_o, SL_IID_PLAY, &pl);
    (*pl_o)->GetInterface(pl_o, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &bq);
    (*pl)->SetPlayState(pl, SL_PLAYSTATE_PLAYING);
}
static void bell(int n) {
    if (!bq) return;
    (*bq)->Clear(bq);
    if (n == 1) (*bq)->Enqueue(bq, bell1, sizeof(bell1));
    else (*bq)->Enqueue(bq, bell3, sizeof(bell3));
}

/* ---------- chizish yordamchilari ---------- */
static void tri(const float *v, int n, int mode, float r, float g, float b, float a) {
    glUniform4f(locCol, r, g, b, a);
    glVertexAttribPointer(locPos, 2, GL_FLOAT, GL_FALSE, 0, v);
    glDrawArrays(mode, 0, n);
}
static void circle(float x, float y, float rad, float r, float g, float b, float a) {
    float v[(SEG + 2) * 2];
    v[0] = x; v[1] = y;
    for (int i = 0; i <= SEG; i++) {
        float t = 2 * PI * i / SEG;
        v[2 + i * 2] = x + cosf(t) * rad;
        v[3 + i * 2] = y + sinf(t) * rad;
    }
    tri(v, SEG + 2, GL_TRIANGLE_FAN, r, g, b, a);
}
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    float v[8] = { x, y, x + w, y, x, y + h, x + w, y + h };
    tri(v, 4, GL_TRIANGLE_STRIP, r, g, b, a);
}
static void line(float x1, float y1, float x2, float y2, float th, float r, float g, float b, float a) {
    float dx = x2 - x1, dy = y2 - y1, l = hypotf(dx, dy);
    if (l < 0.001f) return;
    float nx = -dy / l * th / 2, ny = dx / l * th / 2;
    float v[8] = { x1 + nx, y1 + ny, x1 - nx, y1 - ny, x2 + nx, y2 + ny, x2 - nx, y2 - ny };
    tri(v, 4, GL_TRIANGLE_STRIP, r, g, b, a);
}

/* ---------- 5x7 shrift ---------- */
static const char CH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-/!.' ";
static const unsigned char FONT[43][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x11,0x19,0x15,0x13,0x11,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    {0x11,0x11,0x11,0x0A,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    {0x00,0x04,0x00,0x00,0x00,0x04,0x00},{0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    {0x01,0x01,0x02,0x04,0x08,0x10,0x10},{0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x06,0x04,0x08,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00}
};
#define TMAX 48
static float tbuf[TMAX * 35 * 12];

// sc: piksel o'lchami (U birligida). cx, cy: matn markazi (piksel).
static void text(const char *s, float cx, float cy, float sc, float r, float g, float b, float a) {
    float p = sc * U;
    int n = (int)strlen(s);
    if (n > TMAX) n = TMAX;
    float x0 = cx - (n * 6 - 1) * p / 2, y0 = cy - 3.5f * p;
    int k = 0;
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        const char *q = strchr(CH, c);
        if (!q) continue;
        const unsigned char *gl = FONT[q - CH];
        for (int row = 0; row < 7; row++)
            for (int col = 0; col < 5; col++)
                if (gl[row] & (1 << (4 - col))) {
                    float x = x0 + (i * 6 + col) * p, y = y0 + row * p;
                    float v[12] = { x, y, x + p, y, x, y + p, x + p, y, x + p, y + p, x, y + p };
                    memcpy(&tbuf[k], v, sizeof(v));
                    k += 12;
                }
    }
    if (k) tri(tbuf, k / 2, GL_TRIANGLES, r, g, b, a);
}

/* ---------- o'yin holati ---------- */
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static float SXW(float x) { return CX + (flip ? -x : x) * U; }
static float SYW(float y) { return CY + (flip ? -y : y) * U; }

static void layout(void) {
    U = H / 100.f; CX = W / 2.f; CY = H / 2.f; RING = RING_HALF * U;
    if (lay == 0) {          // 1: yurish past-chap, qo'llar yuqori burchaklar
        mv.cx = 20 * U;     mv.cy = 76 * U; mv.r = 13 * U;
        jl.cx = 15 * U;     jl.cy = 18 * U; jl.r = 12 * U;
        jr.cx = W - 15 * U; jr.cy = 18 * U; jr.r = 12 * U;
    } else if (lay == 1) {   // 2: qo'llar past burchaklar, yurish yuqori-chap
        mv.cx = 20 * U;     mv.cy = 24 * U; mv.r = 13 * U;
        jl.cx = 15 * U;     jl.cy = 82 * U; jl.r = 12 * U;
        jr.cx = W - 15 * U; jr.cy = 82 * U; jr.r = 12 * U;
    } else {                 // 3: faqat 2 joystik (yurish yo'q, qo'llar past burchaklarda)
        mv.cx = -1000;      mv.cy = -1000;  mv.r = 0;
        jl.cx = 15 * U;     jl.cy = 82 * U; jl.r = 12 * U;
        jr.cx = W - 15 * U; jr.cy = 82 * U; jr.r = 12 * U;
    }
}
static void release_joys(void) {
    mv.pid = jl.pid = jr.pid = -1;
    mv.vx = mv.vy = jl.vx = jl.vy = jr.vx = jr.vy = 0;
}
static void joy_set(Joy *j, float x, float y) {
    float dx = (x - j->cx) / j->r, dy = (y - j->cy) / j->r, m = hypotf(dx, dy);
    if (m > 1) { dx /= m; dy /= m; }
    j->vx = dx; j->vy = dy;
}
static void start_round(void) {
    layout(); game_reset(&g); release_joys();
    rtime = ROUND_SEC; state = S_INTRO; stimer = 2.0f;
}
static void start_match(void) {
    pw = ew = 0; rnd = 1; isko = 0;
    start_round();
}
static void end_round(int res, int byko) {
    lastres = res; isko = byko;
    if (res == 1) pw++; else if (res == 2) ew++;
    bell(3);
    if (pw == 2 || ew == 2 || rnd >= 3) {
        finalres = pw > ew ? 1 : (ew > pw ? 2 : 3);
        state = S_END; stimer = 0;
    } else {
        state = S_BREAK; stimer = 0;
    }
}
static void time_up(void) {
    float a = g.f[0].hp, b = g.f[1].hp;
    end_round(a > b + 0.5f ? 1 : (b > a + 0.5f ? 2 : 3), 0);
}

static void update(float dt) {
    if (state == S_INTRO) { stimer -= dt; if (stimer <= 0) { state = S_FIGHT; bell(1); } return; }
    if (state == S_BREAK) { stimer += dt; return; }
    if (state == S_END) { stimer += dt; return; }
    if (state != S_FIGHT) return;

    Input in[2];
    memset(in, 0, sizeof(in));
    float sg = flip ? -1.f : 1.f;
    in[0].mvx = mv.vx * sg; in[0].mvy = mv.vy * sg;
    // qo'l joystigi avatarga bog'langan: ekrandagi "yuqori" = raqib tomoni
    in[0].arm[0][0] = jl.vx; in[0].arm[0][1] = -jl.vy;
    in[0].arm[1][0] = jr.vx; in[0].arm[1][1] = -jr.vy;
    if (lay == 2) game_two_stick(&g, 0, dt, &in[0]);   // yurish qo'l joystiklaridan chiqariladi
    ai_input(&g, dt, diff, &in[1]);
    game_step(&g, dt, in);

    if (g.f[1].hp <= 0) end_round(1, 1);
    else if (g.f[0].hp <= 0) end_round(2, 1);
    else { rtime -= dt; if (rtime <= 0) time_up(); }
}

/* ---------- chizish ---------- */
static void draw_fighter(const Fighter *f, float cr, float cg, float cb) {
    float k = f->flash > 0 ? 0.6f : 0;
    float bx = SXW(f->x), by = SYW(f->y);
    line(SXW(f->sx[0]), SYW(f->sy[0]), SXW(f->sx[1]), SYW(f->sy[1]), 4 * U, cr * .7f + k, cg * .7f + k, cb * .7f + k, 1);
    circle(bx, by, 5.5f * U, cr + k, cg + k, cb + k, 1);
    for (int i = 0; i < 2; i++)
        line(SXW(f->sx[i]), SYW(f->sy[i]), SXW(f->gx[i]), SYW(f->gy[i]), 1.6f * U, cr * .6f, cg * .6f, cb * .6f, 1);
    circle(SXW(f->x + f->fx * 1.5f), SYW(f->y + f->fy * 1.5f), 3.2f * U, cr * .8f + k, cg * .8f + k, cb * .8f + k, 1);
    for (int i = 0; i < 2; i++) {
        if (f->guard[i] > 0.25f) circle(SXW(f->gx[i]), SYW(f->gy[i]), (1.f + 5.f * f->guard[i]) * U, .3f, .7f, 1, 0.25f);
        circle(SXW(f->gx[i]), SYW(f->gy[i]), 2.6f * U, .9f, .1f, .1f, 1);
    }
}
static void half_disc(float cx, float cy, float r, int lower, float R, float G, float B, float A) {
    float v[(SEG + 2) * 2];
    v[0] = cx; v[1] = cy;
    for (int i = 0; i <= SEG; i++) {
        float t = PI * i / SEG + (lower ? 0.f : PI);
        v[2 + i * 2] = cx + cosf(t) * r;
        v[3 + i * 2] = cy + sinf(t) * r;
    }
    tri(v, SEG + 2, GL_TRIANGLE_FAN, R, G, B, A);
}
static void draw_joy(const Joy *j) {
    circle(j->cx, j->cy, j->r, 1, 1, 1, 0.15f);
    circle(j->cx + j->vx * j->r, j->cy + j->vy * j->r, j->r * 0.45f, 1, 1, 1, j->pid >= 0 ? 0.7f : 0.35f);
}
// qo'l joystigi: yuqori yarmi (qizil) - zarba, pastki yarmi (ko'k) - himoya
static void draw_joy_arm(const Joy *j) {
    half_disc(j->cx, j->cy, j->r, 0, 1.f, .3f, .3f, 0.22f);
    half_disc(j->cx, j->cy, j->r, 1, .3f, .5f, 1.f, 0.28f);
    line(j->cx - j->r, j->cy, j->cx + j->r, j->cy, 0.4f * U, 1, 1, 1, 0.5f);
    circle(j->cx + j->vx * j->r, j->cy + j->vy * j->r, j->r * 0.45f, 1, 1, 1, j->pid >= 0 ? 0.75f : 0.4f);
}

static void draw_game(void) {
    rect(CX - RING, CY - RING, 2 * RING, 2 * RING, .22f, .24f, .32f, 1);
    float t = 1.2f * U;
    rect(CX - RING, CY - RING, 2 * RING, t, .8f, .1f, .1f, 1);
    rect(CX - RING, CY + RING - t, 2 * RING, t, .8f, .1f, .1f, 1);
    rect(CX - RING, CY - RING, t, 2 * RING, .8f, .1f, .1f, 1);
    rect(CX + RING - t, CY - RING, t, 2 * RING, .8f, .1f, .1f, 1);

    draw_fighter(&g.f[1], .85f, .4f, .2f);
    if (g.ai.state == 1) circle(SXW(g.f[1].x), SYW(g.f[1].y), 8 * U, 1, .2f, .2f, 0.25f);   // hujum ogohlantirishi
    draw_fighter(&g.f[0], .2f, .5f, 1.f);

    // HUD: HP, vaqt, raund
    rect(CX - 48 * U, 3 * U, 36 * U, 3 * U, .2f, .2f, .2f, 1);
    rect(CX - 48 * U, 3 * U, 36 * U * clampf(g.f[0].hp / 100, 0, 1), 3 * U, .2f, .6f, 1, 1);
    rect(CX + 12 * U, 3 * U, 36 * U, 3 * U, .2f, .2f, .2f, 1);
    rect(CX + 12 * U, 3 * U, 36 * U * clampf(g.f[1].hp / 100, 0, 1), 3 * U, .9f, .4f, .2f, 1);
    char buf[48];
    int secs = (int)ceilf(rtime);
    snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
    text(buf, CX, 4.5f * U, .9f, 1, 1, 1, 1);
    snprintf(buf, sizeof(buf), "RAUND %d/3", rnd);
    text(buf, CX, 12 * U, .5f, .85f, .85f, .85f, .9f);

    if (lay != 2) draw_joy(&mv);
    draw_joy_arm(&jl); draw_joy_arm(&jr);

    if (state == S_INTRO) {
        snprintf(buf, sizeof(buf), "RAUND %d", rnd);
        text(buf, CX, CY, 2.2f, 1, .9f, .2f, 1);
    } else if (state == S_BREAK) {
        rect(0, 0, (float)W, (float)H, 0, 0, 0, 0.5f);
        const char *t1 = isko ? (lastres == 1 ? "NOKAUT! RAUND G'ALABA" : "NOKAUT. RAUND MAG'LUBIYAT")
                              : (lastres == 1 ? "RAUND G'ALABA" : (lastres == 2 ? "RAUND MAG'LUBIYAT" : "RAUND DURANG"));
        text(t1, CX, CY - 8 * U, 1.0f, 1, 1, 1, 1);
        snprintf(buf, sizeof(buf), "SCHET %d:%d", pw, ew);
        text(buf, CX, CY + 10 * U, 1.1f, 1, .9f, .2f, 1);
        if (stimer > 1) text("KEYINGI RAUND: EKRANGA TEGING", CX, CY + 26 * U, .6f, 1, 1, 1, 0.9f);
    } else if (state == S_END) {
        if (finalres == 1) rect(0, 0, (float)W, (float)H, 0, .6f, .2f, 0.55f);
        else if (finalres == 2) rect(0, 0, (float)W, (float)H, .7f, 0, 0, 0.55f);
        else rect(0, 0, (float)W, (float)H, .3f, .3f, .35f, 0.55f);
        const char *s = finalres == 1 ? "G'ALABA!" : (finalres == 2 ? "MAG'LUBIYAT" : "DURANG");
        text(s, CX, CY - 8 * U, 1.5f, 1, 1, 1, 1);
        snprintf(buf, sizeof(buf), "RAUNDLAR %d:%d", pw, ew);
        text(buf, CX, CY + 8 * U, 1.0f, 1, .9f, .2f, 1);
        if (stimer > 1) text("MENYUGA QAYTISH: EKRANGA TEGING", CX, CY + 26 * U, .6f, 1, 1, 1, 0.9f);
    }
}

/* ---------- menyu ---------- */
static const char *DIFF_NAME[3] = { "1 ODDIY", "2 TEZ", "3 HIMOYA" };
static const char *DIFF_DESC[3] = {
    "ODDIY: HOZIRGI HOLAT",
    "TEZ: DUSHMAN 2 BARAVAR KO'P HUJUM QILADI",
    "HIMOYA: TEZ HUJUM VA DOIM BLOK"
};
static int in(float x, float y, float cx, float cy, float w, float h) {
    return fabsf(x - cx) < w / 2 && fabsf(y - cy) < h / 2;
}
static const char *HELP_TITLE[3] = { "QANDAY O'YNALADI", "3 JOYSTIK BOSHQARUVI", "2 JOYSTIK BOSHQARUVI" };
static const char *HELP_LINES[3][9] = {
    { "MAQSAD: RAQIBNI NOKAUT QILISH YOKI",
      "HAR RAUNDDA RAQIBDAN KO'P HP QOLDIRING.",
      "O'YIN 3 RAUND. HAR BIRI 1:30 DAQIQA.",
      "2 RAUNDNI YUTGAN G'ALABA QOZONADI.",
      "HP 0 BO'LSA RAUND NOKAUT BILAN TUGAYDI.",
      "RAUND BOSHI VA OXIRIDA QO'NG'IROQ CHALINADI.",
      "RAUNDLAR ORASIDA EKRANGA TEGIB DAVOM ETING.", NULL },
    { "CHAP-PAST: YURISH. YUQORI BURCHAKLAR: QO'LLAR.",
      "QO'L JOYSTIGI RAQIB TOMONGA BOG'LANGAN:",
      "YUQORI YARMI QIZIL: ZARBA TOMONI.",
      "PASTKI YARMI KO'K: HIMOYA TOMONI.",
      "TEZ SURSANGIZ ZARBA KUCHLI BO'LADI.",
      "SEKIN BOSSANGIZ ZARBA BO'LMAYDI.",
      "HIMOYA QO'LI FAQAT O'Z TOMONINI YOPADI.", NULL },
    { "IKKALA JOYSTIK BIR TOMONGA SURILSA YURASIZ.",
      "OLDINGA: RAQIB TOMON. ORQAGA: UZOQLASHISH.",
      "YONGA: RAQIB ATROFIDA AYLANISH.",
      "YURGANDA QO'LLAR HIMOYA HOLATIDA BO'LADI.",
      "YENGIL ORQAGA YOKI ICHKARIGA SURSANGIZ -",
      "JOYIDA HIMOYA QILASIZ.",
      "1-2 KOMBO: QO'LLARNI NAVBAT BILAN URING.",
      "ORQAGA TUGMASI: MENYUGA QAYTISH." }
};
static void draw_help(void) {
    text(HELP_TITLE[help_page], CX, 11 * U, 1.0f, 1, .85f, .2f, 1);
    for (int i = 0; i < 9 && HELP_LINES[help_page][i]; i++)
        text(HELP_LINES[help_page][i], CX, (26 + i * 8) * U, .55f, 1, 1, 1, 1);
    char b[16];
    snprintf(b, sizeof(b), "%d/3", help_page + 1);
    text(b, W - 14 * U, 94 * U, .7f, .8f, .8f, .8f, 1);
    text(help_page < 2 ? "EKRANGA TEGING: KEYINGI SAHIFA" : "EKRANGA TEGING: MENYU", CX, 94 * U, .55f, .8f, .8f, .8f, 1);
}

static void draw_menu(void) {
    rect(W - 40 * U, 4 * U, 36 * U, 9 * U, .2f, .22f, .3f, 1);
    text("YORIQNOMA", W - 22 * U, 8.5f * U, .55f, 1, 1, 1, 1);
    text("BOKS", CX, 12 * U, 2.2f, 1, .25f, .2f, 1);
    rect(CX - 30 * U, 22 * U, 60 * U, 0.8f * U, .9f, .2f, .2f, 1);
    text("JOYSTIK JOYLASHUVI", CX, 27 * U, .6f, .8f, .8f, .8f, 1);
    static const char *LAY_NAME[3] = { "3J-1", "3J-2", "2J" };
    static const char *LAY_HINT[3] = { "3 JOYSTIK: YURISH CHAP-PASTDA", "3 JOYSTIK: YURISH YUQORI-CHAPDA",
                                       "2 JOYSTIK: IKKALASI BIR TOMONGA - YURISH" };
    for (int i = 0; i < 3; i++) {
        float cx = CX + (i - 1) * 38 * U, cy = 39 * U;
        if (lay == i) rect(cx - 18.5f * U, cy - 10 * U, 37 * U, 20 * U, 1, .8f, .1f, 1);
        rect(cx - 17.5f * U, cy - 9 * U, 35 * U, 18 * U, .2f, .22f, .3f, 1);
        float mx = 0, my = 0, lxx, lyy, rxx, ryy;
        int has_mv = 1;
        if (i == 0)      { mx = -12; my = 4;  lxx = -12; lyy = -4; rxx = 12; ryy = -4; }
        else if (i == 1) { mx = -12; my = -4; lxx = -12; lyy = 4;  rxx = 12; ryy = 4; }
        else             { has_mv = 0; lxx = -12; lyy = 4; rxx = 12; ryy = 4; }
        if (has_mv) circle(cx + mx * U, cy + my * U, 2.6f * U, .3f, .6f, 1, 1);
        circle(cx + lxx * U, cy + lyy * U, 2.6f * U, .9f, .2f, .2f, 1);
        circle(cx + rxx * U, cy + ryy * U, 2.6f * U, .9f, .2f, .2f, 1);
        text(LAY_NAME[i], cx, cy, .6f, .8f, .8f, .8f, .8f);
    }
    text(LAY_HINT[lay], CX, 51.2f * U, .5f, .7f, .7f, .7f, 1);
    text("QIYINLIK", CX, 56.5f * U, .6f, .8f, .8f, .8f, 1);
    for (int i = 0; i < 3; i++) {
        float cx = CX + (i - 1) * 34 * U, cy = 65 * U;
        if (diff == i) rect(cx - 17 * U, cy - 5.5f * U, 34 * U, 11 * U, 1, .8f, .1f, 1);
        rect(cx - 16 * U, cy - 4.5f * U, 32 * U, 9 * U, diff == i ? .3f : .2f, diff == i ? .32f : .22f, diff == i ? .4f : .3f, 1);
        text(DIFF_NAME[i], cx, cy, .55f, 1, 1, 1, 1);
    }
    text(DIFF_DESC[diff], CX, 74.5f * U, .55f, .8f, .8f, .8f, 1);
    rect(CX - 33 * U, 80 * U, 66 * U, 14 * U, .15f, .6f, .25f, 1);
    text("BOSHLASH", CX, 87 * U, 1.1f, 1, 1, 1, 1);
}
static void menu_tap(float x, float y) {
    for (int i = 0; i < 3; i++)
        if (in(x, y, CX + (i - 1) * 38 * U, 39 * U, 36 * U, 18 * U)) { lay = i; return; }
    for (int i = 0; i < 3; i++)
        if (in(x, y, CX + (i - 1) * 34 * U, 65 * U, 32 * U, 9 * U)) { diff = i; return; }
    if (in(x, y, W - 22 * U, 8.5f * U, 36 * U, 9 * U)) { state = S_HELP; help_page = 0; return; }
    if (in(x, y, CX, 87 * U, 66 * U, 14 * U)) start_match();
}

static void draw(void) {
    glViewport(0, 0, W, H);
    glClearColor(.07f, .07f, .09f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog); glUniform2f(locRes, (float)W, (float)H);
    if (state == S_MENU) draw_menu(); else if (state == S_HELP) draw_help(); else draw_game();
}

/* ---------- kirish ---------- */
static int32_t on_input(struct android_app *app, AInputEvent *e) {
    (void)app;
    if (AInputEvent_getType(e) == AINPUT_EVENT_TYPE_KEY) {
        if (AKeyEvent_getKeyCode(e) != AKEYCODE_BACK) return 0;
        int inmenu = (state == S_MENU);
        if (!inmenu && AKeyEvent_getAction(e) == AKEY_EVENT_ACTION_UP) { state = S_MENU; release_joys(); layout(); }
        return inmenu ? 0 : 1;
    }
    if (AInputEvent_getType(e) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t act = AMotionEvent_getAction(e);
    int32_t a = act & AMOTION_EVENT_ACTION_MASK;
    int idx = (act & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    Joy *js[3] = { &mv, &jl, &jr };

    if (a == AMOTION_EVENT_ACTION_DOWN || a == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        float x = AMotionEvent_getX(e, idx), y = AMotionEvent_getY(e, idx);
        if (state == S_MENU) { menu_tap(x, y); return 1; }
        if (state == S_HELP) { if (++help_page >= 3) state = S_MENU; return 1; }
        if (state == S_END) { if (stimer > 1) { state = S_MENU; release_joys(); layout(); } return 1; }
        if (state == S_BREAK) { if (stimer > 1) { rnd++; start_round(); } return 1; }
        if (state != S_FIGHT) return 1;
        int id = AMotionEvent_getPointerId(e, idx);
        for (int i = 0; i < 3; i++) {
            if (js[i]->pid < 0 && hypotf(x - js[i]->cx, y - js[i]->cy) < js[i]->r * 1.8f) {
                js[i]->pid = id; joy_set(js[i], x, y); break;
            }
        }
    } else if (a == AMOTION_EVENT_ACTION_MOVE) {
        int n = (int)AMotionEvent_getPointerCount(e);
        for (int i = 0; i < 3; i++) {
            if (js[i]->pid < 0) continue;
            for (int k = 0; k < n; k++)
                if (AMotionEvent_getPointerId(e, k) == js[i]->pid)
                    joy_set(js[i], AMotionEvent_getX(e, k), AMotionEvent_getY(e, k));
        }
    } else if (a == AMOTION_EVENT_ACTION_UP || a == AMOTION_EVENT_ACTION_POINTER_UP) {
        int id = AMotionEvent_getPointerId(e, idx);
        for (int i = 0; i < 3; i++)
            if (js[i]->pid == id) { js[i]->pid = -1; js[i]->vx = js[i]->vy = 0; }
    } else if (a == AMOTION_EVENT_ACTION_CANCEL) {
        release_joys();
    }
    return 1;
}

/* ---------- EGL / GLES ---------- */
static GLuint compile(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL); glCompileShader(s);
    return s;
}
static int gl_init(ANativeWindow *w) {
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(dpy, 0, 0);
    const EGLint at[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                          EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
    EGLConfig cfg; EGLint n, fmt;
    eglChooseConfig(dpy, at, &cfg, 1, &n);
    eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_VISUAL_ID, &fmt);
    ANativeWindow_setBuffersGeometry(w, 0, 0, fmt);
    surf = eglCreateWindowSurface(dpy, cfg, w, NULL);
    const EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) return -1;
    eglQuerySurface(dpy, surf, EGL_WIDTH, &W); eglQuerySurface(dpy, surf, EGL_HEIGHT, &H);

    const char *vs = "attribute vec2 p; uniform vec2 res;"
                     "void main(){gl_Position=vec4(p.x/res.x*2.-1.,1.-p.y/res.y*2.,0.,1.);}";
    const char *fs = "precision mediump float; uniform vec4 col; void main(){gl_FragColor=col;}";
    prog = glCreateProgram();
    glAttachShader(prog, compile(GL_VERTEX_SHADER, vs));
    glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fs));
    glLinkProgram(prog);
    locRes = glGetUniformLocation(prog, "res");
    locCol = glGetUniformLocation(prog, "col");
    locPos = glGetAttribLocation(prog, "p");
    glUseProgram(prog); glEnableVertexAttribArray(locPos);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    layout();
    if (!started) { game_reset(&g); started = 1; }
    return 0;
}
static void gl_term(void) {
    if (dpy != EGL_NO_DISPLAY) {
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (ctx != EGL_NO_CONTEXT) eglDestroyContext(dpy, ctx);
        if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
        eglTerminate(dpy);
    }
    dpy = EGL_NO_DISPLAY; ctx = EGL_NO_CONTEXT; surf = EGL_NO_SURFACE;
    release_joys();
}
static void on_cmd(struct android_app *app, int32_t cmd) {
    if (cmd == APP_CMD_INIT_WINDOW && app->window) gl_init(app->window);
    else if (cmd == APP_CMD_TERM_WINDOW) gl_term();
}
static double now_sec(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void android_main(struct android_app *app) {
    app->onAppCmd = on_cmd;
    app->onInputEvent = on_input;
    release_joys();
    srand((unsigned)time(NULL));
    snd_init();
    double last = now_sec();

    while (1) {
        int ev; struct android_poll_source *src;
        while (ALooper_pollAll(surf != EGL_NO_SURFACE ? 0 : -1, NULL, &ev, (void **)&src) >= 0) {
            if (src) src->process(app, src);
            if (app->destroyRequested) { gl_term(); return; }
        }
        if (surf == EGL_NO_SURFACE) { last = now_sec(); continue; }

        int w, h;
        eglQuerySurface(dpy, surf, EGL_WIDTH, &w); eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
        if (w != W || h != H) { W = w; H = h; layout(); }

        double t = now_sec();
        float dt = clampf((float)(t - last), 0, 0.05f);
        last = t;
        update(dt);
        draw();
        eglSwapBuffers(dpy, surf);
    }
}
