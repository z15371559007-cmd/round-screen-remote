#!/usr/bin/env bash
# Regenerates firmware/main/fonts/rsr_font_cjk_24.c with every non-ASCII character
# used in firmware string literals. Run after adding new Chinese UI text.
# Needs: node, `npm i lv_font_conv`, and a Noto Sans SC OTF (NotoSansSC-Medium.otf).
set -euo pipefail
FONT=${1:?usage: tools/gen_font.sh /path/to/NotoSansSC-Medium.otf}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CHARS=$(python3 - "$ROOT/firmware/main" <<'PY'
import re,glob,sys,os
cs=set()
for f in glob.glob(os.path.join(sys.argv[1],'**/*.c'),recursive=True):
    if '/fonts/' in f: continue
    for m in re.findall(r'"([^"\n]*)"',open(f,encoding='utf-8').read()):
        cs|={c for c in m if ord(c)>0x7f}
cs|=set("…、，。：？！（）·—“”●○")
print(''.join(sorted(cs)),end='')
PY
)
npx lv_font_conv --font "$FONT" --size 24 --bpp 4 --format lvgl --no-compress \
  -r 0x20-0x7E --symbols "$CHARS" --lv-include lvgl.h \
  --lv-font-name rsr_font_cjk_24 -o "$ROOT/firmware/main/fonts/rsr_font_cjk_24.c"
echo "glyphs: $CHARS"
