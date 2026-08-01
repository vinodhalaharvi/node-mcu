// OTA + mDNS + web dashboard for the ESP8266 (NodeMCU).
//
// Quality-of-life upgrade of the sensor node:
//   * ArduinoOTA        -> reflash over Wi-Fi (no USB cable after the first flash)
//   * mDNS              -> reachable as http://node-1.local/
//   * ESP8266WebServer  -> a one-page live dashboard (rssi / heap / uptime)
//
// It also POSTs the same reading JSON to the Go server on an interval, so it's
// a drop-in replacement for firmware/node with extras. OTA + web + POST all
// share the loop, so nothing blocks: the periodic POST is timer-driven, not a
// delay().

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Injected at compile time via -D flags (see Makefile). Fallbacks let the
// sketch still compile if you forget.
#ifndef WIFI_SSID
#define WIFI_SSID "unset-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "unset-pass"
#endif
#ifndef POST_URL
#define POST_URL "http://192.168.1.50:8080/readings"
#endif
#ifndef OTA_PASS
#define OTA_PASS ""          // empty = no OTA password (LAN-only convenience)
#endif

const char* SSID     = WIFI_SSID;
const char* PASS     = WIFI_PASS;
const char* URL      = POST_URL;
const char* HOSTNAME = "node-1";

static const uint32_t POST_INTERVAL_MS = 5000;

ESP8266WebServer server(80);
static uint32_t lastPost = 0;

// Reading JSON shared by the dashboard (/metrics) and the POST to the server.
static void readingJson(char* out, size_t n) {
  snprintf(out, n,
    "{\"device\":\"node-1\",\"uptime_ms\":%lu,\"rssi\":%ld,\"heap\":%u}",
    millis(), (long) WiFi.RSSI(), ESP.getFreeHeap());
}

// GET /metrics -> live JSON (this is what the dashboard polls)
static void handleMetrics() {
  char body[160];
  readingJson(body, sizeof(body));
  server.send(200, "application/json", body);
}

// GET / -> tiny self-refreshing dashboard
static void handleRoot() {
  static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><title>node-1</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  body{font:16px system-ui,sans-serif;margin:2rem;background:#0b1020;color:#e8eefc}
  h1{font-size:1.2rem;margin:0 0 1rem}
  .g{display:grid;grid-template-columns:auto auto;gap:.4rem 1.5rem;max-width:20rem}
  .k{color:#8aa0c8}.v{font-variant-numeric:tabular-nums;text-align:right}
  small{color:#5f7099}
</style></head><body>
<h1>node-1 &middot; live</h1>
<div class="g">
  <div class="k">uptime</div><div class="v" id="uptime">-</div>
  <div class="k">rssi</div><div class="v" id="rssi">-</div>
  <div class="k">free heap</div><div class="v" id="heap">-</div>
</div>
<p><small id="ts">connecting...</small></p>
<script>
async function tick(){
  try{
    const r = await fetch('/metrics'); const d = await r.json();
    document.getElementById('uptime').textContent = (d.uptime_ms/1000).toFixed(0)+' s';
    document.getElementById('rssi').textContent   = d.rssi+' dBm';
    document.getElementById('heap').textContent   = d.heap.toLocaleString()+' B';
    document.getElementById('ts').textContent     = 'updated '+new Date().toLocaleTimeString();
  }catch(e){ document.getElementById('ts').textContent = 'fetch failed'; }
}
tick(); setInterval(tick, 2000);
</script>
</body></html>)HTML";
  server.send_P(200, "text/html", PAGE);
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOSTNAME);
  WiFi.begin(SSID, PASS);
  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.print("ip: "); Serial.println(WiFi.localIP());
}

static void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME);
  if (strlen(OTA_PASS) > 0) ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() { Serial.println("OTA: start"); });
  ArduinoOTA.onEnd([]()   { Serial.println("\nOTA: done"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA error [%u]\n", e); });
  ArduinoOTA.begin();      // also brings up the mDNS responder (node-1.local)
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nnode-1: OTA + mDNS + web dashboard");
  connectWiFi();
  setupOTA();

  server.on("/", handleRoot);
  server.on("/metrics", handleMetrics);
  server.begin();
  MDNS.addService("http", "tcp", 80);   // advertise the dashboard over mDNS
  Serial.println("dashboard: http://node-1.local/");
}

static void postReading() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClient client;
  HTTPClient http;
  http.begin(client, URL);
  http.addHeader("Content-Type", "application/json");
  char body[160];
  readingJson(body, sizeof(body));
  int code = http.POST(body);
  Serial.printf("POST -> %d\n", code);
  http.end();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();

  if (millis() - lastPost >= POST_INTERVAL_MS) {
    lastPost = millis();
    postReading();
  }
}
