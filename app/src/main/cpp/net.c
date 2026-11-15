#include "net.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* ---------- paket formati ----------
   Sarlavha 10 bayt: 'B' 'K' ver tur | sessiya u32 | seq u16   (little-endian)
   ROOM    : port u16, band u8, xona id u32, nom[16]
   HELLO   : nonce u32, nom[16]            (sessiya = 0)
   WELCOME : nonce u32, nom[16]            (sessiya = host bergan yangi id)
   BUSY    : nonce u32
   PING/PONG: vaqt u32 (ms)
   BYE     : -
   DATA    : tur u8, ma'lumot...             */
enum { T_ROOM = 1, T_PROBE = 2, T_HELLO = 3, T_WELCOME = 4, T_BUSY = 5,
       T_PING = 6, T_PONG = 7, T_BYE = 8, T_DATA = 16 };
#define HDR 10

static void wr16(uint8_t *p, uint16_t v) { p[0] = v & 255; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (v >> (8 * i)) & 255; }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static uint32_t rnd32(Net *n) {
    uint32_t x = n->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return n->rng = x ? x : 0x9E3779B9u;
}

static void copy_name(char *dst, const char *src, int maxlen) {
    int i = 0;
    for (; src && src[i] && i < maxlen; i++) dst[i] = src[i];
    for (; i <= maxlen; i++) dst[i] = 0;
}

/* ---------- mahalliy tarmoq ---------- */
typedef struct { uint32_t ip, bc; int score; } IfInfo;

static int starts(const char *s, const char *p) { return strncmp(s, p, strlen(p)) == 0; }

static int collect_ifs(IfInfo *out, int max) {
    int cnt = 0;
    struct ifaddrs *ifa = NULL;
    if (getifaddrs(&ifa) == 0) {
        for (struct ifaddrs *p = ifa; p && cnt < max; p = p->ifa_next) {
            if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
            if (!(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
            uint32_t ip = ntohl(((struct sockaddr_in *)p->ifa_addr)->sin_addr.s_addr);
            if ((ip >> 24) == 127 || (ip >> 16) == 0xA9FE) continue;       // loopback, link-local
            uint32_t mask = 0xFFFFFF00u;                                   // niqob yo'q bo'lsa /24
            if (p->ifa_netmask) mask = ntohl(((struct sockaddr_in *)p->ifa_netmask)->sin_addr.s_addr);
            if (mask == 0) mask = 0xFFFFFF00u;
            int sc = 0;
            const char *nm = p->ifa_name ? p->ifa_name : "";
            if (starts(nm, "wlan") || starts(nm, "ap") || starts(nm, "swlan") || starts(nm, "wl") ||
                starts(nm, "eth") || starts(nm, "en")) sc += 4;
            if (starts(nm, "rmnet") || starts(nm, "ccmni") || starts(nm, "tun") || starts(nm, "dummy") ||
                starts(nm, "v4-") || starts(nm, "docker") || starts(nm, "veth") || starts(nm, "br-")) sc -= 5;
            if ((ip >> 24) == 10 || (ip >> 16) == 0xC0A8 || ((ip >> 20) == 0xAC1)) sc += 2;   // 10/8, 192.168/16, 172.16/12
            out[cnt].ip = ip; out[cnt].bc = (ip & mask) | ~mask; out[cnt].score = sc; cnt++;
        }
        freeifaddrs(ifa);
    }
    if (cnt == 0) {   // zaxira: UDP "connect" hiylasi (paket yuborilmaydi), faqat marshrut bo'lsa ishlaydi
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s >= 0) {
            struct sockaddr_in a; memset(&a, 0, sizeof a);
            a.sin_family = AF_INET; a.sin_port = htons(9); a.sin_addr.s_addr = htonl(0xC0000201u);
            if (connect(s, (struct sockaddr *)&a, sizeof a) == 0) {
                struct sockaddr_in me; socklen_t l = sizeof me;
                if (getsockname(s, (struct sockaddr *)&me, &l) == 0) {
                    uint32_t ip = ntohl(me.sin_addr.s_addr);
                    if (ip) { out[0].ip = ip; out[0].bc = (ip & 0xFFFFFF00u) | 0xFF; out[0].score = 0; cnt = 1; }
                }
            }
            close(s);
        }
    }
    for (int i = 1; i < cnt; i++) {        // ball bo'yicha kamayish tartibida (barqaror)
        IfInfo t = out[i]; int j = i - 1;
        while (j >= 0 && out[j].score < t.score) { out[j + 1] = out[j]; j--; }
        out[j + 1] = t;
    }
    return cnt;
}

int net_local_ips(uint32_t *ips, int max) {
    IfInfo f[16];
    int c = collect_ifs(f, 16), k = 0;
    for (int i = 0; i < c && k < max; i++) {
        int dup = 0;
        for (int j = 0; j < k; j++) if (ips[j] == f[i].ip) dup = 1;
        if (!dup) ips[k++] = f[i].ip;
    }
    return k;
}

void net_ip_str(uint32_t ip, char out[16]) {
    snprintf(out, 16, "%u.%u.%u.%u", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}

int net_parse_ip(const char *s, uint32_t *ip) {
    unsigned a, b, c, d; char tail;
    if (!s || sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    for (const char *p = s; *p; p++) if (!((*p >= '0' && *p <= '9') || *p == '.')) return 0;
    if (ip) *ip = (a << 24) | (b << 16) | (c << 8) | d;
    return 1;
}

const char *net_err_text(NetErr e) {
    switch (e) {
    case NETERR_SOCKET: return "TARMOQ XATOSI";
    case NETERR_TIMEOUT: return "ULANIB BO'LMADI";
    case NETERR_BUSY: return "XONA BAND";
    case NETERR_LOST: return "ALOQA UZILDI";
    case NETERR_PEER_LEFT: return "RAQIB CHIQIB KETDI";
    default: return "";
    }
}

/* ---------- soket yordamchilari ---------- */
static int open_udp(uint16_t port, int share) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) return -1;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    if (share) {
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
        setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    }
    int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0 || fcntl(s, F_SETFL, fl | O_NONBLOCK) < 0) { close(s); return -1; }
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_ANY); a.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    return s;
}

static void sendp(Net *n, int fd, uint32_t ip, uint16_t port, uint8_t type, uint32_t sess, uint16_t seq,
                  const uint8_t *pl, int len) {
    if (fd < 0 || len < 0 || len > NET_MAX_DATA + 1) return;
    n->tx_pkts++;
    if (n->sim_loss > 0 && (int)(rnd32(n) % 100) < n->sim_loss) return;
    uint8_t b[HDR + NET_MAX_DATA + 1];
    b[0] = 'B'; b[1] = 'K'; b[2] = NET_PROTO_VER; b[3] = type;
    wr32(b + 4, sess); wr16(b + 8, seq);
    if (len) memcpy(b + HDR, pl, (size_t)len);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(ip); a.sin_port = htons(port);
    sendto(fd, b, (size_t)(HDR + len), 0, (struct sockaddr *)&a, sizeof a);   // xatolar (ENETUNREACH...) e'tiborsiz
}

// Broadcast manzillari: 255.255.255.255 + har bir interfeysning o'z broadcast manzili.
static int bcast_targets(Net *n, uint32_t *out, int max) {
    int k = 0;
    out[k++] = 0xFFFFFFFFu;
    IfInfo f[16];
    int c = collect_ifs(f, 16);
    for (int i = 0; i < c && k < max; i++) {
        int dup = 0;
        for (int j = 0; j < k; j++) if (out[j] == f[i].bc) dup = 1;
        if (!dup) out[k++] = f[i].bc;
    }
    if (n->loopback_test && k < max) out[k++] = 0x7FFFFFFFu;   // 127.255.255.255
    return k;
}

#define ROOM_LEN 23
static void room_payload(Net *n, uint8_t *pl) {
    wr16(pl, n->game_port); pl[2] = (n->state == NET_CONNECTED); wr32(pl + 3, n->room_id);
    memset(pl + 7, 0, NET_NAME_LEN); memcpy(pl + 7, n->name, strlen(n->name));
}

static void send_beacon(Net *n) {
    uint8_t pl[ROOM_LEN];
    room_payload(n, pl);
    uint32_t t[20]; int c = bcast_targets(n, t, 20);
    for (int i = 0; i < c; i++) sendp(n, n->sock, t[i], NET_DISCOVERY_PORT, T_ROOM, 0, 0, pl, ROOM_LEN);
}

static void send_probe(Net *n) {
    uint32_t t[20]; int c = bcast_targets(n, t, 20);
    for (int i = 0; i < c; i++) sendp(n, n->dsock, t[i], NET_GAME_PORT, T_PROBE, 0, 0, NULL, 0);
}

/* ---------- boshqaruv ---------- */
void net_init(Net *n) {
    memset(n, 0, sizeof *n);
    n->sock = n->dsock = -1;
    n->rtt_ms = -1;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    n->rng = (uint32_t)(ts.tv_nsec ^ (ts.tv_sec * 2654435761u) ^ (uintptr_t)n ^ (uint32_t)getpid() * 40503u);
    if (!n->rng) n->rng = 1;
}

static void reset_session(Net *n) {
    n->err = NETERR_NONE; n->session = 0; n->nonce = 0; n->peer_ip = 0; n->peer_port = 0;
    n->peer_name[0] = 0; n->rtt_ms = -1; n->nrooms = 0; n->qh = n->qt = 0;
    n->tx_seq = 0; n->rx_seq = 0; n->rx_seq_ok = 0;
    n->tx_pkts = n->rx_pkts = n->dropped_old = n->q_overflow = 0;
}

static void close_socks(Net *n) {
    if (n->sock >= 0) close(n->sock);
    if (n->dsock >= 0) close(n->dsock);
    n->sock = n->dsock = -1;
}

void net_close(Net *n) {
    if (n->state == NET_CONNECTED && n->sock >= 0)
        for (int i = 0; i < 3; i++) sendp(n, n->sock, n->peer_ip, n->peer_port, T_BYE, n->session, 0, NULL, 0);
    close_socks(n);
    n->state = NET_IDLE; n->role = NET_ROLE_NONE;
    reset_session(n);
}

int net_host(Net *n, const char *name, int port) {
    net_close(n);
    n->game_port = port ? (uint16_t)port : NET_GAME_PORT;
    copy_name(n->name, name, NET_NAME_LEN);
    n->sock = open_udp(n->game_port, 0);
    if (n->sock < 0) { n->state = NET_ENDED; n->err = NETERR_SOCKET; return -1; }
    n->role = NET_ROLE_HOST; n->state = NET_HOSTING;
    n->room_id = rnd32(n);
    n->t_beacon = -1e9;
    return 0;
}

int net_search(Net *n) {
    net_close(n);
    n->dsock = open_udp(NET_DISCOVERY_PORT, 1);
    if (n->dsock < 0) { n->state = NET_ENDED; n->err = NETERR_SOCKET; return -1; }
    n->role = NET_ROLE_GUEST; n->state = NET_SEARCHING;
    n->t_probe = -1e9;
    return 0;
}

int net_join(Net *n, uint32_t ip, uint16_t port, const char *name) {
    net_close(n);
    n->sock = open_udp(0, 0);
    if (n->sock < 0) { n->state = NET_ENDED; n->err = NETERR_SOCKET; return -1; }
    copy_name(n->name, name, NET_NAME_LEN);
    n->role = NET_ROLE_GUEST; n->state = NET_CONNECTING;
    n->peer_ip = ip; n->peer_port = port ? port : NET_GAME_PORT;
    n->nonce = rnd32(n);
    n->t_hello = -1e9; n->t_start = -1;   // t_start birinchi net_update'da o'rnatiladi
    return 0;
}

int net_connected(const Net *n) { return n->state == NET_CONNECTED; }

/* ---------- navbat ---------- */
static void push_msg(Net *n, uint8_t kind, const uint8_t *d, int len) {
    int next = (n->qt + 1) % NET_QUEUE;
    if (next == n->qh) { n->qh = (n->qh + 1) % NET_QUEUE; n->q_overflow++; }   // eng eskisini tashla
    NetMsg *m = &n->q[n->qt];
    m->kind = kind; m->len = (uint16_t)len;
    if (len) memcpy(m->data, d, (size_t)len);
    n->qt = next;
}

int net_recv(Net *n, NetMsg *out) {
    if (n->qh == n->qt) return 0;
    *out = n->q[n->qh];
    n->qh = (n->qh + 1) % NET_QUEUE;
    return 1;
}

int net_send(Net *n, uint8_t kind, const void *data, int len) {
    if (n->state != NET_CONNECTED || len < 0 || len > NET_MAX_DATA) return -1;
    uint8_t b[NET_MAX_DATA + 1];
    b[0] = kind;
    if (len) memcpy(b + 1, data, (size_t)len);
    sendp(n, n->sock, n->peer_ip, n->peer_port, T_DATA, n->session, n->tx_seq++, b, len + 1);
    return 0;
}

/* ---------- qabul qilish ---------- */
static void end_with(Net *n, NetErr e) { n->state = NET_ENDED; n->err = e; }

static void add_room(Net *n, uint32_t ip, const uint8_t *pl, int pn, double now) {
    if (pn < ROOM_LEN) return;
    uint16_t port = rd16(pl);
    uint32_t id = rd32(pl + 3);
    int idx = -1;
    for (int i = 0; i < n->nrooms; i++) if (n->rooms[i].id == id) idx = i;
    if (idx < 0) {
        if (n->nrooms >= NET_MAX_ROOMS) return;
        idx = n->nrooms++;
    }
    NetRoom *r = &n->rooms[idx];
    r->ip = ip; r->port = port; r->id = id; r->full = pl[2] != 0; r->seen = now;
    memcpy(r->name, pl + 7, NET_NAME_LEN); r->name[NET_NAME_LEN] = 0;
}

static void handle(Net *n, uint32_t sip, uint16_t sport, const uint8_t *b, int len, double now) {
    if (len < HDR || b[0] != 'B' || b[1] != 'K' || b[2] != NET_PROTO_VER) return;
    uint8_t type = b[3];
    uint32_t sess = rd32(b + 4);
    uint16_t seq = rd16(b + 8);
    const uint8_t *pl = b + HDR; int pn = len - HDR;
    int from_peer = (sip == n->peer_ip && sport == n->peer_port);

    switch (type) {
    case T_ROOM:
        if (n->state == NET_SEARCHING) add_room(n, sip, pl, pn, now);
        return;
    case T_PROBE:
        if (n->role == NET_ROLE_HOST && (n->state == NET_HOSTING || n->state == NET_CONNECTED)) {
            uint8_t r[ROOM_LEN];
            room_payload(n, r);
            sendp(n, n->sock, sip, sport, T_ROOM, 0, 0, r, ROOM_LEN);
        }
        return;
    case T_HELLO: {
        if (n->role != NET_ROLE_HOST || pn < 4 + NET_NAME_LEN) return;
        uint32_t nonce = rd32(pl);
        uint8_t w[4 + NET_NAME_LEN];
        if (n->state == NET_HOSTING) {
            n->peer_ip = sip; n->peer_port = sport; n->nonce = nonce;
            do { n->session = rnd32(n); } while (n->session == 0);
            memcpy(n->peer_name, pl + 4, NET_NAME_LEN); n->peer_name[NET_NAME_LEN] = 0;
            n->state = NET_CONNECTED; n->err = NETERR_NONE;
            n->t_rx = now; n->t_ping = now; n->rtt_ms = -1;
            n->tx_seq = 0; n->rx_seq = 0; n->rx_seq_ok = 0; n->qh = n->qt = 0;
        } else if (!(n->state == NET_CONNECTED && from_peer && nonce == n->nonce)) {
            uint8_t bz[4]; wr32(bz, nonce);
            sendp(n, n->sock, sip, sport, T_BUSY, 0, 0, bz, 4);
            return;
        }
        wr32(w, nonce); memset(w + 4, 0, NET_NAME_LEN); memcpy(w + 4, n->name, strlen(n->name));
        sendp(n, n->sock, sip, sport, T_WELCOME, n->session, 0, w, sizeof w);
        return;
    }
    case T_WELCOME:
        if (n->state == NET_CONNECTING && from_peer && pn >= 4 + NET_NAME_LEN && rd32(pl) == n->nonce && sess) {
            n->session = sess;
            memcpy(n->peer_name, pl + 4, NET_NAME_LEN); n->peer_name[NET_NAME_LEN] = 0;
            n->state = NET_CONNECTED; n->t_rx = now; n->t_ping = now; n->rtt_ms = -1;
            n->tx_seq = 0; n->rx_seq = 0; n->rx_seq_ok = 0; n->qh = n->qt = 0;
        }
        return;
    case T_BUSY:
        if (n->state == NET_CONNECTING && from_peer && pn >= 4 && rd32(pl) == n->nonce) end_with(n, NETERR_BUSY);
        return;
    default: break;
    }

    // Quyidagilar faqat ulangan hamkordan va to'g'ri sessiya bilan
    if (n->state != NET_CONNECTED || !from_peer || sess != n->session) return;
    n->t_rx = now;
    switch (type) {
    case T_PING:
        sendp(n, n->sock, n->peer_ip, n->peer_port, T_PONG, n->session, 0, pl, pn < 4 ? pn : 4);
        break;
    case T_PONG:
        if (pn >= 4) {
            int32_t d = (int32_t)((uint32_t)(now * 1000.0) - rd32(pl));
            if (d >= 0 && d < 10000) n->rtt_ms = n->rtt_ms < 0 ? (float)d : n->rtt_ms * 0.8f + (float)d * 0.2f;
        }
        break;
    case T_BYE:
        end_with(n, NETERR_PEER_LEFT);
        break;
    case T_DATA:
        if (pn < 1) break;
        if (n->rx_seq_ok && (int16_t)(seq - n->rx_seq) <= 0) { n->dropped_old++; break; }   // eski yoki takror
        n->rx_seq = seq; n->rx_seq_ok = 1;
        push_msg(n, pl[0], pl + 1, pn - 1);
        break;
    default: break;
    }
}

static void drain(Net *n, int fd, double now) {
    if (fd < 0) return;
    uint8_t buf[HDR + NET_MAX_DATA + 64];
    for (int i = 0; i < 64; i++) {
        struct sockaddr_in a; socklen_t al = sizeof a;
        ssize_t r = recvfrom(fd, buf, sizeof buf, 0, (struct sockaddr *)&a, &al);
        if (r < 0) break;   // EAGAIN yoki xato
        n->rx_pkts++;
        if (n->sim_loss > 0 && (int)(rnd32(n) % 100) < n->sim_loss) continue;
        handle(n, ntohl(a.sin_addr.s_addr), ntohs(a.sin_port), buf, (int)r, now);
        if (n->state == NET_ENDED) break;
    }
}

void net_update(Net *n, double now) {
    if (n->state == NET_IDLE || n->state == NET_ENDED) return;
    drain(n, n->sock, now);
    drain(n, n->dsock, now);

    switch (n->state) {
    case NET_HOSTING:
        if (now - n->t_beacon >= NET_BEACON_SEC) { n->t_beacon = now; send_beacon(n); }
        break;
    case NET_SEARCHING: {
        if (now - n->t_probe >= NET_BEACON_SEC) { n->t_probe = now; send_probe(n); }
        int k = 0;
        for (int i = 0; i < n->nrooms; i++)
            if (now - n->rooms[i].seen <= NET_ROOM_TTL) n->rooms[k++] = n->rooms[i];
        n->nrooms = k;
        break;
    }
    case NET_CONNECTING: {
        if (n->t_start < 0) n->t_start = now;
        if (now - n->t_start >= NET_CONNECT_TMO) { end_with(n, NETERR_TIMEOUT); break; }
        if (now - n->t_hello >= NET_HELLO_SEC) {
            n->t_hello = now;
            uint8_t h[4 + NET_NAME_LEN];
            wr32(h, n->nonce); memset(h + 4, 0, NET_NAME_LEN); memcpy(h + 4, n->name, strlen(n->name));
            sendp(n, n->sock, n->peer_ip, n->peer_port, T_HELLO, 0, 0, h, sizeof h);
        }
        break;
    }
    case NET_CONNECTED:
        if (now - n->t_rx >= NET_LOST_TMO) { end_with(n, NETERR_LOST); break; }
        if (now - n->t_ping >= NET_PING_SEC) {
            n->t_ping = now;
            uint8_t t[4]; wr32(t, (uint32_t)(now * 1000.0));
            sendp(n, n->sock, n->peer_ip, n->peer_port, T_PING, n->session, 0, t, 4);
        }
        if (n->role == NET_ROLE_HOST && now - n->t_beacon >= NET_BEACON_SEC) { n->t_beacon = now; send_beacon(n); }
        break;
    default: break;
    }
}
