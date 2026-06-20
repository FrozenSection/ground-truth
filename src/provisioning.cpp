#include "provisioning.h"
#include "config.h"
#include "settings.h"

#include <WiFi.h>
#include <esp_mac.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Captive-portal provisioning, ported from the proven countdown/AirBox build:
// pure ESPAsyncWebServer + DNSServer, no WiFiManager. Credentials in NVS
// (namespace "wifi"). The setup AP is WPA2 (AP_PASS); the portal also surfaces
// the device MAC for campus device-registration networks.
namespace {
  Preferences  prefs;
  DNSServer    dns;
  AsyncWebServer portalServer(WEB_PORT);

  volatile bool g_saveRequested = false;
  String        g_newSsid, g_newPass;

  const char PORTAL_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Truth setup</title><style>
body{font:16px/1.4 system-ui,sans-serif;max-width:440px;margin:2rem auto;padding:0 1rem}
h1{font-size:1.3rem}label{display:block;margin:.8rem 0 .2rem;font-size:.85rem;opacity:.8}
input,select{width:100%;box-sizing:border-box;font:inherit;padding:.55rem;border:1px solid #888;border-radius:8px}
button{margin-top:1.2rem;width:100%;font:inherit;padding:.6rem;border:0;border-radius:8px;background:#333;color:#fff}
small{opacity:.6}</style></head><body>
<h1>Ground Truth &mdash; setup</h1>
<p>Pick your WiFi network and enter the password. The device reboots and joins it.</p>
<label for="ssid">Network</label>
<select id="ssid"><option>scanning&hellip;</option></select>
<label for="manual">&hellip;or type it manually</label>
<input id="manual" placeholder="(leave blank to use the list above)">
<label for="pass">Password</label>
<input id="pass" type="password" placeholder="WiFi password">
<button onclick="save()">Save &amp; connect</button>
<p><small>Device MAC <span id="mac">&hellip;</span> &mdash; register this with a
campus/school network if it asks.</small></p>
<p><small id="status"></small></p>
<script>
fetch('/info').then(r=>r.json()).then(d=>{document.getElementById('mac').textContent=d.mac||'?'});
function refresh(){fetch('/scan').then(r=>r.json()).then(d=>{
  if(d.scanning){setTimeout(refresh,1200);return}
  var s=document.getElementById('ssid');s.innerHTML='';
  (d.networks||[]).forEach(function(n){var o=document.createElement('option');
    o.value=n.ssid;o.textContent=n.ssid+' ('+n.rssi+'dBm)'+(n.enc?' \u{1F512}':'');s.appendChild(o)});
  if(!s.options.length){var o=document.createElement('option');o.textContent='(none found)';s.appendChild(o)}
});}
function save(){var ssid=document.getElementById('manual').value||document.getElementById('ssid').value;
  var b=new URLSearchParams({ssid:ssid,pass:document.getElementById('pass').value});
  document.getElementById('status').textContent='Saving… device will reboot.';
  fetch('/save',{method:'POST',body:b});}
refresh();
</script></body></html>)HTML";

  void sendScanJson(AsyncWebServerRequest* req) {
    int n = WiFi.scanComplete();
    JsonDocument doc;
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true, true);   // re-kick so the list self-heals
      doc["scanning"] = true;
    } else if (n == WIFI_SCAN_RUNNING) {
      doc["scanning"] = true;
    } else {
      doc["scanning"] = false;
      JsonArray arr = doc["networks"].to<JsonArray>();
      for (int i = 0; i < n && i < 20; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["ssid"] = WiFi.SSID(i);
        o["rssi"] = WiFi.RSSI(i);
        o["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
      }
    }
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    serializeJson(doc, *res);
    req->send(res);
  }

  void registerPortalRoutes() {
    auto portal = [](AsyncWebServerRequest* req) {
      req->send(200, "text/html", PORTAL_HTML);
    };
    portalServer.on("/", HTTP_GET, portal);
    portalServer.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) { sendScanJson(req); });
    portalServer.on("/info", HTTP_GET, [](AsyncWebServerRequest* req) {
      JsonDocument doc;
      doc["mac"] = WiFi.macAddress();
      doc["ap"]  = AP_NAME;
      AsyncResponseStream* res = req->beginResponseStream("application/json");
      serializeJson(doc, *res);
      req->send(res);
    });
    portalServer.on("/save", HTTP_POST, [](AsyncWebServerRequest* req) {
      g_newSsid = req->hasParam("ssid", true) ? req->getParam("ssid", true)->value() : String("");
      g_newPass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : String("");
      req->send(200, "application/json", "{\"ok\":true}");
      g_saveRequested = true;
    });
    auto bounce = [](AsyncWebServerRequest* req) {
      req->redirect(String("http://") + AP_IP_STR + "/");
    };
    portalServer.on("/generate_204", HTTP_GET, bounce);          // Android
    portalServer.on("/gen_204", HTTP_GET, bounce);
    portalServer.on("/ncsi.txt", HTTP_GET, bounce);              // Windows
    portalServer.on("/connecttest.txt", HTTP_GET, bounce);
    portalServer.on("/hotspot-detect.html", HTTP_GET, portal);   // Apple
    portalServer.onNotFound(bounce);
  }

  bool tryConnectCreds(const String& ssid, const String& pass, uint32_t timeoutMs) {
    Serial.printf("[wifi] connecting to \"%s\"...\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
      delay(400);
      Serial.print('.');
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("\n[wifi] connected, ip=%s rssi=%d mac=%s\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                    WiFi.macAddress().c_str());
      return true;
    }
    Serial.println("\n[wifi] connect timed out");
    return false;
  }

  void runPortalBlocking() {
    IPAddress apIP(192, 168, 4, 1);
    Serial.println("[wifi] starting WPA2 captive portal (AP: " AP_NAME ")");
    // Quiesce the STA first: a failed connect leaves auto-reconnect re-associating
    // in the background, and every association aborts the scan -> "scanning..."
    // forever.
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true);
    delay(150);
    WiFi.mode(WIFI_AP_STA);                          // AP serves the page; STA scans
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_NAME, AP_PASS);                   // WPA2-protected setup network
    WiFi.scanNetworks(true, true);
    dns.start(53, "*", apIP);
    registerPortalRoutes();
    portalServer.begin();
    Serial.println("[wifi] portal ready at http://" AP_IP_STR
                   "  (join " AP_NAME " / " AP_PASS ")");

    while (true) {
      dns.processNextRequest();
      if (g_saveRequested) {
        Serial.printf("[wifi] saving creds for \"%s\", rebooting\n", g_newSsid.c_str());
        prefs.begin("wifi", false);
        prefs.putString("ssid", g_newSsid);
        prefs.putString("pass", g_newPass);
        prefs.putBool("fresh", true);
        prefs.end();
        delay(600);
        ESP.restart();
      }
      delay(10);
    }
  }
}  // namespace

namespace provisioning {

bool hasCreds() {
  prefs.begin("wifi", true);
  bool has = prefs.getString("ssid", "").length() > 0;
  prefs.end();
  return has;
}

bool tryConnect() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  if (!ssid.length()) return false;
  return tryConnectCreds(ssid, pass, WIFI_CONNECT_TIMEOUT_MS);
}

void openPortal() { runPortalBlocking(); }   // blocks; reboots on save

void forgetWiFi() {
  Preferences p;
  p.begin("wifi", false);
  p.clear();
  p.end();
  Serial.println(F("[wifi] credentials cleared"));
}

bool freshlyProvisioned() {
  prefs.begin("wifi", true);
  bool fresh = prefs.getBool("fresh", false);
  prefs.end();
  return fresh;
}

void clearFresh() {
  prefs.begin("wifi", false);
  prefs.remove("fresh");
  prefs.end();
}

void beginStored() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");
  prefs.end();
  if (!ssid.length()) return;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(ssid.c_str(), pass.c_str());
}

String storedSsid() {
  prefs.begin("wifi", true);
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid;
}

String staMac() {
  // Read the station MAC from efuse directly — valid even before the WiFi radio starts
  // (arduino-esp32 3.x's WiFi.macAddress() returns 00:00 until then, and the Connect
  // screen needs this for managed-network registration).
  uint8_t m[6]; esp_read_mac(m, ESP_MAC_WIFI_STA);
  char b[18];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

bool ssidVisible(const String& ssid) {
  if (!ssid.length()) return false;
  int n = WiFi.scanNetworks();                 // sync; ~2-4s. Used sparingly.
  bool found = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == ssid) { found = true; break; }
  }
  WiFi.scanDelete();
  return found;
}

// ---- Runtime Config mode (Gate 1c) ----
void startConfigAP() {
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP_STA);                       // AP serves config; STA keeps scan/connect available
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_NAME, AP_PASS);                // WPA2-protected
  dns.start(53, "*", apIP);                     // captive: everything resolves to the device
  WiFi.scanNetworks(true, true);               // kick an async scan so the SSID list is ready
  Serial.println(F("[cfg-ap] up -> join " AP_NAME " / " AP_PASS "  at http://" AP_IP_STR));
}

void stopConfigAP() {
  dns.stop();
  WiFi.softAPdisconnect(true);                  // drop the AP
  if (settings::get().wifiEnabled) { WiFi.mode(WIFI_STA); beginStored(); }  // resume home WiFi
  else                              WiFi.mode(WIFI_OFF);                     // back to Ethernet-only
  Serial.println(F("[cfg-ap] down"));
}

void configAPLoop() { dns.processNextRequest(); }

void saveCreds(const String& ssid, const String& pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.putBool("fresh", true);                 // verify on next boot (typo -> portal+error)
  prefs.end();
  Serial.printf("[wifi] creds saved for \"%s\"\n", ssid.c_str());
}

String scanJson() {
  int n = WiFi.scanComplete();
  JsonDocument doc;
  if (n == WIFI_SCAN_FAILED) { WiFi.scanNetworks(true, true); doc["scanning"] = true; }
  else if (n == WIFI_SCAN_RUNNING) { doc["scanning"] = true; }
  else {
    doc["scanning"] = false;
    JsonArray arr = doc["networks"].to<JsonArray>();
    for (int i = 0; i < n && i < 20; i++) {
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["enc"]  = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }
  }
  String out; serializeJson(doc, out); return out;
}

}  // namespace provisioning
