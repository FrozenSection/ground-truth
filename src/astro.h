#pragma once
#include <Arduino.h>
#include <time.h>

// Astronomy computed on-device, no network. Moon phase is location-independent
// (date/time only) so it lands at Gate 2 to populate the footer mirror. Sunrise/
// sunset (location-dependent, Dusk2Dawn) arrives at Gate 3.
namespace astro {
  struct Moon {
    float       illum;    // 0..1 illuminated fraction
    int         ageDays;  // 0..29 days into the synodic cycle
    bool        waxing;   // lit limb on the right when waxing
    const char* name;     // phase name
  };
  Moon moon(time_t utc);

  // Sunrise/sunset for the active location (Dusk2Dawn). Minutes past local
  // midnight; valid=false on polar no-rise/no-set days.
  struct Sun { bool valid; int riseMin, setMin; };
  Sun sun(float lat, float lon, time_t utc);

  String hm12(int minutesPastMidnight);   // 312 -> "5:12a"
}
