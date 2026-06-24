# Ground Truth — Owner's Guide

*Your seismic desk display — a live window into the earthquakes around you.*

<!-- Optional: add a personal line / dedication from you here. -->


## 1. Get it online

- **WiFi:** join **GroundTruth-Setup** from your phone (password below) and pick your network on
  the page that pops up — or browse to `192.168.4.1`.
- **Ethernet:** just plug in. On a campus/managed network, **register the device's Ethernet MAC**
  first — it's the device's hardware ID, shown on its **Info** screen (tap the button) and on the
  phone setup page, so you can read it *before* it's online. If both links are up, it prefers wired.


## 2. The button

- **Tap** → next screen (Map → Timeline → Info)
- **Hold 3 s** → settings


## 3. The screens

- **Map** — recent quakes nearby, scaled by magnitude
- **Timeline** — the last 7 days
- **Info** — clock, IP, and the device's MAC addresses
- **The big number** is the most *significant* nearby quake (size + how widely it was felt) —
  from the last 24 hours if there's been recent activity, otherwise the biggest of the past 7 days.
- **"Quiet"** = nothing notable lately (not an error). And a **still screen is normal** — e-paper
  only redraws when something changes.


## 4. Settings

- **At home:** `http://groundtruth.local` (or the IP on the Info screen).
- **On a network that hides it:** hold the button → join **GroundTruth-Setup** → `192.168.4.1`.
- Location, radius, magnitude threshold, time zone, and WiFi all live there. **Diagnostics** shows
  signal, uptime, free memory, and the last reset reason.


## 5. On a managed network (dorm, lab, campus)

Use the **Ethernet** jack and register the **Ethernet MAC** with housing / IT. Optional good-citizen
step: flip **WiFi off** (Settings → Network) so it isn't broadcasting on a wired network.


## 6. If something's off

- **Frozen or blank:** power-cycle it (it also self-recovers within ~30 s via a watchdog).
- **"Offline" mark:** check the link, or hold the button to reconnect.


## Good to know (for the curious)

- It's **open-source** — `github.com/FrozenSection/ground-truth`
- **Firmware updates** over the network at `/update` (or USB).
- If USGS ever moves its feed, the **data-source URL is editable** in Settings → Advanced — so it
  keeps working without a reflash.


## This device

- **Setup-WiFi password:** `________________`  *(also shown on the device's setup screen)*
- **Firmware update:** user `admin`, password `________________`
- **Ethernet MAC** — register this on a campus/managed network: `__ : __ : __ : __ : __ : __`
- **WiFi MAC** — if a WiFi network asks for it: `__ : __ : __ : __ : __ : __`

*Earthquake data from the U.S. Geological Survey · groundtruth.local*


---
### Editing notes (delete this section before printing)

- **Personal touch:** add a dedication line under the title if you like.
- **Section 5 / "for the curious":** drop either if not relevant (e.g. no managed network; or you'd
  rather not point him at the repo).
- **Passwords & MACs:** fill the blanks in "This device" — the two unique passwords, plus the
  Ethernet (and WiFi) MAC read off the device's Info screen. All device-specific; MACs never change.
- **Tone:** written for a computer-comfortable reader. Loosen or tighten freely.
- **Length:** tuned to one printed page in two columns — adding much will spill to a second page.
