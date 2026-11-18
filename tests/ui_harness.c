// main.c ning HAQIQIY menyu/tarmoq kodini Linux'da ishga tushiradi (Android/GL stublar bilan) va tugmalarni "bosadi".
// Har bir rol alohida jarayon: host, guestsearch, guestip. Ular loopback orqali bir-biri bilan gaplashadi.
// Chizilgan matnlar ekranda nima ko'rinishini tekshirish uchun yozib olinadi (run.sh main.c nusxasini avtomatik tayyorlaydi).
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
static void text(const char *s, float cx, float cy, float sc, float r, float g_, float b, float a);
#include MAIN_SRC

static char frame[4096], lastframe[4096];
static void text(const char *s, float cx, float cy, float sc, float r, float g_, float b, float a) {
    char t[128]; snprintf(t, sizeof t, "%s", s);
    int l = (int)strlen(t); while (l > 0 && t[l - 1] == '.' && (state != S_IPENTRY)) t[--l] = 0;   // animatsiyali nuqtalarni tashlab ketamiz
    if (strlen(frame) + strlen(t) + 2 < sizeof frame) { strcat(frame, t); strcat(frame, " | "); }
    text_real(s, cx, cy, sc, r, g_, b, a);
}
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } else printf("  ok: %s\n", msg); fflush(stdout); } while (0)
static double T0;
static void step(void) {
    double t = now_sec();
    tick(0.016f, t);
    frame[0] = 0; draw();
    if (strcmp(frame, lastframe)) { printf("[%5.1fs st=%d net=%d] %s\n", t - T0, state, net.state, frame); fflush(stdout); strcpy(lastframe, frame); }
    usleep(16000);
}
static void run_for(double s) { double e = now_sec() + s; while (now_sec() < e) step(); }
#define WAIT(cond, secs) ({ double e_ = now_sec() + (secs); while (now_sec() < e_ && !(cond)) step(); (cond) ? 1 : 0; })

// ---- PvP yordamchilari ----
#include <math.h>
static const char *out_dir(void) { const char *e = getenv("BOKS_OUT"); return e ? e : "/tmp/boks-tests"; }
static void write_state(const char *name) {
    char p[300]; snprintf(p, sizeof p, "%s/%s", out_dir(), name);
    char tmp[320]; snprintf(tmp, sizeof tmp, "%s.tmp", p);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    fprintf(f, "%f %f %f %f %f %f\n", g.f[0].x, g.f[0].y, g.f[0].hp, g.f[1].x, g.f[1].y, g.f[1].hp);
    fclose(f); rename(tmp, p);
}
static int read_state(const char *name, float v[6], double secs) {
    char p[300]; snprintf(p, sizeof p, "%s/%s", out_dir(), name);
    double e = now_sec() + secs;
    while (now_sec() < e) {
        FILE *f = fopen(p, "r");
        if (f) { int n = fscanf(f, "%f %f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]); fclose(f); if (n == 6) return 1; }
        step();
    }
    return 0;
}
static void tap_px(float x, float y);
static void tap_topic(int i) { tap_px(CX + ((i % 2) ? 45 : -45) * U, (32 + (i / 2) * 19) * U); }
static float dist01(void) { return hypotf(g.f[0].x - g.f[1].x, g.f[0].y - g.f[1].y); }
static void flick(Joy *j, double hold, double rest) { j->vy = -1; run_for(hold); j->vy = 0; run_for(rest); }

static void tap_px(float x, float y) {
    AInputEvent d = { AINPUT_EVENT_TYPE_MOTION, AMOTION_EVENT_ACTION_DOWN, 0, x, y }, u = d; u.action = AMOTION_EVENT_ACTION_UP;
    on_input(NULL, &d); on_input(NULL, &u); step();
}
static void tap(float dx, float y) { tap_px(CX + dx * U, y * U); }
static void tap_back(void) { tap_px(18 * U, 91 * U); }
static void key_back(void) {
    AInputEvent k = { AINPUT_EVENT_TYPE_KEY, AKEY_EVENT_ACTION_UP, AKEYCODE_BACK, 0, 0 };
    on_input(NULL, &k); step();
}
static void tap_key(char c) {
    int i = c >= '1' && c <= '9' ? c - '1' : (c == '.' ? 9 : (c == '0' ? 10 : 11));
    tap(((i % 3) - 1) * 24.f, 35 + (i / 3) * 14.f);
}
static void type_ip(const char *s) { for (int i = 0; i < 20; i++) tap_key('<'); for (; *s; s++) tap_key(*s); }

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    srand((unsigned)getpid());
    net_init(&net); net.loopback_test = 1;
    W = 1920; H = 900; layout(); started = 1; game_reset(&g);
    T0 = now_sec();
    const char *role = argv[1];

    if (!strcmp(role, "host")) {
        int rounds = argc > 2 ? atoi(argv[2]) : 2;
        tap_px(22 * U, 8.5f * U);
        CHECK(state == S_ONLINE, "ONLAYN tugmasi onlayn ekranini ochdi");
        tap(0, 42);
        CHECK(state == S_HOSTING && net.state == NET_HOSTING, "XONA OCHISH: xona ochildi");
        for (int r = 0; r < rounds; r++) {
            CHECK(WAIT(state == S_LOBBY, 30), "mehmon ulandi, lobbi ochildi");
            run_for(2.0);
            CHECK(net.state == NET_CONNECTED && net.rtt_ms >= 0, "ulanish turibdi, ping o'lchandi");
            printf("  ping=%.1f ms, raqib=%s\n", net.rtt_ms, net.peer_name);
            CHECK(WAIT(net.state == NET_ENDED, 15) && net.err == NETERR_PEER_LEFT, "mehmon chiqib ketdi: RAQIB CHIQIB KETDI");
            run_for(0.3);
            tap_back();
            CHECK(state == S_HOSTING && net.state == NET_HOSTING, "QAYTA OCH: xona qayta ochildi");
        }
        tap_back(); CHECK(state == S_ONLINE && net.state == NET_IDLE, "BEKOR: onlayn menyuga, xona yopildi");
        key_back(); CHECK(state == S_MENU, "Android orqaga tugmasi: asosiy menyu");
    } else if (!strcmp(role, "guestsearch")) {
        run_for(0.5);
        tap_px(22 * U, 8.5f * U);
        tap(0, 64);
        CHECK(state == S_SEARCH && net.state == NET_SEARCHING, "XONAGA QO'SHILISH: qidiruv boshlandi");
        CHECK(WAIT(net.nrooms > 0, 12), "xona ro'yxatda paydo bo'ldi");
        run_for(0.2);
        tap(0, 28);
        CHECK(state == S_LOBBY, "xona qatoriga tegildi: ulanish ekrani");
        CHECK(WAIT(net.state == NET_CONNECTED, 6), "ULANDI");
        run_for(2.0);
        CHECK(net.state == NET_CONNECTED && net.rtt_ms >= 0, "ulanish turibdi, ping o'lchandi");
        printf("  ping=%.1f ms, raqib=%s\n", net.rtt_ms, net.peer_name);
        tap_back(); CHECK(state == S_SEARCH && net.state == NET_SEARCHING, "ORQAGA: qidiruvga qaytdi (BYE yuborildi)");
        tap_back(); CHECK(state == S_ONLINE, "ORQAGA: onlayn menyu");
        key_back(); CHECK(state == S_MENU, "orqaga tugmasi: asosiy menyu");
    } else if (!strcmp(role, "guestip")) {
        run_for(0.5);
        tap_px(22 * U, 8.5f * U);
        tap(0, 64);
        tap(46, 91);
        CHECK(state == S_IPENTRY, "IP KIRITISH ekrani ochildi");
        printf("  oldindan to'ldirilgan: '%s'\n", ipbuf);
        type_ip("1.2");
        CHECK(!strcmp(ipbuf, "1.2"), "klaviatura: 1.2 yozildi, O'CH eski matnni o'chirdi");
        tap(52, 56);
        CHECK(state == S_IPENTRY && !strcmp(online_msg, "IP NOTO'G'RI"), "to'liq bo'lmagan IP: IP NOTO'G'RI");
        type_ip("127.0.0.1");
        CHECK(!strcmp(ipbuf, "127.0.0.1"), "127.0.0.1 klaviaturadan kiritildi");
        tap_key('.'); tap_key('5'); tap_key('.'); tap_key('.');
        CHECK(!strcmp(ipbuf, "127.0.0.15"), "4-nuqtadan keyingi ortiqcha nuqtalar rad etildi");
        type_ip("127.0.0.1");
        tap(52, 56);
        CHECK(state == S_LOBBY && net.state == NET_CONNECTING, "ULANISH: ulanmoqda");
        CHECK(WAIT(net.state == NET_CONNECTED, 6), "IP orqali ULANDI");
        run_for(2.0);
        CHECK(net.state == NET_CONNECTED && !strncmp(net.peer_name, "BOKSCHI-", 8), "ulanish turibdi, raqib nomi to'g'ri");
        key_back(); CHECK(state == S_SEARCH, "orqaga tugmasi lobbidan qidiruvga qaytardi");
        key_back(); key_back(); CHECK(state == S_MENU, "yana orqaga: asosiy menyu");
    } else if (!strcmp(role, "pvphost")) {
        float v[6];
        char p[300]; snprintf(p, sizeof p, "%s/pvp_A.txt", out_dir()); remove(p);
        snprintf(p, sizeof p, "%s/pvp_B.txt", out_dir()); remove(p);
        tap_px(22 * U, 8.5f * U); tap(0, 42);
        CHECK(WAIT(state == S_LOBBY, 40), "mehmon ulandi");
        run_for(2.5);   // mehmon lobbi matnini tekshirib ulgurishi uchun
        CHECK(strstr(lastframe, "BOSHLASH") != NULL, "lobbida xostga BOSHLASH tugmasi ko'rinadi");
        fatigue_opt = 1;                                  // xost himoya charchog'i qoidasini yoqdi
        run_for(0.3);
        CHECK(strstr(lastframe, "QOIDA: HIMOYA CHARCHOGI YOQIQ") != NULL, "lobbida xostga qoida ko'rinadi");
        tap(0, 78);
        CHECK(pvp == 1 && me == 0 && state == S_INTRO && g.f[0].y > 0 && g.fatigue == 1, "BOSHLASH: xost o'yinni boshladi (charchoq qoidasi bilan)");
        CHECK(WAIT(state == S_FIGHT, 6) && bell_count >= 1, "intro tugadi, jang boshlandi, qo'ng'iroq chalindi");
        jl.vx = .35f; jl.vy = .95f; jr.vx = -.35f; jr.vy = .95f;   // xost himoyani 5.5 s ushlab turadi (charchoq)
        run_for(5.5);
        CHECK(g.f[0].gscale[0] < 0.11f && g.f[0].gscale[1] < 0.11f, "xostda ikkala qo'lning himoya doirasi 10 baravar kichraydi");
        jl.vx = jl.vy = jr.vx = jr.vy = 0;
        CHECK(WAIT(dist01() < 20, 12) && g.f[1].y > -14, "mehmonning yurishi xostda raqibni harakatlantirdi");
        run_for(0.3);
        double te = now_sec() + 10;
        while (now_sec() < te && g.f[1].hp >= 90) { flick(&jr, 0.08, 0.45); flick(&jl, 0.08, 0.45); }
        CHECK(g.f[1].hp < 90, "xost zarbalari mehmonga tegdi (HP kamaydi)");
        run_for(2.0);
        write_state("pvp_A.txt");
        printf("  xost: hp xost=%.1f mehmon=%.1f ping=%.1f\n", g.f[0].hp, g.f[1].hp, net.rtt_ms);
        run_for(2.0);
        rtime = 0.2f;                                   // 1-raundni vaqt bo'yicha tugatamiz
        CHECK(WAIT(state == S_BREAK && stimer > 1.2, 6), "1-raund tugadi (vaqt)");
        CHECK(pw == 1 && ew == 0 && lastres == 1, "xost 1-raundda g'olib: hisob 1:0");
        tap_px(CX, CY);
        CHECK((ready_host == 1 && state == S_BREAK) || state == S_INTRO, "xost tegdi: raqibni kutadi (yoki raund allaqachon boshlandi)");
        CHECK(WAIT(state == S_INTRO && rnd == 2, 8), "ikkalasi tegdi: 2-raund boshlandi");
        CHECK(ready_host == 0 && ready_guest == 0, "tayyorlik bayroqlari tozalandi");
        CHECK(WAIT(state == S_FIGHT, 6), "2-raund jangi");
        run_for(1.0);
        g.f[1].hp = 0;                                  // mehmon nokautga tushdi
        CHECK(WAIT(state == S_END, 3) && finalres == 1 && isko == 1 && pw == 2, "nokaut: o'yin tugadi, xost g'olib 2:0");
        CHECK(WAIT(stimer > 1.2, 3), "yakuniy ekran");
        tap_px(CX, CY);
        CHECK(WAIT(state == S_INTRO && rnd == 1 && pw == 0 && ew == 0, 8), "ikkalasi qayta o'ynashga rozi: yangi o'yin");
        CHECK(WAIT(state == S_FIGHT, 6), "qayta o'yin jangi boshlandi");
        double t0 = now_sec();
        net.sim_loss = 25;                              // 25% paket yo'qotish: ham yuborish, ham qabul qilish
        while (now_sec() < t0 + 4) { flick(&jr, 0.08, 0.3); flick(&jl, 0.08, 0.3); }
        net.sim_loss = 0; jl.vy = jr.vy = 0;
        CHECK(net.state == NET_CONNECTED && pvp == 1, "25% yo'qotishda ulanish uzilmadi");
        while (now_sec() < t0 + 7.0) step();
        write_state("pvp_B.txt");
        printf("  xost (yo'qotishdan keyin): hp xost=%.1f mehmon=%.1f\n", g.f[0].hp, g.f[1].hp);
        double tl = now_sec();
        CHECK(WAIT(state == S_LOBBY && net.state == NET_ENDED && net.err == NETERR_PEER_LEFT && pvp == 0, 15), "mehmon chiqdi: xost lobbida RAQIB CHIQIB KETDI ni ko'radi");
        printf("  xost chiqishni bildi: %.2f s ichida\n", now_sec() - tl);
        (void)v;
    } else if (!strcmp(role, "pvpguest")) {
        float v[6];
        run_for(0.5);
        tap_px(22 * U, 8.5f * U); tap(0, 64);
        CHECK(WAIT(net.nrooms > 0, 20), "xona topildi");
        run_for(0.2); tap(0, 28);
        CHECK(WAIT(net.state == NET_CONNECTED, 8), "ulandi");
        run_for(1.0);
        CHECK(strstr(lastframe, "XOST O'YINNI BOSHLASHINI KUTING") != NULL, "mehmon lobbida xostni kutadi (BOSHLASH tugmasi yo'q)");
        CHECK(WAIT(pvp == 2 && state == S_INTRO, 40), "xost boshladi: mehmon o'yin ekraniga o'tdi");
        CHECK(me == 1 && flip == 1, "mehmon o'zini pastda ko'radi (tasvir 180 gradus aylantirilgan)");
        step(); step();
        CHECK(g.fatigue == 1 && strstr(lastframe, "HIMOYA CHARCHOGI YOQIQ") != NULL, "mehmon qoidani xostdan oldi va raund boshida ko'radi");
        CHECK(WAIT(state == S_FIGHT, 6) && bell_count >= 1, "jang boshlandi, mehmonda ham qo'ng'iroq chalindi");
        float y0 = g.f[1].y;
        mv.vy = -1;                                     // mehmon ekranida tepaga = raqib tomon
        CHECK(WAIT(dist01() < 14, 8), "mehmon raqibga yaqinlashdi (o'z ekranida)");
        mv.vy = 0;
        CHECK(g.f[1].y > y0 + 5, "mehmonning yurishi o'z ekranida ko'rinadi");
        CHECK(WAIT(g.f[0].gscale[0] < 0.15f && g.f[0].guard[0] > 0.5f, 9), "xostning charchagan himoyasi mehmon ekranida kichraydi");
        CHECK(WAIT(g.f[1].hp < 90, 15), "xost zarbalari mehmon ekranida HP ni kamaytirdi");
        CHECK(read_state("pvp_A.txt", v, 20), "xost holati olindi");
        run_for(0.4);
        printf("  mehmon: o'z=(%.2f,%.2f,hp %.2f) raqib=(%.2f,%.2f,hp %.2f)\n", gt.f[1].x, gt.f[1].y, gt.f[1].hp, gt.f[0].x, gt.f[0].y, gt.f[0].hp);
        CHECK(fabsf(gt.f[0].x - v[0]) < .05f && fabsf(gt.f[0].y - v[1]) < .05f && fabsf(gt.f[1].x - v[3]) < .05f && fabsf(gt.f[1].y - v[4]) < .05f, "joylar xostniki bilan mos (< 0.05)");
        CHECK(fabsf(gt.f[0].hp - v[2]) < .05f && fabsf(gt.f[1].hp - v[5]) < .05f, "HP xostniki bilan mos (< 0.05)");
        CHECK(fabsf(g.f[1].x - v[3]) < .2f && fabsf(g.f[1].y - v[4]) < .2f, "chizilayotgan jangchi ham xost holatiga yetib bordi");
        CHECK(WAIT(state == S_BREAK, 12), "1-raund tugadi (xostdan keldi)");
        CHECK(pw == 0 && ew == 1 && lastres == 2 && isko == 0, "hisob mehmon nuqtai nazaridan: 0:1, raund mag'lubiyati");
        CHECK(bell_count >= 2, "raund oxirida qo'ng'iroq chalindi");
        CHECK(WAIT(stimer > 1.2, 4) && strstr(lastframe, "EKRANGA TEGING") != NULL, "tanaffus: keyingi raund uchun tegish taklifi");
        tap_px(CX, CY);
        CHECK(my_ready == 1, "mehmon tegdi: tayyor");
        CHECK(WAIT(state == S_INTRO && rnd == 2, 8), "ikkalasi tegdi: 2-raund boshlandi");
        CHECK(WAIT(state == S_FIGHT, 6), "2-raund jangi");
        CHECK(WAIT(state == S_END, 15), "nokaut: o'yin tugadi");
        CHECK(finalres == 2 && isko == 1 && ew == 2 && pw == 0, "mehmon ekrani: MAG'LUBIYAT, 0:2");
        CHECK(WAIT(stimer > 1.2, 4), "yakuniy ekran");
        tap_px(CX, CY);
        CHECK(WAIT(state == S_INTRO && rnd == 1 && pw == 0 && ew == 0, 8), "qayta o'yin boshlandi (hisob 0:0)");
        CHECK(WAIT(state == S_FIGHT, 6), "qayta o'yin jangi");
        double t0 = now_sec();
        net.sim_loss = 25;
        y0 = g.f[1].y; mv.vy = -1;
        while (now_sec() < t0 + 1.2) step();
        mv.vy = 0;
        while (now_sec() < t0 + 4) { flick(&jl, 0.08, 0.3); flick(&jr, 0.08, 0.3); }
        net.sim_loss = 0; jl.vy = jr.vy = 0;
        CHECK(net.state == NET_CONNECTED && pvp == 2, "25% yo'qotishda ulanish uzilmadi");
        CHECK(g.f[1].y > y0 + 5, "yo'qotish davrida ham harakat xostga yetib bordi");
        while (now_sec() < t0 + 7.3) step();
        CHECK(read_state("pvp_B.txt", v, 10), "xostning oxirgi holati olindi");
        printf("  mehmon (yo'qotishdan keyin): o'z=(%.2f,%.2f,hp %.2f) raqib=(%.2f,%.2f,hp %.2f)\n", gt.f[1].x, gt.f[1].y, gt.f[1].hp, gt.f[0].x, gt.f[0].y, gt.f[0].hp);
        CHECK(fabsf(gt.f[0].x - v[0]) < .05f && fabsf(gt.f[0].y - v[1]) < .05f && fabsf(gt.f[1].x - v[3]) < .05f && fabsf(gt.f[1].y - v[4]) < .05f, "yo'qotishdan keyin joylar mos (< 0.05)");
        CHECK(fabsf(gt.f[0].hp - v[2]) < .05f && fabsf(gt.f[1].hp - v[5]) < .05f, "yo'qotishdan keyin HP mos (< 0.05)");
        {   // prediksiya: xostdan hech narsa kelmasa ham o'z qo'li joystikka darhol javob beradi
            run_for(0.3);
            float rest = g.f[1].glove[1][0];
            net.sim_loss = 100;
            jr.vy = -1;
            for (int i = 0; i < 5; i++) step();
            float own = g.f[1].glove[1][0], host_side = gt.f[1].glove[1][0];
            jr.vy = 0; run_for(0.3); net.sim_loss = 0;
            printf("  qo'lqop oldinga: tinch=%.1f prediksiya=%.1f xostdan(kechikkan)=%.1f\n", rest, own, host_side);
            CHECK(own > rest + 8 && host_side < rest + 3, "paket kelmasa ham o'z qo'li 5 kadrda cho'zildi (prediksiya), xost holati esa eski");
            CHECK(WAIT(fabsf(g.f[1].glove[1][0] - gt.f[1].glove[1][0]) < 0.5f, 3), "aloqa tiklangach o'z qo'li xost holatiga mos");
        }
        app_paused();                                    // ilova fonga o'tdi (bosh ekran / boshqa ilova)
        CHECK(state == S_MENU && pvp == 0 && flip == 0 && net.state == NET_IDLE, "fonga o'tish: menyuga, PvP tozalandi, raqibga BYE ketdi");
        run_for(0.3);
    } else if (!strcmp(role, "settings")) {
        snprintf(data_path, sizeof data_path, "%s", out_dir());
        char p[320]; snprintf(p, sizeof p, "%s/boks.cfg", out_dir()); remove(p);
        settings_load();
        CHECK(lay == 0 && diff == 0, "fayl yo'q: standart sozlamalar");
        state = S_MENU; layout();
        tap_px(CX + 38 * U, 39 * U); tap_px(CX + 34 * U, 65 * U);
        CHECK(lay == 2 && diff == 2, "menyu: 2 joystik va 3-qiyinlik tanlandi");
        lay = 0; diff = 0; settings_load();
        CHECK(lay == 2 && diff == 2, "qayta ishga tushganda sozlamalar tiklandi");
        FILE *f = fopen(p, "w"); fputs("GARBAGE 9 9\n", f); fclose(f);
        lay = 1; diff = 1; settings_load();
        CHECK(lay == 1 && diff == 1, "buzuq fayl sozlamalarni o'zgartirmaydi");
        f = fopen(p, "w"); fputs("BOKS1 7 -1\n", f); fclose(f);
        lay = 1; diff = 1; settings_load();
        CHECK(lay == 1 && diff == 1, "chegaradan tashqari qiymatlar rad etildi");
        data_path[0] = 0; lay = 1; settings_save(); settings_load();
        CHECK(lay == 1, "papka berilmasa xavfsiz (hech narsa yozilmaydi)");
        // himoya charchog'i tugmasi va saqlanishi
        state = S_MENU; layout(); fatigue_opt = 0; lay = 0; diff = 0;
        tap_px(CX, 80.5f * U);
        CHECK(fatigue_opt == 1, "menyu: himoya charchog'i yoqildi");
        step();
        CHECK(strstr(lastframe, "HIMOYA CHARCHOGI - YOQIQ") != NULL, "tugma matni YOQIQ");
        data_path[0] = 0; snprintf(data_path, sizeof data_path, "%s", out_dir()); settings_save();
        fatigue_opt = 0; settings_load();
        CHECK(fatigue_opt == 1, "charchoq qoidasi saqlandi va qayta ishga tushganda tiklandi");
        f = fopen(p, "w"); fputs("BOKS2 1 1 5\n", f); fclose(f); fatigue_opt = 0; lay = 0; settings_load();
        CHECK(fatigue_opt == 0 && lay == 0, "BOKS2 da noto'g'ri qiymat rad etildi");
        f = fopen(p, "w"); fputs("BOKS1 2 1\n", f); fclose(f); lay = 0; diff = 0; settings_load();
        CHECK(lay == 2 && diff == 1, "eski BOKS1 formati ham o'qiladi");
        // yo'riqnoma: katalog + mavzular
        state = S_MENU; layout();
        tap_px(W - 22 * U, 8.5f * U);
        CHECK(state == S_HELP && help_page == -1, "YORIQNOMA katalogi ochildi");
        step();
        CHECK(strstr(lastframe, "QIYINLIK DARAJALARI") != NULL && strstr(lastframe, "HIMOYA CHARCHOGI") != NULL && strstr(lastframe, "2 TELEFONDA O'YNASH") != NULL, "katalogda hamma 6 mavzu ko'rinadi");
        tap_topic(2);
        CHECK(help_page == 2, "katalogdan 3-mavzu (2 joystik) ochildi");
        tap_px(CX, CY);
        CHECK(help_page == -1, "sahifada teginish katalogga qaytaradi");
        tap_topic(3); step();
        CHECK(help_page == 3 && strstr(lastframe, "OSON:") != NULL && strstr(lastframe, "O'RTA:") != NULL && strstr(lastframe, "QIYIN:") != NULL, "qiyinlik darajalari mavzusi: oson, o'rta, qiyin");
        key_back();
        CHECK(state == S_HELP && help_page == -1, "orqaga tugmasi sahifadan katalogga qaytaradi");
        tap_topic(4); step();
        CHECK(help_page == 4 && strstr(lastframe, "10 BARAVAR") != NULL && strstr(lastframe, "3 SONIYADAN") != NULL, "himoya charchog'i mavzusi: 3 soniya va 10 baravar");
        tap_px(CX, CY);
        tap_topic(5); step();
        CHECK(help_page == 5 && strstr(lastframe, "WI-FI") != NULL, "2 telefonda o'ynash mavzusi: Wi-Fi/hotspot");
        tap_px(CX, CY);
        CHECK(help_page == -1, "katalog qayta ko'rinadi");
        tap_px(CX, 91 * U);
        CHECK(state == S_MENU, "katalogdagi ORQAGA tugmasi menyuga qaytaradi");
        tap_px(W - 22 * U, 8.5f * U); key_back();
        CHECK(state == S_MENU, "orqaga tugmasi (katalogda) menyuga qaytaradi");
    } else if (!strcmp(role, "guestbad")) {
        // hech kim yo'q manzilga ulanish: VAQT TUGADI ekrani
        run_for(0.3);
        tap_px(22 * U, 8.5f * U); tap(0, 64); tap(46, 91);
        type_ip("127.0.0.1");
        tap(52, 56);
        CHECK(WAIT(net.state == NET_ENDED, 8) && net.err == NETERR_TIMEOUT, "javobsiz manzil: ULANIB BO'LMADI (4 s)");
        run_for(0.3);
    }
    printf("%s %s (%d xato)\n", role, fails ? "XATO" : "OK", fails);
    return fails ? 1 : 0;
}
