#pragma once
#include <Arduino.h>
#include <time.h>
#include <vector>

// USGS FDSN event query -> filtered parse -> distance/bearing -> headline +
// aggregates. One HTTPS request per poll over a 7-day window (limit-capped),
// from which we derive: the headline (most-significant), 24 h / 7 d counts, a
// magnitude histogram, the map dots, and the all-time record (NVS high-water).
namespace seismic {

  struct Quake {
    float   mag;
    float   lat, lon;
    float   depthKm;
    time_t  t;            // event time (UTC epoch)
    int     sig;          // USGS significance
    int     felt;         // DYFI felt reports (0 if none)
    bool    alert;        // PAGER alert present
    float   distKm;       // from home
    float   bearingDeg;   // 0..360 from home (0 = N)
    String  place;
  };

  void begin();           // load record high-water from NVS
  bool fetch();           // do the query + parse + compute; true on success

  bool   hasData();
  time_t lastFetch();     // 0 if never

  const std::vector<Quake>& events();   // newest-first, capped
  int  headlineIndex();                 // -1 if none
  int  count24h();
  int  count7d();
  bool count7dCapped();                 // hit the query limit
  int  feltCount24h();
  void histogram(int& m2, int& m3, int& m4);
  void magRange(float& lo, float& hi);
  float  recordMag();
  String recordDate();                  // "May 3" ("" if none)

  const char* bearingName(float deg);   // 8-point compass
  String      badge(const Quake& q);    // "felt nearby" / "significant" / ""
}
