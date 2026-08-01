# ── config ──────────────────────────────────────────────────
FQBN    ?= esp8266:esp8266:nodemcuv2
SKETCH  ?= firmware/node
BAUD    ?= 115200

# load .env if present (never committed)
ifneq (,$(wildcard .env))
include .env
export
endif

PORT ?= /dev/cu.usbserial-0001

# quote-escape creds and pass them as -D defines at compile time
BUILD_PROPS = --build-property "compiler.cpp.extra_flags=\
-DWIFI_SSID=\"$(WIFI_SSID)\" \
-DWIFI_PASS=\"$(WIFI_PASS)\" \
-DPOST_URL=\"$(POST_URL)\""

.PHONY: build flash monitor run server clean env

## build: compile the firmware with injected credentials
build:
	arduino-cli compile --fqbn $(FQBN) $(BUILD_PROPS) $(SKETCH)

## flash: build then upload to the board
flash: build
	arduino-cli upload -p $(PORT) --fqbn $(FQBN) $(SKETCH)

## monitor: open serial (DTR/RTS off so it doesn't hold reset)
monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD) -c dtr=off -c rts=off

## run: flash then immediately monitor
run: flash monitor

## server: start the Go ingest server
server:
	cd server && go run .

## env: show what creds the build will use (redacts password)
env:
	@echo "SSID = $(WIFI_SSID)"
	@echo "PASS = $(if $(WIFI_PASS),****,<empty>)"
	@echo "URL  = $(POST_URL)"
	@echo "PORT = $(PORT)"

## clean: remove arduino build artifacts
clean:
	rm -rf $(SKETCH)/build

## help: list targets
help:
	@grep -E '^## ' $(MAKEFILE_LIST) | sed 's/## //'

