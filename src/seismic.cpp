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

  std::vector<seismic::Quake>   g_events;
  std::vector<seismic::Cluster> g_clusters;
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

  // Most-significant event with t >= cutoff (-1 if none). Tiebreak: sig, mag, recency.
  int bestSignificant(time_t cutoff) {
    int idx = -1, bestSig = -1; float bestMag = -100; time_t bestT = 0;
    for (size_t i = 0; i < g_events.size(); i++) {
      const auto& q = g_events[i];
      if (q.t < cutoff) continue;
      bool better = q.sig > bestSig ||
                    (q.sig == bestSig && q.mag > bestMag) ||
                    (q.sig == bestSig && q.mag == bestMag && q.t > bestT);
      if (better) { bestSig = q.sig; bestMag = q.mag; bestT = q.t; idx = (int)i; }
    }
    return idx;
  }

  void selectHeadline() {
    // Hybrid: most-significant in the last 24 h (keeps the hero fresh when there's
    // recent activity), else most-significant over the whole 7-day window (so a
    // quiet day still shows the region's biggest recent event rather than nothing).
    if (timekeeper::synced()) {
      g_headline = bestSignificant(timekeeper::now() - 86400L);
      if (g_headline < 0) g_headline = bestSignificant(0);
    } else {
      g_headline = bestSignificant(0);
    }
  }

  // Collapse tight swarms (>= SWARM_MIN_COUNT within SWARM_RADIUS_KM) to one
  // marker. The headline is excluded so it always shows as its own dot.
  void computeClusters() {
    g_clusters.clear();
    for (auto& q : g_events) q.clustered = false;
    std::vector<bool> assigned(g_events.size(), false);
    if (g_headline >= 0) assigned[g_headline] = true;

    for (size_t i = 0; i < g_events.size(); i++) {
      if (assigned[i]) continue;
      std::vector<size_t> group;
      for (size_t j = 0; j < g_events.size(); j++) {
        if (assigned[j]) continue;
        if (haversineKm(g_events[i].lat, g_events[i].lon,
                        g_events[j].lat, g_events[j].lon) <= SWARM_RADIUS_KM)
          group.push_back(j);
      }
      if ((int)group.size() < SWARM_MIN_COUNT) continue;

      double sLat = 0, sLon = 0; bool felt = false;
      for (size_t k : group) {
        assigned[k] = true;
        g_events[k].clustered = true;
        sLat += g_events[k].lat; sLon += g_events[k].lon;
        if (g_events[k].felt > 0) felt = true;
      }
      double cLat = sLat / group.size(), cLon = sLon / group.size();
      const auto& c = settings::get();
      seismic::Cluster cu;
      cu.distKm     = haversineKm(c.lat, c.lon, cLat, cLon);
      cu.bearingDeg = bearingDeg(c.lat, c.lon, cLat, cLon);
      cu.count      = (int)group.size();
      cu.anyFelt    = felt;
      g_clusters.push_back(cu);
    }
    if (!g_clusters.empty())
      Serial.printf("[usgs] %d swarm cluster(s) collapsed\n", (int)g_clusters.size());
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
    q.clustered  = false;
    g_events.push_back(q);
  }

  selectHeadline();
  computeClusters();
  updateRecord();
  g_lastFetch = timekeeper::synced() ? timekeeper::now() : 1;
  Serial.printf("[usgs] %d events%s, headline idx %d\n",
                (int)g_events.size(), g_capped ? " (capped)" : "", g_headline);
  return true;
}

bool   hasData()   { return g_lastFetch != 0; }
time_t lastFetch() { return g_lastFetch; }
const std::vector<Quake>& events() { return g_events; }
const std::vector<Cluster>& clusters() { return g_clusters; }
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
  if (q.alert)                            return "alert";
  if (q.felt > 0 && q.distKm <= 50)       return "felt nearby";
  if (q.felt > 0)                         return "felt";
  if (q.sig >= 600)                       return "significant";
  return "";
}

}  // namespace seismic
