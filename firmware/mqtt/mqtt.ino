// MQTT sensor node for the ESP8266 (NodeMCU) with Home Assistant auto-discovery.
//
// Publishes readings straight to an MQTT broker -- no Go server in the middle.
// On (re)connect it sends retained Home Assistant MQTT-discovery configs, so
// rssi / heap / uptime show up automatically as sensors under a "NodeMCU node-1"
// device in Home Assistant, no YAML required. A Last-Will message marks the node
// offline in HA if it drops off the network.
//
// Requires the PubSubClient library:
//   arduino-cli lib install PubSubClient      (or: make deps)
//
// Broker + identity are injected as -D defines (see Makefile / .env):
//   MQTT_HOST MQTT_PORT MQTT_USER MQTT_PASS DEVICE_ID

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#ifndef WIFI_SSID
#define WIFI_SSID "unset-ssid"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "unset-pass"
#endif
#ifndef MQTT_HOST
#define MQTT_HOST "192.168.1.50"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef MQTT_USER
#define MQTT_USER ""
#endif
#ifndef MQTT_PASS
#define MQTT_PASS ""
#endif
#ifndef DEVICE_ID
#define DEVICE_ID "node-1"
#endif

const char* SSID = WIFI_SSID;
const char* PASS = WIFI_PASS;

static const uint32_t PUBLISH_INTERVAL_MS = 5000;

WiFiClient net;
PubSubClient mqtt(net);

String T_STATE;    // nodemcu/<id>/state   (JSON reading)
String T_STATUS;   // nodemcu/<id>/status  (online/offline, LWT, retained)
static uint32_t lastPublish = 0;

static void buildTopics() {
  T_STATE  = String("nodemcu/") + DEVICE_ID + "/state";
  T_STATUS = String("nodemcu/") + DEVICE_ID + "/status";
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_ID);
  WiFi.begin(SSID, PASS);
  Serial.print("wifi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.print(" ok, ip="); Serial.println(WiFi.localIP());
}

// One Home Assistant discovery config per sensor, retained so HA can pick it up
// whenever it (re)starts. Uses HA's abbreviated keys to stay compact.
static void publishDiscovery(const char* key, const char* name,
                             const char* unit, const char* devClass,
                             const char* valueTemplate) {
  String topic = String("homeassistant/sensor/") + DEVICE_ID + "_" + key + "/config";
  String payload = String("{")
    + "\"name\":\"" + name + "\","
    + "\"uniq_id\":\"" + DEVICE_ID + "_" + key + "\","
    + "\"stat_t\":\"" + T_STATE + "\","
    + "\"avty_t\":\"" + T_STATUS + "\","
    + "\"val_tpl\":\"" + valueTemplate + "\",";
  if (unit[0])     payload += String("\"unit_of_meas\":\"") + unit + "\",";
  if (devClass[0]) payload += String("\"dev_cla\":\"") + devClass + "\",";
  payload += String("\"dev\":{\"ids\":[\"") + DEVICE_ID + "\"],"
    + "\"name\":\"NodeMCU " + DEVICE_ID + "\","
    + "\"mdl\":\"ESP8266\",\"mf\":\"Espressif\"}}";
  mqtt.publish(topic.c_str(), payload.c_str(), true);   // retained
}

static void announce() {
  mqtt.publish(T_STATUS.c_str(), "online", true);       // retained availability
  publishDiscovery("rssi",   "RSSI",      "dBm", "signal_strength", "{{ value_json.rssi }}");
  publishDiscovery("heap",   "Free heap", "B",   "data_size",       "{{ value_json.heap }}");
  publishDiscovery("uptime", "Uptime",    "s",   "duration",        "{{ (value_json.uptime_ms / 1000) | int }}");
}

static bool connectMQTT() {
  if (mqtt.connected()) return true;
  Serial.print("mqtt connecting to " MQTT_HOST "...");
  String clientId = String("nodemcu-") + DEVICE_ID;
  bool ok;
  // Last-Will: broker retains "offline" on T_STATUS if we disconnect ungracefully.
  if (strlen(MQTT_USER) > 0)
    ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS,
                      T_STATUS.c_str(), 0, true, "offline");
  else
    ok = mqtt.connect(clientId.c_str(), nullptr, nullptr,
                      T_STATUS.c_str(), 0, true, "offline");
  if (ok) { Serial.println(" ok"); announce(); }
  else    { Serial.printf(" failed rc=%d\n", mqtt.state()); }
  return ok;
}

static void publishReading() {
  char body[160];
  snprintf(body, sizeof(body),
    "{\"device\":\"" DEVICE_ID "\",\"uptime_ms\":%lu,\"rssi\":%ld,\"heap\":%u}",
    millis(), (long) WiFi.RSSI(), ESP.getFreeHeap());
  bool ok = mqtt.publish(T_STATE.c_str(), body);
  Serial.printf("pub %s -> %s\n", T_STATE.c_str(), ok ? "ok" : "FAIL");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n" DEVICE_ID ": MQTT node (Home Assistant auto-discovery)");
  buildTopics();
  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(512);   // HA discovery configs exceed PubSubClient's 256B default
}

void loop() {
  if (!connectMQTT()) { delay(2000); return; }
  mqtt.loop();
  if (millis() - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = millis();
    publishReading();
  }
}
