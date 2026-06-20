#include "display.h"
#include "config.h"
#include "settings.h"
#include "timekeeper.h"
#include "seismic.h"
#include "astro.h"
#include "viewstate.h"
#include <WiFi.h>      // Info page: IP / MAC / RSSI
#include <math.h>
#include <time.h>

// ============================================================================
// Ground Truth — Gate 4 page renderer.
// Layouts are the locked designer frames in docs/DISPLAY-SPEC.md:
//   §2 bands (hero 0–90 / content 90–242 / footer 242–300, dividers at 90/242)
//   §3 persistent Sky Footer · §4 Page-1 radial Map · §5 Page-2 lollipop Timeline
//   §6 shared Hero · §7 alternate/system states.
// One full-buffer render per call (full refresh; partial-refresh page swaps are a
// later optimization). The renderer reads the live module state directly — it's
// the view layer, driven from the main loop after each fetch / view change.
// ============================================================================

#ifdef HEADLESS_DISPLAY
// ----------------------------------------------------------------------------
// Headless: render compact text frames to serial (no panel).
// ----------------------------------------------------------------------------
namespace {
  void frame(const String& title, const String& a, const String& b = "",
             const String& c = "", const String& d = "") {
    Serial.println(F("\n+------------------ e-paper ------------------+"));
    Serial.printf("|  %-42s|\n", title.c_str());
    for (const String* s : {&a, &b, &c, &d})
      if (s->length()) Serial.printf("|  %-42s|\n", s->c_str());
    Serial.println(F("+---------------------------------------------+"));
  }
}

namespace epd {
void begin() { Serial.println(F("[epd] headless serial-preview mode")); }

void splash() {
  frame(PROJECT_NAME, RECIPIENT_SPLASH, "Firmware v" FIRMWARE_VERSION);
}

void connectScreen(const String& apName, const String& apPass,
                   const String& wifiMac, const String& ethMac, bool error) {
  frame(error ? "Couldn't join WiFi" : "Connect Ground Truth",
        "1. Join WiFi " + apName + " / " + apPass + "  (192.168.4.1)",
        "2. Or plug in Ethernet (no setup)",
        "WiFi MAC: " + wifiMac,
        "Ethernet MAC: " + (ethMac.length() ? ethMac : String("(board not installed)")));
}

void changeWifiConfirm(const String& ssid) {
  frame("Change WiFi?", "Current: " + ssid,
        "[#] Tap to confirm", "[ ] Do nothing to cancel");
}

void message(const String& title, const String& line1, const String& line2) {
  frame(title, line1, line2);
}

void renderView(uint8_t view, bool timeOK, bool online, bool stale) {
  if (view == VIEW_INFO) {
    frame("View: Info",
          timeOK ? (timekeeper::clockHM(settings::get().clock24h) + timekeeper::ampm(settings::get().clock24h) +
                    "  " + timekeeper::dateStr()) : String("Setting clock..."),
          String(MDNS_HOSTNAME) + ".local   IP " + WiFi.localIP().toString(),
          "WiFi MAC " + WiFi.macAddress() + "   Eth (not installed)",
          String("v") + FIRMWARE_VERSION + (online ? "   online" : "   offline"));
    return;
  }
  const char* vn = (view == VIEW_TIMELINE) ? "Timeline" : "Map";
  int h = seismic::headlineIndex();
  String head = (h < 0)
      ? String("Quiet - no events in range")
      : (String("M") + String(seismic::events()[h].mag, 1) + "  " + seismic::events()[h].place);
  String counts = String("24h:") + (timeOK ? String(seismic::count24h()) : String("-")) +
                  "  7d:" + seismic::count7d() + (seismic::count7dCapped() ? "+" : "");
  String foot = timeOK
      ? (timekeeper::clockHM(settings::get().clock24h) + timekeeper::ampm(settings::get().clock24h) +
         "  " + timekeeper::dateStr())
      : String("Setting clock...");
  String flags = String(online ? "" : "[reconnecting] ") + (stale ? "[STALE] " : "");
  frame(String("View: ") + vn + "   " + flags, head, counts,
        foot, "loc: " + settings::locLabel());
}

void tickClock(uint8_t view, bool timeOK) {
  if (timeOK)
    Serial.printf("[epd] %s tick: %s%s\n", view == VIEW_INFO ? "info" : "footer",
                  timekeeper::clockHM(settings::get().clock24h).c_str(),
                  timekeeper::ampm(settings::get().clock24h).c_str());
}
}  // namespace epd

#else
// ----------------------------------------------------------------------------
// Hardware: GxEPD2 full-buffer driver with the real Public Sans layouts.
// ----------------------------------------------------------------------------
#include <GxEPD2_BW.h>
#include <qrcode.h>
#include "fonts/PublicSans_Bold54px7b.h"
#include "fonts/PublicSans_Bold36px7b.h"
#include "fonts/PublicSans_Bold24px7b.h"
#include "fonts/PublicSans_Bold15px7b.h"
#include "fonts/PublicSans_Bold11px7b.h"
#include "fonts/PublicSans_Regular13px7b.h"
#include "fonts/PublicSans_Bold13px7b.h"
#include "fonts/PublicSans_Medium9px7b.h"
#include "fonts/PublicSans_SemiBold10px7b.h"

#define F_MAG   (&PublicSans_Bold54px7b)     // magnitude (hero)
#define F_STAT  (&PublicSans_Bold36px7b)     // stat numerals
#define F_TIME  (&PublicSans_Bold24px7b)     // footer clock / screen titles
#define F_PLACE (&PublicSans_Bold15px7b)     // place / section heads
#define F_BADGE (&PublicSans_Bold11px7b)     // badge / caps labels / sun-moon
#define F_BODY  (&PublicSans_Regular13px7b)  // detail / recency / date
#define F_VALUE (&PublicSans_Bold13px7b)     // Info-page key values (web / IP / MAC)
#define F_MICRO (&PublicSans_Medium9px7b)    // axis / ring / micro labels
#define F_LABEL (&PublicSans_SemiBold10px7b) // location name / "Largest"

namespace {
  GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
      GxEPD2_420_GDEY042T81(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

  // ---- text with a UTF-8 "·" (0xC2 0xB7) -> drawn mid-dot shim ----------------
  // Our bitmap fonts are single-byte ASCII; the only non-ASCII the module strings
  // carry is the mid-dot separator (dateStr(), composed stat lines). We render it
  // as a small filled circle so "Sat · Jun 13" reads as designed without bloating
  // every glyph table with Latin-1. measure()/emit() walk bytes identically so
  // alignment stays correct.
  const GFXfont* curFont = nullptr;
  const int DOTADV = 5;
  void setF(const GFXfont* f) { curFont = f; display.setFont(f); }
  int chAdv(uint8_t c) {
    if (!curFont) return 6;
    if (c < curFont->first || c > curFont->last) return curFont->glyph[0].xAdvance;
    return curFont->glyph[c - curFont->first].xAdvance;
  }
  int measure(const char* s) {
    int w = 0;
    while (*s) {
      uint8_t c = *s;
      if (c == 0xC2 && (uint8_t)s[1] == 0xB7) { w += DOTADV; s += 2; continue; }
      if (c < 0x80) { w += chAdv(c); s++; } else s++;
    }
    return w;
  }
  void emit(const char* s) {
    while (*s) {
      uint8_t c = *s;
      if (c == 0xC2 && (uint8_t)s[1] == 0xB7) {
        int16_t x = display.getCursorX(), y = display.getCursorY();
        display.fillCircle(x + 2, y - 3, 1, GxEPD_BLACK);
        display.setCursor(x + DOTADV, y); s += 2; continue;
      }
      if (c < 0x80) { display.write(c); s++; } else s++;
    }
  }
  // align: 0 left, 1 center, 2 right — (x,y) is the text baseline anchor.
  void txt(int x, int y, const String& s, const GFXfont* f, uint8_t align = 0) {
    setF(f); display.setTextColor(GxEPD_BLACK);
    int xx = x;
    if (align) { int w = measure(s.c_str()); xx = (align == 1) ? x - w / 2 : x - w; }
    display.setCursor(xx, y); emit(s.c_str());
  }
  int wOf(const String& s, const GFXfont* f) { setF(f); return measure(s.c_str()); }
  // centered text on a white knockout (legible over rings / lines).
  void txtKO(int cx, int y, const String& s, const GFXfont* f) {
    setF(f); int w = measure(s.c_str());
    display.fillRect(cx - w / 2 - 1, y - 8, w + 2, 11, GxEPD_WHITE);
    display.setCursor(cx - w / 2, y); display.setTextColor(GxEPD_BLACK); emit(s.c_str());
  }
  String ellipsize(String s, const GFXfont* f, int maxw) {
    setF(f);
    if (measure(s.c_str()) <= maxw) return s;
    while (s.length() && measure((s + "..").c_str()) > maxw) s.remove(s.length() - 1);
    s.trim(); return s + "..";
  }
  // greedy word-wrap into up to two lines (overflow collapses into line b, ellipsized).
  void wrap2(const String& s, const GFXfont* f, int maxw, String& a, String& b) {
    setF(f); a = ""; b = "";
    String word; int n = s.length();
    auto take = [&](const String& wd) {
      if (a == "") a = wd;
      else if (b == "" && measure((a + " " + wd).c_str()) <= maxw) a += " " + wd;
      else b = b.length() ? b + " " + wd : wd;
    };
    for (int i = 0; i < n; i++) {
      char ch = s[i];
      if (ch == ' ') { if (word.length()) { take(word); word = ""; } }
      else word += ch;
    }
    if (word.length()) take(word);
    a = ellipsize(a, f, maxw);
    if (b.length()) b = ellipsize(b, f, maxw);
  }

  // ---- primitive glyphs ------------------------------------------------------
  void dashedCircle(int cx, int cy, int r) {
    for (int a = 0; a < 360; a += 6) {
      float ra = a * (float)M_PI / 180.0f;
      display.drawPixel(cx + (int)lround(r * cosf(ra)), cy + (int)lround(r * sinf(ra)), GxEPD_BLACK);
    }
  }
  void dashedHLine(int x0, int y, int x1) {
    for (int x = x0; x <= x1; x += 4) display.drawPixel(x, y, GxEPD_BLACK);
  }
  void sunGlyph(int cx, int cy) {
    display.fillCircle(cx, cy, 3, GxEPD_BLACK);
    for (int a = 0; a < 360; a += 45) {
      float ra = a * (float)M_PI / 180.0f;
      display.drawLine(cx + (int)lround(5 * cosf(ra)), cy + (int)lround(5 * sinf(ra)),
                       cx + (int)lround(7 * cosf(ra)), cy + (int)lround(7 * sinf(ra)), GxEPD_BLACK);
    }
  }
  void arrow(int x, int y, bool up) {  // ~7px tall, baseline-ish at y
    display.drawLine(x, y - 6, x, y, GxEPD_BLACK);
    if (up) { display.drawLine(x, y - 6, x - 3, y - 3, GxEPD_BLACK);
              display.drawLine(x, y - 6, x + 3, y - 3, GxEPD_BLACK); }
    else    { display.drawLine(x, y, x - 3, y - 3, GxEPD_BLACK);
              display.drawLine(x, y, x + 3, y - 3, GxEPD_BLACK); }
  }
  void homePin(int x, int yBase) {  // tiny house, sits on the text row at yBase
    int by = yBase - 1;
    display.drawLine(x, by - 3, x + 4, by - 7, GxEPD_BLACK);
    display.drawLine(x + 4, by - 7, x + 8, by - 3, GxEPD_BLACK);
    display.drawRect(x + 1, by - 3, 7, 4, GxEPD_BLACK);
  }
  void offlineGlyph(int x, int y) {  // slashed-WiFi
    display.fillCircle(x, y + 2, 1, GxEPD_BLACK);
    for (int a = 200; a <= 340; a += 14) { float ra = a * (float)M_PI / 180.0f;
      display.drawPixel(x + (int)lround(5 * cosf(ra)), y + 2 + (int)lround(5 * sinf(ra)), GxEPD_BLACK); }
    for (int a = 210; a <= 330; a += 12) { float ra = a * (float)M_PI / 180.0f;
      display.drawPixel(x + (int)lround(8 * cosf(ra)), y + 2 + (int)lround(8 * sinf(ra)), GxEPD_BLACK); }
    display.drawLine(x - 8, y - 6, x + 8, y + 6, GxEPD_BLACK);
  }
  // moon: outline + scanline-filled SHADOW (DISPLAY-SPEC §8). Almanac convention — the
  // lit limb stays paper-white and the unlit part is inked, so the glyph reads like the
  // real sky: full moon = open disc, new moon = solid black, crescent = bright sliver.
  void moonGlyph(int cx, int cy, int R, float k, bool waxing) {
    display.drawCircle(cx, cy, R, GxEPD_BLACK);
    for (int dy = -R; dy <= R; dy++) {
      int xspan = (int)lround(sqrtf((float)(R * R - dy * dy)));
      float xt = xspan * (1.0f - 2.0f * k);          // terminator x at this row
      int x0, x1;
      if (waxing) { x0 = -xspan; x1 = (int)lround(xt); }    // lit limb on the right -> ink the left/shadow
      else        { x0 = (int)lround(-xt); x1 = xspan; }    // lit limb on the left  -> ink the right/shadow
      if (x1 >= x0) display.drawFastHLine(cx + x0, cy + dy, x1 - x0 + 1, GxEPD_BLACK);
    }
  }
  void qr(int x, int y, const String& text) {
    QRCode q; uint8_t buf[qrcode_getBufferSize(3)];
    if (qrcode_initText(&q, buf, 3, ECC_LOW, text.c_str()) != 0) return;
    const int s = 3;
    for (uint8_t j = 0; j < q.size; j++)
      for (uint8_t i = 0; i < q.size; i++)
        if (qrcode_getModule(&q, i, j)) display.fillRect(x + i * s, y + j * s, s, s, GxEPD_BLACK);
  }

  // ---- band: page indicator + offline glyph (hero top-right) -----------------
  void pageDots(int activeView) {
    for (int i = 0; i < VIEW_COUNT; i++) {
      int cx = 368 + i * 12;
      if (i == activeView) display.fillCircle(cx, 13, 3, GxEPD_BLACK);
      else display.drawCircle(cx, 13, 3, GxEPD_BLACK);
    }
  }

  // ---- shared hero (y 0–90) --------------------------------------------------
  void drawHero(bool timeOK, bool online, bool stale) {
    pageDots(viewstate::current());
    if (!online) offlineGlyph(346, 13);

    int h = seismic::headlineIndex();
    if (h < 0) {                                   // Quiet
      txt(9, 64, "Quiet", F_MAG);
      int qx = 9 + wOf("Quiet", F_MAG) + 22;       // anchor the caption past "Quiet" + a gap
      txt(qx, 44, "No events in range", F_PLACE);
      if (timeOK) txt(qx, 66, timekeeper::dateStr(), F_BODY);
      display.drawLine(0, 90, 400, 90, GxEPD_BLACK);
      return;
    }
    const auto& q = seismic::events()[h];

    // STALE keeps the top-left slot. (The significance badge was removed — a far-away
    // "felt" flag isn't meaningful and read as orphaned; the felt count still shows in
    // the stat column.)
    if (stale) { display.fillRect(11, 7, 8, 8, GxEPD_BLACK); txt(24, 16, "STALE DATA", F_BADGE); }

    txt(9, 66, String("M") + String(q.mag, 1), F_MAG);

    // place = the quake's own USGS location; tighten "N km" -> "Nkm" (global rule).
    String place = q.place; place.replace(" km", "km");
    String a, b; wrap2(place, F_PLACE, 392 - 150, a, b);
    int y2, y3;                                          // baselines for the two detail lines
    if (b.length()) { txt(150, 28, a, F_PLACE); txt(150, 44, b, F_PLACE); y2 = 62; y3 = 80; }
    else            { txt(150, 34, a, F_PLACE);                          y2 = 52; y3 = 70; }

    // line: depth + recency ("X ago" dropped in the Acquiring-time state)
    char det[40];
    if (timeOK) snprintf(det, sizeof(det), "depth %dkm \xC2\xB7 %s",
                         (int)lround(q.depthKm), timekeeper::relative(q.t).c_str());
    else        snprintf(det, sizeof(det), "depth %dkm", (int)lround(q.depthKm));
    txt(150, y2, det, F_BODY);

    // line: distance + bearing FROM the monitoring location, named so it's unambiguous.
    String home = settings::get().name; int cma = home.indexOf(',');
    if (cma > 0) home = home.substring(0, cma);          // city part of "Davis, CA"
    home.trim(); if (!home.length()) home = "home";      // hand-entered coords
    char dist[48];
    snprintf(dist, sizeof(dist), "%dkm %s of %s",
             (int)lround(q.distKm), seismic::bearingName(q.bearingDeg), home.c_str());
    txt(150, y3, dist, F_BODY);
    display.drawLine(0, 90, 400, 90, GxEPD_BLACK);
  }

  // ---- Page 1 content: radial map (left) ------------------------------------
  void drawMapPanel() {
    const auto& cfg = settings::get();
    const int cx = 100, cy = 170, Ro = 58;
    const int rr[3] = {34, 47, 58};                 // √-scale: R/3, 2R/3, R
    dashedCircle(cx, cy, rr[0]); dashedCircle(cx, cy, rr[1]);
    display.drawCircle(cx, cy, rr[2], GxEPD_BLACK);
    display.drawLine(cx - 6, cy, cx + 6, cy, GxEPD_BLACK);
    display.drawLine(cx, cy - 6, cx, cy + 6, GxEPD_BLACK);
    txt(cx, 105, "N", F_MICRO, 1);

    int R = cfg.radiusKm;                           // radius-driven ring labels
    int lab[3] = {R / 3, (2 * R) / 3, R};
    // Fan the three labels across the lower arc (SW / S / SE) so they sit on their own
    // ring and don't stack/collide — the old single-diagonal placement ran them together.
    const float labBearing[3] = {230.0f, 180.0f, 130.0f};
    for (int i = 0; i < 3; i++) {
      float a = labBearing[i] * (float)M_PI / 180.0f;
      int lx = cx + (int)lround(rr[i] * sinf(a));
      int ly = cy - (int)lround(rr[i] * cosf(a)) + 3;   // just below the ring point
      txtKO(lx, ly, String(lab[i]) + "km", F_MICRO);    // no space — tighter at 9px
    }

    auto plot = [&](float dk, float bd, int& px, int& py) {
      float r = Ro * sqrtf(fminf(dk, (float)R) / (float)R);
      float a = bd * (float)M_PI / 180.0f;
      px = cx + (int)lround(r * sinf(a));
      py = cy - (int)lround(r * cosf(a));
    };

    const auto& evs = seismic::events();
    int h = seismic::headlineIndex();
    for (size_t i = 0; i < evs.size(); i++) {
      const auto& q = evs[i];
      if (q.clustered || (int)i == h) continue;
      int px, py; plot(q.distKm, q.bearingDeg, px, py);
      int rad = q.mag >= 4 ? 5 : q.mag >= 3 ? 3 : 2;
      if (q.depthKm >= 8) display.fillCircle(px, py, rad, GxEPD_BLACK);   // deep = filled
      else                display.drawCircle(px, py, rad, GxEPD_BLACK);   // shallow = hollow
    }
    for (const auto& cl : seismic::clusters()) {                          // swarm ×n collapse
      int px, py; plot(cl.distKm, cl.bearingDeg, px, py);
      dashedCircle(px, py, 10);
      display.fillCircle(px, py, 4, GxEPD_BLACK);
      txt(px + 11, py - 5, String("x") + cl.count, F_BADGE);
    }
    if (h >= 0) {                                                         // headline = double ring
      const auto& q = evs[h]; int px, py; plot(q.distKm, q.bearingDeg, px, py);
      display.drawCircle(px, py, 8, GxEPD_BLACK);
      display.drawCircle(px, py, 6, GxEPD_BLACK);
      display.fillCircle(px, py, 3, GxEPD_BLACK);
    }
  }

  // Big stat numeral, left-aligned at x212. Auto-shrinks so a wide value (a 2-digit
  // count, or the capped "100+") never reaches the label column at STAT_LABEL_X —
  // that keeps "IN 24 H" / "IN 7 DAYS" aligned regardless of the count's width.
  static const int STAT_LABEL_X = 264;
  void bigStat(int y, const String& s) {
    const int budget = STAT_LABEL_X - 212 - 4;          // px available before the label
    const GFXfont* nf = (wOf(s, F_STAT) <= budget) ? F_STAT
                      : (wOf(s, F_TIME) <= budget) ? F_TIME : F_PLACE;
    txt(212, y, s, nf);
  }

  // ---- Page 1 content: stat column (right) ----------------------------------
  void drawStatColumn(bool timeOK) {
    display.drawLine(204, 92, 204, 241, GxEPD_BLACK);

    bigStat(130, timeOK ? String(seismic::count24h()) : String("-"));
    txt(STAT_LABEL_X, 116, "IN 24 H", F_MICRO);
    if (timeOK) { int f = seismic::feltCount24h();
      txt(STAT_LABEL_X, 130, f > 0 ? (String(f) + " felt nearby") : String("none felt"), F_BADGE); }

    bigStat(180, String(seismic::count7d()) + (seismic::count7dCapped() ? "+" : ""));
    txt(STAT_LABEL_X, 166, "IN 7 DAYS", F_MICRO);
    float lo, hi; seismic::magRange(lo, hi);
    String mr;
    if (seismic::count7d() <= 0) mr = "--";
    else {
      char a[8], b[8]; snprintf(a, 8, "%.1f", lo); snprintf(b, 8, "%.1f", hi);
      mr = (strcmp(a, b) == 0) ? (String("M") + a) : (String("M") + a + " - M" + b);
    }
    txt(STAT_LABEL_X, 180, mr, F_BADGE);

    display.drawLine(212, 192, 388, 192, GxEPD_BLACK);
    display.drawCircle(219, 204, 3, GxEPD_BLACK); txt(230, 208, "shallow \xC2\xB7 <8km", F_MICRO);
    display.fillCircle(219, 220, 3, GxEPD_BLACK); txt(230, 224, "deep \xC2\xB7 >=8km", F_MICRO);
    if (seismic::recordMag() > 0) {
      char r[44]; snprintf(r, sizeof(r), "Largest: M%.1f \xC2\xB7 %s",
                           seismic::recordMag(), seismic::recordDate().c_str());
      txt(212, 238, r, F_LABEL);
    }
  }

  // ---- Page 2 content: 7-day lollipop strip chart ---------------------------
  void drawTimelinePanel(bool timeOK) {
    const int x0 = 22, x1 = 388, baseY = 224, topY = 124;
    const int gy[3] = {205, 167, 129}; const char* gl[3] = {"M2", "M3", "M4"};
    for (int i = 0; i < 3; i++) { dashedHLine(x0, gy[i], x1); txt(x0 - 4, gy[i] + 3, gl[i], F_MICRO, 2); }
    display.drawLine(x0, baseY, x1, baseY, GxEPD_BLACK);

    float lo, hi; seismic::magRange(lo, hi);
    char folded[64];
    snprintf(folded, sizeof(folded), "TODAY %s   7-DAY MAX %s   LARGEST %s",
             timeOK ? String(seismic::count24h()).c_str() : "-",
             seismic::count7d() > 0 ? (String("M") + String(hi, 1)).c_str() : "-",
             seismic::recordMag() > 0 ? (String("M") + String(seismic::recordMag(), 1)).c_str() : "-");
    txt(x0, 112, folded, F_MICRO);

    if (!timeOK) { txt(204, 178, "Setting clock...", F_BODY, 1); return; }

    time_t now = timekeeper::now(), start = now - 7 * 86400;
    for (int d = 0; d < 7; d++) {                    // weekday letters along the baseline
      time_t dt = start + (time_t)d * 86400; struct tm lt; localtime_r(&dt, &lt);
      int wx = x0 + (int)lround((d + 0.5) / 7.0 * (x1 - x0));
      char s[2] = { "SMTWTFS"[lt.tm_wday], 0 };
      txt(wx, baseY + 14, s, F_MICRO, 1);
    }
    auto magY = [&](float m) {
      int y = 205 - (int)lround(38.0f * (m - 2.0f));
      if (y > baseY - 2) y = baseY - 2; if (y < topY) y = topY; return y;
    };
    const auto& evs = seismic::events(); int h = seismic::headlineIndex();
    for (size_t i = 0; i < evs.size(); i++) {
      const auto& q = evs[i];
      if (q.t < start) continue;
      int ex = x0 + (int)lround((double)(q.t - start) / (7.0 * 86400) * (x1 - x0));
      int ey = magY(q.mag); int rad = q.mag >= 4 ? 4 : q.mag >= 3 ? 3 : 2;
      display.drawLine(ex, baseY, ex, ey, GxEPD_BLACK);
      display.fillCircle(ex, ey, rad, GxEPD_BLACK);
      if ((int)i == h) { display.drawCircle(ex, ey, rad + 2, GxEPD_BLACK);
                         txt(ex + 6, ey + 3, String("M") + String(q.mag, 1), F_MICRO); }
    }
  }

  // ---- persistent Sky Footer (y 242–300) ------------------------------------
  void drawFooter(bool timeOK) {
    const auto& cfg = settings::get();
    display.drawLine(0, 242, 400, 242, GxEPD_BLACK);
    if (!timeOK) { txt(200, 276, "Setting clock...", F_BODY, 1); return; }
    display.drawLine(138, 242, 138, 300, GxEPD_BLACK);
    display.drawLine(268, 242, 268, 300, GxEPD_BLACK);

    // cell 1 — time / date / monitoring location
    String hm = timekeeper::clockHM(cfg.clock24h);
    txt(12, 265, hm, F_TIME);
    String ap = timekeeper::ampm(cfg.clock24h);
    if (ap.length()) txt(12 + wOf(hm, F_TIME) + 3, 265, ap, F_BADGE);
    txt(12, 280, timekeeper::dateStr(), F_MICRO);
    homePin(14, 295);
    txt(24, 295, ellipsize(settings::locLabel(), F_MICRO, 108), F_MICRO);

    // cell 2 — sun
    sunGlyph(150, 261);
    astro::Sun s = astro::sun(cfg.lat, cfg.lon, timekeeper::now());
    if (s.valid) {
      arrow(166, 258, true);  txt(174, 261, astro::hm12(s.riseMin, cfg.clock24h), F_BADGE);
      arrow(166, 277, false); txt(174, 277, astro::hm12(s.setMin, cfg.clock24h), F_BADGE);
      int dl = s.setMin - s.riseMin; if (dl < 0) dl += 1440;
      char b[20]; snprintf(b, sizeof(b), "Daylight: %dh %02dm", dl / 60, dl % 60);
      txt(146, 294, b, F_MICRO);
    } else txt(150, 272, "sun --", F_BADGE);

    // cell 3 — moon. Names run long ("Waxing crescent" = 15 ch); at 9px Medium the
    // longest measures ~78 px and the cell allows 88 (x304..392), so all eight phase
    // names fit. ellipsize() is a hard guard so nothing can ever overrun the edge.
    astro::Moon m = astro::moon(timekeeper::now());
    moonGlyph(286, 270, 12, m.illum, m.waxing);            // disc nudged left
    txt(308, 266, ellipsize(m.name, F_MICRO, 84), F_MICRO); // text nudged right -> ~10px gap off the disc
    char mb[20]; snprintf(mb, sizeof(mb), "%d%% \xC2\xB7 day %d", (int)lround(m.illum * 100), m.ageDays);
    txt(308, 281, mb, F_MICRO);
  }

  // ---- Page 3 content: Info — "B / balanced" (clock header band, y 36–116) --
  // The clock is a left-aligned header (echoing the hero magnitude); date + monitoring
  // location sit to its right. This band is what tickClock() partial-refreshes each minute.
  void drawInfoClock(bool timeOK) {
    if (!timeOK) { txt(14, 92, "Setting clock...", F_PLACE); return; }
    const auto& cfg = settings::get();
    String hm = timekeeper::clockHM(cfg.clock24h);
    txt(14, 96, hm, F_MAG);
    String ap = timekeeper::ampm(cfg.clock24h);
    if (ap.length()) txt(14 + wOf(hm, F_MAG) + 6, 96, ap, F_PLACE);
    txt(196, 70, timekeeper::dateStr(), F_BODY);
    homePin(196, 92);
    txt(208, 92, ellipsize(settings::locLabel(), F_BADGE, 180), F_BADGE);
  }

  // Full Info page: masthead + clock header + a two-column DEVICE table so the MAC / IP /
  // version are reachable on-glass (e.g. to register on a managed network) with no web.
  void drawInfoPanel(bool timeOK, bool online) {
    txt(16, 22, "GROUND TRUTH", F_LABEL);                 // masthead
    pageDots(viewstate::current());                       // ● ○ ○
    display.drawLine(16, 34, 384, 34, GxEPD_BLACK);
    drawInfoClock(timeOK);
    display.drawLine(16, 116, 384, 116, GxEPD_BLACK);

    unsigned long up = millis() / 1000;
    char ub[28]; snprintf(ub, sizeof(ub), "v" FIRMWARE_VERSION "  \xC2\xB7  up %luh %lum",
                          up / 3600, (up % 3600) / 60);
    String st = online ? String("online") : String("offline - reconnecting");
    if (timeOK && seismic::hasData()) st += "  \xC2\xB7  data " + timekeeper::relative(seismic::lastFetch());

    // Ethernet MAC becomes a key (bold) value like WiFi MAC once the W5500 lands (Gate 1b);
    // until then the row reads "not installed" in the regular weight.
    bool ethUp = false;                                   // Gate 1b: ETH link + real MAC
    String ethVal = ethUp ? String("--") : String("not installed");

    int y = 152;                                          // table centered under the clock divider
    auto row = [&](const char* label, const String& val, bool key) {
      txt(16, y, label, F_MICRO);
      txt(118, y, val, key ? F_VALUE : F_BODY);           // key values (web/IP/MAC[/eth]) bold
      y += 22;
    };
    row("WEB",      String(MDNS_HOSTNAME) + ".local",            true);
    row("IP",       online ? WiFi.localIP().toString() : String("--"), true);
    row("WIFI MAC", WiFi.macAddress(),                           true);
    row("ETHERNET", ethVal,                                      ethUp);
    row("FIRMWARE", String(ub),                                  false);
    row("STATUS",   st,                                          false);
  }

  void beginFull() { display.setRotation(0); display.setFullWindow(); }
}  // namespace

namespace epd {
void begin() {
  pinMode(SRAM_CS, OUTPUT); digitalWrite(SRAM_CS, HIGH);   // park unused Friend CS lines
  pinMode(SD_CS,   OUTPUT); digitalWrite(SD_CS,   HIGH);
  display.init(115200);
  display.setTextWrap(false);   // fixed-layout panel: clip any overrun, never wrap to the far edge
  Serial.println(F("[epd] GxEPD2 GDEY042T81 ready (Gate 4 renderer)"));
}

void renderView(uint8_t view, bool timeOK, bool online, bool stale) {
  beginFull(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    if (view == VIEW_INFO) { drawInfoPanel(timeOK, online); }
    else {
      drawHero(timeOK, online, stale);
      if (view == VIEW_TIMELINE) drawTimelinePanel(timeOK);
      else { drawMapPanel(); drawStatColumn(timeOK); }
      drawFooter(timeOK);
    }
  } while (display.nextPage());
}

void splash() {
  beginFull(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    txt(200, 124, PROJECT_NAME, F_TIME, 1);
    txt(200, 156, RECIPIENT_SPLASH, F_BODY, 1);
    txt(200, 184, "v" FIRMWARE_VERSION, F_MICRO, 1);
  } while (display.nextPage());
}

void connectScreen(const String& apName, const String& apPass,
                   const String& wifiMac, const String& ethMac, bool error) {
  beginFull(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    txt(200, 26, error ? "Couldn't join WiFi" : "Connect Ground Truth", F_TIME, 1);
    display.drawLine(8, 38, 392, 38, GxEPD_BLACK);

    // left — WiFi path + join QR
    txt(12, 60, "1. Join this WiFi", F_PLACE);
    txt(12, 82, apName, F_BODY);
    txt(12, 100, String("pass:  ") + apPass, F_BODY);
    txt(12, 118, "then open 192.168.4.1", F_MICRO);
    qr(12, 128, String("WIFI:T:WPA;S:") + apName + ";P:" + apPass + ";;");

    // right — Ethernet path + both MACs for managed-network registration
    txt(212, 60, "2. Or plug in", F_PLACE);
    txt(212, 78, "Ethernet (no setup)", F_BODY);
    display.drawRect(208, 92, 184, 116, GxEPD_BLACK);
    txt(216, 110, "Register on a managed", F_MICRO);
    txt(216, 122, "network with these:", F_MICRO);
    txt(216, 144, "WiFi MAC", F_MICRO);
    txt(216, 160, wifiMac, F_BODY);
    txt(216, 182, "Ethernet MAC", F_MICRO);
    txt(216, 198, ethMac.length() ? ethMac : String("(board not installed)"), F_BODY);

    txt(200, 232, "Connects on whichever link comes up first", F_MICRO, 1);
  } while (display.nextPage());
}

void changeWifiConfirm(const String& ssid) {
  beginFull(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    txt(200, 90, "Change WiFi?", F_TIME, 1);
    txt(200, 128, String("Current network: ") + ssid, F_BODY, 1);
    display.fillRect(96, 156, 11, 11, GxEPD_BLACK);  txt(116, 166, "Tap to confirm", F_BODY);
    display.drawRect(96, 182, 11, 11, GxEPD_BLACK);  txt(116, 192, "Do nothing to cancel", F_BODY);
  } while (display.nextPage());
}

void message(const String& title, const String& line1, const String& line2) {
  beginFull(); display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    txt(200, 120, title, F_TIME, 1);
    if (line1.length()) txt(200, 152, line1, F_BODY, 1);
    if (line2.length()) txt(200, 174, line2, F_BODY, 1);
  } while (display.nextPage());
}

void tickClock(uint8_t view, bool timeOK) {
  if (view == VIEW_INFO) {
    display.setPartialWindow(0, 36, 400, 80);     // the clock-header band (between dividers)
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); drawInfoClock(timeOK); } while (display.nextPage());
  } else {
    display.setPartialWindow(0, 242, 400, 58);    // the Sky Footer band
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); drawFooter(timeOK); } while (display.nextPage());
  }
}
}  // namespace epd

#endif  // HEADLESS_DISPLAY
