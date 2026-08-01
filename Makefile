# ── config ──────────────────────────────────────────────────
FQBN    ?= esp8266:esp8266:nodemcuv2

# Sketches (override with SKETCH=...):
#   firmware/node      RSSI + heap sensor node (default)
#   firmware/presence  Wi-Fi probe-request presence sniffer
#   firmware/web       OTA + mDNS + web dashboard
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

# espota.py ships with the installed esp8266 core; find it under ~/.arduino15
ARDUINO_DATA := $(shell arduino-cli config get directories.data 2>/dev/null)
ESPOTA = $(firstword $(wildcard \
  $(ARDUINO_DATA)/packages/esp8266/hardware/esp8266/*/tools/espota.py \
  $(HOME)/Library/Arduino15/packages/esp8266/hardware/esp8266/*/tools/espota.py \
  $(HOME)/.arduino15/packages/esp8266/hardware/esp8266/*/tools/espota.py))

# quote-escape creds and pass them as -D defines at compile time
BUILD_PROPS = --build-property "compiler.cpp.extra_flags=\
-DWIFI_SSID=\"$(WIFI_SSID)\" \
-DWIFI_PASS=\"$(WIFI_PASS)\" \
-DPOST_URL=\"$(POST_URL)\" \
-DOTA_PASS=\"$(OTA_PASS)\""

.PHONY: build flash ota ota-build monitor run server clean env help

## build: compile the firmware with injected credentials
build:
	arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPS) $(SKETCH)

## flash: build then upload to the board over USB
flash: build
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH)

## ota: build then push over Wi-Fi via espota (board must already run firmware/web)
ota: ota-build
	test -n "$(ESPOTA)" || { echo "espota.py not found under ~/.arduino15 - is the esp8266 core installed?"; exit 1; }
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

## env: show what creds the build will use (redacts passwords)
env:
	@echo "SSID     = $(WIFI_SSID)"
	@echo "PASS     = $(if $(WIFI_PASS),****,<empty>)"
	@echo "URL      = $(POST_URL)"
	@echo "PORT     = $(PORT)"
	@echo "OTA_HOST = $(OTA_HOST)"
	@echo "OTA_PASS = $(if $(OTA_PASS),****,<empty>)"

## clean: remove arduino build artifacts
clean:
	rm -rf $(SKETCH)/build "$(BUILD_DIR)"

## help: list targets
help:
	grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //'
