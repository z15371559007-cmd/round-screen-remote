#!/usr/bin/env python3
"""Fail if any non-ASCII character in firmware string literals is missing from rsr_font_cjk_24.c.
Missing glyphs render as boxes; base and additive fallback glyphs are both checked."""
import glob, os, re, sys
root = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'main')
font = '\n'.join(open(os.path.join(root, 'fonts', name), encoding='utf-8').read()
                  for name in ('rsr_font_cjk_24.c', 'rsr_font_cjk_24_extra.c')
                  if os.path.exists(os.path.join(root, 'fonts', name)))
lists = {n: [int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', b)]
         for n, b in re.findall(r'static const uint16_t (\w+)\[\] = \{(.*?)\};', font, re.S)}
have = set()
for st, ln, ul in re.findall(r'\.range_start = (\d+), \.range_length = (\d+).*?\.unicode_list = (\w+)', font, re.S):
    st, ln = int(st), int(ln)
    have |= {chr(c) for c in range(st, st + ln)} if ul == 'NULL' else {chr(st + o) for o in lists[ul]}
missing = {}
for f in glob.glob(os.path.join(root, '**', '*.c'), recursive=True):
    if '/fonts/' in f:
        continue
    src = open(f, encoding='utf-8').read()
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)
    src = re.sub(r'//[^\n]*', '', src)
    for lit in re.findall(r'"([^"\n]*)"', src):
        for c in lit:
            if ord(c) > 0x7f and c not in have:
                missing.setdefault(c, os.path.relpath(f, root))
if missing:
    print('Glyphs missing from rsr_font_cjk_24 (run tools/gen_font.sh):')
    for c, f in sorted(missing.items()):
        print(f'  {c} U+{ord(c):04X}  ({f})')
    sys.exit(1)
print(f'font ok: {len(have)} glyphs cover all UI strings')
