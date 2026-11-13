// BOKS v2 - Android (NDK, NativeActivity, GLES2, OpenSL ES)
// Menyu, 3 raund x 1.5 daqiqa, qo'ng'iroq, 2 xil joystik joylashuvi, 3 xil qiyinlik.

#include <android_native_app_glue.h>
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
#define PUNCH_T 0.28f
#define EPT (diff >= 1 ? 0.14f : 0.22f)   // dushman zarbasi davomiyligi

typedef struct { float cx, cy, r, vx, vy; int pid; } Joy;
typedef struct { float t, dx, dy; int hit, was; } Arm;
typedef struct { float bx, by, fx, fy, qx, qy, hx, hy; } Pose;
enum { S_MENU, S_INTRO, S_FIGHT, S_BREAK, S_END };

static EGLDisplay dpy = EGL_NO_DISPLAY;
static EGLSurface surf = EGL_NO_SURFACE;
static EGLContext ctx = EGL_NO_CONTEXT;
static GLuint prog;
static GLint locRes, locCol, locPos;
static int W, H, started = 0;
static float U, CX, CY, RING;

static Joy mv, jl, jr;
static Arm al, ar;
static float px, py, lx, ly, tw, php;
static float ex, ey, ehp, ecd, etimer, eblock, eflash, pflash, now_t;
static int estate, ehand, ehit, guard;

static int state = S_MENU, lay = 0, diff = 0;
static int rnd = 1, pw = 0, ew = 0, lastres = 3, finalres = 3, isko = 0;
static float stimer, rtime;

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
static const char CH[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-/!. ";
static const unsigned char FONT[42][7] = {
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
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x00,0x00,0x00,0x00,0x00,0x00,0x00}
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
static void layout(void) {
    U = H / 100.f; CX = W / 2.f; CY = H / 2.f; RING = 40 * U;
    if (lay == 0) {          // 1: yurish past-chap, qo'llar yuqori burchaklar
        mv.cx = 20 * U;     mv.cy = 76 * U; mv.r = 13 * U;
        jl.cx = 15 * U;     jl.cy = 18 * U; jl.r = 12 * U;
        jr.cx = W - 15 * U; jr.cy = 18 * U; jr.r = 12 * U;
    } else {                 // 2: qo'llar past burchaklar, yurish yuqori-chap
        mv.cx = 20 * U;     mv.cy = 24 * U; mv.r = 13 * U;
        jl.cx = 15 * U;     jl.cy = 82 * U; jl.r = 12 * U;
        jr.cx = W - 15 * U; jr.cy = 82 * U; jr.r = 12 * U;
    }
}
static void release_joys(void) {
    mv.pid = jl.pid = jr.pid = -1;
    mv.vx = mv.vy = jl.vx = jl.vy = jr.vx = jr.vy = 0;
}
static void reset(void) {
    px = CX; py = CY + 22 * U; ex = CX; ey = CY - 22 * U;
    lx = ly = tw = 0; php = ehp = 100; ecd = 1.5f; etimer = 0; eblock = 0;
    eflash = pflash = 0; estate = 0; ehit = 0; guard = 0;
    al.t = ar.t = -1; al.was = ar.was = 0;
}
static void start_round(void) {
    layout(); reset(); release_joys();
    rtime = ROUND_SEC; state = S_INTRO; stimer = 2.0f;
}
static void start_match(void) {
    pw = ew = 0; rnd = 1; isko = 0;
    start_round();
}
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static int eblocking(void) { return eblock > 0 || (diff == 2 && estate == 0); }

static Pose player_pose(void) {
    Pose p;
    float a = atan2f(ey - py, ex - px);
    p.fx = cosf(a); p.fy = sinf(a);
    p.bx = px + lx * 4 * U; p.by = py + ly * 4 * U;
    float sa = a + tw;
    p.qx = -sinf(sa); p.qy = cosf(sa);
    p.hx = p.bx + lx * 2 * U + p.fx * 1.5f * U;
    p.hy = p.by + ly * 2 * U + p.fy * 1.5f * U;
    return p;
}
static void player_fist(const Pose *p, int right, const Arm *a, float *x, float *y) {
    float s = right ? 1.f : -1.f;
    float shx = p->bx + p->qx * 5 * U * s, shy = p->by + p->qy * 5 * U * s;
    if (guard) {
        *x = p->hx + p->fx * 4 * U + p->qx * 2.6f * U * s;
        *y = p->hy + p->fy * 4 * U + p->qy * 2.6f * U * s;
        return;
    }
    *x = shx + p->fx * 6 * U; *y = shy + p->fy * 6 * U;
    if (a->t >= 0) {
        float ext = sinf(PI * clampf(a->t / PUNCH_T, 0, 1));
        *x += a->dx * 16 * U * ext; *y += a->dy * 16 * U * ext;
    }
}
static void joy_set(Joy *j, float x, float y) {
    float dx = (x - j->cx) / j->r, dy = (y - j->cy) / j->r, m = hypotf(dx, dy);
    if (m > 1) { dx /= m; dy /= m; }
    j->vx = dx; j->vy = dy;
}

static void arm_update(Arm *a, Joy *j, int right, float dt) {
    float m = hypotf(j->vx, j->vy);
    if (a->t >= 0) {
        a->t += dt;
        Pose p = player_pose();
        float fx, fy;
        player_fist(&p, right, a, &fx, &fy);
        float ext = sinf(PI * clampf(a->t / PUNCH_T, 0, 1));
        if (!a->hit && ext > 0.8f && hypotf(fx - ex, fy - ey) < 8.5f * U) {
            a->hit = 1;
            ehp -= eblocking() ? 1.f : 7.f;
            eflash = 0.15f;
            float d = hypotf(ex - px, ey - py) + 0.001f;
            ex += (ex - px) / d * 2 * U; ey += (ey - py) / d * 2 * U;
        }
        if (a->t >= PUNCH_T) a->t = -1;
    } else if (!guard && m > 0.7f && !a->was) {
        a->t = 0; a->hit = 0; a->dx = j->vx / m; a->dy = j->vy / m;
        if (eblock <= 0 && (rand() % 100) < 30) eblock = 0.5f;
    }
    a->was = m > 0.5f;
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
    end_round(php > ehp + 0.5f ? 1 : (ehp > php + 0.5f ? 2 : 3), 0);
}

static void update(float dt) {
    now_t += dt;
    if (state == S_INTRO) { stimer -= dt; if (stimer <= 0) { state = S_FIGHT; bell(1); } return; }
    if (state == S_BREAK) { stimer += dt; return; }
    if (state == S_END) { stimer += dt; return; }
    if (state != S_FIGHT) return;

    px += mv.vx * 30 * U * dt; py += mv.vy * 30 * U * dt;

    float lm = hypotf(jl.vx, jl.vy), rm = hypotf(jr.vx, jr.vy);
    guard = (lm > 0.25f && rm > 0.25f);
    float k = clampf(12 * dt, 0, 1);
    float tlx = 0, tly = 0, ttw = 0;
    if (guard) {
        tlx = (jl.vx + jr.vx) / 2; tly = (jl.vy + jr.vy) / 2;
        ttw = (jr.vy - jl.vy) * 0.6f;
    }
    lx += (tlx - lx) * k; ly += (tly - ly) * k; tw += (ttw - tw) * k;

    float dx = px - ex, dy = py - ey, d = hypotf(dx, dy) + 0.001f, mind = 12 * U;
    if (d < mind) {
        float push = (mind - d) / 2;
        px += dx / d * push; py += dy / d * push; ex -= dx / d * push; ey -= dy / d * push;
    }
    float lim = RING - 6 * U;
    px = clampf(px, CX - lim, CX + lim); py = clampf(py, CY - lim, CY + lim);
    ex = clampf(ex, CX - lim, CX + lim); ey = clampf(ey, CY - lim, CY + lim);

    arm_update(&al, &jl, 0, dt);
    arm_update(&ar, &jr, 1, dt);

    eblock -= dt; eflash -= dt; pflash -= dt; ecd -= dt;
    dx = px - ex; dy = py - ey; d = hypotf(dx, dy) + 0.001f;
    float nx = dx / d, ny = dy / d;
    if (estate == 0) {
        float sp = 0;
        if (d > 26 * U) sp = 16 * U; else if (d < 20 * U) sp = -12 * U;
        float st = sinf(now_t * 1.3f) * 8 * U;
        ex += (nx * sp - ny * st) * dt; ey += (ny * sp + nx * st) * dt;
        if (ecd <= 0 && d < 28 * U) { estate = 1; etimer = (diff >= 1) ? 0.22f : 0.35f; ehand = rand() & 1; }
    } else if (estate == 1) {
        etimer -= dt;
        if (etimer <= 0) { estate = 2; etimer = EPT; ehit = 0; }
    } else {
        etimer -= dt;
        float ext = sinf(PI * clampf(1 - etimer / EPT, 0, 1));
        if (!ehit && ext > 0.8f) {
            ehit = 1;
            if (d < 26 * U) {
                float dmg = 8;
                if (guard) dmg = (hypotf(lx, ly) > 0.6f) ? 0 : 1.2f;
                if (dmg > 0) pflash = 0.15f;
                php -= dmg;
            }
        }
        if (etimer <= 0) {
            estate = 0;
            ecd = (0.7f + 1.2f * (rand() / (float)RAND_MAX)) * (diff >= 1 ? 0.44f : 1.f);
        }
    }

    if (ehp <= 0) end_round(1, 1);
    else if (php <= 0) end_round(2, 1);
    else { rtime -= dt; if (rtime <= 0) time_up(); }
}

/* ---------- chizish ---------- */
static void draw_fighter(float bx, float by, float qx, float qy, float hx, float hy,
                         float fist[2][2], float cr, float cg, float cb, float flash) {
    float k = flash > 0 ? 0.6f : 0;
    line(bx - qx * 5 * U, by - qy * 5 * U, bx + qx * 5 * U, by + qy * 5 * U, 4 * U, cr * .7f + k, cg * .7f + k, cb * .7f + k, 1);
    circle(bx, by, 5.5f * U, cr + k, cg + k, cb + k, 1);
    for (int i = 0; i < 2; i++) {
        float s = i ? 1.f : -1.f;
        line(bx + qx * 5 * U * s, by + qy * 5 * U * s, fist[i][0], fist[i][1], 1.6f * U, cr * .6f, cg * .6f, cb * .6f, 1);
    }
    circle(hx, hy, 3.2f * U, cr * .8f + k, cg * .8f + k, cb * .8f + k, 1);
    for (int i = 0; i < 2; i++) circle(fist[i][0], fist[i][1], 2.6f * U, .9f, .1f, .1f, 1);
}
static void draw_joy(const Joy *j) {
    circle(j->cx, j->cy, j->r, 1, 1, 1, 0.15f);
    circle(j->cx + j->vx * j->r, j->cy + j->vy * j->r, j->r * 0.45f, 1, 1, 1, j->pid >= 0 ? 0.7f : 0.35f);
}

static void draw_game(void) {
    rect(CX - RING, CY - RING, 2 * RING, 2 * RING, .22f, .24f, .32f, 1);
    float t = 1.2f * U;
    rect(CX - RING, CY - RING, 2 * RING, t, .8f, .1f, .1f, 1);
    rect(CX - RING, CY + RING - t, 2 * RING, t, .8f, .1f, .1f, 1);
    rect(CX - RING, CY - RING, t, 2 * RING, .8f, .1f, .1f, 1);
    rect(CX + RING - t, CY - RING, t, 2 * RING, .8f, .1f, .1f, 1);

    {   // dushman
        float dx = px - ex, dy = py - ey, d = hypotf(dx, dy) + 0.001f;
        float nx = dx / d, ny = dy / d, qx = -ny, qy = nx;
        float fist[2][2];
        float ext = sinf(PI * clampf(1 - etimer / EPT, 0, 1));
        for (int h = 0; h < 2; h++) {
            float s = h ? 1.f : -1.f;
            float sx = ex + qx * 5 * U * s, sy = ey + qy * 5 * U * s;
            float fx = sx + nx * 6 * U, fy = sy + ny * 6 * U;
            if (eblocking()) { fx = ex + nx * 8 * U + qx * 2.6f * U * s; fy = ey + ny * 8 * U + qy * 2.6f * U * s; }
            else if (estate == 1 && h == ehand) { fx = sx + nx * U; fy = sy + ny * U; }
            else if (estate == 2 && h == ehand) { fx = sx + nx * (6 + 14 * ext) * U; fy = sy + ny * (6 + 14 * ext) * U; }
            fist[h][0] = fx; fist[h][1] = fy;
        }
        draw_fighter(ex, ey, qx, qy, ex + nx * 1.5f * U, ey + ny * 1.5f * U, fist, .85f, .4f, .2f, eflash);
        if (estate == 1) circle(ex, ey, 8 * U, 1, .2f, .2f, 0.25f);
    }
    {   // o'yinchi
        Pose p = player_pose();
        float fist[2][2];
        player_fist(&p, 0, &al, &fist[0][0], &fist[0][1]);
        player_fist(&p, 1, &ar, &fist[1][0], &fist[1][1]);
        draw_fighter(p.bx, p.by, p.qx, p.qy, p.hx, p.hy, fist, .2f, .5f, 1.f, pflash);
        if (guard) circle(p.bx, p.by, 9 * U, .3f, .7f, 1, 0.2f);
    }

    // HUD: HP, vaqt, raund
    rect(CX - 48 * U, 3 * U, 36 * U, 3 * U, .2f, .2f, .2f, 1);
    rect(CX - 48 * U, 3 * U, 36 * U * clampf(php / 100, 0, 1), 3 * U, .2f, .6f, 1, 1);
    rect(CX + 12 * U, 3 * U, 36 * U, 3 * U, .2f, .2f, .2f, 1);
    rect(CX + 12 * U, 3 * U, 36 * U * clampf(ehp / 100, 0, 1), 3 * U, .9f, .4f, .2f, 1);
    char buf[48];
    int secs = (int)ceilf(rtime);
    snprintf(buf, sizeof(buf), "%d:%02d", secs / 60, secs % 60);
    text(buf, CX, 4.5f * U, .9f, 1, 1, 1, 1);
    snprintf(buf, sizeof(buf), "RAUND %d/3", rnd);
    text(buf, CX, 12 * U, .5f, .85f, .85f, .85f, .9f);

    draw_joy(&mv); draw_joy(&jl); draw_joy(&jr);

    if (state == S_INTRO) {
        snprintf(buf, sizeof(buf), "RAUND %d", rnd);
        text(buf, CX, CY, 2.2f, 1, .9f, .2f, 1);
    } else if (state == S_BREAK) {
        rect(0, 0, (float)W, (float)H, 0, 0, 0, 0.5f);
        const char *t1 = isko ? (lastres == 1 ? "NOKAUT! RAUND GALABA" : "NOKAUT. RAUND MAGLUBIYAT")
                              : (lastres == 1 ? "RAUND GALABA" : (lastres == 2 ? "RAUND MAGLUBIYAT" : "RAUND DURANG"));
        text(t1, CX, CY - 8 * U, 1.1f, 1, 1, 1, 1);
        snprintf(buf, sizeof(buf), "SCHET %d:%d", pw, ew);
        text(buf, CX, CY + 10 * U, 1.1f, 1, .9f, .2f, 1);
        if (stimer > 1) text("KEYINGI RAUND: EKRANGA TEGING", CX, CY + 26 * U, .6f, 1, 1, 1, 0.9f);
    } else if (state == S_END) {
        if (finalres == 1) rect(0, 0, (float)W, (float)H, 0, .6f, .2f, 0.55f);
        else if (finalres == 2) rect(0, 0, (float)W, (float)H, .7f, 0, 0, 0.55f);
        else rect(0, 0, (float)W, (float)H, .3f, .3f, .35f, 0.55f);
        const char *s = finalres == 1 ? "GALABA!" : (finalres == 2 ? "MAGLUBIYAT" : "DURANG");
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
    "TEZ: DUSHMAN 2 BARAVAR KOP HUJUM QILADI",
    "HIMOYA: TEZ HUJUM VA DOIM BLOK"
};
static int in(float x, float y, float cx, float cy, float w, float h) {
    return fabsf(x - cx) < w / 2 && fabsf(y - cy) < h / 2;
}
static void draw_menu(void) {
    text("BOKS", CX, 12 * U, 2.2f, 1, .25f, .2f, 1);
    rect(CX - 30 * U, 22 * U, 60 * U, 0.8f * U, .9f, .2f, .2f, 1);
    text("JOYSTIK JOYLASHUVI", CX, 27 * U, .6f, .8f, .8f, .8f, 1);
    for (int i = 0; i < 2; i++) {
        float cx = CX + (i ? 22 : -22) * U, cy = 39 * U;
        if (lay == i) rect(cx - 21 * U, cy - 10 * U, 42 * U, 20 * U, 1, .8f, .1f, 1);
        rect(cx - 20 * U, cy - 9 * U, 40 * U, 18 * U, .2f, .22f, .3f, 1);
        float mx, my, lxx, lyy, rxx, ryy;
        if (i == 0) { mx = -15; my = 4; lxx = -15; lyy = -4; rxx = 15; ryy = -4; }
        else        { mx = -15; my = -4; lxx = -15; lyy = 4; rxx = 15; ryy = 4; }
        circle(cx + mx * U, cy + my * U, 2.6f * U, .3f, .6f, 1, 1);
        circle(cx + lxx * U, cy + lyy * U, 2.6f * U, .9f, .2f, .2f, 1);
        circle(cx + rxx * U, cy + ryy * U, 2.6f * U, .9f, .2f, .2f, 1);
        char n[2] = { (char)('1' + i), 0 };
        text(n, cx, cy, .8f, .8f, .8f, .8f, .7f);
    }
    text("QIYINLIK", CX, 55 * U, .6f, .8f, .8f, .8f, 1);
    for (int i = 0; i < 3; i++) {
        float cx = CX + (i - 1) * 34 * U, cy = 64 * U;
        if (diff == i) rect(cx - 17 * U, cy - 5.5f * U, 34 * U, 11 * U, 1, .8f, .1f, 1);
        rect(cx - 16 * U, cy - 4.5f * U, 32 * U, 9 * U, diff == i ? .3f : .2f, diff == i ? .32f : .22f, diff == i ? .4f : .3f, 1);
        text(DIFF_NAME[i], cx, cy, .55f, 1, 1, 1, 1);
    }
    text(DIFF_DESC[diff], CX, 73.5f * U, .55f, .8f, .8f, .8f, 1);
    rect(CX - 33 * U, 80 * U, 66 * U, 14 * U, .15f, .6f, .25f, 1);
    text("BOSHLASH", CX, 87 * U, 1.1f, 1, 1, 1, 1);
}
static void menu_tap(float x, float y) {
    for (int i = 0; i < 2; i++)
        if (in(x, y, CX + (i ? 22 : -22) * U, 39 * U, 40 * U, 18 * U)) { lay = i; return; }
    for (int i = 0; i < 3; i++)
        if (in(x, y, CX + (i - 1) * 34 * U, 64 * U, 32 * U, 9 * U)) { diff = i; return; }
    if (in(x, y, CX, 87 * U, 66 * U, 14 * U)) start_match();
}

static void draw(void) {
    glViewport(0, 0, W, H);
    glClearColor(.07f, .07f, .09f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog); glUniform2f(locRes, (float)W, (float)H);
    if (state == S_MENU) draw_menu(); else draw_game();
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
    if (!started) { reset(); started = 1; }
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
