# ── config ──────────────────────────────────────────────────
FQBN    ?= esp8266:esp8266:nodemcuv2

# Sketches (override with SKETCH=...):
#   firmware/node      RSSI + heap sensor node (default)
#   firmware/presence  Wi-Fi probe-request presence sniffer
#   firmware/web       OTA + mDNS + web dashboard
#   firmware/mqtt      MQTT publisher w/ Home Assistant auto-discovery
SKETCH  ?= firmware/node
BAUD    ?= 115200

# load .env if present (never committed)
ifneq (,$(wildcard .env))
include .env
export
endif

PORT      ?= /dev/cu.usbserial-0001
OTA_HOST  ?= node-1.local
OTA_PORT  ?= 8266
BUILD_DIR ?= /tmp/node-mcu-build

# node identity + MQTT broker (MQTT_HOST/USER/PASS come from .env)
DEVICE_ID ?= node-1
MQTT_PORT ?= 1883

# espota.py ships with the installed esp8266 core; find it under the arduino data dir
ARDUINO_DATA := $(shell arduino-cli config get directories.data 2>/dev/null)
ESPOTA = $(firstword $(wildcard \
  $(ARDUINO_DATA)/packages/esp8266/hardware/esp8266/*/tools/espota.py \
  $(HOME)/Library/Arduino15/packages/esp8266/hardware/esp8266/*/tools/espota.py \
  $(HOME)/.arduino15/packages/esp8266/hardware/esp8266/*/tools/espota.py))

# quote-escape creds/config and pass them as -D defines at compile time
BUILD_PROPS = --build-property "compiler.cpp.extra_flags=\
-DWIFI_SSID=\"$(WIFI_SSID)\" \
-DWIFI_PASS=\"$(WIFI_PASS)\" \
-DPOST_URL=\"$(POST_URL)\" \
-DOTA_PASS=\"$(OTA_PASS)\" \
-DDEVICE_ID=\"$(DEVICE_ID)\" \
-DMQTT_HOST=\"$(MQTT_HOST)\" \
-DMQTT_PORT=$(MQTT_PORT) \
-DMQTT_USER=\"$(MQTT_USER)\" \
-DMQTT_PASS=\"$(MQTT_PASS)\""

.PHONY: build flash ota ota-build monitor run server deps clean env help

## build: compile the firmware with injected credentials
build:
	arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPS) $(SKETCH)

## flash: build then upload to the board over USB
flash: build
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH)

## ota: build then push over Wi-Fi via espota (board must already run firmware/web)
ota: ota-build
	test -n "$(ESPOTA)" || { echo "espota.py not found - is the esp8266 core installed?"; exit 1; }
	python3 "$(ESPOTA)" -i $(OTA_HOST) -p $(OTA_PORT) $(if $(OTA_PASS),-a "$(OTA_PASS)",) -f "$$(ls $(BUILD_DIR)/*.ino.bin | head -1)"

## ota-build: compile the current sketch into a standalone .bin for OTA
ota-build:
	rm -rf "$(BUILD_DIR)"
	arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPS) --output-dir "$(BUILD_DIR)" $(SKETCH)

## monitor: open serial (DTR/RTS off so it doesn't hold reset)
monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD) -c dtr=off -c rts=off

## run: flash then immediately monitor
run: flash monitor

## server: start the Go ingest server
server:
	cd server && go run .

## deps: install Arduino libraries used by the sketches (PubSubClient for MQTT)
deps:
	arduino-cli lib install PubSubClient

## env: show what creds/config the build will use (redacts passwords)
env:
	@echo "SSID      = $(WIFI_SSID)"
	@echo "PASS      = $(if $(WIFI_PASS),****,<empty>)"
	@echo "URL       = $(POST_URL)"
	@echo "PORT      = $(PORT)"
	@echo "OTA_HOST  = $(OTA_HOST)"
	@echo "OTA_PASS  = $(if $(OTA_PASS),****,<empty>)"
	@echo "DEVICE_ID = $(DEVICE_ID)"
	@echo "MQTT_HOST = $(MQTT_HOST)"
	@echo "MQTT_PORT = $(MQTT_PORT)"
	@echo "MQTT_USER = $(if $(MQTT_USER),$(MQTT_USER),<empty>)"
	@echo "MQTT_PASS = $(if $(MQTT_PASS),****,<empty>)"

## clean: remove arduino build artifacts
clean:
	rm -rf $(SKETCH)/build "$(BUILD_DIR)"

## help: list targets
help:
	grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //'
