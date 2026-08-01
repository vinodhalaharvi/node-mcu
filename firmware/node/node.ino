#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Injected at compile time via -D flags (see Makefile).
// Fallbacks let the sketch still compile if you forget.
#ifndef WIFI_SSID
#define WIFI_SSID "unset-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "unset-pass"
#endif
#ifndef POST_URL
#define POST_URL "http://192.168.1.50:8080/readings"
#endif

const char* SSID = WIFI_SSID;
const char* PASS = WIFI_PASS;
const char* URL  = POST_URL;

void setup() {
  Serial.begin(115200);
  WiFi.begin(SSID, PASS);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(WiFi.localIP());
}

void loop() {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, URL);
  http.addHeader("Content-Type", "application/json");

  char body[128];
  snprintf(body, sizeof(body),
    "{\"device\":\"node-1\",\"uptime_ms\":%lu,\"value\":%d}",
    millis(), analogRead(A0));

  int code = http.POST(body);
  Serial.printf("POST -> %d\n", code);
  http.end();
  delay(5000);
}
