#pragma once
#include <Arduino.h>

// W5500 wired Ethernet (Gate 1b). Brought up via arduino-esp32 3.x ETH-over-SPI, sharing
// the e-paper's SPI bus at 8 MHz. lwIP-integrated, so the existing HTTPS fetch routes over
// whichever link is up — `online = WiFi || eth::up()`. See docs/WIRING.md for pins.
namespace neteth {
  void   begin();     // install the W5500 driver on the shared SPI bus
  bool   present();   // driver installed OK (board responded)
  bool   up();        // link up AND has an IP
  String mac();       // W5500 MAC ("" if the driver didn't install)
  String ip();        // "0.0.0.0" until it has a lease
  String activeIP();  // the device's current IP: WiFi if joined, else Ethernet, else 0.0.0.0
}
