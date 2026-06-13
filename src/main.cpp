// Ground Truth — Seismic Desk Display
// Gate 1: connectivity + input. Captive-portal provisioning (WPA2), auto-AP
// fallback on a sustained move, transient-vs-persistent reconnect supervision,
// and the single smart button (tap = next view, hold = re-provision). No seismic
// data or final layouts yet — those are Gates 2-4. Serial-testable via the
// `headless` env.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

#include "config.h"
#include "provisioning.h"
#include "viewstate.h"
#include "display.h"

// Set from the button poll. Cleared by loop().
static volatile bool g_wifiResetPending = false;
static volatile bool g_viewChanged      = false;

// Smart button (GPIO BUTTON_PIN -> GND, pressed = LOW), sampled every loop pass.
//   tap (short)            -> next view (persisted)
//   hold >= BUTTON_HOLD_MS -> re-provision WiFi
// NOTE: Gate 4 wraps the hold in a two-step on-screen "Change WiFi? tap to
// confirm" frame (e-paper can't animate a countdown). For Gate 1 the hold fires
// the reset directly so the path is exercisable over serial.
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

static void showRunning(bool online) {
  epd::runningStatus(viewstate::name(viewstate::current()), online,
                     WiFi.localIP().toString(), provisioning::staMac());
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== %s v%s — Gate 1 ===\n", PROJECT_NAME, FIRMWARE_VERSION);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  viewstate::begin();
  epd::begin();
  epd::splash();
  Serial.printf("[net] station MAC: %s\n", provisioning::staMac().c_str());

  // ---- WiFi bring-up policy (no RTC, no offline data — so we provision rather
  // than run dark) ----
  if (!provisioning::hasCreds()) {
    epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac());
    provisioning::openPortal();                 // blocks; reboots on save
  }

  epd::message("Connecting", "joining WiFi...");
  bool online = provisioning::tryConnect();

  if (online) {
    provisioning::clearFresh();                 // creds proven good
  } else if (provisioning::freshlyProvisioned()) {
    // Creds entered moments ago don't connect (typo / wrong network) -> say so
    // and reopen the portal instead of auto-AP looping.
    Serial.println(F("[wifi] fresh creds failed -> portal"));
    epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac(), /*error=*/true);
    provisioning::openPortal();
  } else {
    // Previously-good creds aren't connecting. If the stored SSID isn't even in
    // range, we've likely moved (dorm move) -> open the setup AP unattended.
    // If it IS in range, treat it as transient and keep retrying in loop().
    if (!provisioning::ssidVisible(provisioning::storedSsid())) {
      Serial.println(F("[wifi] stored SSID not in range -> auto-AP"));
      epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac());
      provisioning::openPortal();
    } else {
      Serial.println(F("[wifi] SSID present but not connecting -> retry in loop"));
    }
  }

  showRunning(online);
}

void loop() {
  static unsigned long lastTick    = 0;
  static int           wasOnline   = -1;       // -1 forces first update
  static bool          servicesUp  = false;
  static unsigned long offlineSince = 0;
  static unsigned long lastReconnect = 0;
  static int           reconnectTries = 0;

  pollButton();                                // ~20ms sampling, before the gate

  if (g_wifiResetPending) {
    g_wifiResetPending = false;
    Serial.println(F("[main] WiFi reset -> clearing creds, rebooting to portal"));
    epd::message("WiFi reset", "rebooting to setup...");
    provisioning::forgetWiFi();
    delay(400);
    ESP.restart();
  }

  unsigned long nowMs = millis();
  if (nowMs - lastTick < TICK_INTERVAL_MS) { delay(20); return; }
  lastTick = nowMs;

  bool online = (WiFi.status() == WL_CONNECTED);

  // mDNS up on (re)connect, down on drop.
  if (online && !servicesUp) {
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", WEB_PORT);
      servicesUp = true;
      Serial.printf("[net] online -> http://%s.local  ip=%s\n",
                    MDNS_HOSTNAME, WiFi.localIP().toString().c_str());
    }
  }
  if (!online && servicesUp) { MDNS.end(); servicesUp = false; }

  // Reconnect supervision + auto-AP on a sustained move.
  if (online) {
    offlineSince = 0;
    reconnectTries = 0;
  } else {
    if (offlineSince == 0) offlineSince = nowMs;
    if (lastReconnect == 0 || nowMs - lastReconnect > RECONNECT_EVERY_MS) {
      if (++reconnectTries % 4 == 0) {
        Serial.println(F("[net] reconnect not sticking -> full radio restart"));
        WiFi.disconnect(true);
        delay(100);
        provisioning::beginStored();
      } else {
        WiFi.reconnect();
      }
      lastReconnect = nowMs;
    }
    // Persistent: offline long enough AND the SSID is gone -> reopen the portal.
    if (nowMs - offlineSince > AUTO_AP_AFTER_MS &&
        !provisioning::ssidVisible(provisioning::storedSsid())) {
      Serial.println(F("[net] sustained offline + SSID absent -> auto-AP"));
      epd::setupScreen(AP_NAME, AP_PASS, provisioning::staMac());
      provisioning::openPortal();              // blocks; reboots on save
    }
  }

  if ((int)online != wasOnline) {
    Serial.printf("[net] WiFi %s\n", online ? "up" : "down");
    wasOnline = online;
    showRunning(online);
  }

  if (g_viewChanged) {
    g_viewChanged = false;
    showRunning(online);                       // Gate 4: redraw the actual page
  }
}
