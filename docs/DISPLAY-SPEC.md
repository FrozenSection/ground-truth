# Ground Truth — Display Design Spec

> **⟶ For a designer handoff, use [`DESIGN-HANDOFF.md`](DESIGN-HANDOFF.md)** — a clean,
> current snapshot of every page as built (v0.9.0). This file is the internal working
> history (negotiation rounds, rationale) and is kept for reference.

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
| Footer time | 24 | Bold |
| Stat numerals (24 h / 7 d) | 36 | Bold |
| Place | 15 | Bold |
| Detail / recency / date | 13 | Regular |
| Badge / caps labels | 9–11 | Bold |
| Axis / map micro-labels | 9 | Reg/Medium |

- **Comfort floor ≈ 9 px** (~1.9 mm at 120 ppi). The designer's 8.5 px micro-labels
  get bumped to **9–10 px** on the panel; if a band is still tight, drop the least
  important labels (day letters, histogram) before shrinking type further.

**Legibility pass — accepted (designer, shape/spacing) with `fontconvert` guidance:**
- At **9 px use Medium, not Regular** — keeps stems ≥ 1 device pixel on the 1-bit raster.
- Keep **+0.5–1 px tracking** on caps labels.
- Watch the `·` (mid-dot) and `<` glyphs at 9 px; **fallback: swap the mid-dot for a
  spaced hyphen** if it drops out on the panel.
- Font set is **Bold / SemiBold / Medium / Regular** (in `assets/fonts/PublicSans/`).
  **SemiBold** is used for the Quiet caption + monitoring location, matching the pass.
- **Not final:** that sheet is a browser (anti-aliased) render — the real check is
  fontconvert → render on the GDEY042T81 → read at ~60 cm. **On-panel re-shoot is a
  Gate 4 task (needs hardware).**

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
| **Time / date / location** | 8–138 | Time (~24 px) + am/pm, date (~11 px), and the **monitoring location** — home-pin glyph + name (~9.5 px) | `3:42 pm` · `Sat · Jun 13` · `⌂ Davis, CA` |
| **Sun** | 146–268 | Small sun glyph + rise/set + `Daylight:` length | `↑ 5:48a` `↓ 8:21p` `Daylight: 14h 33m` |
| **Moon** | 276–392 | Drawn moon disc (see §8) + phase name + % | `Waxing gibbous` `73% · day 10` |

- **Monitoring location** (designer round 3): a **home-pin glyph + place name** in cell 1,
  **persistent on both pages**, visually distinct from the hero's *event* place. Cell 1 is
  the tight one (~115 px) — **truncate long names with ellipsis**. The name comes from the
  geocode search (firmware: store it; fall back to `lat, lon` / `Custom` for hand-entered
  coords). The clock dropped to 24 px to make room — fine.

- **12/24-hour is a user setting** (`clock_24h` in NVS) and applies to **both the clock
  and the sunrise/sunset times** (`15:42`, `↑ 5:48 / ↓ 20:33` in 24-hour mode).
- The third sun line is the **daylight duration, labeled** — `Daylight: 13h 12m` (not a
  bare interval). Optional if space is tight.
- **Distance is km everywhere** (miles dropped — the rings and depth are km, so a mixed
  unit lost consistency).
- **Footer is approved as drawn** (designer's `SkyFooter` component). Moon math in §8.

---

## 4. Page 1 — "Map" (default) · spatial / *where*  — **variant B-tight (locked)**

Hero (§6) + a **radial seismicity map** (left) and a **sparse big-numeral stat
column** (right), split by a 1 px vertical at **x=204**.

Radial map (left, center ~cx 100, cy 170):
- **Three distance rings, √ (square-root) scale** — 100 / 200 / 300 km land at
  **r ≈ 34 / 47 / 58 px**. (Linear buried near-home events at ~19 px; √ gives them
  room. This is the chosen scale.) Outer ring solid; inner two dashed.
- **N** label + center cross at the location; **all three rings labeled** (`100 / 200 /
  300 km`) with white-knockout backgrounds so they read over the rings (designer
  round 3). **Radius-driven** — firmware computes labels = ⅓ · ⅔ · 1 × the configured
  radius, **not** a hardcoded 300.
- **Monitoring-location label** lives in the **footer** now (persistent on both pages)
  — see §3.
- **Each quake (last 7 days) = a dot** placed by true bearing + distance, **sized by
  magnitude** (≈ 2 px M2 → 5 px M4+).
- **Headline = double ring** (r ≈ 8.5 + 6 outlines, r 4 filled center) — distinct from
  the hollow "shallow" glyph so a shallow headline never reads as a plain shallow dot.
- **Swarm collapse** (designer rule, in firmware): ≥ 6 events within ~15 km collapse
  to one marker — dashed halo + filled dot + **`×n`** count. The headline always breaks
  out as its own dot; the cluster counts the remainder.
- **Depth cue (kept, but tune):** hollow = shallow, filled = deep. Threshold **≈ 8 km**
  (most NorCal crustal quakes are < 15 km; 10 km marked nearly everything "shallow").
  This is the 3rd thing each dot encodes — if it reads muddy on the panel, it's the
  first thing to drop.

Stat column (right, x 205–392) — **B-tight** (fills the dead space the earlier B left):
- **`3`** big (36 px) = 24 h count, with a two-item descriptor to its right:
  `IN 24 H` (caps label) over `1 felt nearby`.
- **`18`** big (36 px) = 7 d count, descriptor `IN 7 DAYS` over the **magnitude range**
  `M2.0 – M4.2` (collapses to a single `M2.6` when the range is one magnitude;
  `1 felt nearby` → `none felt` when none).
- A 1 px divider, then the **depth key**: hollow ○ `shallow · <8 km`, filled ● `deep ·
  ≥8 km`.
- One line: **`Largest: M4.8 · May 3`** (all-time max since power-on, persisted in NVS).
  *(Labeled "Largest:", not "Record" — clearer.)*
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
- One folded stats line top-left: `TODAY 3 · 7-DAY MAX M4.2 · LARGEST M4.8`.

Chosen over variant B (filled spikes blob/ghost at 1-bit) and C (daily columns lose
within-day timing). Quiet-week read: see §7.

---

## 5b. Page 3 — "Info" · device · *built in firmware, designer pass welcome*

A **full-screen page (no shared hero/footer)** added so the device's identity is readable
**on-glass, with no web** — the main use is reading the **MAC** to register on a managed/
dorm network, plus the IP to reach the web UI and the firmware version for support. The
clock doubles as a calm at-rest face. Third stop in the tap cycle (`● ○ ○`).

- **Clock band (y ~44–168):** large clock (54 px) + am/pm, centered; date (13 px) under it;
  **home-pin + monitoring location** (10 px) under that. This band **partial-refreshes each
  minute** (no flash), like the footer clock on the other pages.
- **1 px divider** at y≈168.
- **Device block (y ~180–290):** a `DEVICE` caps header, then label/value rows —
  `Web groundtruth.local` · `IP …` · `WiFi MAC …` · `Ethernet …` (shows "not installed"
  until the W5500 lands) · `Firmware v… · up Hh Mm` · `Status online · data N ago`.

Reads from live device state (no new `/api/state` fields — the mirror reuses `host/ip/mac/
fw/uptime/online/fetch`). **Open for the designer:** type scale + spacing of the device
block, whether to two-column or right-align the values, and whether the clock wants a label.

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
| Offline indicator | slashed-WiFi glyph top-right (left of the dots) | shown when reconnecting | `Hero` `offline` prop; place text shifts left to clear it. |

---

## 7. Alternate states

Your mockup states are approved. Since round 2, firmware changes added **one new state
(Acquiring time)** and tightened the behavior of two others (Setup, Reconnecting) —
flagged ⟵ NEW / ⟵ CHANGED below.

| State | When | Conveys |
|---|---|---|
| **Boot splash** | first ~2 s | Project name + personal line `‹name› · UC Davis Geology` + tagline. **Name comes from gitignored `personalization.h`, never the repo** (§0). |
| **Setup / Connect** ⟵ **CHANGED** | first setup, a bad password, or a deliberate WiFi change | **One unified screen, two paths:** (A) join WiFi **GroundTruth-Setup** + QR → pick network; or (B) **plug in Ethernet** (DHCP, no config). Lists **both MACs prominently** — `WiFi MAC` and `Ethernet MAC` — for managed/campus registration (§10.9). Device proceeds when *either* link comes up — **no screen swap when a cable is plugged** (the Ethernet MAC is known at boot regardless of cable). Shown only for first setup / bad creds / deliberate change — not on a transient drop (that's "Reconnecting"). |
| **Acquiring time** ⟵ **NEW** | clock not yet trustworthy (early boot, or NTP blocked before the first data fetch) | Footer replaced by a centered **`Setting clock…`**; hero keeps magnitude / place / distance but **no "X ago"**; the **24 h stat shows `—`**. Map, 7-day count, and "Largest" stay valid. Brief — clears once NTP or the HTTPS `Date` header lands. Needs a calm 1-bit treatment. |
| **Connecting / reconnecting** | WiFi lost (router reboot, transient, or a move) | Keep the last good frame + a **small offline/`reconnecting` indicator**; the device retries STA indefinitely. **⟵ CHANGED: it never auto-opens the setup AP anymore** — so this is the long-lived state on any outage, and deserves a clear, non-alarming indicator on the normal frame. |
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
- **Moon:** circle outline; the **shadowed (unlit) fraction is filled solid black**, the
  **lit limb stays paper-white** — the almanac convention, so the glyph reads like the sky
  (full moon = open disc, new moon = solid black, crescent = bright sliver on the lit
  limb). Terminator is an elliptical arc with **x-radius = R·|1 − 2k|** (k = illuminated
  fraction), **lit on the right when waxing**. (GFX has no arc-fill primitive, so firmware
  fills the shadow side by scanline; the web mirror draws a black disc and paints the lit
  region back to paper.)
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
| distance | int, km | `38 km` | km everywhere (miles dropped). |
| bearing | 8-point compass | `SW` | from home location. |
| depth | float km | `8 km` | always km. |
| age | relative | `3 hours ago` | `just now / N min / N h / N d`. |
| count 24h / 7d | int 0…~hundreds | `3` / `18` | 7d can spike during a swarm. |
| mag histogram | counts by band | M2:11 M3:5 M4:2 | bars scale to widest. |
| record | float + date | `M4.8 · May 3` | persists across reboots. |
| time / date | local | `3:42 pm` / `Sat · Jun 13` | 12/24h configurable. |
| sunrise / sunset | local time | `5:48a` / `8:21p` | follows the 12/24 h setting. |
| daylight length | duration | `Daylight: 13h 12m` | labeled; optional if tight. |
| moon phase | name + % + age | `Waxing gibbous` `73% · day 10` | 8 phase names. |
| active page | 1…N | indicator dots | filled = current. |

---

## 10. ⟶ Changes since your round-2 renders (2026-06-13)

Your round-2 production renders (B-tight Map, lollipop Timeline, all states + WiFi QR,
Public Sans) are **accepted and in the firmware** — the **double-ring headline** and the
**swarm `×n` collapse** included. A few small **label/behaviour tweaks** landed after,
from on-device review; please fold these into the final art:

1. **"REC" → "Largest:"** — the Page 1 record line reads `Largest: M4.8 · May 3` (with
   the colon). On Page 2's folded stat strip, `LARGEST M4.8`.
2. **Daylight label** — the footer's third sun line is `Daylight: 13h 12m` (label in
   front), not a bare interval.
3. **km only** — the miles option was dropped; hero distance, rings, and depth are all
   km. No unit toggle to depict.
4. **12/24-hour applies to the sun too** — in 24-hour mode rise/set show `5:48 / 20:33`
   (no am/pm), matching the clock.
5. **NEW state to design — "Acquiring time"** (§7). When the clock isn't yet trustworthy
   the footer is replaced by a centered `Setting clock…`, the hero drops its "X ago"
   line, and the 24 h stat shows `—` (map / 7-day / Largest stay). Needs a calm 1-bit
   treatment — it's brief but real on a slow-NTP / campus network.
6. **"Reconnecting" is now the long-lived offline state** (§7) — the device no longer
   auto-opens the setup AP when WiFi drops; it holds the last frame and retries. So it
   wants a clear, non-alarming **offline / reconnecting indicator** on the normal frame,
   and the **Setup screen is now first-setup / bad-creds / deliberate-change only**.

### New design requests for this round (Scott)
7. **Show the monitoring location on screen** (e.g. below the map). It must hold names
   longer than "Davis, CA" — the device can monitor **anywhere** (flexibility is the
   point), so size for ~20–24 chars ("San Luis Obispo, CA"). **Make it visually distinct
   from the hero's event place** — that line is the *quake's* location; this is *where
   the device is watching* (e.g. a small home-pin glyph or a quiet labeled line). *Open
   question:* keep it **Page-1-only** (under the map) or **persistent on both pages**
   (it's page-independent context)? We lean persistent.
8. **Map scale key.** The √-spaced rings aren't self-explanatory (only the outer is
   labeled). Either label all three rings or add a tiny key — **drop it if it crowds the
   map**. The ring distances are **radius-driven** (configurable), so the key reflects
   the set radius (rings ≈ R⁄3 · 2R⁄3 · R), not a hard "300 km".
9. **Unified "Connect" setup screen showing BOTH MAC addresses.** Decision (2026-06-13):
   the device is **dual-connected — WiFi *and* wired Ethernet (W5500)** — for max
   flexibility without dorm-WiFi (eduroam) complexity: **home/apartment → WiFi; managed/
   dorm → Ethernet + MAC registration**. Design **one** setup screen (no variants, no
   mid-startup swap) that shows:
   - the **WiFi path** — join `GroundTruth-Setup` + the QR, and
   - the **Ethernet path** — "plug in Ethernet", and
   - **both MACs listed** — `WiFi MAC` and `Ethernet MAC` (needed to register on UC
     Davis / managed networks; both are known at boot, cable or not).
   The device connects via whichever link comes up first. Size for **two 17-char MAC
   rows + a QR** on 400×300. (Hardware: W5500 board + RJ45 — details TBD; doesn't change
   the screen's content.)

Everything else in §1–§9 is current. Still genuinely useful from you: the **Public Sans
TTF** (the single file we embed) and a **true-size legibility pass** on the 9–10 px
micro-labels. Deliver 1-bit PNG or vector with exact pixel coords.

### Round 3 — accepted (designer "Frames" sheet, 2026-06-13)
A full 12-frame build reference. All of §10.1–9 delivered + every state. **Accepted:**
- **7 location** → home-pin + name in **footer cell 1** (persistent both pages). ✓
- **8 scale key** → **all three rings labeled** with white-knockout backgrounds. ✓
- **9 Connect screen** → one screen, both paths, **both MACs** in a "register on a managed
  network" box. ✓
- **States** → `Acquiring time` (footer → "Setting clock…", 24 h = `—`, hero drops the
  recency line) and `Reconnecting` (slashed-WiFi indicator in the hero, full data
  retained) both nailed.

**Open / firmware-side (on us, not the designer):**
- Location-name **data source** — wire the geocode result into a stored label + fallback.
- **Ring labels radius-driven** (¼-thirds of the configured radius, not literal 300).
- `/api/state` gains `loc.name` + a hero `offline` flag; footer clock spec → 24 px.
- Footer cell 1 is the **width-tight spot** — confirm long names truncate cleanly.

### Gate 4 — built (firmware, v0.8.0, 2026-06-14)
Everything in §3–§7 is now rendered on the panel (`src/display.cpp`). The above
firmware-side opens are all closed: geocode name stored in NVS → footer cell 1
(home-pin + ellipsis-truncated to ~108 px) and `/api/state.loc.name`; ring labels are
radius-driven white-knockout `R⁄3 · 2R⁄3 · R`; the hero draws the slashed-WiFi glyph
from a root `offline` flag; the footer clock is 24 px and ticks via partial refresh.

Implementation notes the designer should be aware of (for the next art pass):
- **Public Sans is embedded as bitmap glyph tables** (`fontconvert` @ DPI 72 → the
  spec's px == em). Range is **ASCII 32–126 only**. Consequences in firmware:
  - the **`·` mid-dot is drawn as a small filled circle** (a UTF-8 shim), not a glyph;
  - the **swarm count is `xN`** (lowercase x), not `×N`, and the **mag range uses `-`**,
    not an en-dash. If the designer wants true `×` / `–`, we extend the glyph range.
- **Refresh model:** full refresh (≈1–2 s flash) on poll / view-flip / connectivity
  change; the **footer alone partial-refreshes each minute** to tick the clock without a
  flash. Page-flip is currently a full refresh (the §2 "partial top-242" optimization is
  available later if the flash on tap feels heavy on the panel).
- **Still pending — the on-panel re-shoot** (§1 legibility pass): only confirmable on the
  GDEY042T81 read at ~60 cm. The 9–10 px Medium/SemiBold micro-labels are the watch items.
