// Tarmoq qatlami sinovi: bitta jarayonda ikkita Net (host va mehmon), haqiqiy UDP soketlar (loopback).
// Vaqt simulyatsiya qilinadi (dt=10 ms), shuning uchun 10 soniyalik stsenariylar ~soniyada tugaydi.
#include "../app/src/main/cpp/net.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); fails++; } else printf("  ok: %s\n", msg); } while (0)

static double now = 1.0;
static void tick(Net *a, Net *b, Net *c) {
    now += 0.01;
    if (a) net_update(a, now);
    if (b) net_update(b, now);
    if (c) net_update(c, now);
    usleep(150);
}
// shart bajarilguncha (yoki sec simulyatsiya soniyasi) aylantiradi; qancha soniya ketganini qaytaradi
#define RUN(sec, cond) ({ double t0 = now; while (now - t0 < (sec) && !(cond)) tick(H, G, X); now - t0; })

static Net *H, *G, *X;
static Net h, g, x;

static void fresh(void) {
    net_close(&h); net_close(&g); net_close(&x);
    net_init(&h); net_init(&g); net_init(&x);
    h.loopback_test = g.loopback_test = x.loopback_test = 1;
    H = &h; G = &g; X = NULL;
    now += 10;
}
static void connect_pair(void) {
    net_host(&h, "HOST", 0);
    net_join(&g, 0x7F000001u, 0, "MEHMON");
    RUN(3, h.state == NET_CONNECTED && g.state == NET_CONNECTED);
}
static int raw_send(uint16_t port, const void *buf, int len) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(0x7F000001u); a.sin_port = htons(port);
    int r = (int)sendto(s, buf, (size_t)len, 0, (struct sockaddr *)&a, sizeof a);
    close(s);
    return r;
}

int main(void) {
    printf("N1: IP yordamchilari\n");
    uint32_t ip = 0; char s[16];
    CHECK(net_parse_ip("192.168.43.1", &ip) && ip == 0xC0A82B01u, "192.168.43.1 to'g'ri o'qiladi");
    net_ip_str(ip, s);
    CHECK(strcmp(s, "192.168.43.1") == 0, "IP matnga qaytadi");
    CHECK(!net_parse_ip("192.168.43", NULL) && !net_parse_ip("256.1.1.1", NULL) && !net_parse_ip("1.2.3.4.5", NULL) &&
          !net_parse_ip("1.2.3.4.", NULL) && !net_parse_ip("a.b.c.d", NULL) && !net_parse_ip("", NULL) &&
          !net_parse_ip("1.2.3.-4", NULL), "noto'g'ri IP rad etiladi");
    uint32_t ips[NET_MAX_IPS]; int ni = net_local_ips(ips, NET_MAX_IPS);
    printf("  mahalliy IP soni: %d", ni); for (int i = 0; i < ni; i++) { net_ip_str(ips[i], s); printf(" %s", s); } printf("\n");
    CHECK(ni >= 0, "net_local_ips ishlaydi (konteynerda 0 bo'lishi mumkin)");

    printf("N2: xona ochish va topish (broadcast + PROBE)\n");
    fresh();
    net_host(&h, "XONA-1234", 0);
    net_search(&g);
    double t = RUN(2.5, g.nrooms >= 1);
    CHECK(g.nrooms == 1, "mehmon xonani topdi");
    printf("  topish vaqti: %.2f s (simulyatsiya)\n", t);
    CHECK(g.nrooms == 1 && strcmp(g.rooms[0].name, "XONA-1234") == 0 && g.rooms[0].port == NET_GAME_PORT && !g.rooms[0].full,
          "xona nomi, porti va 'band emas' belgisi to'g'ri");
    CHECK(t < 1.5, "1.5 s ichida topildi");
    net_close(&h);
    RUN(NET_ROOM_TTL + 1.5, g.nrooms == 0);
    CHECK(g.nrooms == 0, "xona yopilgach ro'yxatdan o'chadi (TTL)");

    printf("N3: salomlashish (HELLO/WELCOME)\n");
    fresh(); connect_pair();
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "ikkalasi ulandi");
    CHECK(strcmp(h.peer_name, "MEHMON") == 0 && strcmp(g.peer_name, "HOST") == 0, "nomlar almashildi");
    CHECK(h.session != 0 && h.session == g.session, "sessiya id bir xil");
    RUN(1.0, 0);
    CHECK(h.rtt_ms >= 0 && g.rtt_ms >= 0, "ping o'lchandi");
    printf("  ping host=%.1f mehmon=%.1f ms\n", h.rtt_ms, g.rtt_ms);
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "1 s dan keyin ham ulangan");

    printf("N4: qidiruvdan tanlab ulanish (to'liq yo'l)\n");
    fresh();
    net_host(&h, "XONA-7", 0);
    net_search(&g);
    RUN(2.5, g.nrooms >= 1);
    net_join(&g, g.rooms[0].ip, g.rooms[0].port, "MEN");
    RUN(3, h.state == NET_CONNECTED && g.state == NET_CONNECTED);
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "ro'yxatdagi xonaga ulandi");
    CHECK(g.dsock < 0, "qidiruv soketi yopildi");

    printf("N5: ma'lumot almashish, tartib va eski paketlarni tashlash\n");
    fresh(); connect_pair();
    for (int i = 0; i < 10; i++) { uint8_t d[3] = { (uint8_t)i, 7, 9 }; net_send(&g, 20, d, 3); tick(H, G, X); }
    RUN(0.1, 0);
    NetMsg m; int got = 0, order_ok = 1, last = -1;
    while (net_recv(&h, &m)) { if (m.kind != 20 || m.len != 3 || m.data[1] != 7) order_ok = 0; if (m.data[0] <= last) order_ok = 0; last = m.data[0]; got++; }
    CHECK(got == 10 && order_ok, "10 ta paket tartib bilan keldi");
    // soxta eski paket: sessiya to'g'ri, lekin manba boshqa port -> rad etilishi kerak
    {
        uint8_t b[14] = { 'B', 'K', NET_PROTO_VER, 16 };
        for (int i = 0; i < 4; i++) b[4 + i] = (h.session >> (8 * i)) & 255;
        b[8] = 1; b[9] = 0; b[10] = 99; b[11] = 1; b[12] = 2; b[13] = 3;
        raw_send(h.game_port, b, 12);
        RUN(0.1, 0);
        CHECK(!net_recv(&h, &m), "boshqa manbadan kelgan paket rad etildi");
    }
    // xost -> mehmon, so'ng takror/eski seq (xostning o'z soketidan emas, shuning uchun ichki sanagichni tekshiramiz)
    unsigned long before = g.dropped_old;
    uint8_t d1[1] = { 1 };
    net_send(&h, 21, d1, 1); net_send(&h, 21, d1, 1);
    RUN(0.1, 0);
    int c2 = 0; while (net_recv(&g, &m)) c2++;
    CHECK(c2 == 2 && g.dropped_old == before, "xost -> mehmon: 2 paket yetib keldi");
    g.rx_seq = (uint16_t)(g.rx_seq + 100);            // sanagichni "kelajakka" suramiz: keyingi paketlar eski bo'ladi
    net_send(&h, 21, d1, 1);
    RUN(0.1, 0);
    CHECK(!net_recv(&g, &m) && g.dropped_old == before + 1, "eski seq tashlandi");

    printf("N6: band xona (BUSY)\n");
    fresh(); connect_pair();
    net_join(&x, 0x7F000001u, 0, "UCHINCHI"); X = &x;
    RUN(2, x.state == NET_ENDED);
    CHECK(x.state == NET_ENDED && x.err == NETERR_BUSY, "uchinchi o'yinchiga BUSY qaytdi");
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "mavjud ulanish buzilmadi");
    X = NULL;

    printf("N7: 30%% paket yo'qotish: ulanish va ma'lumot oqimi\n");
    fresh(); h.sim_loss = g.sim_loss = 30;
    net_host(&h, "HOST", 0); net_join(&g, 0x7F000001u, 0, "MEHMON");
    t = RUN(NET_CONNECT_TMO, h.state == NET_CONNECTED && g.state == NET_CONNECTED);
    printf("  ulanish %.2f s\n", t);
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "yo'qotishda ham ulandi");
    int sent = 0, rcv = 0;
    for (int i = 0; i < 600; i++) { uint8_t d[1] = { 1 }; if (i % 3 == 0) { net_send(&g, 30, d, 1); sent++; } tick(H, G, X); while (net_recv(&h, &m)) rcv++; }
    printf("  yuborildi %d, yetdi %d (%.0f%%)\n", sent, rcv, 100.0 * rcv / sent);
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "6 s davomida uzilmadi");
    CHECK(rcv > sent * 0.4 && rcv < sent * 0.95, "taxminan 50% (ikki tomonlama filtr) yetdi");

    printf("N8: uzilishni aniqlash\n");
    fresh(); connect_pair();
    double t1 = now;
    while (now - t1 < 5 && h.state == NET_CONNECTED) { now += 0.01; net_update(&h, now); usleep(100); }   // mehmon "yo'q"
    double dt = now - t1;
    printf("  host uzilishni %.2f s da aniqladi\n", dt);
    CHECK(h.state == NET_ENDED && h.err == NETERR_LOST, "host: ALOQA UZILDI");
    CHECK(dt >= NET_LOST_TMO - 0.05 && dt < NET_LOST_TMO + 0.5, "taxminan 3 s ichida");
    fresh(); connect_pair();
    net_close(&g);
    RUN(0.5, h.state == NET_ENDED);
    CHECK(h.state == NET_ENDED && h.err == NETERR_PEER_LEFT, "BYE: host darrov bildi (RAQIB CHIQIB KETDI)");
    CHECK(g.state == NET_IDLE, "mehmon IDLE");

    printf("N9: host chiqib ketsa mehmon bilib oladi, qayta ochish mumkin\n");
    fresh(); connect_pair();
    net_close(&h);
    RUN(0.5, g.state == NET_ENDED);
    CHECK(g.state == NET_ENDED && g.err == NETERR_PEER_LEFT, "mehmon: RAQIB CHIQIB KETDI");
    CHECK(net_host(&h, "YANGI", 0) == 0 && h.state == NET_HOSTING, "xost xonani qayta ocha oladi (port bo'shadi)");

    printf("N10: javob bermaydigan manzilga ulanish -> TIMEOUT\n");
    fresh();
    net_join(&g, 0x7F000001u, 47999, "MEN");
    t = RUN(6, g.state == NET_ENDED);
    printf("  timeout %.2f s\n", t);
    CHECK(g.state == NET_ENDED && g.err == NETERR_TIMEOUT && t > 3.9 && t < 4.5, "4 s dan keyin VAQT TUGADI");

    printf("N11: port band\n");
    fresh();
    CHECK(net_host(&h, "A", 0) == 0, "birinchi xost ochildi");
    CHECK(net_host(&x, "B", 0) == -1 && x.err == NETERR_SOCKET && x.state == NET_ENDED, "ikkinchisi xato qaytardi (TARMOQ XATOSI)");

    printf("N12: axlat va begona paketlar\n");
    fresh(); connect_pair();
    uint8_t junk[40]; for (int i = 0; i < 40; i++) junk[i] = (uint8_t)(i * 37 + 5);
    raw_send(h.game_port, junk, 40); raw_send(h.game_port, junk, 3); raw_send(h.game_port, "", 0);
    uint8_t badver[12] = { 'B', 'K', 99, 16 }; raw_send(h.game_port, badver, 12);
    uint8_t badsess[12] = { 'B', 'K', NET_PROTO_VER, 8 }; raw_send(h.game_port, badsess, 10);   // begona BYE
    RUN(0.3, 0);
    CHECK(h.state == NET_CONNECTED && g.state == NET_CONNECTED, "axlat/begona BYE ulanishni buzmaydi");

    printf("N13: ulangan xost qidiruvga 'band' deb ko'rinadi\n");
    fresh(); connect_pair();
    net_init(&x); x.loopback_test = 1; net_search(&x); X = &x;
    RUN(2.5, x.nrooms >= 1);
    CHECK(x.nrooms == 1 && x.rooms[0].full == 1, "xona ro'yxatda 'band' belgisi bilan");
    X = NULL;

    printf("N13b: boshqa versiyadagi ilova aniqlanadi\n");
    fresh(); net_init(&x); x.loopback_test = 1; net_search(&x); X = &x;
    RUN(0.3, 0);
    CHECK(x.t_other_ver < 0, "boshida boshqa versiya ko'rinmagan");
    uint8_t oldver[24] = { 'B', 'K', NET_PROTO_VER - 1, 16 }; raw_send(NET_DISCOVERY_PORT, oldver, 24);
    RUN(0.3, x.t_other_ver > 0);
    CHECK(x.t_other_ver > 0 && x.nrooms == 0, "eski versiya paketi xona emas, lekin ogohlantirish belgisi qo'yildi");
    X = NULL;

    printf("N14: IDLE holatida xavfsizlik\n");
    fresh();
    CHECK(net_send(&h, 1, "a", 1) == -1, "ulanmasdan net_send rad etiladi");
    net_update(&h, now); net_close(&h); net_close(&h);
    CHECK(h.state == NET_IDLE, "ikki marta net_close xavfsiz");

    net_close(&h); net_close(&g); net_close(&x);
    printf("\n%s (%d xato)\n", fails ? "XATOLAR BOR" : "HAMMASI O'TDI", fails);
    return fails ? 1 : 0;
}
