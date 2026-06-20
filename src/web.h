#pragma once
#include <Arduino.h>

// STA-mode web server:
//   /          live B-tight display mirror + recent-events table  (the data)
//   /settings  diagnostics, location/config editor, OTA, actions  (the admin)
//   /api/state JSON consumed by both pages
//   /update    ElegantOTA (basic-auth — the one security-critical endpoint)
namespace web {
  void begin();           // start the AsyncWebServer on :80 (idempotent)
  void loop();            // ElegantOTA housekeeping

  // Actions requested from the web (consumed by the main loop, which owns reboot).
  bool consumeReboot();
  bool consumeWifiReset();
  bool consumeApplyConfig();   // settings saved -> re-apply TZ live + force a re-fetch

  // Config mode (button-hold AP): when on, unknown paths redirect to the settings page so a
  // phone's captive-portal check pops it open automatically. Main toggles this on enter/exit.
  void setConfigMode(bool on);
}
