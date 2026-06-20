#include "health.h"
#include "config.h"
#include <esp_task_wdt.h>
#include <esp_system.h>

namespace {
  String g_resetReason = "?";

  const char* reasonStr(esp_reset_reason_t r) {
    switch (r) {
      case ESP_RST_POWERON:   return "power-on";
      case ESP_RST_SW:        return "software reboot";
      case ESP_RST_PANIC:     return "panic / crash";
      case ESP_RST_INT_WDT:   return "interrupt watchdog";
      case ESP_RST_TASK_WDT:  return "task watchdog (hang)";
      case ESP_RST_WDT:       return "watchdog";
      case ESP_RST_BROWNOUT:  return "brownout (power dip)";
      case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
      case ESP_RST_EXT:       return "external reset";
      default:                return "unknown";
    }
  }
}

namespace health {

void begin() {
  g_resetReason = reasonStr(esp_reset_reason());
  Serial.printf("[health] last reset: %s | heap %u\n",
                g_resetReason.c_str(), (unsigned)ESP.getFreeHeap());
}

void startWatchdog() {
  // The IDF inits the Task WDT at boot (5 s, no tasks subscribed). 5 s is far shorter than
  // our longest blocking call (the ~15-17 s USGS fetch), so widen it first, then subscribe
  // the Arduino loop task. A real loop hang then panics -> reboot (reset reason 'task
  // watchdog'). Armed at the END of setup() so the blocking first-time portal / connect
  // can't trip it. trigger_panic gives a backtrace + a clean restart.
  esp_task_wdt_config_t cfg = {
    .timeout_ms    = WDT_TIMEOUT_MS,
    .idle_core_mask = 0,            // watch our loop task only — not idle tasks (TLS/e-paper
                                    // bursts legitimately starve idle and would false-trip)
    .trigger_panic = true,
  };
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK) esp_task_wdt_init(&cfg);  // init if not already
  esp_task_wdt_add(NULL);          // subscribe the current (loop) task
  Serial.printf("[health] task watchdog armed (%lu ms)\n", (unsigned long)WDT_TIMEOUT_MS);
}

void feed() { esp_task_wdt_reset(); }

void tick() {
  static unsigned long last = 0;
  if (millis() - last < HEALTH_LOG_EVERY_MS) return;
  last = millis();
  Serial.printf("[health] heap free %u (min %u) | up %lu s | last reset %s\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMinFreeHeap(),
                millis() / 1000UL, g_resetReason.c_str());
}

String   resetReason() { return g_resetReason; }
uint32_t freeHeap()    { return ESP.getFreeHeap(); }
uint32_t minHeap()     { return ESP.getMinFreeHeap(); }

}  // namespace health
