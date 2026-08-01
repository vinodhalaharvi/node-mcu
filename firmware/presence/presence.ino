// Wi-Fi presence sniffer for the ESP8266 (NodeMCU).
//
// The ESP8266 has no Bluetooth, but its Wi-Fi radio can run in promiscuous
// (monitor) mode. Phones and laptops periodically broadcast 802.11 "probe
// request" frames while scanning for networks; each carries the sender's MAC
// address. By counting the UNIQUE MACs we hear over a short window we get a
// rough "how many devices are nearby" presence signal -- fully passive, no
// pairing, no external sensor.
//
// Promiscuous mode and a normal Wi-Fi connection can't run at once, so each
// cycle is two phases: (1) sniff + channel-hop for a window, tallying unique
// devices; (2) leave promiscuous mode, connect, POST the tally, disconnect.

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

extern "C" {
  #include "user_interface.h"
}

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

const char* SSID = WIFI_SSID;
const char* PASS = WIFI_PASS;
const char* URL  = POST_URL;

// ---- tuning ----
static const uint32_t SNIFF_WINDOW_MS  = 12000;  // total sniff time per cycle
static const uint32_t CHANNEL_DWELL_MS = 300;    // dwell per channel while hopping
static const uint8_t  MAX_CHANNEL      = 13;     // 1..13 (region dependent)
static const uint8_t  MAX_DEVICES      = 80;     // cap on unique MACs tracked

// ---- promiscuous RX buffer layout (Espressif SDK) ----
struct RxControl {
  signed   rssi:8;
  unsigned rate:4;
  unsigned is_group:1;
  unsigned:1;
  unsigned sig_mode:2;
  unsigned legacy_length:12;
  unsigned damatch0:1;
  unsigned damatch1:1;
  unsigned bssidmatch0:1;
  unsigned bssidmatch1:1;
  unsigned MCS:7;
  unsigned CWB:1;
  unsigned HT_length:16;
  unsigned Smoothing:1;
  unsigned Not_Sounding:1;
  unsigned:1;
  unsigned Aggregation:1;
  unsigned STBC:2;
  unsigned FEC_CODING:1;
  unsigned SGI:1;
  unsigned rxend_state:8;
  unsigned ampdu_cnt:8;
  unsigned channel:4;
  unsigned:12;
};

struct SnifferPacket {
  struct RxControl rx_ctrl;
  uint8_t  data[112];   // 802.11 frame: MAC header + start of body
  uint16_t cnt;
  uint16_t len;
};

// ---- unique-device table for the current window ----
static uint8_t macTable[MAX_DEVICES][6];
static int8_t  macRssi[MAX_DEVICES];
static uint8_t macCount = 0;
static int8_t  strongestRssi = -128;   // sentinel: nothing heard yet

static void resetWindow() {
  macCount = 0;
  strongestRssi = -128;
}

static bool sameMac(const uint8_t* a, const uint8_t* b) {
  for (int i = 0; i < 6; i++) if (a[i] != b[i]) return false;
  return true;
}

static void recordDevice(const uint8_t* mac, int8_t rssi) {
  if (rssi > strongestRssi) strongestRssi = rssi;
  for (uint8_t i = 0; i < macCount; i++) {
    if (sameMac(macTable[i], mac)) {          // already counted this window
      if (rssi > macRssi[i]) macRssi[i] = rssi;
      return;
    }
  }
  if (macCount < MAX_DEVICES) {
    memcpy(macTable[macCount], mac, 6);
    macRssi[macCount] = rssi;
    macCount++;
  }
}

// SDK calls this for every sniffed frame. Keep it short; runs from IRAM.
static void IRAM_ATTR snifferCb(uint8_t* buf, uint16_t len) {
  if (len != 128) return;                      // want single management frames
  const SnifferPacket* p = (const SnifferPacket*) buf;
  // Frame Control byte 0 = version(2) | type(2)<<2 | subtype(4)<<4.
  // Probe Request = mgmt (type 0), subtype 4  ->  0x40.
  if (p->data[0] != 0x40) return;
  // Source MAC is Address 2, at offset 10 of the 802.11 MAC header.
  recordDevice(&p->data[10], (int8_t) p->rx_ctrl.rssi);
}

static void sniffWindow() {
  resetWindow();
  WiFi.disconnect();
  wifi_set_opmode(STATION_MODE);
  wifi_set_promiscuous_rx_cb(snifferCb);
  wifi_promiscuous_enable(1);

  uint32_t start = millis();
  uint8_t  channel = 1;
  while (millis() - start < SNIFF_WINDOW_MS) {
    wifi_set_channel(channel);
    delay(CHANNEL_DWELL_MS);                    // yields to the SDK/WDT
    if (++channel > MAX_CHANNEL) channel = 1;
  }

  wifi_promiscuous_enable(0);
  wifi_set_promiscuous_rx_cb(NULL);
}

static bool connectWiFi(uint32_t timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

static void report(uint8_t devices, int8_t strongest) {
  if (!connectWiFi(15000)) {
    Serial.println("wifi connect failed; skipping POST");
    return;
  }
  WiFiClient client;
  HTTPClient http;
  http.begin(client, URL);
  http.addHeader("Content-Type", "application/json");

  char body[192];
  snprintf(body, sizeof(body),
    "{\"device\":\"node-1\",\"uptime_ms\":%lu,\"devices_seen\":%u,"
    "\"strongest_rssi\":%d,\"window_ms\":%lu}",
    millis(), devices, strongest, SNIFF_WINDOW_MS);

  int code = http.POST(body);
  Serial.printf("POST -> %d  (devices=%u strongest=%d dBm)\n",
                code, devices, strongest);
  http.end();
  WiFi.disconnect();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\npresence sniffer: counting nearby probe requests");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
}

void loop() {
  sniffWindow();
  uint8_t devices   = macCount;
  int8_t  strongest = strongestRssi;
  Serial.printf("window done: %u unique device(s), strongest %d dBm\n",
                devices, strongest);
  report(devices, strongest);
  delay(1000);
}
