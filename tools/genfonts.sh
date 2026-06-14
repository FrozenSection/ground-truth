#!/usr/bin/env bash
# Ground Truth — regenerate the Public Sans bitmap glyph headers in src/fonts/.
#
# These headers are the ONLY embedded form of the typeface (the .ttf files in
# assets/fonts/ are build-time inputs, never flashed). We run Adafruit GFX's
# `fontconvert` with DPI forced to 72 so the size argument equals the CSS-pixel
# em size the designer specced in docs/DISPLAY-SPEC.md §1 (a browser `font-size:
# 54px` == fontconvert size 54 at 72 DPI). The generated `…pt7b` suffix is
# renamed to `…px7b` to keep that truthful.
#
# Requires: FreeType (brew install freetype) and the Adafruit GFX lib that
# PlatformIO has already vendored under .pio/libdeps/. Run from the repo root
# after a `pio run` (so .pio/libdeps exists):  tools/genfonts.sh
set -euo pipefail
cd "$(dirname "$0")/.."

FCDIR=".pio/libdeps/adafruit_feather_esp32_v2/Adafruit GFX Library/fontconvert"
[ -f "$FCDIR/fontconvert.c" ] || { echo "fontconvert.c not found — run 'pio run' first"; exit 1; }

A=assets/fonts/PublicSans
OUT=src/fonts
mkdir -p .fonttool "$OUT"

# Build a DPI=72 fontconvert (size arg == pixel em). Compile in-tree so its
# `#include "../gfxfont.h"` resolves.
sed 's/#define DPI 141.*/#define DPI 72/' "$FCDIR/fontconvert.c" > "$FCDIR/fontconvert_px.c"
clang "$FCDIR/fontconvert_px.c" $(pkg-config --cflags --libs freetype2) -o .fonttool/fontconvert_px
rm -f "$FCDIR/fontconvert_px.c"

gen() { # weight px  (ASCII 32..126 — arrows/mid-dots are drawn as primitives)
  local w=$1 px=$2
  .fonttool/fontconvert_px "$A/PublicSans-${w}.ttf" "$px" 32 126 \
    | sed "s/${px}pt7b/${px}px7b/g" > "$OUT/PublicSans_${w}${px}px7b.h"
  echo "  $OUT/PublicSans_${w}${px}px7b.h"
}

echo "Generating glyph headers (DPI=72, ASCII 32-126):"
gen Bold     54   # magnitude (hero)
gen Bold     36   # stat numerals (24h / 7d)
gen Bold     24   # footer time
gen Bold     15   # place
gen Bold     11   # badge / caps labels
gen Regular  13   # detail / recency / date
gen Medium    9   # axis / map micro-labels
gen SemiBold 10   # Quiet caption + monitoring location
echo "done."
