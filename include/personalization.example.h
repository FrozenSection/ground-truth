#pragma once
// Per-build personalization — TEMPLATE.
//
// Copy this file to  include/personalization.h  (which is gitignored) and fill in the values
// for THIS device. personalization.h is NEVER committed, so the recipient's name and this
// device's unique passwords stay out of the public repo (see docs/ROADMAP.md — the
// personal-data audit). Anything left undefined falls back to the default in config.h.
//
// For a gift build, compile with  -DGIFT_BUILD  — it turns the "default password" warnings
// into hard ERRORS, so the firmware won't build until OTA_PASSWORD and AP_PASS are set.

// Boot splash — up to TWO centered lines under the "Ground Truth" title (recipient's name is
// PERSONAL DATA — keep it only here, never committed). Use a plain "-" hyphen, NOT an em-dash:
// the e-paper font is ASCII-only ("·" is fine). Leave RECIPIENT_SPLASH2 unset for one line.
//   #define RECIPIENT_SPLASH  "For Firstname"
//   #define RECIPIENT_SPLASH2 "Stay grounded - Uncle Scott"

// Firmware-update (OTA) password — protects /update, the only firmware-flash path.
// Pick a unique value per device and write it on the recovery card.
//   #define OTA_PASSWORD "pick-a-unique-one"

// Setup-AP password — the WPA2 key for the device's OWN "GroundTruth-Setup" network that it
// raises in Config mode (NOT the recipient's home/dorm WiFi). >=8 chars, unique per device;
// it's also shown on the device's setup screen, and goes on the recovery card.
//   #define AP_PASS "pick-a-unique-one"
