#include "astro.h"
#include <math.h>

namespace astro {

Moon moon(time_t utc) {
  // Synodic month from a known new moon: 2000-01-06 18:14 UTC = 947182440.
  const double SYNODIC = 29.530588853;
  const double ref     = 947182440.0;
  double days  = (double(utc) - ref) / 86400.0;
  double phase = days / SYNODIC;
  phase -= floor(phase);                 // 0..1 (0 = new, 0.5 = full)

  Moon m;
  m.illum   = (1.0 - cos(2.0 * M_PI * phase)) / 2.0;
  m.ageDays = (int)floor(phase * SYNODIC + 0.5);
  m.waxing  = phase < 0.5;

  if      (phase < 0.03 || phase > 0.97) m.name = "New moon";
  else if (phase < 0.22)                 m.name = "Waxing crescent";
  else if (phase < 0.28)                 m.name = "First quarter";
  else if (phase < 0.47)                 m.name = "Waxing gibbous";
  else if (phase < 0.53)                 m.name = "Full moon";
  else if (phase < 0.72)                 m.name = "Waning gibbous";
  else if (phase < 0.78)                 m.name = "Last quarter";
  else                                   m.name = "Waning crescent";
  return m;
}

// NOAA sunrise equation (≈±1 min). Returns local minutes past midnight.
Sun sun(float lat, float lon, time_t utc) {
  const double D = M_PI / 180.0;
  double Jdate = utc / 86400.0 + 2440587.5;          // Julian date
  double lw    = -(double)lon;                        // west-positive longitude
  double n     = round(Jdate - 2451545.0 - 0.0009 - lw / 360.0);
  double Jstar = 2451545.0 + 0.0009 + lw / 360.0 + n; // mean solar noon
  double M  = fmod(357.5291 + 0.98560028 * (Jstar - 2451545.0), 360.0) * D;
  double C  = 1.9148 * sin(M) + 0.0200 * sin(2 * M) + 0.0003 * sin(3 * M);
  double lam = (fmod(M / D + C + 180.0 + 102.9372, 360.0)) * D;
  double Jtransit = Jstar + 0.0053 * sin(M) - 0.0069 * sin(2 * lam);
  double delta = asin(sin(lam) * sin(23.44 * D));
  double latr  = (double)lat * D;
  double cosH  = (sin(-0.83 * D) - sin(latr) * sin(delta)) / (cos(latr) * cos(delta));

  Sun s;
  if (cosH > 1.0 || cosH < -1.0) { s.valid = false; s.riseMin = s.setMin = -1; return s; }
  double H     = acos(cosH) / D;                      // degrees
  long riseUtc = (long)round((Jtransit - H / 360.0 - 2440587.5) * 86400.0);
  long setUtc  = (long)round((Jtransit + H / 360.0 - 2440587.5) * 86400.0);

  struct tm g; gmtime_r(&utc, &g); g.tm_isdst = -1;   // local offset incl. DST
  long off = (long)(utc - mktime(&g));
  auto localMin = [&](long u) { long l = (((u + off) % 86400) + 86400) % 86400; return (int)(l / 60); };
  s.riseMin = localMin(riseUtc);
  s.setMin  = localMin(setUtc);
  s.valid   = true;
  return s;
}

String hm12(int m) {
  if (m < 0) return "--:--";
  int h = (m / 60) % 24, mn = m % 60;
  char suf = h < 12 ? 'a' : 'p';
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  char buf[10];
  snprintf(buf, sizeof(buf), "%d:%02d%c", h12, mn, suf);
  return String(buf);
}

}  // namespace astro
