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

}  // namespace astro
