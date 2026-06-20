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
- **Distances render with no space before the unit** — `4km`, `107km`, `8km`, `<8km`
  (consistent everywhere, incl. the map rings).

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
Three cells split by 1 px verticals at **x=138** and **x=268**. **Each cell's content is
centered** in its column (centers at x≈73 / 207 / 334) — the only band that's centered;
everything else (hero, stats, Info table) stays left-aligned.

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
- **`3`** (36 px, **right-aligned** to x≈256) = 24 h count; label **`IN 24H`** 9 px at
  (264, 116); `1 felt nearby` / `none felt` 11 px at (264, 130).
- **`18`** (36 px, right-aligned to x≈256) = 7 d count; label **`IN 7 DAYS`** 9 px at
  (264, 166); magnitude range `M2.7 - M3.3` 11 px at (264, 180).
- The numerals are **right-aligned** (padded off the center divider, snug to the labels at
  x=264); they auto-shrink if a wide value (`100+`) would otherwise cross the divider.
- 1 px divider (212, 192)→(388, 192), then a **depth key**: hollow ○ `Shallow · < 8km`,
  filled ● `Deep · > 8km` (9 px).
- One line **`Largest: M4.8 · May 3`** 10 px at (212, 238) — all-time max since power-on.

---

## 5. Page 2 — "Timeline" · temporal / *when* — **locked**
Same hero + footer. Content is a **7-day lollipop strip-chart** (x 22–388, baseline y 224).
- Magnitude axis `M2 / M3 / M4` (right-aligned 9 px at y 205 / 167 / 129) with light dashed
  gridlines. Weekday letters along the baseline (y 238).
- **Each quake = a vertical lollipop** from the baseline; height = magnitude, dot sized by
  magnitude. A swarm reads as a picket fence. Headline = tallest stalk, ringed, with its
  `M3.4` label **centered above the dot** (clear of neighbouring lollipops).
- Three stats across the top (y 111), spread **left / center / right** so they don't run
  together — caps label 9 px + **bold value 11 px**: `TODAY 0` · `7-DAY MAX M3.3` ·
  `LARGEST M4.4`.

---

## 6. Hero band — shared, Map & Timeline (y 0–90) — **locked**
| Element | Spec |
|---|---|
| Magnitude | **54 px** bold, left, baseline (9, 66) — e.g. `M3.3` |
| Place | 15 px bold, right column x≈150, **2-line clamp** — the quake's USGS location, e.g. `8km ESE of Cloverdale, CA` |
| Depth + recency | 13 px — `depth 4km · 2 days ago` |
| Distance from home | 13 px — `107km W of Davis` (bearing/distance from the *monitoring* location, named so it's unambiguous) |
| Stale stamp | filled square (11,7) + `STALE DATA` 11 px at (24,16) — **only** when data is old |
| Page indicator | `● ○ ○` top-right |
| Offline indicator | slashed-WiFi glyph top-right (left of the dots) when reconnecting |

The two detail lines sit **tight under the place** (no extra blank line). **No
significance / "FELT" badge** — a far-away felt flag wasn't meaningful and read as orphaned;
the felt *count* still lives in the stat column.

**Which quake is the headline (firmware logic, not a visual element):** a hybrid — the most
*significant* quake in the **last 24 h**, or if today was quiet, the most significant over
the **last 7 days**. ("Significant" = USGS significance: magnitude + felt/impact, tiebroken
by magnitude then recency.) The recency line is the only on-screen hint of which window it
came from; the rule itself is documented in the README rather than called out on the panel.

**Quiet state** (no events, common): big `Quiet` (54 px) at (9, 64); `No events in range`
(15 px) + date set to the right of the word; rings still drawn. Reads as the instrument at
rest, not an error.

---

## 7. Page 3 — "Info" · device — **variant B "Balanced" (locked, built v0.9.1)**
A full-screen page (no shared hero/footer) so the device's identity is readable on-glass
**with no web** — the main use is reading the **MAC** to register on a managed/dorm network,
the **IP** to reach the web UI, and the **firmware version** for support. Third stop in the
tap cycle. Chosen over A (clock-forward, network buried) and C (utility-first, clock folded)
— B keeps both roles: a calm clock header *and* a complete scannable device table.

- **Masthead:** `GROUND TRUTH` caps 10 px at (16, 22). 1 px divider at y=34.
- **Clock header band (y 36–116):** clock **54 px** left at (14, 96) + am/pm; date 13 px at
  (196, 70); **home-pin + monitoring location** 11 px at (196, 92). This band
  **partial-refreshes each minute** (no flash).
- 1 px divider at y=116.
- **Two-column table**, centered in the lower region (label 9 px caps at x 16, value 13 px
  at x 118, rows every 22 px from y≈152): `WEB groundtruth.local` · `IP …` · `WIFI MAC …` —
  these **key values are 13 px Bold**; `ETHERNET not installed` · `FIRMWARE v… · up …` ·
  `STATUS online · data N ago` are 13 px Regular. (No standalone `DEVICE` section header —
  single-section, the rows are self-labeling.) **The Ethernet MAC joins the bold key values
  once the W5500 is installed** (Gate 1b). Reads from existing device state (no new data).

*Firmware note: MAC renders in proportional Public Sans (not the mockup's monospace — we
don't embed a mono face); masthead has no letter-tracking (bitmap fonts are fixed-advance).
The teardrop map-pin in the mockup is drawn as the same home-pin glyph the footer uses.*

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
