#include "web.h"
#include "config.h"
#include "settings.h"
#include "timekeeper.h"
#include "seismic.h"
#include "astro.h"
#include "viewstate.h"
#include "provisioning.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>

namespace {
  AsyncWebServer server(WEB_PORT);
  bool g_started = false;
  volatile bool g_reboot = false, g_wifiReset = false;

  // ---- main page: the 1-bit display mirror + recent events (the "data") ----
  const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Truth</title><style>
:root{font-family:'Helvetica Neue',Arial,sans-serif;color:#1a1a1a}
body{margin:0;background:#d7d7d5;padding:24px}.wrap{max-width:680px;margin:0 auto}
.top{display:flex;justify-content:space-between;align-items:baseline}
h1{font-size:20px;margin:0}a{color:#1a5fb4;text-decoration:none;font-size:13px}
.sub{color:#666;font-size:13px;margin:2px 0 16px}
.frame{height:478px}
.panel{width:400px;height:300px;background:#fff;box-shadow:0 0 0 1px #b6b6b0,0 8px 22px rgba(0,0,0,.16);transform:scale(1.55);transform-origin:top left}
table{width:100%;border-collapse:collapse;font-size:12.5px;margin-top:6px}
th,td{text-align:left;padding:5px 8px;border-bottom:1px solid #cfcfca}
th{color:#9a9a93;font-weight:600;font-size:11px;text-transform:uppercase}
td.r,th.r{text-align:right}.muted{color:#9a9a93}
</style></head><body><div class="wrap">
<div class="top"><h1>Ground Truth</h1><a href="/settings">⚙ Settings</a></div>
<div class="sub" id="sub">loading…</div>
<div class="frame"><svg id="panel" class="panel" viewBox="0 0 400 300"></svg></div>
<h1 style="font-size:15px;margin-top:20px">Recent events</h1>
<table><thead><tr><th>Mag</th><th>Place</th><th class="r">Dist</th><th class="r">Depth</th><th class="r">When</th></tr></thead><tbody id="rows"></tbody></table>
<p class="muted" style="font-size:11px;margin-top:16px">Live mirror · refreshes every 20 s</p>
</div>
<script>
const NS="http://www.w3.org/2000/svg";
function el(t,a){const e=document.createElementNS(NS,t);for(const k in a)e.setAttribute(k,a[k]);return e;}
function tx(x,y,s,o){const e=el("text",Object.assign({x,y,fill:"#000","font-family":"Helvetica Neue,Arial"},o||{}));e.textContent=s;return e;}
function esc(s){return (s||"").replace(/[&<>]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;"}[c]));}
function moon(cx,cy,R,k,w){const rx=R*Math.abs(1-2*k),t=`${cx} ${cy-R}`,b=`${cx} ${cy+R}`,l=w?1:0;let m;if(w)m=k<.5?0:1;else m=k<.5?1:0;return `M ${t} A ${R} ${R} 0 0 ${l} ${b} A ${rx} ${R} 0 0 ${m} ${t} Z`;}
function render(d){
 const s=document.getElementById("panel");s.innerHTML="";const add=e=>s.appendChild(e);const h=d.headline;
 if(h&&h.badge){add(el("circle",{cx:16,cy:13,r:4,fill:"#000"}));add(tx(25,17,h.badge.toUpperCase(),{"font-size":9,"font-weight":700,"letter-spacing":.6}));}
 for(let i=0;i<2;i++)add(el("circle",{cx:368+i*12,cy:13,r:3.5,fill:i===0?"#000":"#fff",stroke:"#000"}));
 if(h){add(tx(9,66,h.mag,{"font-size":52,"font-weight":700,"letter-spacing":-2.5}));
  add(tx(150,32,esc(h.place),{"font-size":15,"font-weight":700}));
  add(tx(150,64,`depth ${h.depthKm} km · ${h.distDisp} ${h.unit} ${h.bearing}`,{"font-size":13}));
  add(tx(150,82,h.rel,{"font-size":13}));}
 else add(tx(9,60,"Quiet",{"font-size":42,"font-weight":700}));
 add(el("line",{x1:0,y1:90,x2:400,y2:90,stroke:"#000"}));add(el("line",{x1:204,y1:92,x2:204,y2:241,stroke:"#000"}));
 const cx=100,cy=170,Ro=58;[58,47,34].forEach((r,i)=>add(el("circle",{cx,cy,r,fill:"none",stroke:"#000","stroke-dasharray":i?"1.5 3":"0"})));
 add(el("line",{x1:cx-6,y1:cy,x2:cx+6,y2:cy,stroke:"#000"}));add(el("line",{x1:cx,y1:cy-6,x2:cx,y2:cy+6,stroke:"#000"}));
 add(tx(cx,104,"N",{"font-size":9,"font-weight":700,"text-anchor":"middle"}));add(tx(133,221,d.loc.radiusKm+" km",{"font-size":8.5}));
 const plot=(dk,bd)=>{const r=Ro*Math.sqrt(Math.min(dk,d.loc.radiusKm)/d.loc.radiusKm),a=bd*Math.PI/180;return [(cx+r*Math.sin(a)).toFixed(1),(cy-r*Math.cos(a)).toFixed(1)];};
 (d.events||[]).forEach(q=>{if(q.head||q.cl)return;const [x,y]=plot(q.dk,q.bd),rad=q.mag>=4?5:q.mag>=3?3.4:2.3;add(el("circle",{cx:x,cy:y,r:rad,fill:q.dep>=8?"#000":"none",stroke:"#000"}));});
 (d.clusters||[]).forEach(c=>{const [x,y]=plot(c.dk,c.bd);add(el("circle",{cx:x,cy:y,r:10,fill:"none",stroke:"#000","stroke-dasharray":"2 2.5"}));add(el("circle",{cx:x,cy:y,r:5,fill:"#000"}));add(tx(+x+10,+y-6,"×"+c.n,{"font-size":11,"font-weight":700}));});
 {const hq=(d.events||[]).find(q=>q.head);if(hq){const [x,y]=plot(hq.dk,hq.bd);add(el("circle",{cx:x,cy:y,r:8.5,fill:"none",stroke:"#000"}));add(el("circle",{cx:x,cy:y,r:6,fill:"none",stroke:"#000"}));add(el("circle",{cx:x,cy:y,r:4,fill:"#000"}));}}
 const t=d.stats;
 add(tx(212,130,t.c24,{"font-size":38,"font-weight":700}));add(tx(250,116,"IN 24 H",{"font-size":9,"font-weight":700}));add(tx(250,130,t.felt24>0?`${t.felt24} felt nearby`:"none felt",{"font-size":11}));
 add(tx(212,180,t.c7+(t.capped?"+":""),{"font-size":38,"font-weight":700}));add(tx(262,166,"IN 7 DAYS",{"font-size":9,"font-weight":700}));
 const mr=t.magLo.toFixed(1)===t.magHi.toFixed(1)?`M${t.magLo.toFixed(1)}`:`M${t.magLo.toFixed(1)} – M${t.magHi.toFixed(1)}`;add(tx(262,180,mr,{"font-size":11}));
 add(el("line",{x1:212,y1:192,x2:388,y2:192,stroke:"#000","stroke-width":.8}));
 add(el("circle",{cx:219,cy:204,r:3.5,fill:"none",stroke:"#000"}));add(tx(230,208,"shallow · <8 km",{"font-size":10}));
 add(el("circle",{cx:219,cy:220,r:3.5,fill:"#000"}));add(tx(230,224,"deep · ≥8 km",{"font-size":10}));
 if(t.recMag>0)add(tx(212,238,`Record  M${t.recMag.toFixed(1)} · ${t.recDate}`,{"font-size":9.5,"font-weight":600}));
 add(el("line",{x1:0,y1:242,x2:400,y2:242,stroke:"#000"}));add(el("line",{x1:138,y1:242,x2:138,y2:300,stroke:"#000"}));add(el("line",{x1:268,y1:242,x2:268,y2:300,stroke:"#000"}));
 add(tx(16,271,d.time.hm,{"font-size":24,"font-weight":700,"letter-spacing":-1}));if(d.time.ampm)add(tx(16+d.time.hm.length*14,271,d.time.ampm,{"font-size":12,"font-weight":600}));
 add(tx(16,291,d.time.date,{"font-size":12}));
 add(el("circle",{cx:159,cy:260,r:3.5,fill:"#000"}));
 [[159,250,159,253],[159,267,159,270],[149,260,152,260],[166,260,169,260],[152,253,154,255],[164,265,166,267],[166,253,164,255],[154,265,152,267]].forEach(L=>add(el("line",{x1:L[0],y1:L[1],x2:L[2],y2:L[3],stroke:"#000","stroke-width":1.2})));
 if(d.sun){add(tx(176,260,"↑ "+d.sun.rise,{"font-size":12,"font-weight":600}));add(tx(176,276,"↓ "+d.sun.set,{"font-size":12,"font-weight":600}));add(tx(148,291,d.sun.day,{"font-size":11}));}
 else{add(tx(176,266,"sun —",{"font-size":12,fill:"#888"}));}
 if(d.moon){add(el("circle",{cx:291,cy:271,r:12,fill:"none",stroke:"#000"}));add(el("path",{d:moon(291,271,12,d.moon.illum,d.moon.waxing),fill:"#000"}));add(tx(308,266,d.moon.name,{"font-size":11.5,"font-weight":600}));add(tx(308,281,`${Math.round(d.moon.illum*100)}% · day ${d.moon.age}`,{"font-size":11}));}
}
function tick(){fetch("/api/state").then(r=>r.json()).then(d=>{
 document.getElementById("sub").textContent=(d.online?"online":"offline")+" · "+d.host+".local · updated "+(d.fetch?d.fetch.rel:"never");
 render(d);const rows=document.getElementById("rows");rows.innerHTML="";
 (d.events||[]).slice(0,12).forEach(q=>{rows.innerHTML+=`<tr><td>M${q.mag.toFixed(1)}</td><td>${esc(q.pl)}</td><td class="r">${q.dd} ${d.loc.units} ${q.bn}</td><td class="r">${q.dep} km</td><td class="r muted">${q.rel}</td></tr>`;});
}).catch(e=>{document.getElementById("sub").textContent="fetch error: "+e;});}
tick();setInterval(tick,20000);
</script></body></html>)HTML";

  // ---- settings page: diagnostics, config editor, actions, OTA ----
  const char SETTINGS_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Truth — settings</title><style>
:root{font-family:'Helvetica Neue',Arial,sans-serif;color:#1a1a1a}
body{margin:0;background:#d7d7d5;padding:24px}.wrap{max-width:560px;margin:0 auto}
.top{display:flex;justify-content:space-between;align-items:baseline}
h1{font-size:20px;margin:0}h2{font-size:13px;text-transform:uppercase;letter-spacing:.5px;color:#9a9a93;margin:24px 0 8px}
a{color:#1a5fb4;text-decoration:none;font-size:13px}
.card{background:#fff;border-radius:10px;padding:18px 20px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;font-size:13.5px}
.kv b{color:#9a9a93;font-weight:600}
label{display:block;font-size:12px;color:#666;margin:10px 0 3px}
input,select{width:100%;box-sizing:border-box;font:inherit;padding:.5rem;border:1px solid #bbb;border-radius:7px}
.row{display:flex;gap:12px}.row>div{flex:1}
button{margin-top:14px;font:inherit;padding:.55rem 1rem;border:0;border-radius:7px;background:#333;color:#fff;cursor:pointer}
button.warn{background:#fff;color:#a3301f;border:1px solid #d8a99f}
.msg{font-size:13px;color:#1a7f37;margin-top:10px}
</style></head><body><div class="wrap">
<div class="top"><h1>Settings</h1><a href="/">‹ Display</a></div>

<h2>Diagnostics</h2>
<div class="card"><div class="kv" id="diag">loading…</div></div>

<h2>Location &amp; behavior</h2>
<div class="card"><form id="cfg">
  <label><input type="checkbox" id="manual"> Manual location (uncheck for IP-geo)</label>
  <div class="row"><div><label>Latitude</label><input id="lat" type="number" step="0.0001"></div>
  <div><label>Longitude</label><input id="lon" type="number" step="0.0001"></div></div>
  <div class="row"><div><label>Radius (km)</label><input id="radius" type="number"></div>
  <div><label>Min magnitude</label><input id="minmag" type="number" step="0.1"></div></div>
  <div class="row"><div><label>Distance units</label><select id="units"><option value="km">km</option><option value="mi">mi</option></select></div>
  <div><label>Poll interval (min)</label><input id="poll" type="number"></div></div>
  <div class="row"><div><label>Clock</label><select id="clock"><option value="12">12-hour</option><option value="24">24-hour</option></select></div>
  <div><label>POSIX timezone</label><input id="tz" type="text"></div></div>
  <button type="submit">Save &amp; reboot</button><span class="msg" id="msg"></span>
</form></div>

<h2>Firmware</h2>
<div class="card"><a href="/update">Open firmware update (OTA) ›</a>
<p style="font-size:12px;color:#666;margin:8px 0 0">Asks for a username/password — that's the device's update credential.</p></div>

<h2>Actions</h2>
<div class="card">
  <button onclick="act('/api/reboot','Reboot the device?')">Reboot</button>
  <button class="warn" onclick="act('/api/wifi/reset','Forget WiFi and reopen the setup portal?')">Change WiFi</button>
</div>
<div style="height:40px"></div>
</div>
<script>
function f(id){return document.getElementById(id);}
fetch("/api/state").then(r=>r.json()).then(d=>{
 const up=Math.floor(d.uptime/3600)+"h "+Math.floor(d.uptime%3600/60)+"m";
 f("diag").innerHTML=`<b>Firmware</b><span>v${d.fw}</span><b>Status</b><span>${d.online?"online":"offline"} · synced ${d.synced}</span>`+
  `<b>Signal</b><span>${d.rssi} dBm</span><b>IP</b><span>${d.ip}</span><b>MAC</b><span>${d.mac}</span>`+
  `<b>Host</b><span>${d.host}.local</span><b>Uptime</b><span>${up}</span>`+
  `<b>Last fetch</b><span>${d.fetch?d.fetch.rel:"never"}</span><b>Record</b><span>${d.stats.recMag>0?"M"+d.stats.recMag.toFixed(1)+" · "+d.stats.recDate:"—"}</span>`;
 const c=d.cfg;f("manual").checked=c.manual;f("lat").value=c.lat;f("lon").value=c.lon;f("radius").value=c.radiusKm;
 f("minmag").value=c.minMag;f("units").value=c.unitsKm?"km":"mi";f("poll").value=c.pollMin;f("clock").value=c.clock24h?"24":"12";f("tz").value=c.tz;
});
f("cfg").addEventListener("submit",e=>{e.preventDefault();
 const b=new URLSearchParams({manual:f("manual").checked?1:0,lat:f("lat").value,lon:f("lon").value,
  radius:f("radius").value,minmag:f("minmag").value,units:f("units").value,poll:f("poll").value,
  clock:f("clock").value,tz:f("tz").value});
 fetch("/api/config",{method:"POST",body:b}).then(()=>{f("msg").textContent="Saved — rebooting…";});
});
function act(url,q){if(confirm(q))fetch(url,{method:"POST"}).then(()=>alert("Done — the device is restarting."));}
</script></body></html>)HTML";

  void buildState(JsonDocument& doc) {
    const auto& c = settings::get();
    doc["fw"]   = FIRMWARE_VERSION;
    doc["host"] = MDNS_HOSTNAME;
    doc["mac"]  = WiFi.macAddress();
    doc["ip"]   = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["view"] = viewstate::name(viewstate::current());
    doc["online"] = (WiFi.status() == WL_CONNECTED);
    doc["synced"] = timekeeper::synced();
    doc["uptime"] = (long)(millis() / 1000);

    JsonObject loc = doc["loc"].to<JsonObject>();
    loc["lat"] = c.lat; loc["lon"] = c.lon; loc["radiusKm"] = c.radiusKm;
    loc["minMag"] = c.minMag; loc["units"] = settings::distUnit();

    JsonObject cf = doc["cfg"].to<JsonObject>();
    cf["manual"] = c.locManual; cf["lat"] = c.lat; cf["lon"] = c.lon;
    cf["radiusKm"] = c.radiusKm; cf["minMag"] = c.minMag; cf["unitsKm"] = c.unitsKm;
    cf["pollMin"] = c.pollMin; cf["clock24h"] = c.clock24h; cf["tz"] = c.tz;

    JsonObject tm = doc["time"].to<JsonObject>();
    tm["hm"] = timekeeper::clockHM(c.clock24h);
    tm["ampm"] = timekeeper::ampm(c.clock24h);
    tm["date"] = timekeeper::dateStr();

    if (timekeeper::synced()) {
      time_t n = timekeeper::now();
      astro::Moon m = astro::moon(n);
      JsonObject mo = doc["moon"].to<JsonObject>();
      mo["illum"] = m.illum; mo["waxing"] = m.waxing; mo["name"] = m.name; mo["age"] = m.ageDays;

      astro::Sun s = astro::sun(c.lat, c.lon, n);
      if (s.valid) {
        JsonObject so = doc["sun"].to<JsonObject>();
        so["rise"] = astro::hm12(s.riseMin);
        so["set"]  = astro::hm12(s.setMin);
        int dl = s.setMin - s.riseMin; if (dl < 0) dl += 1440;
        char b[12]; snprintf(b, sizeof(b), "%dh %02dm", dl / 60, dl % 60);
        so["day"] = b;
      }
    }

    if (seismic::hasData()) {
      JsonObject ft = doc["fetch"].to<JsonObject>();
      ft["epoch"] = (long)seismic::lastFetch();
      ft["rel"]   = timekeeper::relative(seismic::lastFetch());
    }

    int m2, m3, m4; seismic::histogram(m2, m3, m4);
    float lo, hi;   seismic::magRange(lo, hi);
    JsonObject st = doc["stats"].to<JsonObject>();
    st["c24"] = seismic::count24h(); st["c7"] = seismic::count7d();
    st["capped"] = seismic::count7dCapped(); st["felt24"] = seismic::feltCount24h();
    st["m2"] = m2; st["m3"] = m3; st["m4"] = m4;
    st["magLo"] = lo; st["magHi"] = hi;
    st["recMag"] = seismic::recordMag(); st["recDate"] = seismic::recordDate();

    const auto& evs = seismic::events();
    int hi_idx = seismic::headlineIndex();
    if (hi_idx >= 0) {
      const auto& q = evs[hi_idx];
      JsonObject hd = doc["headline"].to<JsonObject>();
      hd["mag"] = String("M") + String(q.mag, 1);
      hd["place"] = q.place;
      hd["distDisp"] = (int)round(settings::toDisplayDist(q.distKm));
      hd["unit"] = settings::distUnit();
      hd["bearing"] = seismic::bearingName(q.bearingDeg);
      hd["depthKm"] = (int)round(q.depthKm);
      hd["rel"] = timekeeper::relative(q.t);
      hd["badge"] = seismic::badge(q);
    }

    JsonArray arr = doc["events"].to<JsonArray>();
    for (size_t i = 0; i < evs.size(); i++) {
      const auto& q = evs[i];
      JsonObject o = arr.add<JsonObject>();
      o["mag"] = q.mag; o["dk"] = q.distKm; o["bd"] = q.bearingDeg;
      o["dep"] = (int)round(q.depthKm); o["pl"] = q.place;
      o["dd"] = (int)round(settings::toDisplayDist(q.distKm));
      o["bn"] = seismic::bearingName(q.bearingDeg);
      o["rel"] = timekeeper::relative(q.t);
      o["head"] = ((int)i == hi_idx);
      o["cl"] = q.clustered;
    }

    JsonArray cls = doc["clusters"].to<JsonArray>();
    for (const auto& c : seismic::clusters()) {
      JsonObject o = cls.add<JsonObject>();
      o["dk"] = c.distKm; o["bd"] = c.bearingDeg; o["n"] = c.count; o["felt"] = c.anyFelt;
    }
  }

  void handleConfig(AsyncWebServerRequest* req) {
    settings::Config c = settings::get();
    auto P = [&](const char* k, String& dst) {
      if (req->hasParam(k, true)) dst = req->getParam(k, true)->value();
    };
    String v;
    P("manual", v); c.locManual = (v == "1");
    v = ""; P("lat", v);    if (v.length()) c.lat = v.toFloat();
    v = ""; P("lon", v);    if (v.length()) c.lon = v.toFloat();
    v = ""; P("radius", v); if (v.length()) c.radiusKm = v.toInt();
    v = ""; P("minmag", v); if (v.length()) c.minMag = v.toFloat();
    v = ""; P("units", v);  if (v.length()) c.unitsKm = (v == "km");
    v = ""; P("poll", v);   if (v.length()) c.pollMin = v.toInt();
    v = ""; P("clock", v);  if (v.length()) c.clock24h = (v == "24");
    v = ""; P("tz", v);     if (v.length()) c.tz = v;
    settings::update(c);
    req->send(200, "application/json", "{\"ok\":true}");
    g_reboot = true;   // main loop reboots so TZ/location re-init cleanly
  }
}  // namespace

namespace web {

void begin() {
  if (g_started) return;
  g_started = true;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) { r->send_P(200, "text/html", PAGE_HTML); });
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* r) { r->send_P(200, "text/html", SETTINGS_HTML); });
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* r) {
    JsonDocument doc; buildState(doc);
    AsyncResponseStream* res = r->beginResponseStream("application/json");
    serializeJson(doc, *res);
    r->send(res);
  });
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* r) {
    r->send(200, "application/json", "{\"ok\":true}"); g_reboot = true;
  });
  server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest* r) {
    r->send(200, "application/json", "{\"ok\":true}"); g_wifiReset = true;
  });
  server.onNotFound([](AsyncWebServerRequest* r) { r->send(404, "text/plain", "not found"); });

  ElegantOTA.setAuth(OTA_USERNAME, OTA_PASSWORD);   // /update is authenticated
  ElegantOTA.begin(&server);

  server.begin();
  Serial.printf("[web] http://%s.local/ (mirror) + /settings + /update\n", MDNS_HOSTNAME);
}

void loop() { ElegantOTA.loop(); }

bool consumeReboot()    { if (g_reboot)    { g_reboot = false;    return true; } return false; }
bool consumeWifiReset() { if (g_wifiReset) { g_wifiReset = false; return true; } return false; }

}  // namespace web
