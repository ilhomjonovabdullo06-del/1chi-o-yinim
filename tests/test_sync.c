// sync.c sinovi: paketlash aniqligi, chegaraviy qiymatlar, buzuq paketlar, yumshatish, to'liq o'yin bo'yicha mos kelish.
#include "../app/src/main/cpp/sync.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } else printf("  ok: %s\n", msg); } while (0)

int main(void) {
    Game g, d; MatchInfo m = { 3, 2, 1, 0, 1, 3, 1, 1, 0, 61.37f, 4.25f }, o;
    game_reset(&g); memset(&d, 0, sizeof d);
    g.f[0].x = 12.345f; g.f[0].y = 33.9f; g.f[0].hp = 87.654f; g.f[0].flash = 0.1f;
    g.f[1].x = -33.7f; g.f[1].y = -19.01f; g.f[1].hp = 0.5f;
    g.f[0].glove[1][0] = 22.5f; g.f[0].glove[1][1] = -4.2f; g.f[0].guard[0] = 0.73f;
    g.f[1].glove[0][0] = 3.3f;  g.f[1].glove[0][1] = 2.2f;  g.f[1].guard[1] = 1.0f;
    g.f[0].gscale[0] = 0.37f; g.f[1].gscale[1] = 0.1f; g.fatigue = 1;
    uint8_t b[64];

    printf("T1: holat paketi aylanishi (pack -> unpack)\n");
    int n = sync_pack_snapshot(b, &g, &m);
    CHECK(n == SYNC_SNAP_LEN && n == 48, "uzunlik 48 bayt");
    CHECK(sync_unpack_snapshot(b, n, &d, &o), "yaroqli paket qabul qilindi");
    CHECK(fabsf(d.f[0].x - g.f[0].x) < 0.005f && fabsf(d.f[1].y - g.f[1].y) < 0.005f, "joylar aniqligi < 0.005 birlik");
    CHECK(fabsf(d.f[0].hp - g.f[0].hp) < 0.01f && fabsf(d.f[1].hp - 0.5f) < 0.01f, "HP aniqligi < 0.01");
    CHECK(fabsf(d.f[0].glove[1][0] - 22.5f) < 0.005f && fabsf(d.f[0].glove[1][1] + 4.2f) < 0.005f, "qo'lqop siljishi to'g'ri");
    CHECK(fabsf(d.f[0].guard[0] - 0.73f) < 0.004f && fabsf(d.f[1].guard[1] - 1.f) < 0.004f, "himoya darajasi to'g'ri");
    CHECK(d.f[0].flash > 0 && d.f[1].flash == 0, "zarba chaqnashi bayrog'i");
    CHECK(fabsf(d.f[0].gscale[0] - 0.37f) < 0.004f && fabsf(d.f[1].gscale[1] - 0.1f) < 0.004f && d.fatigue == 1, "charchoq: doira ko'paytiruvchisi va qoida bayrog'i");
    CHECK(o.mstate == 3 && o.rnd == 2 && o.pw == 1 && o.ew == 0 && o.lastres == 1 && o.finalres == 3, "raund ma'lumotlari");
    CHECK(o.isko == 1 && o.ready_host == 1 && o.ready_guest == 0, "bayroqlar: nokaut/tayyorlik");
    CHECK(fabsf(o.rtime - 61.37f) < 0.011f && fabsf(o.stimer - 4.25f) < 0.011f, "taymerlar aniqligi 0.01 s");

    printf("T2: chegaraviy qiymatlar xavfsiz\n");
    Game e = g; e.f[0].hp = -5; e.f[1].hp = 1e9f; e.f[0].x = 500; e.f[0].y = -500; e.f[1].guard[0] = 7; e.f[1].guard[1] = -3;
    MatchInfo me2 = m; me2.rtime = -3; me2.stimer = 1e9f;
    n = sync_pack_snapshot(b, &e, &me2);
    CHECK(sync_unpack_snapshot(b, n, &d, &o), "qabul qilindi");
    CHECK(d.f[0].hp == 0 && d.f[1].hp <= 300.f && d.f[1].hp > 0, "HP 0..300 oralig'iga qisqartirildi");
    CHECK(d.f[1].guard[0] <= 1.f && d.f[1].guard[1] >= 0.f, "himoya 0..1 ga qisqartirildi");
    CHECK(o.rtime == 0 && o.stimer <= 655.36f, "taymer manfiy bo'lmaydi, to'lib ketmaydi");

    printf("T3: buzuq paketlar rad etiladi\n");
    n = sync_pack_snapshot(b, &g, &m);
    CHECK(!sync_unpack_snapshot(b, n - 1, &d, &o) && !sync_unpack_snapshot(b, n + 1, &d, &o), "noto'g'ri uzunlik");
    b[0] = SYNC_VER + 1; CHECK(!sync_unpack_snapshot(b, n, &d, &o), "boshqa protokol versiyasi"); b[0] = SYNC_VER;
    uint8_t junk[SYNC_SNAP_LEN]; srand(5); int accepted = 0;
    for (int i = 0; i < 1000; i++) { for (int k = 0; k < SYNC_SNAP_LEN; k++) junk[k] = rand(); junk[0] = SYNC_VER; if (sync_unpack_snapshot(junk, SYNC_SNAP_LEN, &d, &o)) {
        if (!(isfinite(d.f[0].x) && isfinite(d.f[1].hp))) accepted = -999999; else accepted++; } }
    CHECK(accepted > 0, "tasodifiy baytlar chiqishni buzmaydi (barcha qiymatlar chekli)");

    printf("T4: kirish paketi\n");
    Input in; memset(&in, 0, sizeof in); in.mvx = -0.5f; in.mvy = 1.f; in.arm[0][0] = 0.35f; in.arm[0][1] = -0.95f; in.arm[1][0] = -2.f; in.arm[1][1] = 0.2f;
    n = sync_pack_input(b, &in, 1, 1);
    Input ri; int two, rdy;
    CHECK(n == SYNC_INPUT_LEN && sync_unpack_input(b, n, &ri, &two, &rdy), "8 bayt, qabul qilindi");
    CHECK(fabsf(ri.mvx + .5f) < 0.01f && fabsf(ri.mvy - 1.f) < 0.01f && fabsf(ri.arm[0][1] + .95f) < 0.01f, "qiymatlar aniqligi < 1/127");
    CHECK(ri.arm[1][0] == -1.f, "diapazondan tashqari qiymat -1 gacha qisqartirildi");
    CHECK(two == 1 && rdy == 1, "ikki joystik va tayyorlik bayroqlari");
    CHECK(!sync_unpack_input(b, 7, &ri, &two, &rdy), "qisqa paket rad etildi");

    printf("T5: yumshatish\n");
    Game r; game_reset(&r); Game t = r;
    t.f[0].x = 10; t.f[0].y = 22; t.f[0].glove[1][0] = 20;
    for (int i = 0; i < 6; i++) sync_blend(&r, &t, 0.5f);
    CHECK(fabsf(r.f[0].x - 10) < 0.2f && fabsf(r.f[0].glove[1][0] - 20) < 0.3f, "nishonga yaqinlashadi");
    r.f[1].x = 0; r.f[1].y = -22; t.f[1].x = 20; t.f[1].y = 10;   // katta sakrash
    sync_blend(&r, &t, 0.1f);
    CHECK(r.f[1].x == 20 && r.f[1].y == 10, "katta sakrash (yangi raund) darhol o'tadi");

    printf("T6: haqiqiy o'yin: xost simulyatsiyasi -> mehmon (60 Hz, 15%% yo'qotish) mos keladi\n");
    Game host, guest, tgt; MatchInfo mh = { 2, 1, 0, 0, 3, 3, 0, 0, 0, 90, 0 }, mg;
    game_reset(&host); game_reset(&guest); game_reset(&tgt); srand(3);
    float maxerr = 0; int sent = 0, lost = 0;
    for (int i = 0; i < 60 * 20; i++) {
        float tt = i / 60.f; Input ins[2]; memset(ins, 0, sizeof ins);
        ins[0].mvy = (tt < 4) ? -0.8f : 0; ins[1].mvx = sinf(tt) * 0.5f;
        if (((int)(tt * 2)) % 3 == 0) ins[0].arm[1][1] = 1.f;
        if (((int)(tt * 3)) % 4 == 1) ins[1].arm[0][1] = -0.9f;
        game_step(&host, 1.f / 60, ins);
        {
            n = sync_pack_snapshot(b, &host, &mh); sent++;
            if (rand() % 100 < 15) lost++; else sync_unpack_snapshot(b, n, &tgt, &mg);
        }
        sync_blend(&guest, &tgt, 1.f - expf(-40.f / 60.f)); game_refresh(&guest);
        if (i > 120) { float e1 = hypotf(guest.f[0].x - host.f[0].x, guest.f[0].y - host.f[0].y);
                       float e2 = hypotf(guest.f[1].x - host.f[1].x, guest.f[1].y - host.f[1].y); if (e1 > maxerr) maxerr = e1; if (e2 > maxerr) maxerr = e2; }
    }
    printf("  yuborilgan=%d yo'qolgan=%d, eng katta joy xatosi=%.2f birlik\n", sent, lost, maxerr);
    CHECK(maxerr < 2.0f, "15% yo'qotishda ham joy xatosi < 2 birlik (jangchi radiusi 5.5)");
    CHECK(fabsf(guest.f[0].hp - host.f[0].hp) < 1.f && fabsf(guest.f[1].hp - host.f[1].hp) < 1.f, "oxirida HP mos");
    printf("%s (%d xato)\n", fails ? "XATO BOR" : "HAMMASI O'TDI", fails);
    return fails;
}
