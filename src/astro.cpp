#include "astro.h"
#include <math.h>

namespace astro {

Moon moon(time_t utc) {
  // Illuminated fraction + phase via Meeus, "Astronomical Algorithms" ch. 48 — it
  // accounts for the Sun/Moon orbital anomalies (the elliptical orbit makes the cycle
  // run fast/slow). A plain mean-synodic cosine was off by up to ~7-8 percentage points
  // near the crescents. Still pure on-device math — no almanac, no network.
  const double SYNODIC = 29.530588853;
  const double D2R = M_PI / 180.0;
  double JD = double(utc) / 86400.0 + 2440587.5;                      // Unix epoch -> Julian Day
  double T  = (JD - 2451545.0) / 36525.0;                             // Julian centuries since J2000
  double D  = 297.8501921 + 445267.1114034 * T - 0.0018819 * T * T;   // Moon mean elongation
  double Ms = 357.5291092 + 35999.0502909  * T;                       // Sun  mean anomaly
  double Mp = 134.9633964 + 477198.8675055 * T + 0.0087414 * T * T;   // Moon mean anomaly
  double Dm = fmod(D, 360.0); if (Dm < 0) Dm += 360.0;
  double Dr = Dm * D2R, Msr = fmod(Ms, 360.0) * D2R, Mpr = fmod(Mp, 360.0) * D2R;
  double i  = 180.0 - Dm
              - 6.289 * sin(Mpr) + 2.100 * sin(Msr)
              - 1.274 * sin(2 * Dr - Mpr) - 0.658 * sin(2 * Dr)
              - 0.214 * sin(2 * Mpr) - 0.110 * sin(Dr);               // phase angle (deg)

  Moon m;
  m.illum  = (1.0 + cos(i * D2R)) / 2.0;                              // 0 = new, 1 = full
  m.waxing = (Dm < 180.0);                                           // Moon east of Sun, growing

  // Age + phase name from the elongation implied by the illuminated fraction.
  double psi_p = acos(fmax(-1.0, fmin(1.0, 1.0 - 2.0 * m.illum))) / D2R;   // 0..180
  double psi   = m.waxing ? psi_p : 360.0 - psi_p;                   // 0 new .. 180 full .. 360
  double phase = psi / 360.0;                                        // 0..1 through the cycle
  m.ageDays = ((int)floor(phase * SYNODIC + 0.5)) % 30;

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

String hm12(int m, bool h24) {
  if (m < 0) return "--:--";
  int h = (m / 60) % 24, mn = m % 60;
  char buf[10];
  if (h24) { snprintf(buf, sizeof(buf), "%d:%02d", h, mn); return String(buf); }
  char suf = h < 12 ? 'a' : 'p';
  int h12 = h % 12; if (h12 == 0) h12 = 12;
  snprintf(buf, sizeof(buf), "%d:%02d%c", h12, mn, suf);
  return String(buf);
}

}  // namespace astro
