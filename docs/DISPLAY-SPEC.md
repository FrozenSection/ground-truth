# Ground Truth — Display Design Spec

**Status: decisions locked (2026-06-13).** This started as an open brief; after the
mockup round it's now the **build spec**. The §1 constraints are still hard (physical
display); §3–§8 record the *chosen* layouts, not options. Sections marked
"⟶ remaining" are the only open items for the designer.

> **Personalization / PII:** the boot-splash recipient line is personal data. It is
> supplied at build time from a gitignored file (`include/personalization.h`) and
> **never appears in this repo**. Any mockup file that hardcodes the recipient's name
> must be scrubbed before it is committed here.

---

## 0. What the device is
An always-on e-paper desk object for a geology undergraduate in Northern California.
It shows **live regional earthquake activity** (USGS) as the main event, with a
footer of local time, sunrise/sunset, and moon phase. It has **two interchangeable
full-screen "pages"** the user flips with a single button, plus a few system states
(setup, stale data, etc.). It is a finished gift — calm, legible, intentional. Think
*instrument*, not *dashboard*.

---

## 1. The medium — hard constraints (read first)

| Property | Value | Design implication |
|---|---|---|
| Resolution | **400 × 300 px**, origin top-left (0,0) | Design at 1:1. Every coordinate below is a pixel. |
| Physical size | 4.2" panel, active ≈ 84.8 × 63.6 mm → **~120 ppi** | Roughly print density. A 12 px glyph ≈ 2.5 mm tall. |
| Color | **1-bit. Pure black or pure white per pixel.** | **No grays. No anti-aliasing.** No gradients, no soft shadows, no tints. |
| Greys | Only via **dithering** (checkerboard/stipple) | Acceptable for large fills; **avoid behind text or thin lines** — it shimmers and muddies. Prefer solid black, solid white, or line/hatch patterns. |
| Refresh | Full ≈ **1–2 s with a black flash**; partial ≈ 0.3–0.5 s | The image is **static between updates**. Design must look right frozen. **Nothing may imply motion** (no spinners, progress bars, "…"-as-animation). |
| Persistence | Holds image with no power draw; no backlight | It's lit by room light, like paper. Contrast is everything. |
| Ghosting | Faint residue after many partial refreshes; cleared by periodic full refresh | Don't rely on perfect blacks in a region that only ever partial-refreshes. |

**Typography is bitmap, not vector.** Text is rendered with Adafruit GFX bitmap
fonts converted from a TTF (fontconvert), one glyph table per size/weight baked into
flash. No sub-pixel rendering.

**Typeface (locked): Public Sans** — open-licence, Helvetica-class metrics, hinted
for screens. One TTF, converted at this deliberate size scale:

| Role | px | Weight |
|---|---|---|
| Magnitude (hero) | 54 | Bold |
| Footer time | 27 | Bold |
| Stat numerals (24 h / 7 d) | 36 | Bold |
| Place | 15 | Bold |
| Detail / recency / date | 13 | Regular |
| Badge / caps labels | 9–11 | Bold |
| Axis / map micro-labels | 9 | Reg/Medium |

- **Comfort floor ≈ 9 px** (~1.9 mm at 120 ppi). The designer's 8.5 px micro-labels
  get bumped to **9–10 px** on the panel; if a band is still tight, drop the least
  important labels (day letters, histogram) before shrinking type further.

**Deliver in black on white.** Ink = black, paper = white. (No "dark mode" — it's a
physical white panel.)

---

## 2. Canvas, grid & the band system

```
0,0 ┌────────────────────────────────────────────┐ 400,0
    │  HERO BAND            y 0–90 (90 px)         │  ← changes per page
    ├────────────────────────────────────────────┤  ← divider line y=90
    │  CONTENT BAND         y 90–242 (152 px)      │  ← the page-specific visual
    ├────────────────────────────────────────────┤  ← divider line y=242
    │  SKY FOOTER (persistent) y 242–300 (58 px)   │  ← SAME on every page
    └────────────────────────────────────────────┘ 400,300
```

- **Outer safe margin:** keep text/critical marks ≥ 8 px from every edge.
- **Two horizontal dividers**, 1 px solid black, at y=90 and y=242.
- **The Sky Footer is identical on every page** — design it once (§3). Only the Hero +
  Content bands differ between pages. (This also lets the firmware swap pages with a
  fast partial refresh of just the top 242 px.)
- **Page indicator:** a small `● ○` (filled = current) at **top-right, ~x 366–386,
  y 8–16**, telling the user there's more than one screen. Filled dot = active page.

---

## 3. Sky Footer — persistent, all pages (y 242–300)

Three cells split by 1 px verticals at **x=138** and **x=268**.

| Cell | x range | Contents | Example |
|---|---|---|---|
| **Time / date** | 8–138 | Time large (~24 px), date below (~12 px) | `3:42 pm` / `Sat · Jun 13` |
| **Sun** | 146–268 | Small sun glyph + rise/set + daylight length | `↑ 5:48a` `↓ 8:21p` `14 h 33 m` |
| **Moon** | 276–392 | Drawn moon disc (see §8) + phase name + % | `Waxing gibbous` `73% · day 10` |

- **12/24-hour is a user setting** (`clock_24h` in NVS). Time and the am/pm suffix are
  separate fields, so 24-hour mode just shows `15:42` and blanks the suffix — design
  for both.
- Daylight-length line is optional if space is tight.
- **Footer is approved as drawn** (designer's `SkyFooter` component). Moon math in §8.

---

## 4. Page 1 — "Map" (default) · spatial / *where*  — **variant B-tight (locked)**

Hero (§6) + a **radial seismicity map** (left) and a **sparse big-numeral stat
column** (right), split by a 1 px vertical at **x=204**.

Radial map (left, center ~cx 100, cy 170):
- **Three distance rings, √ (square-root) scale** — 100 / 200 / 300 km land at
  **r ≈ 34 / 47 / 58 px**. (Linear buried near-home events at ~19 px; √ gives them
  room. This is the chosen scale.) Outer ring solid; inner two dashed.
- **N** label + center cross at the location; tiny `300 km` label on the outer ring.
- **Each quake (last 7 days) = a dot** placed by true bearing + distance, **sized by
  magnitude** (≈ 2 px M2 → 5 px M4+). Headline event gets a thin ring.
- **Depth cue (kept, but tune):** hollow = shallow, filled = deep. Threshold **≈ 8 km**
  (most NorCal crustal quakes are < 15 km; 10 km marked nearly everything "shallow").
  This is the 3rd thing each dot encodes — if it reads muddy on the panel, it's the
  first thing to drop.

Stat column (right, x 205–392) — **B-tight** (fills the dead space the earlier B left):
- **`3`** big (36 px) = 24 h count, with a two-item descriptor to its right:
  `IN 24 H` (caps label) over `1 felt nearby`.
- **`18`** big (36 px) = 7 d count, descriptor `IN 7 DAYS` over the **magnitude range**
  `M2.0 – M4.2`.
- A 1 px divider, then the **depth key**: hollow ○ `shallow · <8 km`, filled ● `deep ·
  ≥8 km`.
- One line: **`Record  M4.8 · May 3`** (all-time max since power-on, persisted in NVS).
  *(Spelled out — "REC" read as nebulous. "Record" fits the column at 9.5 px.)*

*(No magnitude histogram — that was the denser variant A, not chosen.)* See the
"B-tight" reference mock from the 2026-06-13 conversation for exact placement.

---

## 5. Page 2 — "Timeline" · temporal / *when*  — **variant A: lollipop strip chart (locked)**

Same hero + footer. Content band is a **7-day seismograph strip-chart**.

- **Time axis** left→right across the last 7 days; day letters (`S M T W T F S`) along
  the baseline (~y 224).
- **Magnitude axis** on the left (`M2 / M3 / M4` at ~y 205 / 167 / 129), light dashed
  gridlines.
- **Each quake = a vertical lollipop** from the baseline; **height = magnitude**,
  topped by a dot sized by magnitude. Clusters render as a picket fence — this is how a
  *swarm* reads at a glance (the common local pattern).
- Headline event = tallest stalk, ringed, small `M4.2` label.
- One folded stats line top-left: `TODAY 3 · 7-DAY MAX M4.2 · REC M4.8`.

Chosen over variant B (filled spikes blob/ghost at 1-bit) and C (daily columns lose
within-day timing). Quiet-week read: see §7.

---

## 6. Hero band — shared pattern (y 0–90)  — **locked (`Hero` component)**

The emotional anchor on both pages: the headline earthquake. Approved as the
designer's parametric `Hero` component.

| Element | Spec | Example | Notes |
|---|---|---|---|
| Magnitude | **54 px** bold, left (x≈9) | `M4.2` | The face of the device. |
| Place | 15 px bold, right column (x≈150) | `14 km NW of Winters, CA` | **2-line clamp** — USGS place strings run long (§9). |
| Detail line | 13 px | `depth 8 km · 38 km SW` | distance + bearing from home. |
| Recency | 13 px | `3 hours ago` | |
| Significance badge | filled dot + caps word, **top-left** | `● FELT NEARBY` | only when USGS `sig` high / `alert` non-null. Doubles as the `STALE` flag. |
| Page indicator | `● ○` top-right (x≈366–386, y≈13) | filled = current page | from the `Hero` component (`page` / `pages` props). |

---

## 7. Alternate states — **approved as designed**, one change

All five system states from the mockups are approved. The **one addition: a WiFi QR on
the Setup screen** (see below).

| State | When | Conveys |
|---|---|---|
| **Boot splash** | first ~2 s | Project name + personal line `‹name› · UC Davis Geology` + tagline. **Name comes from gitignored `personalization.h`, never the repo** (§0). |
| **Setup needed** | no WiFi creds | "Join WiFi network **GroundTruth-Setup**", calm/instructional — **plus a WiFi QR** (new). |
| **Connecting / reconnecting** | WiFi dropping | Small status; keep the last good frame if there is one. |
| **Stale data** | fetch failing, data old | Keep last readout + filled `■ STALE DATA` stamp + `as of 2:10 pm`. |
| **Quiet / no events** (common!) | nothing meets filters | Big `Quiet`, `Nearest M2.5+ is 410 km away`, rings still drawn with the distant event pinned just outside. Reads as the instrument at rest, not an error. |
| **Change WiFi? (confirm)** | after a 3 s hold | Static `Change WiFi?` + current SSID + `■ Tap to confirm` / `□ Do nothing to cancel`. No animation. |

**WiFi QR (Setup screen):** encode the join string
`WIFI:T:WPA;S:GroundTruth-Setup;P:‹AP_PASS›;;` so scanning joins the WPA2 setup AP and
the captive portal pops. Rendered on-device from `ricmoo/QRCode` (proven on the sister
build). Keep `192.168.4.1` + auto-pop as the reliable fallback; don't rely on a typed
`setup.ground.truth`.

---

## 8. Glyph construction notes (for faithful rendering)

These are drawn with primitives, not image assets — design them as such:
- **Moon:** circle outline; **illuminated fraction filled solid black**; terminator is
  an elliptical arc with **x-radius = R·|1 − 2k|** (k = illuminated fraction), **lit on
  the right when waxing**. (GFX has no arc-fill primitive, so firmware fills it by
  scanline — a Gate-4 detail; the geometry is what the designer specs.)
- **Sun (footer):** small circle + short radial tick "rays."
- **Quake dots:** solid filled circles; magnitude → radius. Headline = add a thin ring.
- **Arrows / bearings:** simple glyphs (`↑ ↓` for sun; compass `N`).
Keep line weights ≥ 1 px; thin diagonal hairlines alias badly at 1-bit.

---

## 9. Data dictionary (dynamic fields, with ranges)

| Field | Type / range | Example | Display notes |
|---|---|---|---|
| magnitude | float ~ -1.0 … 8.0 | `4.2` | one decimal, prefixed `M`. |
| place | string, **can be 40+ chars** | `14 km NW of Winters, CA` | truncate with ellipsis or wrap to 2 lines; never overflow. |
| distance | int, km or mi (configurable) | `38 km` | units set in config (default km). |
| bearing | 8-point compass | `SW` | from home location. |
| depth | float km | `8 km` | always km. |
| age | relative | `3 hours ago` | `just now / N min / N h / N d`. |
| count 24h / 7d | int 0…~hundreds | `3` / `18` | 7d can spike during a swarm. |
| mag histogram | counts by band | M2:11 M3:5 M4:2 | bars scale to widest. |
| record | float + date | `M4.8 · May 3` | persists across reboots. |
| time / date | local | `3:42 pm` / `Sat · Jun 13` | 12/24h configurable. |
| sunrise / sunset | local time | `5:48a` / `8:21p` | |
| daylight length | duration | `14 h 33 m` | optional. |
| moon phase | name + % + age | `Waxing gibbous` `73% · day 10` | 8 phase names. |
| active page | 1…N | indicator dots | filled = current. |

---

## 10. ⟶ Remaining for the designer

The layouts are locked above. What's still needed:

1. **Page 1 production render** incorporating **B-tight** (§4): bigger 36 px numerals
   with the right-hand descriptors, depth key, record — i.e. fill the right column the
   way the `B-tight` reference mock shows. Final 1:1 1-bit render.
2. **Page 2 production render** of the chosen **lollipop variant A** (§5).
3. **Setup screen** updated with the **WiFi QR** (§7).
4. **Public Sans** TTF (the single file we embed) — confirm it's the licence/weight set
   you want; we fontconvert from it.
5. **Legibility pass at true size** — print one frame at 100 % and read from ~60 cm. The
   9–10 px micro-labels are the risk; flag any that need bumping or dropping.

Deliver 1-bit PNG or vector with exact pixel coords — either works; we translate to the
device. (Glyph geometry for moon/sun/dots is in §8 if you revise them.)
