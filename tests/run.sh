#!/bin/sh
# Linux'da barcha sinovlarni ishga tushiradi: o'yin mantig'i, tarmoq qatlami, menyu/tarmoq UI (ikki jarayon).
set -e
cd "$(dirname "$0")/.."
CPP=app/src/main/cpp
OUT=${TMPDIR:-/tmp}/boks-tests; mkdir -p $OUT

echo "== o'yin mantig'i =="
gcc -O1 -Wall -Wextra -o $OUT/test_game tests/test_game.c $CPP/game.c -lm
$OUT/test_game > $OUT/game.log; echo "  ok: $(grep -c "ok:" $OUT/game.log), FAIL: $(grep -c FAIL $OUT/game.log)"
! grep -q FAIL $OUT/game.log || { echo "O'YIN TESTLARI XATO"; exit 1; }

echo "== sync: paketlash, yumshatish (ASan/UBSan) =="
gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined -o $OUT/test_sync tests/test_sync.c $CPP/sync.c $CPP/game.c -lm
$OUT/test_sync > $OUT/sync.log; tail -n 1 $OUT/sync.log
! grep -q FAIL $OUT/sync.log || { echo "SYNC TESTLARI XATO"; exit 1; }

echo "== tarmoq qatlami (ASan/UBSan) =="
gcc -O1 -g -Wall -Wextra -fsanitize=address,undefined -o $OUT/test_net tests/test_net.c $CPP/net.c
$OUT/test_net | tail -1

echo "== menyu UI: host + 2 ta mehmon (alohida jarayonlar) =="
# main.c nusxasi: text() ni o'rab olish uchun nomini o'zgartiramiz (asl fayl o'zgarmaydi)
sed 's/^static void text(const char \*s,/static void text_real(const char *s,/' $CPP/main.c > $OUT/main_ui.c
gcc -O1 -g -Wall -Wextra -Wno-unused-function -DMAIN_SRC="\"$OUT/main_ui.c\"" -Itests/stubs -I$CPP \
    -o $OUT/ui tests/ui_harness.c $CPP/game.c $CPP/net.c $CPP/sync.c -lm
$OUT/ui host 2 > $OUT/ui_host.log 2>&1 &  HP=$!
sleep 1
$OUT/ui guestsearch > $OUT/ui_g1.log 2>&1 && G1=0 || G1=1
$OUT/ui guestip     > $OUT/ui_g2.log 2>&1 && G2=0 || G2=1
wait $HP && H=0 || H=1
for f in host g1 g2; do tail -n 1 $OUT/ui_$f.log; done
[ $G1 = 0 ] && [ $G2 = 0 ] && [ $H = 0 ] || { echo "UI TESTLARI XATO (loglar: $OUT)"; exit 1; }

echo "== PvP: xost + mehmon (alohida jarayonlar, o'yin to'liq: raund, nokaut, qayta o'ynash, 25% paket yo'qotish) =="
export BOKS_OUT=$OUT
$OUT/ui pvphost > $OUT/pvp_host.log 2>&1 &  HP=$!
sleep 1.5
$OUT/ui pvpguest > $OUT/pvp_guest.log 2>&1 && PG=0 || PG=1
wait $HP && PH=0 || PH=1
for f in host guest; do tail -n 1 $OUT/pvp_$f.log; done
[ $PG = 0 ] && [ $PH = 0 ] || { echo "PVP TESTLARI XATO (loglar: $OUT/pvp_*.log)"; exit 1; }
echo "HAMMASI O'TDI"
