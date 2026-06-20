# Using Ground Truth

A short guide to living with the display — the button, getting to the settings, and
moving it between a home network and a managed one (a dorm / lab).

## The button (one press does it all)

There's a single button on the device:

- **Tap** — switch the screen: **Map → Timeline → Info → Map**. The Info screen shows the
  clock, the device's IP address, and both hardware MAC addresses.
- **Hold 3 seconds** — open **Config mode** (see below). Tap once to leave it.

## Changing settings

There are two ways to reach the settings page. The settings are the same either way —
location, search radius, minimum magnitude, time zone, clock format, and the network.

### 1. Over the network (easy, at home)

If your computer or phone is on the **same network** as the display, open a browser to:

- **http://groundtruth.local**, or
- **http://&lt;the IP on the Info screen&gt;** — tap the button to the Info screen to read it.

Then click **Settings**.

> On some managed networks (many dorms, labs, guest WiFi) devices are walled off from each
> other, and `groundtruth.local` may not resolve. If the address won't load, use **Config
> mode** instead — it always works.

### 2. Config mode (works anywhere)

Hold the button for 3 seconds. The screen shows a setup card. Now:

1. On your phone, join the WiFi network **`GroundTruth-Setup`** (password **`groundtruth`**).
2. It should pop the settings page open automatically. If not, open a browser to
   **http://192.168.4.1**.
3. Change whatever you like and **Save**.
4. Tap the device button once to leave Config mode (it also closes itself after 10 minutes).

This is a direct connection to the display, so it works even on networks that block
device-to-device traffic. The earthquake display keeps running on Ethernet the whole time.

## Home vs. a dorm / managed network

The display has **both WiFi and a wired Ethernet jack**, so it works in either kind of place:

| Where | How to connect |
| --- | --- |
| **Home / apartment** | WiFi — set the network name + password in Settings (or Config mode). |
| **Dorm / lab / campus** | **Wired Ethernet.** These networks usually require you to **register the device** first. |

### Registering on a campus / managed network

Networks like university **ResNet** ask you to register a device's **MAC address** before it
gets online. The display has two — use the one for the cable you're plugging in:

- **Ethernet MAC** — register this if you're using the **wired** jack (the usual dorm case).
- **WiFi MAC** — register this if you're joining by **WiFi**.

Both are printed on the **Info screen** (tap to it) and on the Settings page under
**Diagnostics**. Plug in the Ethernet cable, register the Ethernet MAC with your housing /
IT portal, and it comes online — no password typed into the device.

### Turning WiFi off (good-citizen dorm mode)

When the display is on wired Ethernet in a dorm, you can switch the **WiFi radio off** so it
isn't quietly looking for your home network in the background (some campus networks prefer
that). In **Settings → Network**, set **WiFi radio** to **Off — Ethernet only** and Apply.

To turn WiFi back on later, either flip it back on in Settings, or just **hold the button**
for 3 seconds — Config mode always brings WiFi back so you can reach the page.

## Moving it to a new home WiFi

Hold the button → join `GroundTruth-Setup` → on the settings page, **Network → Join a
network**, pick your WiFi and enter the password → **Save & connect**. The display restarts
and joins the new network. (You can also use **Forget network** to clear it entirely.)

## Firmware updates

Settings → **Open firmware update (OTA)**. It asks for a username and password — that's the
device's update credential, set when the display was built.
