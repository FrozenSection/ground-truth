# Ground Truth — quick card

*Keep this with the display. Most problems are solved by the **button** or a **power-cycle**.*

## The button (one button does everything)
- **Tap** — change the screen: **Map → Timeline → Info**. The **Info** screen shows the
  clock, the device's **IP address**, and its **MAC addresses**.
- **Hold 3 seconds** — open **Config mode** to change any setting (see below). Tap to leave.

## If something looks wrong

| Symptom | Do this |
| --- | --- |
| **Screen frozen / not updating** | It heals itself within ~30 s (built-in watchdog). If not, **unplug power, wait 5 s, plug back in.** |
| **Screen blank** | Power-cycle. The picture only redraws when something changes — a still screen is normal. |
| **Shows a slashed-WiFi / "offline" mark** | It lost the network and is retrying. Check the cable/WiFi. To point it at a new network, use **Config mode** below. |
| **`STALE DATA` stamp** | It's online but USGS didn't answer; it keeps the last reading and retries. Usually clears itself. |
| **Says `Quiet`** | Normal — no quakes met the filters. Not an error. |

## Change settings, WiFi, or location (Config mode)
1. **Hold the button 3 seconds.** The screen shows a setup card.
2. On your phone, join the WiFi network **`GroundTruth-Setup`** (password **`groundtruth`**).
3. It should open the page automatically; if not, go to **http://192.168.4.1**.
4. Change what you need, **Save**. **Tap the button** to exit.

(On your home network you can also just browse to **http://groundtruth.local** — but Config
mode always works, even on networks that block that.)

## On a dorm / campus network (wired)
1. Plug in an **Ethernet cable**.
2. Register the device's **Ethernet MAC** with your housing/IT portal — it's on the **Info
   screen** (tap to it) and in **Settings → Diagnostics**.
3. Optional good-citizen step: in **Settings → Network**, set **WiFi radio → Off**.

## Credentials & addresses
- **Setup WiFi (Config mode):** `GroundTruth-Setup` / `groundtruth`
- **Web page:** `http://groundtruth.local` (or the IP on the Info screen)
- **Firmware update:** Settings → *Open firmware update* — asks for the update username/password.

## For troubleshooting (Settings → Diagnostics)
Shows **last reset reason** (power-on / watchdog / etc.), **free memory**, **uptime**, both
**MACs**, and **last fetch** — handy if you ever need to describe a problem.
