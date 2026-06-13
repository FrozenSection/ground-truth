#pragma once
#include <Arduino.h>

// STA-mode web server: a live mirror of the display fed by /api/state JSON, plus
// status. (Config editing + ElegantOTA arrive at Gate 5.) Runs only once WiFi is
// connected; the captive portal is a separate, earlier phase.
namespace web {
  void begin();   // start the AsyncWebServer on :80 (idempotent)
}
