# Ground Truth — Display Design Spec (for an independent designer)

A brief you can design against. **The constraints in §1 are hard** (they're the
physical display); everything from §3 on is a *starting layout* — improve the
aesthetics freely as long as §1 holds.

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
fonts. We can convert **any TTF** you choose to the sizes we need (we already did this
for the sister project), so you may pick a typeface — but note:
- No sub-pixel rendering. **Minimum comfortable size ≈ 11 px; body text ≥ 16 px.**
- Each font + size + weight is baked into flash as a glyph table — so spec a small,
  deliberate set (e.g. one sans in 3–4 sizes), not many.
- A clean grotesque/neo-grotesque reads best at 1-bit (we default to a Helvetica/
  Arial-like sans). Hairline or high-contrast serif faces fall apart — avoid.

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

- 12/24-hour is configurable; design for both (`3:42 pm` and `15:42`).
- Daylight-length line is optional if space is tight.

---

## 4. Page 1 — "Map" (default) · spatial / *where*

Hero band (shared element pattern, see §6) + a **radial seismicity map** as content.

**Content band (y 90–242), left ≈ map, right ≈ stats**, split by a 1 px vertical ~x=204.

Radial map (left):
- Davis at center (~cx 100, cy 172). **Three distance rings** at 100 / 200 / 300 km
  (outer ring solid; inner two as light dashed). Outer ring radius ~58 px.
- A small **N** label + center cross marking the location.
- **Each quake (last 7 days) = a filled dot**, placed by true bearing + distance,
  **sized by magnitude** (≈ 2 px for M2, 3.5 px M3, 5 px M4+). The headline event gets
  a thin ring around it.
- *Optional depth cue worth considering:* open (hollow) dot = shallow, filled = deep
  — geologically meaningful, near-free. Designer's call whether it reads cleanly.
- Tiny `300 km` label on the outer ring.

Stats (right):
- `Last 24 h` / `Last 7 days` counts.
- A small **magnitude histogram** (M2 / M3 / M4 bars with counts).
- One line: `Record M4.8 · since May 3` (all-time max since power-on).

Reference mock (proportions only — redesign welcome): see repo commit history /
the conversation that produced this spec.

---

## 5. Page 2 — "Timeline" · temporal / *when*

Same hero + footer. Content band is a **7-day seismograph strip-chart**.

- **Time axis** runs left→right across the last 7 days (day gridlines + day labels
  along the bottom of the band).
- **Magnitude axis** on the left (`M2 / M3 / M4`), with light dashed gridlines.
- **Each quake = a vertical "lollipop"** rising from a baseline; **height = magnitude**,
  topped by a dot sized by magnitude. Clusters naturally render as a picket fence
  (this is how a *swarm* should read at a glance — a common local pattern).
- Headline event = tallest stalk, ringed, with a small `M4.2` label.
- One folded stats line under the chart: `Today 3 · largest 7 d M4.2 · record M4.8`.

The intent: this page should evoke a **drum-recorder seismograph**. Its weak case is a
genuinely quiet week (near-flat baseline) — design the empty/quiet read so it still
looks intentional (see §7).

---

## 6. Hero band — shared pattern (y 0–90)

The emotional anchor on both pages: the headline earthquake.

| Element | Spec | Example | Notes |
|---|---|---|---|
| Magnitude | Very large (~50 px), bold, left | `M4.2` | The face of the device. |
| Place | ~15 px bold, right of magnitude | `14 km NW of Winters, CA` | **Truncate/wrap** — USGS place strings run long (see §9). |
| Detail line | ~13 px | `depth 8 km · 38 km SW` | distance + bearing from home. |
| Recency | ~13 px | `3 hours ago` | |
| Significance badge | small filled dot + word, top area | `● felt nearby` | Shown only when USGS `sig` high / `alert` non-null. |

---

## 7. Required alternate states (please design these too)

These are not edge cases — at least one shows whenever data is missing. They matter as
much as the "happy path."

| State | When | Must convey |
|---|---|---|
| **Boot splash** | first ~2 s after power | Project name + a personal line (recipient name + "UC Davis Geology"). *(Name supplied at build, not in this repo.)* |
| **Setup needed** | no WiFi creds yet | "Join WiFi network **GroundTruth-Setup**" — large, calm, instructional. |
| **Connecting / reconnecting** | WiFi dropping | Small status; keep the last good frame visible if there is one. |
| **Stale data** | fetch failing, last data old | Keep last readout + a clear `stale · last update 2:10p` marker. |
| **No events** (common!) | nothing meets filters | `Quiet — nearest M2.5+ is 410 km away`. **Design it to look intentional, reassuring — not like an error.** On Page 1 the rings still carry it. |
| **Change WiFi? (confirm)** | after a 3 s button hold | A static prompt: `Change WiFi?  Tap to confirm · ignore to cancel`. (No countdown — the display can't animate one.) |

---

## 8. Glyph construction notes (for faithful rendering)

These are drawn with primitives, not image assets — design them as such:
- **Moon:** a circle outline; the **illuminated fraction filled solid black**, with the
  terminator as an elliptical arc (waxing = lit on the right). Phase name + % beside it.
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

## 10. What we need back

- **1:1 black-on-white mockups at 400 × 300**, one per page **and** per state in §7
  (boot, setup, stale, no-events, confirm). 1-bit PNG or vector with exact pixel
  coords — either works; we translate to the device.
- A **type spec**: typeface (single TTF we can embed), and the px size + weight for
  each text role (magnitude / place / detail / labels / footer).
- Notes on any glyph (moon/sun/dots) you redesign.
- **Sanity-check legibility at true size** — print one at 100 % and read it from ~60 cm
  (desk distance). If a label needs squinting, it's too small.

Design freedom is high *above the constraint line in §1*. We'd genuinely like
independent thinking on the hero/stats balance, the map vs. the seismograph
treatments, and the quiet-state designs.
