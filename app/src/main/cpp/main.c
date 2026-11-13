// BOKS - Android (NDK, NativeActivity, GLES2), ustdan ko'rinish
// Chap-past joystik: yurish. Yuqori-chap / yuqori-o'ng: chap / o'ng qo'l.
// Bitta qo'l joystigi tez surilsa: shu yo'nalishga zarba.
// Ikkala qo'l joystigi birga ushlansa: GUARD (blok) + tanani egish/yelka burish.

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#define PI 3.14159265f
#define SEG 28

typedef struct { float cx, cy, r, vx, vy; int pid; } Joy;
typedef struct { float t, dx, dy; int hit, was; } Arm;
typedef struct { float bx, by, fx, fy, qx, qy, hx, hy; } Pose;

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
static int estate, ehand, ehit, over, guard;

#define PUNCH_T 0.28f
#define EPUNCH_T 0.22f

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

/* ---------- o'yin holati ---------- */
static void layout(void) {
    U = H / 100.f; CX = W / 2.f; CY = H / 2.f; RING = 40 * U;
    mv.cx = 20 * U;     mv.cy = 76 * U; mv.r = 13 * U;
    jl.cx = 15 * U;     jl.cy = 18 * U; jl.r = 12 * U;
    jr.cx = W - 15 * U; jr.cy = 18 * U; jr.r = 12 * U;
}
static void reset(void) {
    px = CX; py = CY + 22 * U; ex = CX; ey = CY - 22 * U;
    lx = ly = tw = 0; php = ehp = 100; ecd = 1.5f; etimer = 0; eblock = 0;
    eflash = pflash = 0; estate = 0; ehit = 0; over = 0; guard = 0;
    al.t = ar.t = -1; al.was = ar.was = 0;
}
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static Pose player_pose(void) {
    Pose p;
    float a = atan2f(ey - py, ex - px);
    p.fx = cosf(a); p.fy = sinf(a);
    p.bx = px + lx * 4 * U; p.by = py + ly * 4 * U;
    float sa = a + tw;
    p.qx = -sinf(sa); p.qy = cosf(sa);           // yelka o'qi (o'ng tomonga)
    p.hx = p.bx + lx * 2 * U + p.fx * 1.5f * U;  // bosh tanadan ko'proq egiladi
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
            ehp -= eblock > 0 ? 1.f : 7.f;
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

static void update(float dt) {
    now_t += dt;
    if (over) return;

    // yurish
    px += mv.vx * 30 * U * dt; py += mv.vy * 30 * U * dt;

    // guard va tana boshqaruvi
    float lm = hypotf(jl.vx, jl.vy), rm = hypotf(jr.vx, jr.vy);
    guard = (lm > 0.25f && rm > 0.25f);
    float k = clampf(12 * dt, 0, 1);
    float tlx = 0, tly = 0, ttw = 0;
    if (guard) {
        tlx = (jl.vx + jr.vx) / 2; tly = (jl.vy + jr.vy) / 2;
        ttw = (jr.vy - jl.vy) * 0.6f;   // yelkalar burilishi
    }
    lx += (tlx - lx) * k; ly += (tly - ly) * k; tw += (ttw - tw) * k;

    // jismlar to'qnashuvi
    float dx = px - ex, dy = py - ey, d = hypotf(dx, dy) + 0.001f, mind = 12 * U;
    if (d < mind) {
        float push = (mind - d) / 2;
        px += dx / d * push; py += dy / d * push; ex -= dx / d * push; ey -= dy / d * push;
    }
    float lim = RING - 6 * U;
    px = clampf(px, CX - lim, CX + lim); py = clampf(py, CY - lim, CY + lim);
    ex = clampf(ex, CX - lim, CX + lim); ey = clampf(ey, CY - lim, CY + lim);

    // zarbalar
    arm_update(&al, &jl, 0, dt);
    arm_update(&ar, &jr, 1, dt);

    // dushman AI
    eblock -= dt; eflash -= dt; pflash -= dt; ecd -= dt;
    dx = px - ex; dy = py - ey; d = hypotf(dx, dy) + 0.001f;
    float nx = dx / d, ny = dy / d;
    if (estate == 0) {
        float sp = 0;
        if (d > 26 * U) sp = 16 * U; else if (d < 20 * U) sp = -12 * U;
        float st = sinf(now_t * 1.3f) * 8 * U;
        ex += (nx * sp - ny * st) * dt; ey += (ny * sp + nx * st) * dt;
        if (ecd <= 0 && d < 28 * U) { estate = 1; etimer = 0.35f; ehand = rand() & 1; }
    } else if (estate == 1) {
        etimer -= dt;
        if (etimer <= 0) { estate = 2; etimer = EPUNCH_T; ehit = 0; }
    } else {
        etimer -= dt;
        float ext = sinf(PI * clampf(1 - etimer / EPUNCH_T, 0, 1));
        if (!ehit && ext > 0.8f) {
            ehit = 1;
            if (d < 26 * U) {
                float dmg = 8;
                if (guard) dmg = (hypotf(lx, ly) > 0.6f) ? 0 : 1.2f; // egilib qochish / blok
                if (dmg > 0) pflash = 0.15f;
                php -= dmg;
            }
        }
        if (etimer <= 0) { estate = 0; ecd = 0.7f + 1.2f * (rand() / (float)RAND_MAX); }
    }

    if (ehp <= 0) over = 1;
    else if (php <= 0) over = 2;
}

/* ---------- chizish ---------- */
static void draw_fighter(float bx, float by, float fx, float fy, float qx, float qy,
                         float hx, float hy, float fist[2][2],
                         float cr, float cg, float cb, float flash) {
    float k = flash > 0 ? 0.6f : 0;
    line(bx - qx * 5 * U, by - qy * 5 * U, bx + qx * 5 * U, by + qy * 5 * U, 4 * U, cr * .7f + k, cg * .7f + k, cb * .7f + k, 1);
    circle(bx, by, 5.5f * U, cr + k, cg + k, cb + k, 1);
    for (int i = 0; i < 2; i++) {
        float s = i ? 1.f : -1.f;
        line(bx + qx * 5 * U * s, by + qy * 5 * U * s, fist[i][0], fist[i][1], 1.6f * U, cr * .6f, cg * .6f, cb * .6f, 1);
    }
    circle(hx, hy, 3.2f * U, cr * .8f + k, cg * .8f + k, cb * .8f + k, 1);
    for (int i = 0; i < 2; i++) circle(fist[i][0], fist[i][1], 2.6f * U, .9f, .1f, .1f, 1);
    (void)fx; (void)fy;
}

static void draw_joy(const Joy *j) {
    circle(j->cx, j->cy, j->r, 1, 1, 1, 0.15f);
    circle(j->cx + j->vx * j->r, j->cy + j->vy * j->r, j->r * 0.45f, 1, 1, 1, j->pid >= 0 ? 0.7f : 0.35f);
}

static void draw(void) {
    glViewport(0, 0, W, H);
    glClearColor(.07f, .07f, .09f, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog); glUniform2f(locRes, (float)W, (float)H);

    // ring
    rect(CX - RING, CY - RING, 2 * RING, 2 * RING, .22f, .24f, .32f, 1);
    float t = 1.2f * U;
    rect(CX - RING, CY - RING, 2 * RING, t, .8f, .1f, .1f, 1);
    rect(CX - RING, CY + RING - t, 2 * RING, t, .8f, .1f, .1f, 1);
    rect(CX - RING, CY - RING, t, 2 * RING, .8f, .1f, .1f, 1);
    rect(CX + RING - t, CY - RING, t, 2 * RING, .8f, .1f, .1f, 1);

    // dushman
    {
        float dx = px - ex, dy = py - ey, d = hypotf(dx, dy) + 0.001f;
        float nx = dx / d, ny = dy / d, qx = -ny, qy = nx;
        float fist[2][2];
        float ext = sinf(PI * clampf(1 - etimer / EPUNCH_T, 0, 1));
        for (int h = 0; h < 2; h++) {
            float s = h ? 1.f : -1.f;
            float sx = ex + qx * 5 * U * s, sy = ey + qy * 5 * U * s;
            float fx = sx + nx * 6 * U, fy = sy + ny * 6 * U;
            if (eblock > 0) { fx = ex + nx * 8 * U + qx * 2.6f * U * s; fy = ey + ny * 8 * U + qy * 2.6f * U * s; }
            else if (estate == 1 && h == ehand) { fx = sx + nx * U; fy = sy + ny * U; }
            else if (estate == 2 && h == ehand) { fx = sx + nx * (6 + 14 * ext) * U; fy = sy + ny * (6 + 14 * ext) * U; }
            fist[h][0] = fx; fist[h][1] = fy;
        }
        draw_fighter(ex, ey, nx, ny, qx, qy, ex + nx * 1.5f * U, ey + ny * 1.5f * U, fist, .85f, .4f, .2f, eflash);
        if (estate == 1) circle(ex, ey, 8 * U, 1, .2f, .2f, 0.25f);   // ogohlantirish
    }
    // o'yinchi
    {
        Pose p = player_pose();
        float fist[2][2];
        player_fist(&p, 0, &al, &fist[0][0], &fist[0][1]);
        player_fist(&p, 1, &ar, &fist[1][0], &fist[1][1]);
        draw_fighter(p.bx, p.by, p.fx, p.fy, p.qx, p.qy, p.hx, p.hy, fist, .2f, .5f, 1.f, pflash);
        if (guard) circle(p.bx, p.by, 9 * U, .3f, .7f, 1, 0.2f);
    }

    // HP
    rect(CX - 38 * U, 3 * U, 36 * U, 3 * U, .2f, .2f, .2f, 1);
    rect(CX - 38 * U, 3 * U, 36 * U * clampf(php / 100, 0, 1), 3 * U, .2f, .6f, 1, 1);
    rect(CX + 2 * U, 3 * U, 36 * U, 3 * U, .2f, .2f, .2f, 1);
    rect(CX + 2 * U, 3 * U, 36 * U * clampf(ehp / 100, 0, 1), 3 * U, .9f, .4f, .2f, 1);

    draw_joy(&mv); draw_joy(&jl); draw_joy(&jr);

    if (over) {
        if (over == 1) rect(0, 0, (float)W, (float)H, 0, .8f, .2f, 0.4f);   // g'alaba
        else rect(0, 0, (float)W, (float)H, .9f, 0, 0, 0.4f);               // mag'lubiyat
    }
}

/* ---------- kirish (multitouch) ---------- */
static int32_t on_input(struct android_app *app, AInputEvent *e) {
    (void)app;
    if (AInputEvent_getType(e) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int32_t act = AMotionEvent_getAction(e);
    int32_t a = act & AMOTION_EVENT_ACTION_MASK;
    int idx = (act & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    Joy *js[3] = { &mv, &jl, &jr };

    if (a == AMOTION_EVENT_ACTION_DOWN || a == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        if (over) { reset(); return 1; }
        int id = AMotionEvent_getPointerId(e, idx);
        float x = AMotionEvent_getX(e, idx), y = AMotionEvent_getY(e, idx);
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
        for (int i = 0; i < 3; i++) { js[i]->pid = -1; js[i]->vx = js[i]->vy = 0; }
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
    mv.pid = jl.pid = jr.pid = -1; mv.vx = mv.vy = jl.vx = jl.vy = jr.vx = jr.vy = 0;
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
    mv.pid = jl.pid = jr.pid = -1;
    srand((unsigned)time(NULL));
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
