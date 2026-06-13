#include "timekeeper.h"
#include "config.h"

namespace timekeeper {

void begin(const String& tz) {
  configTzTime(tz.c_str(), NTP_SERVER_1, NTP_SERVER_2);
  Serial.printf("[time] NTP started (tz=%s)\n", tz.c_str());
}

bool synced() { return time(nullptr) > 1700000000; }   // ~2023-11

time_t now() { return time(nullptr); }

static void local(struct tm& out) { time_t t = time(nullptr); localtime_r(&t, &out); }

String clockHM(bool h24) {
  struct tm lt; local(lt);
  int h = lt.tm_hour;
  if (!h24) { h = h % 12; if (h == 0) h = 12; }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", h, lt.tm_min);
  return String(buf);
}

String ampm(bool h24) {
  if (h24) return "";
  struct tm lt; local(lt);
  return lt.tm_hour < 12 ? "am" : "pm";
}

String dateStr() {
  struct tm lt; local(lt);
  char buf[20];
  strftime(buf, sizeof(buf), "%a · %b %e", &lt);   // "Sat · Jun 13"
  return String(buf);
}

String hms(time_t t) {
  struct tm lt; localtime_r(&t, &lt);
  int h = lt.tm_hour % 12; if (h == 0) h = 12;
  char buf[10];
  snprintf(buf, sizeof(buf), "%d:%02d%c", h, lt.tm_min, lt.tm_hour < 12 ? 'a' : 'p');
  return String(buf);
}

String relative(time_t t) {
  long s = (long)(time(nullptr) - t);
  if (s < 0) s = 0;
  if (s < 90)            return "just now";
  long m = s / 60;
  if (m < 60)            return String(m) + " min ago";
  long h = m / 60;
  if (h < 24)            return String(h) + (h == 1 ? " hour ago" : " hours ago");
  long d = h / 24;
  return String(d) + (d == 1 ? " day ago" : " days ago");
}

String iso8601UTC(time_t t) {
  struct tm g; gmtime_r(&t, &g);
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &g);
  return String(buf);
}

}  // namespace timekeeper
