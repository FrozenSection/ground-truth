#pragma once
#include <Arduino.h>

// Device health / hardening (Gate 6). The display has no hardware reset button, runs
// unattended for months, and the maker loses access after it ships — so it must catch its
// own hangs and make a remote soak diagnosable.
//   - Task watchdog on the loop task: a genuine loop hang reboots the device.
//   - Reset-reason captured at boot: power-on vs panic vs watchdog vs brownout.
//   - Heap watch: free + lowest-ever, to surface a slow leak before it bites.
namespace health {
  void     begin();        // call EARLY in setup(): capture+log the reset reason
  void     startWatchdog(); // call at the END of setup(): arm the WDT (after blocking setup)
  void     feed();         // pet the watchdog — call at the top of every loop() iteration
  void     tick();         // periodic heap log — call in loop(); self-throttles

  String   resetReason();  // why we last booted (human string)
  uint32_t freeHeap();     // current free heap, bytes
  uint32_t minHeap();      // lowest free heap since boot — a leak walks this down
}
