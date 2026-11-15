// Boks: tarmoq qatlami (UDP). Android'ga bog'liq emas, Linux'da ikki jarayon orasida sinash mumkin.
// Hammasi bloklanmaydi: net_update(now) har kadrda chaqiriladi.
//
// Xona ochish (host):  net_host()   -> har 1 s da xona "mayoq"i (broadcast) yuboradi, PROBE'ga javob beradi.
// Xona qidirish:       net_search() -> mayoqlarni tinglaydi, net.rooms[] to'ladi.
// Ulanish:             net_join(ip) -> HELLO/WELCOME "salomlashish", keyin NET_CONNECTED.
// Uzilish:             3 s hech narsa kelmasa yoki BYE kelsa -> NET_ENDED (net.err sababi).
// Ma'lumot:            net_send(kind, ...) / net_recv(...): ishonchsiz (UDP), eski paketlar tashlanadi.
#ifndef NET_H
#define NET_H

#include <stdint.h>

#define NET_DISCOVERY_PORT 47777   // mehmon xonalarni shu portda tinglaydi
#define NET_GAME_PORT      47778   // host shu portda o'yin/salomlashuv paketlarini qabul qiladi
#define NET_PROTO_VER      1
#define NET_NAME_LEN       16
#define NET_MAX_ROOMS      8
#define NET_MAX_DATA       512     // bitta paketdagi foydali ma'lumot chegarasi
#define NET_QUEUE          32
#define NET_MAX_IPS        6

#define NET_BEACON_SEC     1.0     // xona mayog'i oralig'i
#define NET_ROOM_TTL       3.5     // mayoq kelmasa xona ro'yxatdan o'chadi
#define NET_HELLO_SEC      0.3     // HELLO qayta yuborish oralig'i
#define NET_CONNECT_TMO    4.0     // ulanish uchun umumiy vaqt
#define NET_PING_SEC       0.25
#define NET_LOST_TMO       3.0     // shuncha vaqt hech narsa kelmasa uzildi

typedef enum { NET_IDLE, NET_HOSTING, NET_SEARCHING, NET_CONNECTING, NET_CONNECTED, NET_ENDED } NetState;
typedef enum { NETERR_NONE, NETERR_SOCKET, NETERR_TIMEOUT, NETERR_BUSY, NETERR_LOST, NETERR_PEER_LEFT } NetErr;
typedef enum { NET_ROLE_NONE, NET_ROLE_HOST, NET_ROLE_GUEST } NetRole;

typedef struct {
    uint32_t ip;                 // xost tartibida (a.b.c.d -> a<<24|...)
    uint16_t port;
    uint32_t id;                 // xonaning tasodifiy id'si (bir xona bir necha manzildan kelsa birlashtirish uchun)
    int full;                    // xona band (mehmon bor)
    char name[NET_NAME_LEN + 1];
    double seen;                 // oxirgi mayoq vaqti
} NetRoom;

typedef struct { uint8_t kind; uint16_t len; uint8_t data[NET_MAX_DATA]; } NetMsg;

typedef struct {
    NetState state;
    NetErr err;
    NetRole role;
    int sock, dsock;             // o'yin soketi, qidiruv (discovery) soketi
    uint16_t game_port;
    char name[NET_NAME_LEN + 1], peer_name[NET_NAME_LEN + 1];
    uint32_t session, nonce, room_id;
    uint32_t peer_ip; uint16_t peer_port;
    double t_beacon, t_hello, t_ping, t_rx, t_start, t_probe;
    uint16_t tx_seq, rx_seq; int rx_seq_ok;
    float rtt_ms;                // silliqlangan ping (-1 = hali yo'q)
    NetRoom rooms[NET_MAX_ROOMS]; int nrooms;
    NetMsg q[NET_QUEUE]; int qh, qt;
    unsigned long tx_pkts, rx_pkts, dropped_old, q_overflow;
    int sim_loss;                // sinov: paket yo'qotish foizi (0..100), yuborish va qabul qilishda
    int loopback_test;           // sinov: 127.255.255.255'ga ham broadcast qilish
    uint32_t rng;
} Net;

void net_init(Net *n);
void net_close(Net *n);                       // BYE yuboradi, soketlarni yopadi, IDLE
int  net_host(Net *n, const char *name, int port);   // port=0 -> NET_GAME_PORT. 0 = OK, -1 = xato (n->err)
int  net_search(Net *n);                      // 0 = OK (qidiruv soketi ochilmasa ham IP orqali ulanish mumkin)
int  net_join(Net *n, uint32_t ip, uint16_t port, const char *name);   // port=0 -> NET_GAME_PORT
void net_update(Net *n, double now);          // har kadrda; now = monoton soat (soniya)

int  net_send(Net *n, uint8_t kind, const void *data, int len);   // 0 = OK, faqat NET_CONNECTED'da
int  net_recv(Net *n, NetMsg *out);           // 1 = xabar bor, 0 = yo'q
int  net_connected(const Net *n);

// Yordamchilar
int  net_local_ips(uint32_t *ips, int max);   // mahalliy IPv4'lar (Wi-Fi/hotspot birinchi). Soni.
void net_ip_str(uint32_t ip, char out[16]);
int  net_parse_ip(const char *s, uint32_t *ip);  // 1 = to'g'ri "a.b.c.d"
const char *net_err_text(NetErr e);           // menyu uchun qisqa matn (katta harflar)

#endif
