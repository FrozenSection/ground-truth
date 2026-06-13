#include "display.h"
#include "config.h"

#ifdef HEADLESS_DISPLAY
// ----------------------------------------------------------------------------
// Headless: render frames to the serial monitor (no panel needed).
// ----------------------------------------------------------------------------
namespace {
  void frame(const String& title, const String& a, const String& b,
             const String& c = "", const String& d = "") {
    Serial.println(F("\n+------------------ e-paper ------------------+"));
    Serial.printf("|  %-42s|\n", title.c_str());
    if (a.length()) Serial.printf("|  %-42s|\n", a.c_str());
    if (b.length()) Serial.printf("|  %-42s|\n", b.c_str());
    if (c.length()) Serial.printf("|  %-42s|\n", c.c_str());
    if (d.length()) Serial.printf("|  %-42s|\n", d.c_str());
    Serial.println(F("+---------------------------------------------+"));
  }
}

namespace epd {
void begin() { Serial.println(F("[epd] headless serial-preview mode")); }

void message(const String& title, const String& line1, const String& line2) {
  frame(title, line1, line2);
}

void splash() {
  frame(PROJECT_NAME, "Seismic Desk Display", "Firmware v" FIRMWARE_VERSION);
}

void setupScreen(const String& apName, const String& apPass,
                 const String& mac, bool error) {
  frame(error ? "Couldn't join WiFi" : "Setup needed",
        "Join WiFi:  " + apName,
        "Password:   " + apPass,
        "then open http://" AP_IP_STR,
        "Device MAC: " + mac);
}

void runningStatus(const String& viewName, bool online,
                   const String& ip, const String& mac) {
  frame("View: " + viewName,
        online ? "online" : "offline (reconnecting)",
        online ? ("http://" MDNS_HOSTNAME ".local  (" + ip + ")") : String(""),
        "MAC: " + mac);
}
}  // namespace epd

#else
// ----------------------------------------------------------------------------
// Hardware: GxEPD2 full-buffer driver, simple full-refresh status frames.
// (Partial refresh + the map/timeline layouts come at Gate 4.)
// ----------------------------------------------------------------------------
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>

namespace {
  GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
      GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

  // Draw a titled text frame: bold title, then up to four body lines.
  void frame(const String& title, const String* lines, int n) {
    display.setRotation(0);
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setTextColor(GxEPD_BLACK);
      display.drawRect(4, 4, display.width() - 8, display.height() - 8, GxEPD_BLACK);
      display.setFont(&FreeSansBold18pt7b);
      display.setCursor(20, 64);
      display.print(title);
      display.drawLine(20, 80, display.width() - 20, 80, GxEPD_BLACK);
      display.setFont(&FreeSans12pt7b);
      int y = 120;
      for (int i = 0; i < n; i++) {
        display.setCursor(20, y);
        display.print(lines[i]);
        y += 34;
      }
    } while (display.nextPage());
  }
}

namespace epd {
void begin() {
  // Park the unused Friend chip-selects HIGH so they don't fight the EPD on the
  // shared SPI bus (countdown gotcha).
  pinMode(SRAM_CS, OUTPUT); digitalWrite(SRAM_CS, HIGH);
  pinMode(SD_CS,   OUTPUT); digitalWrite(SD_CS,   HIGH);
  display.init(115200);
  Serial.println(F("[epd] GxEPD2 GDEY042T81 ready"));
}

void message(const String& title, const String& line1, const String& line2) {
  String lines[2]; int n = 0;
  if (line1.length()) lines[n++] = line1;
  if (line2.length()) lines[n++] = line2;
  frame(title, lines, n);
}

void splash() {
  String lines[2] = { "Seismic Desk Display", "Firmware v" FIRMWARE_VERSION };
  frame(PROJECT_NAME, lines, 2);
}

void setupScreen(const String& apName, const String& apPass,
                 const String& mac, bool error) {
  String lines[4] = {
    "Join WiFi:  " + apName,
    "Password:   " + apPass,
    "then open http://" AP_IP_STR,
    "MAC " + mac
  };
  frame(error ? "Couldn't join WiFi" : "Setup needed", lines, 4);
}

void runningStatus(const String& viewName, bool online,
                   const String& ip, const String& mac) {
  String lines[3] = {
    online ? "online" : "offline - reconnecting",
    online ? ("http://" MDNS_HOSTNAME ".local") : String(""),
    "MAC " + mac
  };
  frame("View: " + viewName, lines, 3);
}
}  // namespace epd

#endif  // HEADLESS_DISPLAY
