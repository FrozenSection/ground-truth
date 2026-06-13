#pragma once
#include <Arduino.h>

// E-paper status rendering for Gate 1 (boot / provisioning / connectivity).
// Full page layouts (map, timeline) arrive at Gate 4. Built so the `headless`
// env (-DHEADLESS_DISPLAY) prints frames to serial instead of driving a panel —
// lets us exercise connectivity + button + view logic on a bare Feather.
namespace epd {
  void begin();

  // Centered status frame (full refresh): title + up to two lines.
  void message(const String& title, const String& line1 = "",
               const String& line2 = "");

  // Boot splash (project name + firmware version).
  void splash();

  // Provisioning screen: tells the user to join the WPA2 setup AP, shows the
  // password and the device MAC (for campus registration). `error` flips the
  // headline to a "couldn't join" retry message.
  void setupScreen(const String& apName, const String& apPass,
                   const String& mac, bool error = false);

  // A minimal running status frame (until Gate 4 view layouts exist): which view
  // is active, online state, IP/host, MAC.
  void runningStatus(const String& viewName, bool online,
                     const String& ip, const String& mac);
}
