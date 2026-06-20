# Ground Truth — Display Design Handoff

**A clean, current snapshot of the e-paper UI as built (firmware v0.9.0, 2026-06-20).**
This is the shareable brief — hand it to a designer to refine. Everything here is *live on
the device*; coordinates are the real pixel positions in the firmware. Where something is
open for redesign it's flagged **⟶ open**. (The longer `DISPLAY-SPEC.md` is our internal
working history; this doc supersedes it for a fresh pass.)

---

## 0. What the device is
An always-on e-paper desk object for a geology undergraduate. It shows **live regional
earthquake activity** (USGS) with a footer of local time, sun, and moon. The user flips
between **three full-screen pages** with a single button. It's a finished gift — calm,
legible, intentional. Think **instrument, not dashboard**.

---

## 1. The medium — hard constraints (read first)

| Property | Value | Implication |
|---|---|---|
| Resolution | **400 × 300 px**, origin top-left | Design at 1:1. Every coordinate below is a pixel. |
| Physical | 4.2" panel, ~120 ppi | Roughly print density; a 12 px glyph ≈ 2.5 mm. |
| Color | **1-bit — pure black or white per pixel** | **No grays, no anti-aliasing, no gradients, no soft shadows.** |
| Greys | only via dithering (stipple) | OK for large fills; **never behind text or thin lines.** |
| Refresh | full ≈ 1–2 s w/ black flash; partial ≈ 0.3–0.5 s | Image is **static between updates**. Must look right frozen. **Nothing may imply motion** (no spinners/progress bars). |
| Persistence | holds with no power; no backlight | Lit by room light, like paper. **Contrast is everything.** |

**Deliver black-on-white.** Ink = black, paper = white. No dark mode (it's a white panel).

### Typography (as built) — **Public Sans** (open licence)
Embedded as bitmap glyph tables at these exact sizes/weights:

| px | Weight | Used for |
|---|---|---|
| 54 | Bold | magnitude (hero), Info clock |
| 36 | Bold | stat numerals (24 h / 7 d) |
| 24 | Bold | footer clock, screen titles |
| 15 | Bold | place, section heads |
| 13 | Regular | detail / recency / date, Info values |
| 11 | Bold | badges, caps labels, sun/moon labels |
| 10 | SemiBold | monitoring-location label, "Largest" |
| 9 | Medium | micro-labels (ring / axis / diagnostics labels) |

- **Comfort floor ≈ 9 px.** At 9 px use **Medium** (keeps stems ≥ 1 device pixel). Keep
  +0.5–1 px tracking on caps labels.
- **Firmware glyph set is ASCII only.** Consequences the design must live with (or we
  extend the font): the swarm count renders **`xN`** (lowercase x), not `×N`; the
  magnitude range uses a hyphen `-`, not an en-dash; the **`·` mid-dot is drawn as a small
  filled circle**. If you want true `×` / `–` / `·` glyphs, say so and we'll widen the range.

---

## 2. Canvas, grid & bands
```
0,0 ┌────────────────────────────────────────────┐ 400,0
    │  HERO BAND            y 0–90   (shared)      │  ← Map & Timeline only
    ├────────────────────────────────────────────┤  ← divider y=90
    │  CONTENT BAND         y 90–242               │  ← page-specific
    ├────────────────────────────────────────────┤  ← divider y=242
    │  SKY FOOTER           y 242–300 (shared)     │  ← Map & Timeline only
    └────────────────────────────────────────────┘ 400,300
```
- **Outer safe margin:** keep text/critical marks ≥ 8 px from every edge.
- **Page indicator:** `● ○ ○` at top-right (dots at x 368 / 380 / 392, y 13, r 3; filled =
  current page). Present on **all three** pages.
- The **Sky Footer and Hero are identical** on Map & Timeline (designed once). The **Info
  page is its own full-screen layout** (no shared hero/footer).

---

## 3. Sky Footer — shared, Map & Timeline (y 242–300) — **locked**
Three cells split by 1 px verticals at **x=138** and **x=268**.

- **Cell 1 — time/date/place (x 8–138):** clock **24 px** at (12, 265) + am/pm; date
  **9 px** at (12, 280) (e.g. `Fri · Jun 20`); **home-pin glyph + monitoring location**
  **9 px** at (24, 295), ellipsis-truncated to the cell.
- **Cell 2 — sun (x 146–268):** sun glyph at (150, 261); `↑ 5:43` at (174, 261) and
  `↓ 20:35` at (174, 277) (11 px, arrows are drawn glyphs); `Daylight: 14h 52m` 9 px at
  (146, 294).
- **Cell 3 — moon (x 276–392):** drawn moon disc (R 12) at (286, 270); **phase name** 9 px
  at (308, 266); `33% · day 6` 9 px at (308, 281).

**12/24-hour** is a user setting and applies to the clock **and** sun times. The clock band
**partial-refreshes each minute** (no flash).

### Moon glyph (almanac convention — *locked, see §8*)
Circle outline; the **shadowed (unlit) part is inked, the lit limb stays paper-white** — so
full moon = open disc, new moon = solid black, crescent = bright sliver on the lit limb.

---

## 4. Page 1 — "Map" · spatial / *where* — **locked**
Hero (§6) + a **radial seismicity map** (left of a 1 px vertical at x=204) + a **stat
column** (right).

**Radial map** — center (100, 170):
- **Three distance rings, √-scale:** r = **34 / 47 / 58 px** (inner two dashed, outer
  solid). Center cross; **`N`** label 9 px at (100, 105).
- **Ring labels are radius-driven** (`R/3 · 2R/3 · R` of the configured radius, e.g.
  `100km / 200km / 300km`), 9 px, **white-knockout**, fanned around the lower arc
  (SW / S / SE) so they don't collide. *(Recently un-stacked — see if the fan reads well.)*
- **Each quake = a dot** by true bearing+distance, **sized by magnitude** (r ≈ 2 px @M2 →
  5 px @M4+). **Depth cue:** hollow = shallow (<8 km), filled = deep (≥8 km).
- **Headline = double ring** (r 8 + r 6 outline, r 3 filled center).
- **Swarm collapse:** ≥6 events within ~15 km → one marker (dashed halo r 10 + filled r 4 +
  `xN`). Headline always breaks out as its own dot.

**Stat column** (x 205–392):
- **`3`** (36 px) at (212, 130) = 24 h count; label **`IN 24 H`** 9 px at (264, 116);
  `1 felt nearby` / `none felt` 11 px at (264, 130).
- **`18`** (36 px) at (212, 180) = 7 d count; label **`IN 7 DAYS`** 9 px at (264, 166);
  magnitude range `M2.7 - M3.3` 11 px at (264, 180).
- The two labels **align at x=264**; the big numeral auto-shrinks if it would otherwise
  reach the label (so a capped `100+` never collides).
- 1 px divider (212, 192)→(388, 192), then a **depth key**: hollow ○ `shallow · <8 km`,
  filled ● `deep · ≥8 km` (9 px).
- One line **`Largest: M4.8 · May 3`** 10 px at (212, 238) — all-time max since power-on.

---

## 5. Page 2 — "Timeline" · temporal / *when* — **locked**
Same hero + footer. Content is a **7-day lollipop strip-chart** (x 22–388, baseline y 224).
- Magnitude axis `M2 / M3 / M4` (right-aligned 9 px at y 205 / 167 / 129) with light dashed
  gridlines. Weekday letters along the baseline (y 238).
- **Each quake = a vertical lollipop** from the baseline; height = magnitude, dot sized by
  magnitude. A swarm reads as a picket fence. Headline = tallest stalk, ringed, `M3.4` label.
- Folded stats top-left (9 px, y 112): `TODAY 0   7-DAY MAX M3.3   LARGEST M4.4`.

---

## 6. Hero band — shared, Map & Timeline (y 0–90) — **locked**
| Element | Spec |
|---|---|
| Magnitude | **54 px** bold, left, baseline (9, 66) — e.g. `M3.3` |
| Place | 15 px bold, right column x≈150, **2-line clamp** (USGS strings run long) |
| Detail | 13 px at (150, 64) — `depth 4 km · 107 km W` |
| Recency | 13 px at (150, 82) — `2 days ago` |
| Significance badge | filled dot (16,12) + caps `FELT NEARBY` 11 px at (26,16); doubles as `STALE DATA` stamp |
| Page indicator | `● ○ ○` top-right |
| Offline indicator | slashed-WiFi glyph top-right (left of the dots) when reconnecting |

**Quiet state** (no events, common): big `Quiet` (54 px) at (9, 64); `No events in range`
(15 px) + date set to the right of the word; rings still drawn. Reads as the instrument at
rest, not an error.

---

## 7. Page 3 — "Info" · device — ⟶ **OPEN: this is the page to spiff up**
A full-screen page (no shared hero/footer) so the device's identity is readable on-glass
**with no web** — the main use is reading the **MAC** to register on a managed/dorm network,
the **IP** to reach the web UI, and the **firmware version** for support. The clock doubles
as a calm at-rest face. Third stop in the tap cycle.

As currently built (functional, deliberately plain):
- **Clock band (y 44–168):** clock **54 px** centered at (200, 104) + am/pm; date 13 px at
  (200, 130); **home-pin + monitoring location** 10 px at (200, 152). Partial-refreshes each
  minute (no flash).
- 1 px divider at y=168.
- **Device block (y 180–290):** caps header `DEVICE` 9 px at (24, 180), then label/value
  rows (label 9 px at x 24, value 13 px at x 122, rows every 18 px):
  `Web  groundtruth.local` · `IP  192.168.4.30` · `WiFi MAC  14:33:5C:99:0D:CC` ·
  `Ethernet  not installed` *(populates when the wired board lands)* · `Firmware  v0.9.0 ·
  up 3h 12m` · `Status  online · data 4 min ago`.

**⟶ Open for you:** make this feel designed rather than a readout. Type scale & spacing of
the device block; whether values want a second column / right-alignment / rules between
rows; whether the clock wants a label or a different size; whether the MAC (the important
one) should be emphasized. Reads from existing device state — no new data needed.

---

## 8. Glyph construction (drawn with primitives, not images)
- **Moon:** circle outline; **shadow filled solid black, lit limb paper-white**; terminator
  is an elliptical arc, x-radius = R·|1 − 2k| (k = illuminated fraction), lit on the right
  when waxing. Illumination is computed on-device (Meeus, ~1–2% of an almanac).
- **Sun:** small filled circle + short radial "ray" ticks.
- **Quake dots:** filled (deep) or hollow (shallow) circles; magnitude → radius.
- **Arrows / home-pin / slashed-WiFi:** simple line glyphs. Keep weights ≥ 1 px (thin
  diagonals alias badly at 1-bit).

---

## 9. System / alternate states (approved; refine if you like)
| State | Conveys |
|---|---|
| **Boot splash** | project name + recipient line + version (~2 s). |
| **Connect / Setup** | one screen, two paths: join WiFi `GroundTruth-Setup` + **QR**, or plug in Ethernet; lists **both WiFi + Ethernet MACs** for managed-network registration. |
| **Acquiring time** | clock not yet trustworthy → footer/clock shows `Setting clock…`, hero drops the recency line, 24 h stat shows `-`. Brief. |
| **Reconnecting** | WiFi lost → keep the last good frame + a small **slashed-WiFi** glyph; retries indefinitely (long-lived offline state). |
| **Stale data** | fetch failing → keep last readout + `■ STALE DATA` stamp. |
| **Quiet / no events** | common — big `Quiet` + nearest context; rings still drawn. |
| **Change WiFi? (confirm)** | after a 3 s button hold: `Change WiFi?` + current SSID + `■ Tap to confirm` / `□ wait to cancel`. |

---

## 10. What we're asking for this round
1. **Polish the Info page (§7)** — the priority.
2. A **true-size legibility pass** on the 9–10 px micro-labels (ring / axis / diagnostics /
   moon name), read on the physical panel at ~60 cm.
3. Anything else that would make it read more like a finished instrument.

**Deliver as 1-bit PNG or vector at exact pixel coordinates** (the firmware places elements
by pixel). Black-on-white. Flag anything that needs a glyph we don't have (see §1).

> **PII note:** the boot-splash recipient name is personal data — it lives in a gitignored
> file and must **never** appear in any mockup committed to the repo.
