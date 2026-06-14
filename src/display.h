#pragma once
#include <Arduino.h>

// E-paper rendering. Gate 4 brings the real page layouts (docs/DISPLAY-SPEC.md):
// the shared Hero band + persistent Sky Footer, the Page-1 radial Map and Page-2
// lollipop Timeline, and every system/alternate state. The renderer reads the
// live module state (seismic / timekeeper / astro / settings) directly — it's the
// view layer, called from the main loop after each fetch / view change.
//
// The `headless` env (-DHEADLESS_DISPLAY) renders compact text frames to serial
// instead of driving a panel, so connectivity + button + view logic can be
// exercised on a bare Feather.
namespace epd {
  void begin();

  // ---- System screens (full refresh) ----
  void splash();   // boot: project name + recipient line + version

  // Unified Connect/Setup screen (DISPLAY-SPEC §7/§10.9): WiFi path (join AP + QR)
  // and Ethernet path, with BOTH MACs for managed-network registration. `ethMac`
  // is the W5500 address once Gate 1b lands; pass "" until then. `error` flips the
  // headline to the bad-credentials retry copy.
  void connectScreen(const String& apName, const String& apPass,
                     const String& wifiMac, const String& ethMac,
                     bool error = false);

  // Two-step WiFi-change confirm (after a 3 s button hold).
  void changeWifiConfirm(const String& ssid);

  // Generic centered notice (reboot / wifi-reset / transient connecting).
  void message(const String& title, const String& line1 = "",
               const String& line2 = "");

  // ---- Data frame for the active view ----
  // Draws the hero + content (Map or Timeline) + persistent footer.
  //  timeOK : clock trustworthy — false renders the "Acquiring time" treatment
  //           (footer → "Setting clock…", hero drops recency, 24 h stat = "—").
  //  online : false draws the slashed-WiFi reconnecting glyph in the hero.
  //  stale  : true stamps "STALE DATA" (fetch failing, readout retained).
  void renderView(uint8_t view, bool timeOK, bool online, bool stale);

  // Partial refresh of just the Sky Footer band (y 242–300) — ticks the clock
  // without a full-screen black flash. Call on a minute boundary between full
  // renders. No-op cost on a panel that already holds the rest of the image.
  void refreshFooter(bool timeOK);
}
