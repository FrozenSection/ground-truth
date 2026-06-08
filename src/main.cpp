// Ground Truth — Seismic Desk Display
// Gate 0: skeleton. Validates toolchain + display + GxEPD2 driver class before
// any application logic. Renders one static "hello" frame, then a serial
// heartbeat so the UART bridge / board liveness is observable without hardware
// on the bench.

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include "config.h"

// Full-buffer B/W instance for the 4.2" GDEY042T81 (400x300).
GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
    GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

static void drawHelloFrame() {
  display.setRotation(0);  // 400 wide x 300 tall (landscape). Confirm on panel.
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    // Border
    display.drawRect(4, 4, display.width() - 8, display.height() - 8, GxEPD_BLACK);

    // Title
    display.setFont(&FreeSansBold18pt7b);
    display.setCursor(24, 70);
    display.print(PROJECT_NAME);

    // Subtitle
    display.setFont(&FreeSans12pt7b);
    display.setCursor(24, 104);
    display.print("Seismic Desk Display");

    // Divider
    display.drawLine(24, 124, display.width() - 24, 124, GxEPD_BLACK);

    // Status block
    display.setFont(&FreeSans9pt7b);
    display.setCursor(24, 160);
    display.print("Gate 0 - skeleton OK");
    display.setCursor(24, 188);
    display.print("Firmware v" FIRMWARE_VERSION);
    display.setCursor(24, 216);
    display.print("ESP32 Feather V2 - GDEY042T81 400x300");

    // Footer
    display.setCursor(24, display.height() - 24);
    display.print("Hello from the ground.");
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("=== " PROJECT_NAME " ==="));
  Serial.println(F("Firmware v" FIRMWARE_VERSION " - Gate 0 skeleton"));

  Serial.print(F("Display init... "));
  display.init(115200);  // also routes GxEPD2 diagnostics to Serial
  Serial.println(F("ok"));

  Serial.print(F("Rendering hello frame... "));
  drawHelloFrame();
  display.hibernate();  // low-power; e-paper holds the image with no draw
  Serial.println(F("done"));
}

void loop() {
  // Heartbeat so board/UART liveness is visible with no peripherals attached.
  static uint32_t n = 0;
  Serial.printf("[heartbeat] uptime=%lus free_heap=%u n=%lu\n",
                millis() / 1000, ESP.getFreeHeap(), n++);
  delay(5000);
}
