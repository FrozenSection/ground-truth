#include "seismic.h"
#include "config.h"
#include "settings.h"
#include "timekeeper.h"

#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>

namespace {
  const int    QUERY_LIMIT  = 100;            // bounded RAM; "100+" if hit
  const size_t PLACE_CAP    = 48;
  const char*  FDSN = "https://earthquake.usgs.gov/fdsnws/event/1/query";

  std::vector<seismic::Quake> g_events;
  time_t  g_lastFetch  = 0;
  bool    g_capped     = false;
  int     g_headline   = -1;
  float   g_recordMag  = 0;
  String  g_recordDate = "";

  double deg2rad(double d) { return d * M_PI / 180.0; }

  // Haversine great-circle distance (km) home -> event.
  float haversineKm(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0;
    double dlat = deg2rad(lat2 - lat1), dlon = deg2rad(lon2 - lon1);
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(deg2rad(lat1)) * cos(deg2rad(lat2)) * sin(dlon / 2) * sin(dlon / 2);
    return (float)(2 * R * atan2(sqrt(a), sqrt(1 - a)));
  }

  // Initial bearing (deg, 0=N) home -> event.
  float bearingDeg(double lat1, double lon1, double lat2, double lon2) {
    double dlon = deg2rad(lon2 - lon1);
    double y = sin(dlon) * cos(deg2rad(lat2));
    double x = cos(deg2rad(lat1)) * sin(deg2rad(lat2)) -
               sin(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(dlon);
    double b = atan2(y, x) * 180.0 / M_PI;
    if (b < 0) b += 360.0;
    return (float)b;
  }

  String buildUrl() {
    const auto& c = settings::get();
    String u = FDSN;
    u += "?format=geojson";
    u += "&latitude="    + String(c.lat, 4);
    u += "&longitude="   + String(c.lon, 4);
    u += "&maxradiuskm=" + String(c.radiusKm);
    u += "&minmagnitude="+ String(c.minMag, 1);
    u += "&orderby=time&limit=" + String(QUERY_LIMIT);
    if (timekeeper::synced())
      u += "&starttime=" + timekeeper::iso8601UTC(timekeeper::now() - 7L * 86400L);
    return u;
  }

  void selectHeadline() {
    g_headline = -1;
    int bestSig = -1; float bestMag = -100; time_t bestT = 0;
    for (size_t i = 0; i < g_events.size(); i++) {
      const auto& q = g_events[i];
      bool better = q.sig > bestSig ||
                    (q.sig == bestSig && q.mag > bestMag) ||
                    (q.sig == bestSig && q.mag == bestMag && q.t > bestT);
      if (better) { bestSig = q.sig; bestMag = q.mag; bestT = q.t; g_headline = (int)i; }
    }
  }

  void updateRecord() {
    for (const auto& q : g_events) {
      if (q.mag > g_recordMag) {
        g_recordMag = q.mag;
        struct tm lt; localtime_r(&q.t, &lt);
        char buf[12]; strftime(buf, sizeof(buf), "%b %e", &lt);
        g_recordDate = buf;
        Preferences p; p.begin("rec", false);
        p.putFloat("mag", g_recordMag);
        p.putString("date", g_recordDate);
        p.end();
      }
    }
  }
}  // namespace

namespace seismic {

void begin() {
  Preferences p; p.begin("rec", true);
  g_recordMag  = p.getFloat("mag", 0);
  g_recordDate = p.getString("date", "");
  p.end();
}

bool fetch() {
  if (WiFi.status() != WL_CONNECTED) return false;
  String url = buildUrl();
  Serial.printf("[usgs] GET %s\n", url.c_str());

  WiFiClientSecure client;
  client.setInsecure();                 // v1 — display-only data, MITM can't take over
  HTTPClient http;
  http.setTimeout(15000);
  http.setUserAgent("GroundTruth/" FIRMWARE_VERSION);
  if (!http.begin(client, url)) { Serial.println("[usgs] begin failed"); return false; }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[usgs] HTTP %d\n", code);
    http.end();
    return false;
  }

  // Filtered, streamed parse — only the fields we display.
  JsonDocument filter;
  JsonObject fp = filter["features"][0].to<JsonObject>();
  fp["properties"]["mag"]   = true;
  fp["properties"]["place"] = true;
  fp["properties"]["time"]  = true;
  fp["properties"]["sig"]   = true;
  fp["properties"]["felt"]  = true;
  fp["properties"]["alert"] = true;
  fp["geometry"]["coordinates"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) { Serial.printf("[usgs] parse: %s\n", err.c_str()); return false; }

  const auto& c = settings::get();
  JsonArray feats = doc["features"].as<JsonArray>();
  g_events.clear();
  g_capped = feats.size() >= (size_t)QUERY_LIMIT;

  for (JsonObject f : feats) {
    if (g_events.size() >= (size_t)QUERY_LIMIT) break;
    JsonObject pr = f["properties"];
    JsonArray  co = f["geometry"]["coordinates"];
    if (pr["mag"].isNull() || co.size() < 3) continue;

    Quake q;
    q.mag     = pr["mag"].as<float>();
    q.lon     = co[0].as<float>();
    q.lat     = co[1].as<float>();
    q.depthKm = co[2].as<float>();
    q.t       = (time_t)(pr["time"].as<long long>() / 1000);   // ms -> s
    q.sig     = pr["sig"].isNull()   ? 0 : pr["sig"].as<int>();
    q.felt    = pr["felt"].isNull()  ? 0 : pr["felt"].as<int>();
    q.alert   = !pr["alert"].isNull();
    q.place   = pr["place"].isNull() ? String("") : pr["place"].as<String>();
    if (q.place.length() > PLACE_CAP) q.place = q.place.substring(0, PLACE_CAP);
    q.distKm     = haversineKm(c.lat, c.lon, q.lat, q.lon);
    q.bearingDeg = bearingDeg(c.lat, c.lon, q.lat, q.lon);
    g_events.push_back(q);
  }

  selectHeadline();
  updateRecord();
  g_lastFetch = timekeeper::synced() ? timekeeper::now() : 1;
  Serial.printf("[usgs] %d events%s, headline idx %d\n",
                (int)g_events.size(), g_capped ? " (capped)" : "", g_headline);
  return true;
}

bool   hasData()   { return g_lastFetch != 0; }
time_t lastFetch() { return g_lastFetch; }
const std::vector<Quake>& events() { return g_events; }
int  headlineIndex() { return g_headline; }
bool count7dCapped() { return g_capped; }

int count24h() {
  time_t cut = timekeeper::now() - 86400L;
  int n = 0;
  for (const auto& q : g_events) if (q.t >= cut) n++;
  return n;
}

int count7d() { return (int)g_events.size(); }

int feltCount24h() {
  time_t cut = timekeeper::now() - 86400L;
  int n = 0;
  for (const auto& q : g_events) if (q.t >= cut && q.felt > 0) n++;
  return n;
}

void histogram(int& m2, int& m3, int& m4) {
  m2 = m3 = m4 = 0;
  for (const auto& q : g_events) {
    if      (q.mag >= 4) m4++;
    else if (q.mag >= 3) m3++;
    else                 m2++;     // includes anything below 3 (minMag is 2.5)
  }
}

void magRange(float& lo, float& hi) {
  lo = 99; hi = -99;
  for (const auto& q : g_events) { if (q.mag < lo) lo = q.mag; if (q.mag > hi) hi = q.mag; }
  if (g_events.empty()) { lo = hi = 0; }
}

float  recordMag()  { return g_recordMag; }
String recordDate() { return g_recordDate; }

const char* bearingName(float deg) {
  static const char* pts[] = {"N","NE","E","SE","S","SW","W","NW"};
  int idx = (int)((deg + 22.5f) / 45.0f) & 7;
  return pts[idx];
}

String badge(const Quake& q) {
  if (q.alert || q.felt > 0) return "felt nearby";
  if (q.sig >= 600)          return "significant";
  return "";
}

}  // namespace seismic
