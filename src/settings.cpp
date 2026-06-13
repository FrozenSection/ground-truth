#include "settings.h"
#include "config.h"
#include <Preferences.h>

namespace {
  settings::Config g;
}

namespace settings {

void begin() {
  Preferences p;
  p.begin("cfg", true);
  g.locManual = p.getBool ("manual", String(DEFAULT_LOC_MODE) == "manual");
  g.lat       = p.getFloat("lat",    DEFAULT_LAT);
  g.lon       = p.getFloat("lon",    DEFAULT_LON);
  g.radiusKm  = p.getInt  ("radius", DEFAULT_RADIUS_KM);
  g.minMag    = p.getFloat("minmag", DEFAULT_MIN_MAG);
  g.unitsKm   = p.getBool ("km",     String(DEFAULT_UNITS) == "km");
  g.pollMin   = p.getInt  ("poll",   DEFAULT_POLL_MIN);
  g.clock24h  = p.getBool ("h24",    DEFAULT_CLOCK_24H);
  g.tz        = p.getString("tz",    DEFAULT_TZ);
  p.end();
  if (g.pollMin < 1) g.pollMin = 1;
  Serial.printf("[cfg] lat=%.4f lon=%.4f radius=%d minMag=%.1f units=%s poll=%dm tz=%s\n",
                g.lat, g.lon, g.radiusKm, g.minMag, g.unitsKm ? "km" : "mi",
                g.pollMin, g.tz.c_str());
}

const Config& get() { return g; }

void update(const Config& c) {
  g = c;
  if (g.pollMin < 1) g.pollMin = 1;
  Preferences p;
  p.begin("cfg", false);
  p.putBool ("manual", g.locManual);
  p.putFloat("lat",    g.lat);
  p.putFloat("lon",    g.lon);
  p.putInt  ("radius", g.radiusKm);
  p.putFloat("minmag", g.minMag);
  p.putBool ("km",     g.unitsKm);
  p.putInt  ("poll",   g.pollMin);
  p.putBool ("h24",    g.clock24h);
  p.putString("tz",    g.tz);
  p.end();
  Serial.println(F("[cfg] settings updated"));
}

float toDisplayDist(float km) { return g.unitsKm ? km : km * 0.621371f; }
const char* distUnit() { return g.unitsKm ? "km" : "mi"; }

}  // namespace settings
