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
  };

  void begin();
  const Config& get();
  void update(const Config& c);   // write all fields to NVS + update the live copy

  float       toDisplayDist(float km);   // converts to the configured unit
  const char* distUnit();                // "km" or "mi"
}
