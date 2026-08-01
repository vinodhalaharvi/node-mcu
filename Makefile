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

PORT     ?= /dev/cu.usbserial-0001
OTA_HOST ?= node-1.local

# quote-escape creds and pass them as -D defines at compile time
BUILD_PROPS = --build-property "compiler.cpp.extra_flags=\
-DWIFI_SSID=\"$(WIFI_SSID)\" \
-DWIFI_PASS=\"$(WIFI_PASS)\" \
-DPOST_URL=\"$(POST_URL)\" \
-DOTA_PASS=\"$(OTA_PASS)\""

.PHONY: build flash ota monitor run server clean env help

## build: compile the firmware with injected credentials
build:
	arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPS) $(SKETCH)

## flash: build then upload to the board over USB
flash: build
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH)

## ota: build then upload over Wi-Fi (board must already run firmware/web)
ota: build
	arduino-cli upload -p $(OTA_HOST) --fqbn $(FQBN) $(SKETCH) $(if $(OTA_PASS),--upload-field password="$(OTA_PASS)",)

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
	rm -rf $(SKETCH)/build

## help: list targets
help:
	grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //'
