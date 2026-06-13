// Ground Truth — Seismic Desk Display
// Gate 2: seismic data + web mirror, on top of Gate 1 connectivity/input.
// Polls USGS, parses, computes headline + aggregates, serves /api/state and a
// live B-tight mirror page. No final e-paper layouts yet (Gate 4) — the panel
// shows a status/headline summary; the *web page* is the visual for now.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include "config.h"
#include "provisioning.h"
#include "viewstate.h"
#include "display.h"
#include "settings.h"
#include "timekeeper.h"
#include "seismic.h"
#include "web.h"

static volatile bool g_wifiResetPending = false;
static volatile bool g_viewChanged      = false;

// Smart button (GPIO BUTTON_PIN -> GND): tap = next view, hold = re-provision.
static void pollButton() {
  static bool          wasDown   = false;
  static unsigned long downAt    = 0;
  static bool          longFired = false;
  bool down = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long ms = millis();
  if (down && !wasDown) { downAt = ms; longFired = false; }
  if (down && !longFired && ms - downAt >= BUTTON_HOLD_MS) {
    longFired = true;
    Serial.println(F("[btn] hold -> WiFi re-provision (Gate 4 will add confirm)"));
    g_wifiResetPending = true;
  }
  if (!down && wasDown && !longFired && ms - downAt >= BUTTON_DEBOUNCE_MS) {
    viewstate::next();
    g_viewChanged = true;
  }
  wasDown = down;
}

// Interim panel output until the Gate 4 layouts land: headline summary.
static void showHeadline() {
  const auto& evs = seismic::events();
  int h = seismic::headlineIndex();
  if (h < 0) {
    epd::message("Quiet", "no events in range", timekeeper::dateStr());
    return;
  }
  const auto& q = evs[h];
  char dist[40];
  snprintf(dist, sizeof(dist), "%d %s %s · %s",
           (int)round(settings::toDisplayDist(q.distKm)), settings::distUnit(),
           seismic::bearingName(q.bearingDeg), timekeeper::relative(q.t).c_str());
  epd::message(String("M") + String(q.mag, 1), q.place, dist);
}

static void printSummary() {
  int h = seismic::headlineIndex();
  Serial.printf("[gt] %s | 24h:%d 7d:%d%s | rec M%.1f %s\n",
                h < 0 ? "quiet" : (String("headline M") +
                  String(seismic::events()[h].mag, 1)).c_str(),
                seismic::count24h(), seismic::count7d(),
                seismic::count7dCapped() ? "+" : "",
                seismic::recordMag(), seismic::recordDate().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== %s v%s — Gate 2 ===\n", PROJECT_NAME, FIRMWARE_VERSION);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  viewstate::begin();
  settings::begin();
  seismic::begin();
  epd::begin();
  epd::splash();
  Serial.printf("[net] station MAC: %s\n", provisioning::staMac().c_str());

  if (!provisioning::hasCreds()) {
    epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac());
    provisioning::openPortal();                 // blocks; reboots on save
  }

  epd::message("Connecting", "joining WiFi...");
  bool online = provisioning::tryConnect();
  if (online) {
    provisioning::clearFresh();
  } else if (provisioning::freshlyProvisioned()) {
    Serial.println(F("[wifi] fresh creds failed -> portal"));
    epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac(), /*error=*/true);
    provisioning::openPortal();
  } else if (!provisioning::ssidVisible(provisioning::storedSsid())) {
    Serial.println(F("[wifi] stored SSID not in range -> auto-AP"));
    epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac());
    provisioning::openPortal();
  } else {
    Serial.println(F("[wifi] SSID present but not connecting -> retry in loop"));
  }

  epd::message("Ground Truth", online ? "syncing time + data..." : "offline - reconnecting");
}

void loop() {
  static unsigned long lastTick     = 0;
  static int           wasOnline    = -1;
  static bool          servicesUp   = false;
  static bool          netUp        = false;
  static unsigned long netUpMs      = 0;
  static unsigned long offlineSince = 0;
  static unsigned long lastReconnect = 0;
  static int           reconnectTries = 0;
  static unsigned long lastFetchMs  = 0;
  static bool          firstFetch   = false;

  pollButton();

  if (g_wifiResetPending || web::consumeWifiReset()) {
    g_wifiResetPending = false;
    Serial.println(F("[main] WiFi reset -> clearing creds, rebooting to portal"));
    epd::message("WiFi reset", "rebooting to setup...");
    provisioning::forgetWiFi();
    delay(400);
    ESP.restart();
  }
  if (web::consumeReboot()) {
    Serial.println(F("[main] reboot requested (settings/OTA)"));
    epd::message("Rebooting", "applying settings...");
    delay(400);
    ESP.restart();
  }

  unsigned long nowMs = millis();
  if (nowMs - lastTick < TICK_INTERVAL_MS) { delay(20); return; }
  lastTick = nowMs;

  bool online = (WiFi.status() == WL_CONNECTED);

  // mDNS + (once) time/web services on (re)connect.
  if (online && !servicesUp) {
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", WEB_PORT);
      servicesUp = true;
      Serial.printf("[net] online -> http://%s.local  ip=%s\n",
                    MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
    }
  }
  if (!online && servicesUp) { MDNS.end(); servicesUp = false; }
  if (online && !netUp) {
    timekeeper::begin(settings::get().tz);
    web::begin();
    netUp   = true;
    netUpMs = nowMs;
  }
  if (netUp) web::loop();   // ElegantOTA housekeeping

  // Reconnect supervision + auto-AP on a sustained move.
  if (online) { offlineSince = 0; reconnectTries = 0; }
  else {
    if (offlineSince == 0) offlineSince = nowMs;
    if (lastReconnect == 0 || nowMs - lastReconnect > RECONNECT_EVERY_MS) {
      if (++reconnectTries % 4 == 0) {
        Serial.println(F("[net] reconnect not sticking -> full radio restart"));
        WiFi.disconnect(true); delay(100); provisioning::beginStored();
      } else { WiFi.reconnect(); }
      lastReconnect = nowMs;
    }
    if (nowMs - offlineSince > AUTO_AP_AFTER_MS &&
        !provisioning::ssidVisible(provisioning::storedSsid())) {
      Serial.println(F("[net] sustained offline + SSID absent -> auto-AP"));
      epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac());
      provisioning::openPortal();
    }
  }

  if ((int)online != wasOnline) {
    Serial.printf("[net] WiFi %s\n", online ? "up" : "down");
    wasOnline = online;
  }

  // Poll USGS. Wait for NTP (recency/windows need it) but don't block forever on
  // a network that filters NTP — fall through after a grace period (Gate 3 adds
  // the HTTP Date-header fallback).
  if (online && netUp) {
    bool timeReady = timekeeper::synced() || (nowMs - netUpMs > 45000UL);
    unsigned long pollMs = (unsigned long)settings::get().pollMin * 60000UL;
    if (timeReady && (!firstFetch || nowMs - lastFetchMs >= pollMs)) {
      if (seismic::fetch()) {
        lastFetchMs = nowMs; firstFetch = true;
        printSummary();
        showHeadline();
      } else {
        lastFetchMs = nowMs - pollMs + 30000UL;   // retry in ~30 s
      }
    }
  }

  if (g_viewChanged) {
    g_viewChanged = false;
    Serial.printf("[view] now: %s\n", viewstate::name(viewstate::current()));
    showHeadline();   // Gate 4: redraw the actual page
  }
}
