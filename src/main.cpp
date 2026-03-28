#include <Arduino.h>
#include "ssd1306.h"
#include "hx711.h"
#include <WiFi.h>
#include <WebServer.h>

HX711 scales[] = {{D6,D3}, {D7,D3}, {D8,D3}, {D9,D3}};
SSD1306 oled(0x3C, 128, 64);
static const char* TRAILER_SSID     = "Race Trailer";
static const char* TRAILER_PASSWORD = "PUT_YOUR_PASSWORD_HERE";  // set this
static const uint32_t CONNECT_TIMEOUT_MS = 12000;                // 12s is usually plenty

static const char* AP_SSID     = "Race Scales";
static const char* AP_PASSWORD = "12345678"; // 8+ chars required if not open (recommended)

// Optional: give it a consistent hostname when on the trailer Wi-Fi
static const char* HOSTNAME = "race-scales";
WebServer server(80);
String jsonPayload;
static const uint32_t SENSOR_UPDATE_MS = 100; // "new data" cadence (10 Hz)
uint32_t lastSensorUpdate = 0;
uint32_t ts_ms = 0;
uint32_t seq = 0;
String FormatWeight(float weight,int size);
void UpdateWeights();
float weights[] = {0,0,0,0};
bool tarePending[] = {false,false,false,false};
float percents[] = {0,0,0,0};
float totalWeight, crossWeight;
float calOffset = 4180;
float calWeight = 178;
float scaleFactor = calOffset / calWeight * 453.5924;  



bool connectToTrailerWifi() {
  // Fast connect attempt without a full scan:
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);

  // If you want to be extra sure it "looks for" the SSID before attempting:
  // (This adds a little time but makes intent explicit.)
  int found = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  bool sawTrailer = false;
  for (int i = 0; i < found; i++) {
    if (WiFi.SSID(i) == TRAILER_SSID) {
      sawTrailer = true;
      break;
    }
  }
  WiFi.scanDelete();

  if (!sawTrailer) {
    Serial.println("[WiFi] Trailer SSID not found in scan.");
    return false;
  }

  Serial.printf("[WiFi] Found \"%s\". Connecting...\n", TRAILER_SSID);
  WiFi.begin(TRAILER_SSID, TRAILER_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WiFi] Connected to trailer network!");
    Serial.print("      IP: "); Serial.println(WiFi.localIP());
    Serial.print("      RSSI: "); Serial.println(WiFi.RSSI());
    return true;
  }

  Serial.println("[WiFi] Connection attempt timed out.");
  WiFi.disconnect(true, true);
  return false;
}

void startFallbackAP() {
  WiFi.mode(WIFI_AP);

  // Set the AP’s IP so it’s predictable (default is also usually 192.168.4.1)
  //IPAddress apIP(192, 168, 4, 1);
  //IPAddress apGW(192, 168, 4, 1);
  //IPAddress apMask(255, 255, 255, 0);
  //WiFi.softAPConfig(apIP, apGW, apMask);

  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (!ok) {
    Serial.println("[WiFi] softAP failed to start!");
    return;
  }

  Serial.println("[WiFi] Started fallback Access Point!");
  Serial.print("      SSID: "); Serial.println(AP_SSID);
  Serial.print("      IP: ");   Serial.println(WiFi.softAPIP());
  Serial.println("      Connect your phone to this Wi-Fi, then open http://192.168.4.1/");
}





// ======================= Simple UI =======================
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Race Scales</title>
  <style>
    body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Arial;margin:16px}
    .sub{opacity:.7;font-size:14px}
    .grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;max-width:560px}
    .card{border:1px solid #ddd;border-radius:16px;padding:12px}
    .controls{max-width:560px;margin-top:10px}
    .btnRow{display:flex;gap:8px;flex-wrap:wrap;margin-top:8px}
    button{border:1px solid #999;background:#fff;border-radius:10px;padding:8px 12px;font-weight:600;cursor:pointer}
    button:hover{background:#f6f6f6}
    button:disabled{opacity:.5;cursor:wait}
    .row{display:flex;justify-content:space-between;gap:10px}
    .big{font-size:30px;font-weight:750;letter-spacing:.2px}
    .pct{opacity:.7}
    .total{max-width:560px;margin-top:10px}
    .warn{color:#b00}
  </style>
</head>
<body>
  <h2 style="margin:0 0 4px 0;">Race Scales</h2>
  <div class="sub" id="status">Connecting…</div>

  <div class="grid" style="margin-top:12px">
    <div class="card">
      <div class="row"><div>LF</div><div class="pct" id="lfPct">--%</div></div>
      <div class="big" id="lf">--.-</div>
    </div>
    <div class="card">
      <div class="row"><div>RF</div><div class="pct" id="rfPct">--%</div></div>
      <div class="big" id="rf">--.-</div>
    </div>
    <div class="card">
      <div class="row"><div>LR</div><div class="pct" id="lrPct">--%</div></div>
      <div class="big" id="lr">--.-</div>
    </div>
    <div class="card">
      <div class="row"><div>RR</div><div class="pct" id="rrPct">--%</div></div>
      <div class="big" id="rr">--.-</div>
    </div>
  </div>

  <div class="card total">
    <div class="row"><div>Total</div></div>
    <div class="big" id="total">--.-</div>
    <div class="sub">Seq: <span id="seq">--</span> · Age: <span id="age">--</span> ms</div>
  </div>

  <div class="card controls">
    <div class="row"><div>Zero / Tare</div></div>
    <div class="btnRow">
      <button data-zero="0">Zero LF (0)</button>
      <button data-zero="1">Zero RF (1)</button>
      <button data-zero="2">Zero LR (2)</button>
      <button data-zero="3">Zero RR (3)</button>
    </div>
    <div class="btnRow">
      <button data-zero="front">Zero Front (0,1)</button>
      <button data-zero="rear">Zero Rear (2,3)</button>
      <button data-zero="all">Zero All</button>
    </div>
  </div>

<script>
  const els = {
    status: document.getElementById('status'),
    seq: document.getElementById('seq'),
    age: document.getElementById('age'),
    lf: document.getElementById('lf'),
    rf: document.getElementById('rf'),
    lr: document.getElementById('lr'),
    rr: document.getElementById('rr'),
    total: document.getElementById('total'),
    lfPct: document.getElementById('lfPct'),
    rfPct: document.getElementById('rfPct'),
    lrPct: document.getElementById('lrPct'),
    rrPct: document.getElementById('rrPct')
  };
  const zeroButtons = Array.from(document.querySelectorAll('button[data-zero]'));

  let lastOk = performance.now();


  function setNums(d){
    els.seq.textContent = d.seq;
      els.lf.textContent = d.lf;
    els.rf.textContent = d.rf;
    els.lr.textContent = d.lr;
    els.rr.textContent = d.rr;
    els.total.textContent = d.total;
    els.lfPct.textContent = d.lfPct + "%";
    els.rfPct.textContent = d.rfPct + "%";
    els.lrPct.textContent = d.lrPct + "%";
    els.rrPct.textContent = d.rrPct + "%";

    

    // "age" just shows how stale the data is based on ESP timestamp
    const now = Date.now();
    // We don't have ESP epoch time here; just show how long since last successful fetch
    els.age.textContent = Math.round(performance.now() - lastOk);
  }

  async function doZero(target){
    const btn = zeroButtons.find((b)=> b.dataset.zero === target);
    if (btn) btn.disabled = true;
    els.status.textContent = "Zeroing " + target + "...";
    try{
      const r = await fetch('/tare?target=' + encodeURIComponent(target), {
        method:'POST',
        cache:'no-store'
      });
      if(!r.ok) throw new Error("HTTP " + r.status);
      els.status.textContent = "Zeroed " + target;
      tick();
    }catch(e){
      els.status.textContent = "Zero failed (" + target + ")";
    }finally{
      if (btn) btn.disabled = false;
    }
  }

  zeroButtons.forEach((btn)=>{
    btn.addEventListener('click', ()=> doZero(btn.dataset.zero));
  });

  async function tick(){
    try{
      const r = await fetch('/data', { cache:'no-store' });
      if(!r.ok) throw new Error("HTTP " + r.status);
      const d = await r.json();
      lastOk = performance.now();
      els.status.textContent = "Live";
      setNums(d);
    }catch(e){
      const dt = performance.now() - lastOk;
      els.status.textContent = (dt > 10000)
        ? "Waiting for scales…"
        : "Reconnecting…";
    }
  }

  setInterval(tick,500);
  tick();
</script>
</body>
</html>
)HTML";

// ======================= HTTP Handlers =======================
static void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

static void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", jsonPayload);
}

static void queueTare(uint8_t index) {
  if (index < (sizeof(scales) / sizeof(scales[0]))) {
    tarePending[index] = true;
  }
}

static void handleTare() {
  String target = server.arg("target");
  bool handled = true;

  if (target == "all") {
    for (uint8_t i = 0; i < (sizeof(scales) / sizeof(scales[0])); i++) {
      queueTare(i);
    }
  } else if (target == "front") {
    queueTare(0);
    queueTare(1);
  } else if (target == "rear") {
    queueTare(2);
    queueTare(3);
  } else if (target.length() == 1 && isDigit(target[0])) {
    queueTare(target.toInt());
  } else {
    handled = false;
  }

  if (!handled) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid target\"}");
    return;
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

static void setupRoutes() {
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/tare", HTTP_POST, handleTare);
  server.begin();
  Serial.println("[Web] Server started on port 80.");
}

void setup() {
  Wire.begin(D4,D5); // SDA, SCL for ESP32
  oled.begin();

  Serial.begin(115200);
  delay(2000);
  Serial.println("Serial Open");
  
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  if(!connectToTrailerWifi()){
    startFallbackAP();
  }
  setupRoutes();

  for(int i = 0; i<(sizeof(scales)/sizeof(scales[0])); i++){
    scales[i].begin();
    scales[i].tare();
    scales[i].setScale(scaleFactor);
  }
}

void UpdateWeights(){
  oled.clear();
  totalWeight = 0;
  for(int i = 0; i < (sizeof(scales)/sizeof(scales[0])); i++){
        weights[i] = scales[i].getWeight(4);
        totalWeight += weights[i];
   
  }

  if (totalWeight > 0) {
    crossWeight = (weights[0] + weights[3]) / totalWeight * 100;  // times 100 to get percent
    for(int i = 0; i < (sizeof(weights)/sizeof(weights[0])); i++){
      percents[i] = weights[i] / totalWeight * 100;
    }
  } else {
    crossWeight = 0;
    for(int i = 0; i < (sizeof(weights)/sizeof(weights[0])); i++){
      percents[i] = 0;
    }
  }
  Serial.println("Bout to try to write some json");
 String json = "{";
json += "\"seq\":" + String(seq);
json += ",\"ts_ms\":" + String(ts_ms);
json += ",\"lf\":" + FormatWeight(weights[0],16);
json += ",\"rf\":" + FormatWeight(weights[1],16);
json += ",\"lr\":" + FormatWeight(weights[2],16);
json += ",\"rr\":" + FormatWeight(weights[3],16);
json += ",\"lfPct\":" + FormatWeight(percents[0],16);
json += ",\"rfPct\":" + FormatWeight(percents[1],16);
json += ",\"lrPct\":" + FormatWeight(percents[2],16);
json += ",\"rrPct\":" + FormatWeight(percents[3],16);
json += ",\"total\":" + FormatWeight(totalWeight,8);
json += ",\"cross\":" + FormatWeight(crossWeight,8);
json += "}";

  jsonPayload = json;

  oled.drawString(0,0,(FormatWeight(weights[0],16) + "lbs").c_str()); 
  oled.drawString(0,11,(FormatWeight(percents[0],8) + "%").c_str()); 
  oled.drawString(80,0,(FormatWeight(weights[1],16) + "lbs").c_str());
  oled.drawString(80,11,(FormatWeight(percents[1],8) + "%").c_str()); 
  oled.drawString(0,47,(FormatWeight(weights[2],16) + "lbs").c_str()); 
  oled.drawString(0,56,(FormatWeight(percents[2],8) + "%").c_str()); 
  oled.drawString(80,47,(FormatWeight(weights[3],16) + "lbs").c_str());
  oled.drawString(80,56,(FormatWeight(percents[3],8) + "%").c_str());
  oled.drawString(37,26,(FormatWeight(totalWeight,8) + "lbs").c_str());
  oled.drawString(37, 37,(FormatWeight(crossWeight,8) + "%").c_str()); 
  oled.display();
}

String FormatWeight(float weight,int size){
  char buf[size];
  snprintf(buf, sizeof(buf), "%3.2f", weight);
  return String(buf);
}

void loop() { 
  server.handleClient();
  bool ranTare = false;
  for(int i = 0; i < (sizeof(scales)/sizeof(scales[0])); i++){
    if(tarePending[i]){
      scales[i].tare();
      tarePending[i] = false;
      ranTare = true;
      break;
    }
  }

  // Prioritize queued tare work: skip normal weight updates on loop passes
  // where a tare was executed so tare batches complete faster.
  if (ranTare) {
    return;
  }

  const uint32_t now = millis();
  if(now - lastSensorUpdate >= SENSOR_UPDATE_MS){
    lastSensorUpdate = now;
    UpdateWeights();
  }
  //delay(200);
}
