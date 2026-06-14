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
#include "statelock.h"
#include <math.h>

// Definition of the shared-state lock declared in statelock.h. Held briefly when
// publishing seismic results / applying settings (main loop) and when serving
// /api/state (web task).
std::mutex g_stateMutex;

static volatile bool g_wifiResetPending = false;
static volatile bool g_viewChanged      = false;
static bool          g_awaitWifiConfirm = false;   // 3 s hold armed the Change-WiFi confirm
static unsigned long g_confirmUntil     = 0;       // auto-cancel deadline

// Render the active view from live module state. timeOK gates time-derived fields
// (the "Acquiring time" treatment); online drives the reconnecting glyph; stale
// stamps the readout when fetches have lapsed.
static void renderCurrent() {
  bool timeOK = timekeeper::synced();
  bool online = (WiFi.status() == WL_CONNECTED);
  bool stale  = false;
  time_t lf = seismic::lastFetch();
  if (timeOK && seismic::hasData() && lf > 0) {
    long age   = (long)(timekeeper::now() - lf);
    long pollS = (long)settings::get().pollMin * 60;
    stale = age > (pollS * 3 + 120);               // missed ~3 polls -> data is old
  }
  epd::renderView(viewstate::current(), timeOK, online, stale);
}

// Smart button (GPIO BUTTON_PIN -> GND): tap = next view; 3 s hold = arm a
// Change-WiFi confirm (a second tap confirms, inaction cancels — DISPLAY-SPEC §7).
static void pollButton() {
  static bool          wasDown   = false;
  static unsigned long downAt    = 0;
  static bool          longFired = false;
  bool down = (digitalRead(BUTTON_PIN) == LOW);
  unsigned long ms = millis();
  if (down && !wasDown) { downAt = ms; longFired = false; }
  if (down && !longFired && ms - downAt >= BUTTON_HOLD_MS) {
    longFired = true;
    g_awaitWifiConfirm = true; g_confirmUntil = ms + 8000;
    epd::changeWifiConfirm(provisioning::storedSsid());
    Serial.println(F("[btn] hold -> Change-WiFi confirm (tap to confirm, wait to cancel)"));
  }
  if (!down && wasDown && !longFired && ms - downAt >= BUTTON_DEBOUNCE_MS) {
    if (g_awaitWifiConfirm) {                       // confirming the WiFi change
      g_awaitWifiConfirm = false;
      g_wifiResetPending = true;
    } else {                                        // normal tap = cycle view
      viewstate::next();
      g_viewChanged = true;
    }
  }
  wasDown = down;
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
  Serial.printf("\n=== %s v%s ===\n", PROJECT_NAME, FIRMWARE_VERSION);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  viewstate::begin();
  settings::begin();
  seismic::begin();
  epd::begin();
  epd::splash();
  Serial.printf("[net] station MAC: %s\n", provisioning::staMac().c_str());

  if (!provisioning::hasCreds()) {
    // Ethernet MAC is "" until the W5500 lands (Gate 1b) — the Connect screen
    // shows it as not-installed; the WiFi path is the only one until then.
    epd::connectScreen(AP_NAME, AP_PASS, provisioning::staMac(), "");
    provisioning::openPortal();                 // blocks; reboots on save
  }

  epd::message("Connecting", "joining WiFi...");
  bool online = provisioning::tryConnect();
  if (online) {
    provisioning::clearFresh();
  } else if (provisioning::freshlyProvisioned()) {
    // Creds entered moments ago don't connect (typo / wrong network) -> portal+error.
    Serial.println(F("[wifi] fresh creds failed -> portal"));
    epd::connectScreen(AP_NAME, AP_PASS, provisioning::staMac(), "", /*error=*/true);
    provisioning::openPortal();
  } else {
    // Previously-good creds not connecting (router down, transient, or moved). Do NOT
    // auto-open a blocking portal — keep retrying STA in loop() and retain the last
    // frame. A real move is re-provisioned deliberately (button hold / web Change-WiFi),
    // so a managed-network maintenance window can't strand the device in AP mode.
    Serial.println(F("[wifi] not connecting now -> keep retrying in loop"));
  }

  epd::message("Ground Truth", online ? "syncing time + data..." : "offline - reconnecting");
}

void loop() {
  static unsigned long lastTick     = 0;
  static int           wasOnline    = -1;
  static bool          servicesUp   = false;
  static bool          netUp        = false;
  static unsigned long lastReconnect = 0;
  static int           reconnectTries = 0;
  static unsigned long lastFetchMs  = 0;
  static bool          firstFetch   = false;
  static bool          forceFetch   = false;
  static float         appliedLat   = settings::get().lat;   // for location-change detect
  static float         appliedLon   = settings::get().lon;
  static int           lastMinute   = -1;                    // footer partial-refresh boundary
  static int           wasTimeOK    = -1;                    // full re-render when the clock lands

  pollButton();

  // Auto-cancel an unconfirmed Change-WiFi prompt: redraw the live view.
  if (g_awaitWifiConfirm && (long)(millis() - g_confirmUntil) >= 0) {
    g_awaitWifiConfirm = false;
    Serial.println(F("[btn] Change-WiFi confirm timed out -> cancelled"));
    renderCurrent();
  }

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
    netUp = true;
  }
  if (netUp) web::loop();   // ElegantOTA housekeeping

  // Settings saved from the web: re-apply TZ live, reset the all-time record if the
  // location moved materially, and force an immediate re-fetch (no reboot).
  if (web::consumeApplyConfig()) {
    Serial.println(F("[main] settings saved -> re-apply TZ + re-fetch"));
    timekeeper::begin(settings::get().tz);
    const auto& cfg = settings::get();
    if (fabs(cfg.lat - appliedLat) > 0.1f || fabs(cfg.lon - appliedLon) > 0.1f) {
      seismic::resetRecord();
    }
    appliedLat = cfg.lat; appliedLon = cfg.lon;
    forceFetch = true;
  }

  // Reconnect supervision — keep retrying STA forever when offline (never auto-open a
  // blocking portal; reprovision is the deliberate button-hold / web Change-WiFi).
  if (online) { reconnectTries = 0; }
  else if (lastReconnect == 0 || nowMs - lastReconnect > RECONNECT_EVERY_MS) {
    if (++reconnectTries % 4 == 0) {
      Serial.println(F("[net] reconnect not sticking -> full radio restart"));
      WiFi.disconnect(true); delay(100); provisioning::beginStored();
    } else { WiFi.reconnect(); }
    lastReconnect = nowMs;
  }

  if ((int)online != wasOnline) {
    Serial.printf("[net] WiFi %s\n", online ? "up" : "down");
    wasOnline = online;
    if (firstFetch && !g_awaitWifiConfirm) renderCurrent();   // toggle the reconnecting glyph
  }

  // Poll USGS. We fetch as soon as we're online (even before NTP): the fetch reads
  // the HTTPS Date header to set the clock when NTP is blocked, and time-dependent
  // display fields are gated on a trustworthy clock elsewhere.
  if (online && netUp) {
    unsigned long pollMs = (unsigned long)settings::get().pollMin * 60000UL;
    if (!firstFetch || forceFetch || nowMs - lastFetchMs >= pollMs) {
      if (seismic::fetch()) {
        lastFetchMs = nowMs; firstFetch = true; forceFetch = false;
        printSummary();
        renderCurrent();
      } else {
        lastFetchMs = nowMs - pollMs + 30000UL;   // retry in ~30 s
      }
    }
  }

  if (g_viewChanged && !g_awaitWifiConfirm) {
    g_viewChanged = false;
    Serial.printf("[view] now: %s\n", viewstate::name(viewstate::current()));
    renderCurrent();
  }

  // The moment the clock becomes trustworthy: re-fetch AND re-render. The first
  // fetch can run before the clock is good (it bootstraps time from the HTTPS Date
  // header); that pre-sync query omits the 7-day `starttime`, so its window is wrong
  // and can even come back empty — a false "Quiet" that otherwise persists until the
  // next 10-minute poll. Forcing a fetch here makes the dataset properly 7-day-windowed
  // as soon as time is known. (hero recency + 24 h stat also appear, footer leaves the
  // "Setting clock…" state.)
  bool timeOK = timekeeper::synced();
  if ((int)timeOK != wasTimeOK) {
    wasTimeOK = timeOK;
    if (timeOK && firstFetch) { forceFetch = true; renderCurrent(); }
  }

  // Tick the footer clock on each minute boundary via a no-flash partial refresh
  // (full renders only happen on poll / view / connectivity change). Skip while a
  // confirm prompt is up so we don't paint over it.
  if (timeOK && firstFetch && !g_awaitWifiConfirm) {
    struct tm lt; time_t n = timekeeper::now(); localtime_r(&n, &lt);
    if (lt.tm_min != lastMinute) { lastMinute = lt.tm_min; epd::refreshFooter(true); }
  }
}
