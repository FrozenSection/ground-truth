#pragma once
#include <Arduino.h>

// Which full-screen page the device shows. A button tap cycles it; the choice
// persists in NVS (namespace "view") so it survives reboots. Real page layouts
// land at Gate 4 — for now this is the plumbing + the active index.
namespace viewstate {
  void begin();              // load saved index (clamped to a valid view)
  uint8_t current();         // VIEW_MAP / VIEW_TIMELINE / ...
  uint8_t next();            // advance + persist; returns the new index
  const char* name(uint8_t view);   // human label for logs / status
}
