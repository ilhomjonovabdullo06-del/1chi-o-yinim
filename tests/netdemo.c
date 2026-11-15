// Ikki jarayonli qo'lda sinov (haqiqiy vaqt):
//   ./netdemo host            - xona ochadi
//   ./netdemo search          - xonalarni topib, birinchisiga ulanadi
//   ./netdemo join 127.0.0.1  - IP orqali ulanadi
// Ulangach har 0.5 s da "salom N" yuboradi, kelganini chop etadi, ping ko'rsatadi. Ctrl+C - chiqish (BYE yuboriladi).
#include "../app/src/main/cpp/net.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static volatile int quit = 0;
static void onsig(int s) { (void)s; quit = 1; }
static double now_sec(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
int main(int argc, char **argv) {
    if (argc < 2) { puts("foydalanish: netdemo host|search|join IP [loopback]"); return 1; }
    signal(SIGINT, onsig); signal(SIGTERM, onsig);
    Net n; net_init(&n); n.loopback_test = 1;
    char me[32]; snprintf(me, sizeof me, "DEMO-%d", (int)getpid() % 10000);
    if (!strcmp(argv[1], "host")) { if (net_host(&n, me, 0)) { puts("host xato"); return 1; } puts("xona ochildi"); }
    else if (!strcmp(argv[1], "search")) net_search(&n);
    else if (!strcmp(argv[1], "join") && argc > 2) { uint32_t ip; if (!net_parse_ip(argv[2], &ip)) { puts("IP xato"); return 1; } net_join(&n, ip, 0, me); }
    else return 1;
    NetState last = (NetState)-1; double t_say = 0; int cnt = 0, nr = 0;
    while (!quit) {
        double now = now_sec();
        net_update(&n, now);
        if (n.state != last) { printf("[holat %d err=%s peer=%s]\n", n.state, net_err_text(n.err), n.peer_name); last = n.state; fflush(stdout); }
        if (n.state == NET_SEARCHING && n.nrooms > 0 && nr == 0) {
            char ip[16]; net_ip_str(n.rooms[0].ip, ip); printf("topildi: %s %s\n", n.rooms[0].name, ip); fflush(stdout);
            net_join(&n, n.rooms[0].ip, n.rooms[0].port, me); nr = 1;
        }
        if (n.state == NET_CONNECTED) {
            if (now - t_say > 0.5) { char b[32]; int l = snprintf(b, sizeof b, "salom %d", cnt++); net_send(&n, 1, b, l); t_say = now; printf("ping %.1f ms\n", n.rtt_ms); fflush(stdout); }
            NetMsg m; while (net_recv(&n, &m)) { printf("keldi: %.*s\n", m.len, (char *)m.data); fflush(stdout); }
        }
        if (n.state == NET_ENDED) { printf("tugadi: %s\n", net_err_text(n.err)); break; }
        usleep(8000);
    }
    net_close(&n);
    return 0;
}
