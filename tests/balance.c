// Balans o'lchovi: botlar AI ga qarshi 90 soniyalik raund o'ynaydi. Natijalar chop etiladi va chegaralarga tekshiriladi.
#include "../app/src/main/cpp/game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#define DT (1.f/60)
enum { AGGR, DEF, SKILL };
typedef struct { float dealt, taken, ko_time, ko_time_me; int hits_in, hits_out; } Res;

static int g_fat = 0;   // himoya charchog'i qoidasi
static Res play(int bot, float react, int diff, unsigned seed, float secs) {
    Game g; game_reset(&g); g.fatigue = g_fat; srand(seed);
    Res r; memset(&r, 0, sizeof r); r.ko_time = r.ko_time_me = -1;
    float since = 0, cd = 0.3f; int arm = 0; float hold = 0;
    for (int i = 0; i < (int)(secs * 60); i++) {
        float t = i * DT; Input in[2]; memset(in, 0, sizeof in);
        Fighter *me = &g.f[0], *op = &g.f[1];
        float dx = op->x - me->x, dy = op->y - me->y, d = hypotf(dx, dy) + 1e-3f;
        float want = 20.f, sp = d > want + 2 ? 1.f : (d < want - 3 ? -1.f : 0.f);
        in[0].mvx = dx / d * sp * 0.5f; in[0].mvy = dy / d * sp * 0.5f;
        int guard = 0, attack_ok = 1;
        if (bot == DEF) guard = 1;
        if (bot == SKILL) {
            if (g.ai.state != 0) since += DT; else since = 0;
            if (g.ai.state != 0 && since >= react) guard = 1;
            if (ai_blocking(&g, diff)) attack_ok = 0;
            if (g.ai.state != 0 && since < react) attack_ok = 0;   // zarbaga tayyorlanyapti
        }
        if (guard) { in[0].arm[0][0] = .35f; in[0].arm[0][1] = -.95f; in[0].arm[1][0] = -.35f; in[0].arm[1][1] = -.95f; }
        else if (bot != DEF && attack_ok) {
            cd -= DT;
            if (cd <= 0 && hold <= 0) { hold = 0.08f; cd = 0.45f; arm ^= 1; }
        }
        if (hold > 0) { if (!guard) in[0].arm[arm][1] = 1.f; hold -= DT; }
        ai_input(&g, DT, diff, &in[1]);
        float h0 = me->hp, h1 = op->hp;
        game_step(&g, DT, in);
        r.taken += h0 - (me->hp > 0 ? me->hp : 0); r.dealt += h1 - (op->hp > 0 ? op->hp : 0);
        if (r.ko_time < 0 && op->hp <= 0) r.ko_time = t;
        if (r.ko_time_me < 0 && me->hp <= 0) r.ko_time_me = t;
        (void)dx;
        if (op->hp <= 0 || me->hp <= 0) break;                      // KO: raund tugadi
    }
    r.hits_out = g.hits[0]; r.hits_in = g.hits[1];
    return r;
}

static void report(const char *name, int bot, float react, int diff, Res *out) {
    Res s; memset(&s, 0, sizeof s); int kos = 0, kome = 0; float ko_sum = 0, kome_sum = 0; int N = 8;
    for (int k = 0; k < N; k++) {
        Res r = play(bot, react, diff, 100 + k, 90);
        s.dealt += r.dealt; s.taken += r.taken; s.hits_out += r.hits_out; s.hits_in += r.hits_in;
        if (r.ko_time > 0) { kos++; ko_sum += r.ko_time; }
        if (r.ko_time_me > 0) { kome++; kome_sum += r.ko_time_me; }
    }
    s.dealt /= N; s.taken /= N;
    printf("%-34s diff=%d | bot beradi %5.1f  oladi %5.1f | zarba: %4.1f/%4.1f | bot KO qiladi %d/%d (o'rtacha %4.1f s) | bot KO bo'ladi %d/%d (%4.1f s)\n",
           name, diff, s.dealt, s.taken, s.hits_out / (float)N, s.hits_in / (float)N, kos, N, kos ? ko_sum / kos : 0, kome, N, kome ? kome_sum / kome : 0);
    if (out) { *out = s; out->ko_time = kos ? ko_sum / kos : -1; out->ko_time_me = kome ? kome_sum / kome : -1; out->hits_out = kos; out->hits_in = kome; }
}

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } else printf("  ok: %s\n", msg); } while (0)

int main(void) {
    Res a[3], d[3], s25[3], s35[3];
    for (int diff = 0; diff < 3; diff++) report("HUJUMCHI (himoyasiz)", AGGR, 0, diff, &a[diff]);
    for (int diff = 0; diff < 3; diff++) report("HIMOYACHI (doim blok)", DEF, 0, diff, &d[diff]);
    for (int diff = 0; diff < 3; diff++) report("MOHIR (reaksiya 0.25 s)", SKILL, 0.25f, diff, &s25[diff]);
    for (int diff = 0; diff < 3; diff++) report("ODDIY (reaksiya 0.35 s)", SKILL, 0.35f, diff, &s35[diff]);
    printf("\nBalans chegaralari:\n");
    CHECK(a[0].ko_time > 12.f, "oddiy: doim urish bilan AI ni 12 s dan oldin yengib bo'lmaydi");
    CHECK(a[0].taken > 25.f, "oddiy: doim urgan o'yinchi ham sezilarli zarar oladi (>25 HP)");
    CHECK(a[1].taken > a[0].taken + 15.f, "tez: AI ancha ko'proq zarar yetkazadi");
    CHECK(a[2].hits_in == 8, "himoya: doim urgan o'yinchi yutqazadi (8/8)");
    CHECK(d[0].taken < 40.f && d[1].taken < 60.f && d[2].taken < 60.f, "doim himoya: raund davomida nokaut bo'lmaydi");
    CHECK(s25[0].hits_out == 8 && s25[0].ko_time > 20.f && s25[0].ko_time < 60.f, "oddiy: mohir o'yinchi 20-60 s da nokaut qiladi");
    CHECK(s25[0].taken < 30.f, "oddiy: mohir o'yinchi kam zarar oladi (<30)");
    CHECK(s25[1].taken > s25[0].taken + 10.f, "tez: mohir o'yinchi ham ko'proq zarar oladi");
    CHECK(s35[1].hits_in == 8, "tez: sekin reaksiyali o'yinchi yutqazadi (reaksiya muhim)");
    CHECK(s25[2].dealt < s25[0].dealt && s25[2].dealt < s25[1].dealt, "himoya: eng kam zarar yetkaziladi (eng qiyin)");
    printf("\nHimoya charchog'i yoqilganda:\n");
    g_fat = 1; Res df[3], sf[3];
    for (int diff = 0; diff < 3; diff++) report("HIMOYACHI + charchoq", DEF, 0, diff, &df[diff]);
    report("MOHIR + charchoq", SKILL, 0.25f, 0, &sf[0]);
    CHECK(df[0].taken > d[0].taken + 20.f, "doim himoyalangan o'yinchi charchoq qoidasida ancha ko'p zarar oladi (+20 HP)");
    CHECK(sf[0].taken < 40.f && sf[0].hits_out == 8, "mohir o'yinchi (oraliqda himoya qiladi) charchoq qoidasida ham yutadi");
    printf("%s (%d xato)\n", fails ? "XATO BOR" : "HAMMASI O'TDI", fails);
    return fails ? 1 : 0;
}
