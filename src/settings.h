#pragma once
#include <Arduino.h>

// Runtime configuration (charter §6) in NVS namespace "cfg". Seeded from the
// DEFAULT_* values in config.h. Web editing arrives at Gate 5; for now these load
// with defaults (manual-Davis).
namespace settings {
  struct Config {
    bool   locManual;   // true = use lat/lon; false = IP-geo (Gate 5)
    float  lat, lon;
    int    radiusKm;
    float  minMag;
    bool   unitsKm;     // true = km, false = mi
    int    pollMin;
    bool   clock24h;
    String tz;          // POSIX TZ string
    String name;        // monitoring-location label from the geocode search
                        // ("Davis, CA"); "" when coords were hand-entered
  };

  void begin();
  const Config& get();
  void update(const Config& c);   // write all fields to NVS + update the live copy

  // Reject out-of-range / non-finite input (browser controls are not a boundary).
  // Returns false with a field-specific message in `err`.
  bool validate(const Config& c, String& err);
  bool tzAllowed(const String& tz);   // firmware-owned TZ allowlist

  float       toDisplayDist(float km);   // converts to the configured unit
  const char* distUnit();                // "km" or "mi"

  // Monitoring-location label for the footer / mirror: the stored geocode name,
  // or a "38.54, -121.74" fallback when coords were hand-entered (name empty).
  String      locLabel();
}
