// Do'kon uchun skrinshotlar: main.c ning HAQIQIY chizish kodi dasturiy chizgich bilan 3840x2160 ga chiziladi (keyin 1920x1080 ga kichraytiriladi).
// Ishlatish: shots <papka>   -> papkaga 01_*.ppm ... 06_*.ppm yozadi (store_assets.py ularni PNG ga aylantiradi).
#include <stdio.h>
#include <string.h>
#include <time.h>
#include MAIN_SRC

static double clk = 1000;
static const char *outdir;
static void tk(float dt) { clk += dt; tick(dt, clk); }
static void save(const char *name) {
    draw();
    char p[400]; snprintf(p, sizeof p, "%s/%s.ppm", outdir, name);
    FILE *f = fopen(p, "wb"); if (!f) { perror(p); return; }
    fprintf(f, "P6\n%d %d\n255\n", gl_w, gl_h); fwrite(gl_fb, 3, (size_t)gl_w * gl_h, f); fclose(f);
    printf("  yozildi: %s\n", p);
}
static void touch(int on) { mv.pid = on ? 7 : -1; jl.pid = on ? 8 : -1; jr.pid = on ? 9 : -1; }
static float dist(void) { return hypotf(g.f[0].x - g.f[1].x, g.f[0].y - g.f[1].y); }

int main(int argc, char **argv) {
    outdir = argc > 1 ? argv[1] : ".";
    srand(12);
    W = 3840; H = 2160; gl_w = W; gl_h = H; gl_fb = malloc((size_t)W * H * 3);
    net_init(&net); started = 1; lay = 0; diff = 0; layout(); game_reset(&g);

    state = S_MENU; save("01_menu");

    // 2: 3 joystikli jang (zarba lahzasi)
    lay = 0; diff = 0; start_match();
    while (state != S_FIGHT) tk(1.f / 60);
    int cap = 0; double t0 = clk;
    for (int i = 0; i < 60 * 14 && !cap; i++) {
        mv.vx = mv.vy = 0; jl.vx = jl.vy = jr.vx = jr.vy = 0; touch(1);
        if (dist() > 24) mv.vy = -1;                                  // raqibga yaqinlashamiz (ekranda tepaga)
        int ph = i % 36, arm = (i / 36) & 1;
        Joy *j = arm ? &jr : &jl;
        if (ph >= 3 && ph < 12) { j->vx = arm ? -0.1f : 0.1f; j->vy = -1; }
        tk(1.f / 60);
        if (g.f[1].flash > 0 && (g.f[0].glove[0][0] > 16 || g.f[0].glove[1][0] > 16)) cap = 1;
    }
    printf("  3J kadr: %s (%.1f s)\n", cap ? "zarba tegdi" : "zarba tutilmadi", clk - t0);
    save("02_fight_3joystick");

    // 3: 2 joystikli jang, himoya (blok) lahzasi
    lay = 2; diff = 1; start_match();
    while (state != S_FIGHT) tk(1.f / 60);
    cap = 0; touch(1);
    for (int i = 0; i < 60 * 20 && !cap; i++) {
        mv.vx = mv.vy = 0; jl.vx = 0.35f; jl.vy = 0.95f; jr.vx = -0.35f; jr.vy = 0.95f;   // ikkala qo'l orqa yarimda, ichkariga
        tk(1.f / 60);
        if (g.ai.state == 2 && g.f[0].guard[0] > .5f && dist() < 30) cap = 1;
    }
    printf("  2J kadr: %s\n", cap ? "dushman hujumi paytida himoya" : "hujum tutilmadi");
    save("03_fight_2joystick_block");

    // 4: PvP, mehmon ko'rinishi (180 gradus, ping)
    pvp = 2; me = 1; flip = 1; lay = 0; layout(); touch(1); net.rtt_ms = 23;
    jl.vx = jl.vy = jr.vx = jr.vy = 0; mv.vx = 0; mv.vy = -0.8f; jr.vy = -0.9f; jr.vx = -0.1f;
    save("04_pvp_view");
    pvp = 0; me = 0; flip = 0; touch(0); net.rtt_ms = -1;

    // 5: raund natijasi
    lay = 0; layout(); game_reset(&g); state = S_BREAK; lastres = 1; isko = 1; pw = 1; ew = 0; rnd = 1; stimer = 3; rtime = 0; touch(0);
    mv.vx = mv.vy = jl.vx = jl.vy = jr.vx = jr.vy = 0;
    save("05_round_result");

    // 6: yo'riqnoma, PvP sahifasi
    state = S_HELP; help_page = 5; save("06_pvp_help");
    help_page = -1; save("07_help_catalog");
    return 0;
}
