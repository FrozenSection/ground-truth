# Public Sans — display typeface

**License:** SIL Open Font License 1.1 — see [OFL.txt](OFL.txt). Public Sans is by the
[USWDS / Public Sans project](https://github.com/uswds/public-sans). The license +
copyright notice must travel with the font (they're in OFL.txt).

**These are build-time assets, not firmware.** They are run through Adafruit GFX
`fontconvert` to generate bitmap glyph-table headers (`.h`) at the device's fixed sizes;
the TTFs themselves are **never embedded** on the ESP32. Kept here for reproducibility.

Three static weights, mapped to the roles in `docs/DISPLAY-SPEC.md` §1:

| Weight | Roles |
|---|---|
| **Bold** | magnitude (54), stat numerals (36), footer time (24), place (15), badges, axis labels |
| **Regular** | detail / recency / date (13) |
| **Medium** | 9–10 px micro-labels (ring/axis/day letters) |

The full Google-Fonts download (italics, Thin/Black/Light, variable fonts) was trimmed
out — only what the build uses is kept. Sizes to generate land with the Gate 4 display
work.
