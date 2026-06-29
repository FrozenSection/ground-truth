#include "web.h"
#include "config.h"
#include "settings.h"
#include "timekeeper.h"
#include "seismic.h"
#include "astro.h"
#include "viewstate.h"
#include "provisioning.h"
#include "neteth.h"
#include "health.h"
#include "statelock.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>

namespace {
  AsyncWebServer server(WEB_PORT);
  bool g_started = false;
  volatile bool g_reboot = false, g_wifiReset = false, g_factoryReset = false;
  volatile bool g_configMode = false;   // button-hold AP active -> captive-redirect unknown paths
  volatile bool g_hasPending = false;   // a validated Config is queued for the loop to apply (#2)
  settings::Config g_pendingCfg;        // guarded by g_stateMutex

  // Queue a validated config for the main loop to apply (it owns the global). Snapshots the
  // current config under the lock, lets the caller mutate the copy, then publishes it.
  void queueConfig(const settings::Config& c) {
    std::lock_guard<std::mutex> lk(g_stateMutex);
    g_pendingCfg = c;
    g_hasPending = true;
  }
  settings::Config currentConfigLocked() {              // a locked snapshot for the web task
    std::lock_guard<std::mutex> lk(g_stateMutex);
    return settings::get();
  }

  // ---- main page: the 1-bit display mirror + recent events (the "data") ----
  const char PAGE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Truth</title><style>
:root{font-family:'Helvetica Neue',Arial,sans-serif;color:#1a1a1a}
body{margin:0;background:#d7d7d5;padding:0 24px 24px}.wrap{max-width:680px;margin:0 auto}
.hdr{padding:20px 0 10px}
.top{display:flex;justify-content:space-between;align-items:baseline}
h1{font-size:20px;margin:0}a{color:#1a5fb4;text-decoration:none;font-size:13px}
.sub{color:#666;font-size:13px;margin:2px 0 0}
.frame{height:478px}
.panel{width:400px;height:300px;background:#fff;box-shadow:0 0 0 1px #b6b6b0,0 8px 22px rgba(0,0,0,.16);transform:scale(1.55);transform-origin:top left}
.pager{display:flex;gap:8px;align-items:center;margin:4px 0 12px}
.pg{font:inherit;font-size:13px;padding:.35rem .85rem;border:1px solid #b6b6b0;border-radius:7px;background:#ececea;color:#555;cursor:pointer}
.pg.on{background:#333;color:#fff;border-color:#333}
.dev{font-size:12px;color:#9a9a93;margin-left:4px}
table{width:100%;border-collapse:collapse;font-size:12.5px;margin-top:6px}
th,td{text-align:left;padding:5px 8px;border-bottom:1px solid #cfcfca}
th{color:#9a9a93;font-weight:600;font-size:11px;text-transform:uppercase}
td.r,th.r{text-align:right}.muted{color:#9a9a93}
</style></head><body><div class="wrap">
<div class="hdr">
<div class="top"><h1>Ground Truth <span id="ver" style="font-size:12px;color:#9a9a93;font-weight:400"></span></h1><a href="/settings">⚙ Settings</a></div>
<div class="sub" id="sub">loading…</div>
</div>
<div class="pager"><button class="pg" id="pgMap">Map</button><button class="pg" id="pgTl">Timeline</button><button class="pg" id="pgInfo">Info</button><span class="dev" id="devview"></span></div>
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
// ---- page selection (mirror-local; the device's own page is driven by its button) ----
let curPage="Map",userPicked=false,lastData=null;
const PAGES=["Map","Timeline","Info"],PGID={Map:"pgMap",Timeline:"pgTl",Info:"pgInfo"};
function setPage(p){curPage=p;userPicked=true;updateTabs();if(lastData)render(lastData);}
function updateTabs(){PAGES.forEach(p=>{const b=document.getElementById(PGID[p]);if(b)b.classList.toggle("on",curPage===p);});
 const dv=document.getElementById("devview");if(dv)dv.textContent=lastData?("● Device is showing: "+lastData.view):"";}
PAGES.forEach(p=>document.getElementById(PGID[p]).addEventListener("click",()=>setPage(p)));
// clickable ●○○ page dots, shared by every page
function pageDots(add){PAGES.forEach((p,i)=>{const dot=el("circle",{cx:368+i*12,cy:13,r:3.5,fill:p===curPage?"#000":"#fff",stroke:"#000"});dot.style.cursor="pointer";dot.addEventListener("click",()=>setPage(p));add(dot);});}

// ---- shared hero (page dots reflect the page you're VIEWING and are clickable) ----
function drawHero(add,d){const h=d.headline,clip=(s,n)=>s.length>n?s.slice(0,n-1)+"…":s;
 pageDots(add);
 if(d.offline){const ox=346,oy=15;add(el("path",{d:`M ${ox-7} ${oy-3} q 7 -7 14 0 M ${ox-4} ${oy} q 4 -4 8 0`,fill:"none",stroke:"#000","stroke-width":1.1}));add(el("circle",{cx:ox,cy:oy+2,r:1.1,fill:"#000"}));add(el("line",{x1:ox-8,y1:oy-6,x2:ox+8,y2:oy+5,stroke:"#000","stroke-width":1.3}));}
 if(h){add(tx(9,66,h.mag,{"font-size":52,"font-weight":700,"letter-spacing":-2.5}));
  add(tx(150,34,esc(clip(h.place.replace(/ km/g,"km"),33)),{"font-size":15,"font-weight":700}));
  add(tx(150,54,`Depth ${h.depthKm}km${d.timeOK&&h.rel?" · "+h.rel:""}`,{"font-size":13}));
  const home=clip((((d.loc&&d.loc.name)||"").split(",")[0].trim())||"home",22);
  add(tx(150,72,`${h.distDisp}${h.unit} ${h.bearing} of ${esc(home)}`,{"font-size":13}));}
 else add(tx(9,60,"Quiet",{"font-size":42,"font-weight":700}));
 add(el("line",{x1:0,y1:90,x2:400,y2:90,stroke:"#000"}));}

// ---- Page 1: Map ----
function drawMap(add,d){
 add(el("line",{x1:204,y1:92,x2:204,y2:241,stroke:"#000"}));
 const cx=100,cy=170,Ro=58;[58,47,34].forEach((r,i)=>add(el("circle",{cx,cy,r,fill:"none",stroke:"#000","stroke-dasharray":i?"1.5 3":"0"})));
 add(el("line",{x1:cx-6,y1:cy,x2:cx+6,y2:cy,stroke:"#000"}));add(el("line",{x1:cx,y1:cy-6,x2:cx,y2:cy+6,stroke:"#000"}));
 add(tx(cx,104,"N",{"font-size":9,"font-weight":700,"text-anchor":"middle"}));
 const R=d.loc.radiusKm;[[34,230,Math.round(R/3)],[47,180,Math.round(2*R/3)],[58,130,R]].forEach(([rr,bd,v])=>{const a=bd*Math.PI/180;add(tx(cx+rr*Math.sin(a),cy-rr*Math.cos(a)+3,v+"km",{"font-size":8.5,"text-anchor":"middle"}));});
 const plot=(dk,bd)=>{const r=Ro*Math.sqrt(Math.min(dk,d.loc.radiusKm)/d.loc.radiusKm),a=bd*Math.PI/180;return [(cx+r*Math.sin(a)).toFixed(1),(cy-r*Math.cos(a)).toFixed(1)];};
 (d.events||[]).forEach(q=>{if(q.head||q.cl)return;const [x,y]=plot(q.dk,q.bd),rad=q.mag>=4?5:q.mag>=3?3.4:2.3;add(el("circle",{cx:x,cy:y,r:rad,fill:q.dep>=8?"#000":"none",stroke:"#000"}));});
 (d.clusters||[]).forEach(c=>{const [x,y]=plot(c.dk,c.bd);add(el("circle",{cx:x,cy:y,r:10,fill:"none",stroke:"#000","stroke-dasharray":"2 2.5"}));add(el("circle",{cx:x,cy:y,r:5,fill:"#000"}));add(tx(+x+10,+y-6,"×"+c.n,{"font-size":11,"font-weight":700}));});
 {const hq=(d.events||[]).find(q=>q.head);if(hq){const [x,y]=plot(hq.dk,hq.bd);add(el("circle",{cx:x,cy:y,r:8.5,fill:"none",stroke:"#000"}));add(el("circle",{cx:x,cy:y,r:6,fill:"none",stroke:"#000"}));add(el("circle",{cx:x,cy:y,r:4,fill:"#000"}));}}
 const t=d.stats;
 add(tx(256,130,d.timeOK?t.c24:"—",{"font-size":38,"font-weight":700,"text-anchor":"end"}));add(tx(264,116,"IN 24H",{"font-size":9,"font-weight":700}));add(tx(264,130,d.timeOK?(t.felt24>0?`${t.felt24} felt nearby`:"none felt"):"",{"font-size":11}));
 add(tx(256,180,t.c7+(t.capped?"+":""),{"font-size":38,"font-weight":700,"text-anchor":"end"}));add(tx(264,166,"IN 7 DAYS",{"font-size":9,"font-weight":700}));
 const mr=t.magLo.toFixed(1)===t.magHi.toFixed(1)?`M${t.magLo.toFixed(1)}`:`M${t.magLo.toFixed(1)} – M${t.magHi.toFixed(1)}`;add(tx(264,180,mr,{"font-size":11}));
 add(el("line",{x1:212,y1:192,x2:388,y2:192,stroke:"#000","stroke-width":.8}));
 add(el("circle",{cx:219,cy:204,r:3.5,fill:"none",stroke:"#000"}));add(tx(230,208,"Shallow · < 8km",{"font-size":10}));
 add(el("circle",{cx:219,cy:216,r:3.5,fill:"#000"}));add(tx(230,220,"Deep · > 8km",{"font-size":10}));
 if(t.recMag>0)add(tx(212,232,`Largest: M${t.recMag.toFixed(1)} · ${t.recDate}`,{"font-size":9.5,"font-weight":600}));}

// ---- Page 2: Timeline (7-day lollipop strip; mirrors src/display.cpp drawTimelinePanel) ----
function drawTimeline(add,d){
 const x0=22,x1=388,baseY=224,topY=124,M2Y=205,topGridY=129,t=d.stats;
 // Dynamic magnitude ceiling (mirrors src/display.cpp): M2 anchored at the bottom; the top floats
 // to the next whole magnitude above the week's max (floored at M4) so a big quake towers, not clamps.
 let topMag=4;if(t.c7>0){const tm=Math.ceil(t.magHi);if(tm>topMag)topMag=tm;}if(topMag>8)topMag=8;
 const span=(M2Y-topGridY)/(topMag-2);
 for(let m=2;m<=topMag;m++){const gy=Math.round(M2Y-span*(m-2));add(el("line",{x1:x0,y1:gy,x2:x1,y2:gy,stroke:"#000","stroke-dasharray":"1 3"}));add(tx(x0-4,gy+3,"M"+m,{"font-size":9,"text-anchor":"end"}));}
 add(el("line",{x1:x0,y1:baseY,x2:x1,y2:baseY,stroke:"#000"}));
 const mx=t.c7>0?("M"+t.magHi.toFixed(1)):"–",rec=t.recMag>0?("M"+t.recMag.toFixed(1)):"–";
 const seg=(ax,lab,val,anc)=>{const e=el("text",{x:ax,y:111,fill:"#000","font-family":"Helvetica Neue,Arial","text-anchor":anc,"font-size":9,"font-weight":700});const a=document.createElementNS(NS,"tspan");a.textContent=lab+" ";const b=document.createElementNS(NS,"tspan");b.textContent=val;b.setAttribute("font-size","11");e.appendChild(a);e.appendChild(b);add(e);};
 seg(x0,"TODAY",d.timeOK?t.c24:"–","start");seg(204,"7-DAY MAX",mx,"middle");seg(x1,"LARGEST",rec,"end");
 if(!d.timeOK){add(tx(204,178,"Waiting for time sync…",{"font-size":13,"text-anchor":"middle",fill:"#555"}));return;}
 const now=d.now,start=now-7*86400;
 for(let dd=0;dd<7;dd++){const wx=x0+((dd+0.5)/7)*(x1-x0),c="SMTWTFS"[new Date((start+dd*86400)*1000).getDay()];add(tx(wx,baseY+14,c,{"font-size":9,"text-anchor":"middle"}));}
 const magY=m=>{let y=M2Y-span*(m-2);if(y>baseY-2)y=baseY-2;if(y<topY)y=topY;return y;};
 (d.events||[]).forEach(q=>{if(q.t<start)return;const ex=x0+((q.t-start)/(7*86400))*(x1-x0),ey=magY(q.mag),rad=q.mag>=4?4:q.mag>=3?3:2;
  add(el("line",{x1:ex,y1:baseY,x2:ex,y2:ey,stroke:"#000"}));add(el("circle",{cx:ex,cy:ey,r:rad,fill:"#000"}));
  if(q.head){add(el("circle",{cx:ex,cy:ey,r:rad+2,fill:"none",stroke:"#000"}));const lx=Math.max(40,Math.min(372,ex));add(tx(lx,Math.max(118,ey-rad-6),"M"+q.mag.toFixed(1),{"font-size":9,"text-anchor":"middle","font-weight":700}));}});}

// ---- persistent footer (shared by both pages) — each cell center-justified ----
function drawFooter(add,d){
 add(el("line",{x1:0,y1:242,x2:400,y2:242,stroke:"#000"}));
 if(!d.timeOK){add(tx(200,276,"Waiting for time sync…",{"font-size":12,"text-anchor":"middle",fill:"#555"}));return;}
 add(el("line",{x1:138,y1:242,x2:138,y2:300,stroke:"#000"}));add(el("line",{x1:268,y1:242,x2:268,y2:300,stroke:"#000"}));
 const C1=73,C2=207,C3=334;
 // cell 1 — time / date / location (centered)
 add(tx(C1,265,d.time.hm+(d.time.ampm?" "+d.time.ampm:""),{"font-size":24,"font-weight":700,"text-anchor":"middle","letter-spacing":-1}));
 add(tx(C1,280,d.time.date,{"font-size":11,"text-anchor":"middle"}));
 {let nm=(d.loc&&d.loc.name)||"";if(nm.length>20)nm=nm.slice(0,19)+"…";add(tx(C1,295,"⌂ "+nm,{"font-size":9.5,"font-weight":600,"text-anchor":"middle"}));}
 // cell 2 — sun (centered)
 if(d.sun){add(tx(C2,262,"☀   ↑"+d.sun.rise+"  ↓"+d.sun.set,{"font-size":12,"font-weight":600,"text-anchor":"middle"}));add(tx(C2,288,"Daylight: "+d.sun.day,{"font-size":11,"text-anchor":"middle"}));}
 else add(tx(C2,272,"sun —",{"font-size":12,"text-anchor":"middle",fill:"#888"}));
 // cell 3 — moon (disc + name/% group, centered)
 if(d.moon){const nm=d.moon.name,pct=`${Math.round(d.moon.illum*100)}% · day ${d.moon.age}`;
  const tW=Math.max(nm.length,pct.length)*5.4,gx=C3-(24+6+tW)/2,dc=gx+12;
  add(el("circle",{cx:dc,cy:271,r:12,fill:"#000"}));add(el("path",{d:moon(dc,271,12,d.moon.illum,d.moon.waxing),fill:"#fff"}));add(el("circle",{cx:dc,cy:271,r:12,fill:"none",stroke:"#000"}));
  add(tx(gx+30,266,nm,{"font-size":11.5,"font-weight":600}));add(tx(gx+30,281,pct,{"font-size":11}));}}

// ---- Page 3: Info — "B / balanced" (clock header + DEVICE table) ----
function drawInfo(add,d){
 add(tx(16,22,"GROUND TRUTH",{"font-size":10,"font-weight":700,"letter-spacing":2}));
 pageDots(add);
 add(el("line",{x1:16,y1:34,x2:384,y2:34,stroke:"#000"}));
 if(d.timeOK){
  add(tx(14,96,d.time.hm,{"font-size":52,"font-weight":700,"letter-spacing":-2}));
  if(d.time.ampm)add(tx(14+d.time.hm.length*30,96,d.time.ampm,{"font-size":15,"font-weight":600}));
  add(tx(196,70,d.time.date,{"font-size":13}));
  add(tx(196,92,"⌂ "+((d.loc&&d.loc.name)||""),{"font-size":11,"font-weight":600}));
 } else add(tx(14,92,"Waiting for time sync…",{"font-size":13,fill:"#555"}));
 add(el("line",{x1:16,y1:116,x2:384,y2:116,stroke:"#000"}));
 const up=Math.floor(d.uptime/3600)+"h "+Math.floor(d.uptime%3600/60)+"m";
 const eth=d.eth||{},ethVal=eth.present?eth.mac:"not installed";
 const rows=[["WEB",d.host+".local",1],["IP",d.online?d.ip:"--",1],["WIFI MAC",d.mac,1],
  ["ETHERNET",ethVal,eth.up?1:0],["FIRMWARE","v"+d.fw+"  ·  up "+up,0],
  ["STATUS",(d.online?"online":"offline")+(d.fetch&&d.timeOK?"  ·  data "+d.fetch.rel:""),0]];
 let y=152;rows.forEach(([l,v,k])=>{add(tx(16,y,l,{"font-size":9,"font-weight":700,fill:"#555"}));add(tx(118,y,esc(v),{"font-size":13,"font-weight":k?700:400}));y+=22;});}

function render(d){const s=document.getElementById("panel");s.innerHTML="";const add=e=>s.appendChild(e);
 if(curPage==="Info"){drawInfo(add,d);return;}
 drawHero(add,d);(curPage==="Timeline"?drawTimeline:drawMap)(add,d);drawFooter(add,d);}
function tick(){fetch("/api/state").then(r=>r.json()).then(d=>{
 document.getElementById("ver").textContent="v"+d.fw;
 document.getElementById("sub").textContent=(d.online?"online":"offline")+" · "+d.host+".local · updated "+(d.fetch?d.fetch.rel:"never")+(d.timeOK?"":" · ⏳ syncing time");
 lastData=d;if(!userPicked&&d.view)curPage=d.view;updateTabs();   // first load follows the device; then you browse freely
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
.msg{font-size:13px;color:#1a7f37;margin-top:10px;margin-left:14px}
</style></head><body><div class="wrap">
<div class="top"><h1>Settings</h1><span><a href="/">‹ Display</a> &nbsp;·&nbsp; <a href="/guide">Guide</a></span></div>

<h2>Location</h2>
<div class="card"><form id="cfg">
  <label>Find location by place name</label>
  <div class="row"><div style="flex:3"><input id="place" type="text" placeholder="e.g. Davis, CA"></div>
  <div style="flex:1"><button type="button" id="findbtn" style="width:100%;margin-top:0">Find</button></div></div>
  <div style="font-size:12px;color:#666;margin-top:4px" id="geomsg">Saving looks this up and sets the coordinates — or type them in directly below.</div>
  <label>Display label (shown on the device)</label>
  <input id="label" type="text" placeholder="e.g. Davis, CA" maxlength="48">
  <div style="font-size:12px;color:#666;margin-top:4px">Auto-filled from the search — edit it to anything you like (e.g. “Home”).</div>
  <div class="row"><div><label>Latitude</label><input id="lat" type="number" step="0.0001"></div>
  <div><label>Longitude</label><input id="lon" type="number" step="0.0001"></div></div>
  <div class="row"><div><label>Radius (km)</label><input id="radius" type="number"></div>
  <div><label>Min magnitude</label><input id="minmag" type="number" step="0.1"></div></div>
  <div class="row"><div><label>Poll interval (min)</label><input id="poll" type="number"></div>
  <div><label>Clock</label><select id="clock"><option value="12">12-hour</option><option value="24">24-hour</option></select></div></div>
  <label>Time zone</label>
  <select id="tz">
    <optgroup label="United States">
    <option value="PST8PDT,M3.2.0,M11.1.0">Pacific (PT)</option>
    <option value="MST7MDT,M3.2.0,M11.1.0">Mountain (MT)</option>
    <option value="MST7">Arizona (no DST)</option>
    <option value="CST6CDT,M3.2.0,M11.1.0">Central (CT)</option>
    <option value="EST5EDT,M3.2.0,M11.1.0">Eastern (ET)</option>
    <option value="AKST9AKDT,M3.2.0,M11.1.0">Alaska (AKT)</option>
    <option value="HST10">Hawaii (HT)</option></optgroup>
    <optgroup label="International">
    <option value="UTC0">UTC</option>
    <option value="GMT0BST,M3.5.0/1,M10.5.0">UK / Ireland</option>
    <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe</option>
    <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe</option>
    <option value="IST-5:30">India</option>
    <option value="CST-8">China</option>
    <option value="JST-9">Japan</option>
    <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia (Sydney)</option></optgroup>
  </select>
  <input type="hidden" id="manual" value="1">
  <details style="margin-top:.7rem"><summary style="font-size:13px;color:#666;cursor:pointer">Advanced — earthquake data source</summary>
    <label>USGS data URL</label>
    <input id="fdsn" type="text" placeholder="https://earthquake.usgs.gov/fdsnws/event/1/query" autocapitalize="none" autocorrect="off" spellcheck="false">
    <div style="font-size:12px;color:#666;margin:4px 0 0">Leave this alone. Only change it if quake data stops updating for good and USGS has moved the endpoint.</div>
  </details>
  <button type="submit">Save</button><span class="msg" id="msg"></span>
</form></div>

<h2>Network</h2>
<div class="card">
  <div class="row"><div><label>WiFi radio</label>
    <select id="wifi_on"><option value="1">On</option><option value="0">Off — Ethernet only</option></select></div>
  <div><label>Current network</label><input id="cur_ssid" disabled></div></div>
  <p style="font-size:12px;color:#666;margin:.1rem 0 .5rem">Turn WiFi off for a wired-only spot (a dorm on Ethernet) so the device stops broadcasting — the good-citizen setting. A 3-second hold on the device button always reopens this page.</p>
  <button onclick="setWifi()">Apply WiFi on/off</button>
  <hr style="margin:1rem 0;border:0;border-top:1px solid #ddd">
  <label>Join a network</label>
  <select id="wssid"><option>scanning…</option></select>
  <label>…or type the name</label>
  <input id="wmanual" placeholder="(leave blank to use the list)" autocapitalize="none" autocorrect="off" spellcheck="false">
  <label>Password</label>
  <input id="wpass" type="password" placeholder="network password" autocapitalize="none" autocorrect="off" spellcheck="false">
  <button onclick="saveWifi()">Save &amp; connect</button>
  <button class="warn" onclick="act('/api/wifi/reset','Forget the saved WiFi network and reopen the setup portal?')">Forget network</button>
  <p class="msg" id="wmsg"></p>
</div>

<h2>Diagnostics</h2>
<div class="card"><div class="kv" id="diag">loading…</div></div>

<h2>Firmware</h2>
<div class="card"><a href="/update">Open firmware update (OTA) ›</a>
<p style="font-size:12px;color:#666;margin:8px 0 0">Asks for a username/password — that's the device's update credential.</p></div>

<h2>Factory reset</h2>
<div class="card">
<p style="font-size:12px;color:#666;margin:0 0 .5rem">Erases all settings and the saved WiFi, returning the device to factory defaults (Davis, CA · Pacific · 300 km · M2.5). Use it to start fresh or before handing the device on.</p>
<button class="warn" onclick="act('/api/factory-reset','Erase ALL settings and the saved WiFi, and restore factory defaults? This cannot be undone.')">Reset to factory defaults</button></div>

<h2>Actions</h2>
<div class="card">
  <button onclick="act('/api/reboot','Reboot the device?')">Reboot</button>
</div>
<div style="height:40px"></div>
</div>
<script>
function f(id){return document.getElementById(id);}
let lastGeo={q:"",lat:null,lon:null,name:""};   // last successfully resolved search
let labelAuto=true;                              // is #label still an auto-derived value?
f("label").addEventListener("input",()=>{labelAuto=false;});

// Build a clean "Town, ST" label from a Nominatim addressdetails result.
function shortName(r){const a=r.address||{};
 const town=a.city||a.town||a.village||a.hamlet||a.suburb||a.municipality||a.county||r.name||"";
 let region="";const iso=a["ISO3166-2-lvl4"]||a["ISO3166-2-lvl6"]||"";
 if(iso&&iso.indexOf("-")>=0)region=iso.split("-")[1];
 if(!region)region=a.state||a.country||"";
 return region?(town+", "+region):town;}

// Geocode a query -> {lat,lon,name,full}. Rejects "no match" / network error.
function geocode(q){return fetch("https://nominatim.openstreetmap.org/search?format=json&addressdetails=1&limit=1&q="+encodeURIComponent(q))
 .then(r=>r.json()).then(a=>{if(!a||!a.length)return Promise.reject("no match");const r0=a[0];
  return {lat:+(+r0.lat).toFixed(4),lon:+(+r0.lon).toFixed(4),name:shortName(r0),full:r0.display_name};});}

// Apply a geocode result to the form: coords always; label only if still auto/empty.
function applyGeo(g){f("lat").value=g.lat;f("lon").value=g.lon;
 if(labelAuto||!f("label").value.trim()){f("label").value=g.name;labelAuto=true;}
 lastGeo={q:f("place").value.trim(),lat:g.lat,lon:g.lon,name:g.name};}

function load(){fetch("/api/state").then(r=>r.json()).then(d=>{
 const up=Math.floor(d.uptime/3600)+"h "+Math.floor(d.uptime%3600/60)+"m";
 const eth=d.eth||{};const ethTxt=eth.present?(eth.mac+(eth.up?" · up":" · down")):"—";
 // WiFi signal only when actually associated (RSSI is meaningless on Ethernet / radio-off).
 const sig=(d.rssi<0&&d.rssi>-100)?`<b>WiFi signal</b><span>${d.rssi} dBm · ${d.rssi>-60?"Excellent":d.rssi>-67?"Good":d.rssi>-75?"Fair":"Weak"}</span>`:"";
 f("diag").innerHTML=`<b>Firmware</b><span>v${d.fw}</span><b>Status</b><span>${d.online?"online":"offline"} · synced ${d.synced}</span>`+
  `<b>IP</b><span>${d.ip}</span><b>WiFi MAC</b><span>${d.mac}</span>`+sig+
  `<b>Ethernet</b><span>${ethTxt}</span><b>Host</b><span>${d.host}.local</span><b>Uptime</b><span>${up}</span>`+
  `<b>Last reset</b><span>${d.resetReason||"?"}</span><b>Heap</b><span>${(d.heap/1024|0)} KB · min ${(d.heapMin/1024|0)} KB</span>`+
  `<b>Last fetch</b><span>${d.fetch?d.fetch.rel:"never"}</span><b>Largest</b><span>${d.stats.recMag>0?"M"+d.stats.recMag.toFixed(1)+" · "+d.stats.recDate:"—"}</span>`;
 const c=d.cfg;f("lat").value=c.lat;f("lon").value=c.lon;f("radius").value=c.radiusKm;
 f("minmag").value=c.minMag;f("poll").value=c.pollMin;f("clock").value=c.clock24h?"24":"12";f("tz").value=c.tz;
 f("fdsn").value=c.fdsn||"";
 f("wifi_on").value=c.wifiEnabled?"1":"0";f("cur_ssid").value=d.wifiSsid||"(none set)";
 f("place").value=c.name||"";f("label").value=c.name||"";labelAuto=true;
 // Stored coords already correspond to the stored label — seed lastGeo so a save that
 // only tweaks radius/etc doesn't needlessly re-geocode or move the pin.
 lastGeo={q:(c.name||"").trim(),lat:+c.lat,lon:+c.lon,name:c.name||""};
});}
load();
// Run a geocode for the place box, updating the "Found: …" subtext + coords + label.
// Returns a promise so Save can await it; rejects on no-match/network so Save aborts.
function doFind(){const q=f("place").value.trim();if(!q)return Promise.resolve();
 f("geomsg").textContent="searching…";
 return geocode(q).then(g=>{applyGeo(g);f("geomsg").textContent="Found: "+g.full.slice(0,60);})
  .catch(err=>{f("geomsg").textContent=(err==="no match")?"No match — try a city and state.":"Search unavailable — enter coordinates manually.";return Promise.reject(err);});}
f("findbtn").addEventListener("click",()=>{doFind().catch(()=>{});});
// Enter in the search box = Find (preview), not Save — keeps the subtext in sync and
// avoids a surprise commit. (Enter in other fields still submits, the usual form behavior.)
f("place").addEventListener("keydown",e=>{if(e.key==="Enter"){e.preventDefault();doFind().catch(()=>{});}});
f("cfg").addEventListener("submit",e=>{e.preventDefault();
 const place=f("place").value.trim();
 f("msg").style.color="#1a7f37";f("msg").textContent="Saving…";
 // Couple search -> save: if the place text changed since the last resolve, geocode NOW
 // (via doFind, so the subtext updates too) so the saved coordinates always match.
 const prep=(place&&place!==lastGeo.q)?doFind():Promise.resolve();
 prep.then(()=>{
  const label=f("label").value.trim();
  const name=label||(place?lastGeo.name:"");   // editable label wins; else verified name; else blank -> device shows lat,lon
  const b=new URLSearchParams({manual:"1",lat:f("lat").value,lon:f("lon").value,radius:f("radius").value,
   minmag:f("minmag").value,poll:f("poll").value,clock:f("clock").value,tz:f("tz").value,name:name,
   fdsn:f("fdsn").value.trim()});
  return fetch("/api/config",{method:"POST",body:b}).then(r=>r.ok?r.text():r.text().then(t=>Promise.reject(t)));
 })
 .then(()=>{f("msg").textContent="Saved ✓ — applying & refreshing…";setTimeout(load,1600);setTimeout(()=>{f("msg").textContent="Saved ✓";},4200);})
 .catch(err=>{f("msg").style.color="#a3301f";f("msg").textContent=(err==="no match")?("Couldn't find “"+place+"” — check the spelling or enter coordinates."):("Rejected: "+err);});
});
function act(url,q){if(confirm(q))fetch(url,{method:"POST"}).then(()=>alert("Done — the device is restarting. This page is unreachable for ~10 s; reload it then."));}
// Network section: live WiFi scan + on/off + join.
function loadScan(){fetch("/api/wifi/scan").then(r=>r.json()).then(d=>{
 var s=f("wssid");
 if(d.disabled){s.innerHTML="";var o=document.createElement("option");o.textContent="(turn WiFi on above to scan — or type the name below)";s.appendChild(o);return;}
 if(d.scanning){setTimeout(loadScan,1300);return;}
 s.innerHTML="";
 (d.networks||[]).forEach(function(n){var o=document.createElement("option");
   o.value=n.ssid;o.textContent=n.ssid+" ("+n.rssi+"dBm)"+(n.enc?" \u{1F512}":"");s.appendChild(o);});
 if(!s.options.length){var o=document.createElement("option");o.textContent="(none found)";s.appendChild(o);}
});}
loadScan();
function setWifi(){f("wmsg").style.color="#1a7f37";f("wmsg").textContent="Applying… the device restarts (~10 s), then reload.";
 fetch("/api/wifi/enable",{method:"POST",body:new URLSearchParams({on:f("wifi_on").value})});}
function saveWifi(){var ssid=f("wmanual").value.trim()||f("wssid").value;
 if(!ssid||ssid==="(none found)"){f("wmsg").style.color="#a3301f";f("wmsg").textContent="Pick or type a network name.";return;}
 f("wmsg").style.color="#1a7f37";f("wmsg").textContent="Saving… the device restarts and joins “"+ssid+"”, then reload.";
 fetch("/api/wifi/save",{method:"POST",body:new URLSearchParams({ssid:ssid,pass:f("wpass").value})});}
</script></body></html>)HTML";

  // On-device owner's guide — the printed card's content, reachable at /guide once the device
  // is online. NO passwords (those live on the card); MACs + IP are filled live from /api/state.
  const char GUIDE_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Truth — Guide</title>
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Spectral:ital,wght@0,600;1,400;1,600&family=Public+Sans:wght@400;600;700&family=IBM+Plex+Mono:wght@400;600&display=swap" rel="stylesheet">
<style>
:root{--c:#ae4f2b}*{box-sizing:border-box}
body{font:15px/1.55 'Public Sans',-apple-system,system-ui,Arial,sans-serif;max-width:720px;margin:0 auto;padding:2rem 1.2rem 3rem;color:#1f1c18;background:#fff}
.mast{display:flex;justify-content:space-between;align-items:flex-end;flex-wrap:wrap;gap:8px}
h1{font-family:'Spectral',Georgia,serif;font-weight:600;font-size:clamp(34px,7vw,46px);letter-spacing:-.5px;line-height:.95;margin:0}
.tag{font-family:'Spectral',Georgia,serif;font-style:italic;font-size:15.5px;color:#6b655b;margin:7px 0 0}
.mr{text-align:right;padding-bottom:3px}.badge{font-size:10.5px;font-weight:700;letter-spacing:1.5px;color:var(--c)}.mr a{font-size:12px}
.wave{display:block;width:100%;height:22px;margin:16px 0 0}
.s1{height:1px;background:#d8d1c2}.s2{height:1px;background:#ece6d9;margin:2px 0 22px}
h2{display:flex;align-items:center;gap:9px;border-bottom:1px solid #e0dacd;padding-bottom:5px;margin:1.6rem 0 .55rem;font-size:12.5px;font-weight:700;letter-spacing:1px;text-transform:uppercase}
.n{display:inline-flex;align-items:center;justify-content:center;width:21px;height:21px;border:1.5px solid var(--c);border-radius:50%;font-size:11px;font-weight:700;color:var(--c);flex:none}
h2.gk{font-family:'Spectral',Georgia,serif;font-style:italic;font-weight:600;font-size:14px;color:var(--c);text-transform:none;letter-spacing:0}
ul{margin:.3rem 0;padding-left:1.25rem}li{margin:.32rem 0}
code{font-family:'IBM Plex Mono',ui-monospace,Menlo,monospace;font-size:.86em;background:#f5efe5;padding:1px 4px;border-radius:3px}
b{font-weight:700}a{color:var(--c)}
.dev{border:1.5px solid #1f1c18;border-radius:8px;padding:.85rem 1.1rem 1rem;background:#faf6ef;margin-top:1.7rem}
.dev .t{font-weight:700;letter-spacing:1px;text-transform:uppercase;font-size:12.5px;margin-bottom:.55rem}
.dev>div{margin:.28rem 0}.dev .note{font-size:.86rem;color:#6b655b;margin-top:.6rem}
.foot{display:flex;justify-content:space-between;flex-wrap:wrap;gap:6px;font-size:11px;color:#8a847a;margin-top:1.6rem;border-top:1px solid #ece6d9;padding-top:10px}
.foot span:last-child{font-family:'IBM Plex Mono',monospace}
</style></head><body>
<div class="mast">
  <div><h1>Ground Truth</h1><div class="tag">Your seismic desk display — a live window into earthquakes around you.</div></div>
  <div class="mr"><div class="badge">OWNER&rsquo;S GUIDE</div><a href="/settings">Settings &rsaquo;</a></div>
</div>
<svg class="wave" viewBox="0 0 704 22" preserveAspectRatio="none"><path d="M0 11 L92 11 L98 9.5 L104 12.5 L112 11 L150 11 L156 10 L162 12 L170 11 L210 11 L214 9 L218 13 L222 8 L226 14 L230 9.5 L234 12.5 L238 11 L300 11 L305 6 L310 16 L315 3 L320 19 L325 4 L330 17 L335 6 L340 15 L345 8 L350 14 L355 9.5 L360 12.5 L366 11 L430 11 L436 9.5 L442 12.5 L450 11 L520 11 L525 10 L530 12 L536 11 L610 11 L616 9.5 L622 12 L628 11 L704 11" fill="none" stroke="#ae4f2b" stroke-width="1.4" stroke-linejoin="round" stroke-linecap="round"/></svg>
<div class="s1"></div><div class="s2"></div>
<h2><span class="n">1</span>Get it online</h2><ul>
<li><b>WiFi:</b> join <code>GroundTruth-Setup</code> from your phone (password on the card) and pick your network — or open <code>192.168.4.1</code>.</li>
<li><b>Ethernet:</b> just plug in. On a campus/managed network, register the device's Ethernet MAC (below) first. If both links are up, it prefers wired.</li></ul>
<h2><span class="n">2</span>The button</h2><ul><li><b>Tap</b> &rarr; next view (Map &rarr; Timeline &rarr; Info)</li><li><b>Hold 3 s</b> &rarr; settings</li></ul>
<h2><span class="n">3</span>The screens</h2><ul>
<li><b>Map</b> — recent quakes nearby, scaled by magnitude</li>
<li><b>Timeline</b> — the last 7 days</li>
<li><b>Info</b> — clock, IP, and MAC addresses</li>
<li><b>The big number</b> is the most significant nearby quake (size + how widely it was felt) — last 24 h if active, otherwise the biggest of the past 7 days.</li>
<li><b>"Quiet"</b> = nothing notable lately (not an error). A still screen is normal — e-paper only redraws on a change.</li></ul>
<h2><span class="n">4</span>Settings</h2><ul>
<li><b>At home:</b> <code>groundtruth.local</code> (or the IP on the Info screen).</li>
<li><b>On a network that hides it:</b> hold the button &rarr; join <code>GroundTruth-Setup</code> &rarr; <code>192.168.4.1</code>.</li>
<li>Location, radius, magnitude, time zone, WiFi. <b>Diagnostics</b> shows signal, uptime, memory, last reset.</li></ul>
<h2><span class="n">5</span>On a managed network (dorm, lab, campus)</h2>
<p>Use the Ethernet jack and register the Ethernet MAC (below) with housing / IT. Optional: flip WiFi off (Settings &rarr; Network) so it isn't broadcasting on a wired network.</p>
<h2><span class="n">6</span>If something's off</h2><ul>
<li><b>Frozen or blank:</b> power-cycle it (it also self-recovers within ~30 s).</li>
<li><b>"Offline" mark:</b> check the link, or hold the button to reconnect.</li></ul>
<h2 class="gk">Good to know</h2><ul>
<li>Open-source — <code>github.com/FrozenSection/ground-truth</code></li>
<li>Firmware updates at <a href="/update">/update</a> (or USB).</li>
<li>If USGS moves its feed, the data-source URL is editable in Settings &rarr; Advanced.</li></ul>
<div class="dev"><div class="t">This device</div>
<div><b>Ethernet MAC:</b> <code id="emac" style="color:var(--c);font-weight:600">…</code> <span style="color:#8a847a">— register on a managed network</span></div>
<div><b>WiFi MAC:</b> <code id="wmac">…</code></div>
<div><b>Address:</b> <code id="gip">…</code> &middot; <code>groundtruth.local</code></div>
<div class="note">Your passwords are on the printed card that came in the box.</div></div>
<div class="foot"><span>Earthquake data from the U.S. Geological Survey</span><span>groundtruth.local</span></div>
<script>fetch('/api/state').then(r=>r.json()).then(d=>{var e=d.eth||{};
emac.textContent=e.mac||'—';wmac.textContent=d.mac||'—';gip.textContent=d.ip||'—';});</script>
</body></html>)HTML";

  void buildState(JsonDocument& doc) {
    std::lock_guard<std::mutex> lk(g_stateMutex);   // seismic state is mutated in the main loop
    const auto& c = settings::get();
    const bool ok = timekeeper::synced();           // time trustworthy? gate derived fields
    doc["timeOK"]  = ok;
    doc["timeSrc"] = timekeeper::source();
    doc["fw"]   = FIRMWARE_VERSION;
    doc["host"] = MDNS_HOSTNAME;
    doc["mac"]  = provisioning::staMac();   // efuse MAC — valid even with the radio off (ETH-only)
    doc["ip"]   = neteth::activeIP();
    doc["rssi"] = WiFi.RSSI();
    doc["view"] = viewstate::name(viewstate::current());
    bool wifiUp = (WiFi.status() == WL_CONNECTED);
    doc["online"] = wifiUp || neteth::up();
    JsonObject eth = doc["eth"].to<JsonObject>();      // wired Ethernet (W5500)
    eth["present"] = neteth::present(); eth["up"] = neteth::up();
    eth["mac"] = neteth::mac(); eth["ip"] = neteth::ip();
    doc["synced"] = timekeeper::synced();
    doc["uptime"] = (long)(millis() / 1000);
    doc["resetReason"] = health::resetReason();   // why it last booted (soak diagnosis)
    doc["heap"]    = (long)health::freeHeap();
    doc["heapMin"] = (long)health::minHeap();      // lowest-ever free heap — a leak walks this down
    doc["now"]    = ok ? (long)timekeeper::now() : 0;   // device clock — anchors the timeline window

    JsonObject loc = doc["loc"].to<JsonObject>();
    loc["lat"] = c.lat; loc["lon"] = c.lon; loc["radiusKm"] = c.radiusKm;
    loc["minMag"] = c.minMag; loc["units"] = settings::distUnit();
    loc["name"] = settings::locLabel();   // footer cell 1 + map context (geocode or coords)
    // Hero "offline" flag drives the slashed-WiFi glyph — only when BOTH links are down
    // (the device holds the last frame and retries; it no longer auto-opens the portal).
    doc["offline"] = !(wifiUp || neteth::up());

    JsonObject cf = doc["cfg"].to<JsonObject>();
    cf["manual"] = c.locManual; cf["lat"] = c.lat; cf["lon"] = c.lon;
    cf["radiusKm"] = c.radiusKm; cf["minMag"] = c.minMag; cf["unitsKm"] = c.unitsKm;
    cf["pollMin"] = c.pollMin; cf["clock24h"] = c.clock24h; cf["tz"] = c.tz;
    cf["name"] = c.name; cf["wifiEnabled"] = c.wifiEnabled; cf["fdsn"] = c.fdsnUrl;
    doc["wifiSsid"] = provisioning::storedSsid();   // current saved network (Network section)

    JsonObject tm = doc["time"].to<JsonObject>();
    tm["hm"]   = ok ? timekeeper::clockHM(c.clock24h) : "--:--";
    tm["ampm"] = ok ? timekeeper::ampm(c.clock24h) : "";
    tm["date"] = ok ? timekeeper::dateStr() : "";

    if (ok) {
      time_t n = timekeeper::now();
      astro::Moon m = astro::moon(n);
      JsonObject mo = doc["moon"].to<JsonObject>();
      mo["illum"] = m.illum; mo["waxing"] = m.waxing; mo["name"] = m.name; mo["age"] = m.ageDays;

      astro::Sun s = astro::sun(c.lat, c.lon, n);
      if (s.valid) {
        JsonObject so = doc["sun"].to<JsonObject>();
        so["rise"] = astro::hm12(s.riseMin, c.clock24h);
        so["set"]  = astro::hm12(s.setMin, c.clock24h);
        int dl = s.setMin - s.riseMin; if (dl < 0) dl += 1440;
        char b[12]; snprintf(b, sizeof(b), "%dh %02dm", dl / 60, dl % 60);
        so["day"] = b;
      }
    }

    if (seismic::hasData()) {
      JsonObject ft = doc["fetch"].to<JsonObject>();
      ft["epoch"] = (long)seismic::lastFetch();
      ft["rel"]   = ok ? timekeeper::relative(seismic::lastFetch()) : "—";
    }

    int m2, m3, m4; seismic::histogram(m2, m3, m4);
    float lo, hi;   seismic::magRange(lo, hi);
    JsonObject st = doc["stats"].to<JsonObject>();
    st["c24"] = ok ? seismic::count24h() : -1; st["c7"] = seismic::count7d();
    st["capped"] = seismic::count7dCapped(); st["felt24"] = ok ? seismic::feltCount24h() : -1;
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
      hd["rel"] = ok ? timekeeper::relative(q.t) : "";
      hd["badge"] = seismic::badge(q);
    }

    JsonArray arr = doc["events"].to<JsonArray>();
    for (size_t i = 0; i < evs.size(); i++) {
      const auto& q = evs[i];
      JsonObject o = arr.add<JsonObject>();
      o["mag"] = q.mag; o["dk"] = q.distKm; o["bd"] = q.bearingDeg;
      o["t"] = (long)q.t;                              // epoch — timeline x-position
      o["dep"] = (int)round(q.depthKm); o["pl"] = q.place;
      o["dd"] = (int)round(settings::toDisplayDist(q.distKm));
      o["bn"] = seismic::bearingName(q.bearingDeg);
      o["rel"] = ok ? timekeeper::relative(q.t) : "";
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
    settings::Config c = currentConfigLocked();   // snapshot under the lock (web task)
    auto P = [&](const char* k, String& dst) {
      if (req->hasParam(k, true)) dst = req->getParam(k, true)->value();
    };
    String v;
    P("manual", v); c.locManual = (v == "1");
    v = ""; P("lat", v);    if (v.length()) c.lat = v.toFloat();
    v = ""; P("lon", v);    if (v.length()) c.lon = v.toFloat();
    v = ""; P("radius", v); if (v.length()) c.radiusKm = v.toInt();
    v = ""; P("minmag", v); if (v.length()) c.minMag = v.toFloat();
    v = ""; P("poll", v);   if (v.length()) c.pollMin = v.toInt();
    v = ""; P("clock", v);  if (v.length()) c.clock24h = (v == "24");
    v = ""; P("tz", v);     if (v.length()) c.tz = v;
    c.name = ""; P("name", c.name);   // geocode label (may be blank for raw coords)
    v = ""; P("fdsn", v); if (v.length()) c.fdsnUrl = v;   // advanced: data-source URL
    String err;
    if (!settings::validate(c, err)) { req->send(400, "text/plain", err); return; }
    queueConfig(c);   // the main loop applies it (TZ re-apply + re-fetch) — no cross-task write
    req->send(200, "application/json", "{\"ok\":true}");
  }
}  // namespace

namespace web {

void begin() {
  if (g_started) return;
  g_started = true;

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/html", PAGE_HTML); });
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/html", SETTINGS_HTML); });
  server.on("/guide", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/html", GUIDE_HTML); });
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
  server.on("/api/factory-reset", HTTP_POST, [](AsyncWebServerRequest* r) {
    r->send(200, "application/json", "{\"ok\":true}"); g_factoryReset = true;
  });
  server.on("/api/wifi/enable", HTTP_POST, [](AsyncWebServerRequest* r) {
    bool on = r->hasParam("on", true) && r->getParam("on", true)->value() == "1";
    settings::Config c = currentConfigLocked(); c.wifiEnabled = on; queueConfig(c);
    r->send(200, "application/json", "{\"ok\":true}");
    g_reboot = true;   // loop applies the queued config (persists wifiEnabled) BEFORE this reboot fires
  });
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    r->send(200, "application/json", provisioning::scanJson());
  });
  server.on("/api/wifi/save", HTTP_POST, [](AsyncWebServerRequest* r) {
    String ssid = r->hasParam("ssid", true) ? r->getParam("ssid", true)->value() : String("");
    String pass = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : String("");
    if (!ssid.length()) { r->send(400, "text/plain", "network name required"); return; }
    provisioning::saveCreds(ssid, pass);
    settings::Config c = currentConfigLocked(); c.wifiEnabled = true; queueConfig(c);  // setting a network enables WiFi
    r->send(200, "application/json", "{\"ok\":true}");
    g_reboot = true;                                  // reboot to join the new network cleanly
  });
  // Captive-portal helper: in Config mode, send unknown paths to the settings page so a
  // phone's "sign in to network" check pops it open; otherwise a plain 404.
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (g_configMode) r->redirect(String("http://") + AP_IP_STR + "/settings");
    else              r->send(404, "text/plain", "not found");
  });

  // Auth MUST be passed to begin(): begin(server, user="", pass="") internally calls
  // setAuth(user, pass), so a separate setAuth() before it gets overwritten to empty
  // (= auth disabled). /update + /ota/* are the firmware-flash endpoints — keep them locked.
  ElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);

  server.begin();
  Serial.printf("[web] http://%s.local/ (mirror) + /settings + /update\n", MDNS_HOSTNAME);
}

void loop() { ElegantOTA.loop(); }

bool consumeReboot()      { if (g_reboot)      { g_reboot = false;      return true; } return false; }
bool consumeWifiReset()   { if (g_wifiReset)   { g_wifiReset = false;   return true; } return false; }
bool consumeFactoryReset(){ if (g_factoryReset){ g_factoryReset = false; return true; } return false; }
bool consumePendingConfig(settings::Config& out) {
  if (!g_hasPending) return false;                       // cheap unlocked pre-check
  std::lock_guard<std::mutex> lk(g_stateMutex);
  if (!g_hasPending) return false;
  out = g_pendingCfg;
  g_hasPending = false;
  return true;
}
void setConfigMode(bool on) { g_configMode = on; }

}  // namespace web
