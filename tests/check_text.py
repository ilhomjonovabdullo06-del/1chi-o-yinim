#!/usr/bin/env python3
# main.c dagi ko'rinadigan barcha matnlar shriftda (5x7) bor belgilardan iboratligini tekshiradi.
import re, sys
src = open(sys.argv[1], encoding='utf-8').read()
m = re.search(r'static const char CH\[\] = "((?:[^"\\]|\\.)*)";', src)
CH = m.group(1).replace("\\'", "'")
ok = set(CH)
# ko'rinadigan matn manbalari: text(...), btn_draw(...), massivlar (HELP_*, DIFF_*, LAY_*), back_draw(...)
bad = []
skip_words = ('attribute', 'precision', 'gl_', 'BOKS1', 'boks.cfg', '%s/', 'r\\"', 'wb', 'r"', '"w"', '"r"')
n = 0
for mm in re.finditer(r'"((?:[^"\\]|\\.)*)"', src):
    s = mm.group(1).replace("\\'", "'")
    if not s or any(w in s for w in skip_words) or s in ('p', 'res', 'col', 'w', 'r'): continue
    if '\\n' in s or s.startswith(('#', 'static')): continue
    s2 = re.sub(r'%0?\d*[dsu]', '0', s)
    if s2 in ('boks', 'android.app.lib_name'): continue
    n += 1
    miss = sorted({c for c in s2.upper() if c not in ok})
    if miss: bad.append((s, miss))
if bad:
    for s, miss in bad: print(f"  XATO: {s!r} -> shriftda yo'q: {miss}")
    sys.exit(1)
print(f"  {n} ta matn tekshirildi: hammasi shrift belgilaridan iborat")
