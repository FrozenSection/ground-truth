#include "viewstate.h"
#include "config.h"
#include <Preferences.h>

namespace {
  Preferences prefs;
  uint8_t     g_view = VIEW_MAP;
}

namespace viewstate {

void begin() {
  prefs.begin("view", true);
  g_view = prefs.getUChar("idx", VIEW_MAP);
  prefs.end();
  if (g_view >= VIEW_COUNT) g_view = VIEW_MAP;
  Serial.printf("[view] restored: %u (%s)\n", g_view, name(g_view));
}

uint8_t current() { return g_view; }

uint8_t next() {
  g_view = (g_view + 1) % VIEW_COUNT;
  prefs.begin("view", false);
  prefs.putUChar("idx", g_view);
  prefs.end();
  Serial.printf("[view] -> %u (%s)\n", g_view, name(g_view));
  return g_view;
}

const char* name(uint8_t view) {
  switch (view) {
    case VIEW_MAP:      return "Map";
    case VIEW_TIMELINE: return "Timeline";
    default:            return "?";
  }
}

}  // namespace viewstate
