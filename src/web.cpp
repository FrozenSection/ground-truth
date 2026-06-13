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
#include <ArduinoJson.h>

namespace {
  AsyncWebServer server(WEB_PORT);
  bool g_started = false;

  // A faithful-ish 1-bit mirror of the B-tight display, rendered in the browser
  // from /api/state. Auto-refreshes. Intentionally black-on-white to match the
  // physical panel.
  const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Truth</title><style>
:root{font-family:'Helvetica Neue',Arial,sans-serif;color:#1a1a1a}
body{margin:0;background:#d7d7d5;padding:24px}
.wrap{max-width:680px;margin:0 auto}
h1{font-size:20px;margin:0 0 4px}.sub{color:#666;font-size:13px;margin:0 0 18px}
.panel{width:400px;height:300px;background:#fff;box-shadow:0 0 0 1px #b6b6b0,0 8px 22px rgba(0,0,0,.16);transform-origin:top left}
.frame{height:480px}
.kv{display:grid;grid-template-columns:auto 1fr;gap:2px 14px;font-size:13px;color:#33332e;margin:18px 0}
.kv b{color:#9a9a93;font-weight:600}
table{width:100%;border-collapse:collapse;font-size:12.5px;margin-top:8px}
th,td{text-align:left;padding:5px 8px;border-bottom:1px solid #cfcfca}
th{color:#9a9a93;font-weight:600;font-size:11px;text-transform:uppercase;letter-spacing:.5px}
td.r,th.r{text-align:right}
.muted{color:#9a9a93}
</style></head><body><div class="wrap">
<h1>Ground Truth</h1>
<div class="sub" id="sub">loading…</div>
<div class="frame"><svg id="panel" class="panel" viewBox="0 0 400 300" style="transform:scale(1.6)"></svg></div>
<div class="kv" id="status"></div>
<h1 style="font-size:15px;margin-top:22px">Recent events</h1>
<table><thead><tr><th>Mag</th><th>Place</th><th class="r">Dist</th><th class="r">Depth</th><th class="r">When</th></tr></thead>
<tbody id="rows"></tbody></table>
<p class="muted" style="font-size:11px;margin-top:18px">Live mirror · refreshes every 20 s · sun/sunset arrive at Gate 3</p>
</div>
<script>
const SVGNS="http://www.w3.org/2000/svg";
function el(t,a){const e=document.createElementNS(SVGNS,t);for(const k in a)e.setAttribute(k,a[k]);return e;}
function txt(x,y,s,o){const e=el("text",Object.assign({x,y,fill:"#000","font-family":"Helvetica Neue,Arial"},o||{}));e.textContent=s;return e;}
function esc(s){return (s||"").replace(/[&<>]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;"}[c]));}

function moonPath(cx,cy,R,k,waxing){
  const rx=R*Math.abs(1-2*k);
  const top=`${cx} ${cy-R}`, bot=`${cx} ${cy+R}`;
  const limb=waxing?1:0; let term;
  if(waxing) term=k<0.5?0:1; else term=k<0.5?1:0;
  return `M ${top} A ${R} ${R} 0 0 ${limb} ${bot} A ${rx} ${R} 0 0 ${term} ${top} Z`;
}

function render(d){
  const s=document.getElementById("panel"); s.innerHTML="";
  const add=e=>s.appendChild(e);
  const h=d.headline;

  // ---- hero ----
  if(h&&h.badge){ add(el("circle",{cx:16,cy:13,r:4,fill:"#000"}));
    add(txt(25,17,h.badge.toUpperCase(),{ "font-size":9,"font-weight":700,"letter-spacing":.6})); }
  const pages=2, page=1;
  for(let i=0;i<pages;i++) add(el("circle",{cx:368+i*12,cy:13,r:3.5,
    fill:(i+1)===page?"#000":"#fff",stroke:"#000"}));
  if(h){
    add(txt(9,66,h.mag,{ "font-size":52,"font-weight":700,"letter-spacing":-2.5}));
    add(txt(150,32,esc(h.place),{ "font-size":15,"font-weight":700}));
    add(txt(150,64,`depth ${h.depthKm} km · ${h.distDisp} ${h.unit} ${h.bearing}`,{ "font-size":13}));
    add(txt(150,82,h.rel,{ "font-size":13}));
  } else {
    add(txt(9,60,"Quiet",{ "font-size":42,"font-weight":700}));
  }
  add(el("line",{x1:0,y1:90,x2:400,y2:90,stroke:"#000"}));
  add(el("line",{x1:204,y1:92,x2:204,y2:241,stroke:"#000"}));

  // ---- map ----
  const cx=100,cy=170,Rout=58;
  [58,47,34].forEach((r,i)=>add(el("circle",{cx,cy,r,fill:"none",stroke:"#000",
    "stroke-dasharray":i?"1.5 3":"0"})));
  add(el("line",{x1:cx-6,y1:cy,x2:cx+6,y2:cy,stroke:"#000"}));
  add(el("line",{x1:cx,y1:cy-6,x2:cx,y2:cy+6,stroke:"#000"}));
  add(txt(cx,104,"N",{ "font-size":9,"font-weight":700,"text-anchor":"middle"}));
  add(txt(133,221,(d.loc.radiusKm||300)+" km",{ "font-size":8.5}));
  (d.events||[]).forEach(q=>{
    const r=Rout*Math.sqrt(Math.min(q.dk,d.loc.radiusKm)/d.loc.radiusKm);
    const a=q.bd*Math.PI/180, x=cx+r*Math.sin(a), y=cy-r*Math.cos(a);
    const rad=q.mag>=4?5:q.mag>=3?3.4:2.3;
    const deep=q.dep>=8;
    add(el("circle",{cx:x.toFixed(1),cy:y.toFixed(1),r:rad,
      fill:deep?"#000":"none",stroke:"#000"}));
    if(q.head){ add(el("circle",{cx:x.toFixed(1),cy:y.toFixed(1),r:rad+3.5,fill:"none",stroke:"#000","stroke-width":1.2})); }
  });

  // ---- stats (B-tight) ----
  const st=d.stats;
  add(txt(212,130,st.c24,{ "font-size":38,"font-weight":700}));
  add(txt(250,116,"IN 24 H",{ "font-size":9,"font-weight":700}));
  add(txt(250,130,st.felt24>0?`${st.felt24} felt nearby`:"none felt",{ "font-size":11}));
  add(txt(212,180,st.c7+(st.capped?"+":""),{ "font-size":38,"font-weight":700}));
  add(txt(262,166,"IN 7 DAYS",{ "font-size":9,"font-weight":700}));
  add(txt(262,180,`M${st.magLo.toFixed(1)} – M${st.magHi.toFixed(1)}`,{ "font-size":11}));
  add(el("line",{x1:212,y1:192,x2:388,y2:192,stroke:"#000","stroke-width":.8}));
  add(el("circle",{cx:219,cy:204,r:3.5,fill:"none",stroke:"#000"}));
  add(txt(230,208,"shallow · <8 km",{ "font-size":10}));
  add(el("circle",{cx:219,cy:220,r:3.5,fill:"#000"}));
  add(txt(230,224,"deep · ≥8 km",{ "font-size":10}));
  if(st.recMag>0) add(txt(212,238,`REC M${st.recMag.toFixed(1)} · ${(st.recDate||"").toUpperCase()}`,{ "font-size":9.5,"font-weight":600}));

  // ---- footer ----
  add(el("line",{x1:0,y1:242,x2:400,y2:242,stroke:"#000"}));
  add(el("line",{x1:138,y1:242,x2:138,y2:300,stroke:"#000"}));
  add(el("line",{x1:268,y1:242,x2:268,y2:300,stroke:"#000"}));
  add(txt(16,271,d.time.hm,{ "font-size":24,"font-weight":700,"letter-spacing":-1}));
  if(d.time.ampm) add(txt(16+d.time.hm.length*14,271,d.time.ampm,{ "font-size":12,"font-weight":600}));
  add(txt(16,291,d.time.date,{ "font-size":12}));
  add(txt(148,262,"sun —",{ "font-size":12,fill:"#888"}));
  add(txt(148,282,"Gate 3",{ "font-size":10,fill:"#aaa"}));
  if(d.moon){
    add(el("circle",{cx:291,cy:271,r:12,fill:"none",stroke:"#000"}));
    add(el("path",{d:moonPath(291,271,12,d.moon.illum,d.moon.waxing),fill:"#000"}));
    add(txt(308,266,d.moon.name,{ "font-size":11.5,"font-weight":600}));
    add(txt(308,281,`${Math.round(d.moon.illum*100)}% · day ${d.moon.age}`,{ "font-size":11}));
  }
}

function renderStatus(d){
  document.getElementById("sub").textContent =
    (d.online?"online":"offline")+" · "+d.host+".local · updated "+(d.fetch?d.fetch.rel:"never");
  const k=document.getElementById("status");
  k.innerHTML="";
  const add=(a,b)=>{k.innerHTML+=`<b>${a}</b><span>${b}</span>`;};
  add("Firmware","v"+d.fw);
  add("Location",`${d.loc.lat.toFixed(3)}, ${d.loc.lon.toFixed(3)} · ${d.loc.radiusKm} km · ≥M${d.loc.minMag}`);
  add("Active view",d.view);
  add("IP / MAC",`${d.ip}  ·  ${d.mac}`);
  add("Signal",d.rssi+" dBm");
  add("Record",d.stats.recMag>0?`M${d.stats.recMag.toFixed(1)} · ${d.stats.recDate}`:"—");
  const rows=document.getElementById("rows"); rows.innerHTML="";
  (d.events||[]).slice(0,12).forEach(q=>{
    rows.innerHTML+=`<tr><td>M${q.mag.toFixed(1)}</td><td>${esc(q.pl)}</td>`+
      `<td class="r">${q.dd} ${d.loc.units} ${q.bn}</td><td class="r">${q.dep} km</td>`+
      `<td class="r muted">${q.rel}</td></tr>`;
  });
}

function tick(){
  fetch("/api/state").then(r=>r.json()).then(d=>{render(d);renderStatus(d);})
    .catch(e=>{document.getElementById("sub").textContent="fetch error: "+e;});
}
tick(); setInterval(tick,20000);
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

    JsonObject loc = doc["loc"].to<JsonObject>();
    loc["lat"] = c.lat; loc["lon"] = c.lon; loc["radiusKm"] = c.radiusKm;
    loc["minMag"] = c.minMag; loc["units"] = settings::distUnit();

    JsonObject tm = doc["time"].to<JsonObject>();
    tm["hm"]   = timekeeper::clockHM(c.clock24h);
    tm["ampm"] = timekeeper::ampm(c.clock24h);
    tm["date"] = timekeeper::dateStr();

    if (timekeeper::synced()) {
      astro::Moon m = astro::moon(timekeeper::now());
      JsonObject mo = doc["moon"].to<JsonObject>();
      mo["illum"] = m.illum; mo["waxing"] = m.waxing;
      mo["name"] = m.name;   mo["age"] = m.ageDays;
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
      o["mag"] = q.mag;
      o["dk"]  = q.distKm;
      o["bd"]  = q.bearingDeg;
      o["dep"] = (int)round(q.depthKm);
      o["pl"]  = q.place;
      o["dd"]  = (int)round(settings::toDisplayDist(q.distKm));
      o["bn"]  = seismic::bearingName(q.bearingDeg);
      o["rel"] = timekeeper::relative(q.t);
      o["head"] = ((int)i == hi_idx);
    }
  }
}  // namespace

namespace web {

void begin() {
  if (g_started) return;
  g_started = true;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", PAGE_HTML);
  });
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    buildState(doc);
    AsyncResponseStream* res = req->beginResponseStream("application/json");
    serializeJson(doc, *res);
    req->send(res);
  });
  server.onNotFound([](AsyncWebServerRequest* req) { req->send(404, "text/plain", "not found"); });
  server.begin();
  Serial.printf("[web] status server on http://%s.local/\n", MDNS_HOSTNAME);
}

}  // namespace web
