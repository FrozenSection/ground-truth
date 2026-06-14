#include "settings.h"
#include "config.h"
#include <Preferences.h>
#include <math.h>

namespace {
  settings::Config g;

  const char* ALLOWED_TZ[] = {
    "PST8PDT,M3.2.0,M11.1.0", "MST7MDT,M3.2.0,M11.1.0", "MST7",
    "CST6CDT,M3.2.0,M11.1.0", "EST5EDT,M3.2.0,M11.1.0",
    "AKST9AKDT,M3.2.0,M11.1.0", "HST10",
  };

  // Clamp a loaded/parsed config back into valid ranges (NVS-repair on boot).
  void repair(settings::Config& c) {
    if (isnan(c.lat) || c.lat < -90 || c.lat > 90)       c.lat = DEFAULT_LAT;
    if (isnan(c.lon) || c.lon < -180 || c.lon > 180)     c.lon = DEFAULT_LON;
    if (c.radiusKm < 25 || c.radiusKm > 1000)            c.radiusKm = DEFAULT_RADIUS_KM;
    if (isnan(c.minMag) || c.minMag < -1 || c.minMag > 9.9f) c.minMag = DEFAULT_MIN_MAG;
    if (c.pollMin < 5 || c.pollMin > 1440)               c.pollMin = DEFAULT_POLL_MIN;
    if (!settings::tzAllowed(c.tz))                       c.tz = DEFAULT_TZ;
  }
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
  g.unitsKm = true;                 // km everywhere — miles dropped (rings/depth are km)
  repair(g);                        // clamp any out-of-range NVS values back to sane
  Serial.printf("[cfg] lat=%.4f lon=%.4f radius=%d minMag=%.1f units=%s poll=%dm tz=%s\n",
                g.lat, g.lon, g.radiusKm, g.minMag, g.unitsKm ? "km" : "mi",
                g.pollMin, g.tz.c_str());
}

const Config& get() { return g; }

bool tzAllowed(const String& tz) {
  for (const char* t : ALLOWED_TZ) if (tz == t) return true;
  return false;
}

bool validate(const Config& c, String& err) {
  if (isnan(c.lat) || c.lat < -90 || c.lat > 90)   { err = "latitude must be -90 to 90"; return false; }
  if (isnan(c.lon) || c.lon < -180 || c.lon > 180) { err = "longitude must be -180 to 180"; return false; }
  if (c.radiusKm < 25 || c.radiusKm > 1000)        { err = "radius must be 25-1000 km"; return false; }
  if (isnan(c.minMag) || c.minMag < -1 || c.minMag > 9.9f) { err = "min magnitude must be -1.0 to 9.9"; return false; }
  if (c.pollMin < 5 || c.pollMin > 1440)           { err = "poll interval must be 5-1440 min"; return false; }
  if (!tzAllowed(c.tz))                            { err = "unknown time zone"; return false; }
  return true;
}

void update(const Config& c) {
  g = c;
  g.unitsKm = true;                 // km everywhere
  repair(g);
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
