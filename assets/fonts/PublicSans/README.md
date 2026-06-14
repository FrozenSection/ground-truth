# Public Sans — display typeface

**License:** SIL Open Font License 1.1 — see [OFL.txt](OFL.txt). Public Sans is by the
[USWDS / Public Sans project](https://github.com/uswds/public-sans). The license +
copyright notice must travel with the font (they're in OFL.txt).

**These are build-time assets, not firmware.** They are run through Adafruit GFX
`fontconvert` to generate bitmap glyph-table headers (`.h`) at the device's fixed sizes;
the TTFs themselves are **never embedded** on the ESP32. Kept here for reproducibility.

Four static weights, mapped to the roles in `docs/DISPLAY-SPEC.md` §1:

| Weight | Roles |
|---|---|
| **Bold** | magnitude (54), stat numerals (36), footer time (24), place (15), badges, axis labels |
| **SemiBold** | Quiet caption, monitoring location (9–9.5 px) — per the legibility pass |
| **Medium** | 9–10 px micro-labels (ring / axis / day letters) |
| **Regular** | detail / recency / date (13) |

The full Google-Fonts download (italics, Thin/Black/Light, variable fonts) was trimmed
out — only what the build uses is kept.

## Generating the glyph headers

The eight bitmap headers the firmware embeds live in `src/fonts/` and are produced by
[`tools/genfonts.sh`](../../../tools/genfonts.sh): it builds Adafruit GFX's `fontconvert`
with **`DPI` forced to 72** (so the size argument equals the CSS-pixel em size the
designer specced in `docs/DISPLAY-SPEC.md` §1) and renames the `…pt7b` suffix to
`…px7b` to keep that truthful. Range is **ASCII 32–126**; the `·` separator is drawn as
a primitive in the renderer rather than carried as a glyph.

```
brew install freetype      # one-time
pio run                     # populates .pio/libdeps with Adafruit GFX
tools/genfonts.sh           # regenerates src/fonts/PublicSans_*px7b.h
```

| Generated header | px (em) | Weight |
|---|---|---|
| `PublicSans_Bold54px7b.h`     | 54 | Bold — magnitude |
| `PublicSans_Bold36px7b.h`     | 36 | Bold — stat numerals |
| `PublicSans_Bold24px7b.h`     | 24 | Bold — footer clock / titles |
| `PublicSans_Bold15px7b.h`     | 15 | Bold — place |
| `PublicSans_Bold11px7b.h`     | 11 | Bold — badges / caps labels |
| `PublicSans_Regular13px7b.h`  | 13 | Regular — detail / recency / date |
| `PublicSans_Medium9px7b.h`    |  9 | Medium — axis / ring / micro labels |
| `PublicSans_SemiBold10px7b.h` | 10 | SemiBold — location name / "Largest" |
