#pragma once
#include <Arduino.h>
#include <time.h>

// NTP + POSIX TZ time. (Gate 3 adds the HTTP Date-header fallback and sunrise/
// sunset; moon phase lives in astro.h.) Recency + the 24 h/7 d windows need wall
// clock, so this comes in at Gate 2.
namespace timekeeper {
  void   begin(const String& tz);   // configTzTime + NTP servers
  bool   synced();                  // clock has a real (post-2021) time
  time_t now();

  String clockHM(bool h24);   // "3:42" or "15:42"
  String ampm(bool h24);      // "pm" / "am" (empty when h24)
  String dateStr();           // "Sat · Jun 13"
  String hms(time_t t);       // "h:mm a" for an arbitrary local time
  String relative(time_t t);  // "3 hours ago", "just now", ...
  String iso8601UTC(time_t t);// "2026-06-13T21:42:00" (for FDSN starttime)
}
