#include "timekeeper.h"
#include "config.h"

namespace {
  bool g_setViaHttp = false;

  // Days since 1970-01-01 for a civil (proleptic Gregorian) date — Howard Hinnant's
  // algorithm. Lets us turn the UTC fields from strptime into an epoch without timegm
  // (absent in this newlib).
  long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097LL + (long long)doe - 719468;
  }
}

namespace timekeeper {

void begin(const String& tz) {
  configTzTime(tz.c_str(), NTP_SERVER_1, NTP_SERVER_2);
  Serial.printf("[time] NTP started (tz=%s)\n", tz.c_str());
}

bool synced() { return time(nullptr) > 1700000000; }   // ~2023-11

bool setFromHttpDate(const String& httpDate) {
  if (synced()) return false;                 // NTP already gave us real time
  if (httpDate.length() < 25) return false;
  struct tm tm = {};
  if (!strptime(httpDate.c_str(), "%a, %d %b %Y %H:%M:%S", &tm)) return false;
  time_t t = (time_t)(daysFromCivil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday) * 86400LL
                      + tm.tm_hour * 3600LL + tm.tm_min * 60LL + tm.tm_sec);  // header is UTC
  if (t < 1700000000) return false;
  struct timeval tv = { t, 0 };
  settimeofday(&tv, nullptr);
  g_setViaHttp = true;
  Serial.printf("[time] set from HTTP Date header (%ld)\n", (long)t);
  return true;
}

const char* source() {
  if (!synced())    return "none";
  if (g_setViaHttp) return "http";
  return "ntp";
}

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
