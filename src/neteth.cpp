#include "neteth.h"
#include "config.h"
#include <ETH.h>
#include <WiFi.h>
#include <SPI.h>

namespace {
  bool          g_present = false;
  volatile bool g_gotIP   = false;

  void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
    (void)info;
    switch (event) {
      case ARDUINO_EVENT_ETH_START:        Serial.println(F("[eth] start")); break;
      case ARDUINO_EVENT_ETH_CONNECTED:    Serial.println(F("[eth] link up")); break;
      case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.printf("[eth] got IP %s  MAC %s\n",
                      ETH.localIP().toString().c_str(), ETH.macAddress().c_str());
        g_gotIP = true; break;
      case ARDUINO_EVENT_ETH_LOST_IP:      Serial.println(F("[eth] lost IP")); g_gotIP = false; break;
      case ARDUINO_EVENT_ETH_DISCONNECTED: Serial.println(F("[eth] link down")); g_gotIP = false; break;
      case ARDUINO_EVENT_ETH_STOP:         Serial.println(F("[eth] stop")); g_gotIP = false; break;
      default: break;
    }
  }
}

namespace neteth {

void begin() {
  Network.onEvent(onEvent);
  // SPI was already begun by the display (SCK5/MISO21/MOSI19); the W5500 joins that bus
  // with its own CS, clocked at 8 MHz. Link-up + DHCP arrive asynchronously via onEvent().
  g_present = ETH.begin(ETH_PHY_W5500, 1, W5500_CS, W5500_IRQ, W5500_RST, SPI, W5500_SPI_MHZ);
  Serial.printf("[eth] begin %s (cs=%d irq=%d rst=%d, %d MHz)\n",
                g_present ? "ok" : "FAILED — no W5500?",
                W5500_CS, W5500_IRQ, W5500_RST, W5500_SPI_MHZ);
  // Prefer Ethernet for outbound when both links are up. IDF defaults are STA route_prio=100,
  // ETH=50 (so WiFi would win by default); raise ETH above the STA default rather than lowering
  // STA, since the STA netif is recreated on every WiFi reconnect (which would reset its prio).
  if (g_present) ETH.setRoutePrio(110);
}

bool   present() { return g_present; }
bool   up()      { return g_present && g_gotIP; }
String mac()     { return g_present ? ETH.macAddress() : String(""); }
String ip()      { return g_gotIP ? ETH.localIP().toString() : String("0.0.0.0"); }

String activeIP() {
  if (up())                          return ETH.localIP().toString();   // Ethernet is the preferred route
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return String("0.0.0.0");
}

}  // namespace neteth
