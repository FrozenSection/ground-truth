#pragma once
#include <Arduino.h>

// WiFi provisioning — a hand-rolled captive portal on ESPAsyncWebServer +
// DNSServer (the AirBox/countdown pattern; no WiFiManager library). Credentials
// live in NVS (Preferences, namespace "wifi"). The setup AP is WPA2-protected
// (AP_PASS) so provisioning isn't an open broadcast.
namespace provisioning {
  bool hasCreds();    // true if any WiFi credentials are saved in NVS

  // Try the saved credentials once (blocks up to WIFI_CONNECT_TIMEOUT_MS).
  // Returns true if connected. Does NOT open the portal on failure — the caller
  // decides (retry / scan / auto-AP).
  bool tryConnect();

  // Open the WPA2 captive-portal AP (AP_NAME / AP_PASS) and block until the user
  // saves, then reboot. Used for first-time setup and auto-AP recovery.
  void openPortal();

  // Wipe saved WiFi credentials from NVS. Caller reboots afterward.
  void forgetWiFi();

  // The portal sets a "fresh" flag when it saves creds; it survives the reboot.
  // If the very next connect FAILS, the caller knows the just-entered creds are
  // bad (typo, wrong network) and can reopen the portal with an error instead of
  // silently auto-AP looping. Cleared on the first successful connect.
  bool freshlyProvisioned();
  void clearFresh();

  // Non-blocking WiFi.begin() with stored creds — background reconnect path.
  void beginStored();

  String storedSsid();           // saved SSID ("" if none)
  String staMac();               // station MAC (the one campus networks register)
  bool   ssidVisible(const String& ssid);   // quick sync scan; is `ssid` in range?

  // ---- Runtime Config mode (Gate 1c) — non-blocking, unlike openPortal() ----
  // Raise the GroundTruth-Setup AP (AP_STA) + captive DNS *alongside* the running web
  // server, so a phone joins it and reaches the full settings page at 192.168.4.1. The
  // device keeps running (display + Ethernet) the whole time. Does NOT change the WiFi
  // on/off setting — the AP radio is temporary; mode is restored on stop.
  void   startConfigAP();
  void   stopConfigAP();         // drop AP + DNS, restore STA/OFF per the WiFi setting
  void   configAPLoop();         // pump the captive DNS — call from the main loop while active

  void   saveCreds(const String& ssid, const String& pass);  // store to NVS (no reboot)
  String scanJson();             // async WiFi-scan status as JSON (for the settings page)
}
