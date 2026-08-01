# node-mcu

ESP8266 (NodeMCU) sensor node that posts JSON readings over Wi-Fi to a small
Go ingest server. First step of a Rust-firmware + Go/Weft controller pipeline.

The node reports a genuine onboard measurement — Wi-Fi signal strength
(`rssi`, in dBm) to your router — alongside basic chip telemetry (`uptime_ms`
and free `heap`). RSSI changes as the board physically moves, so it doubles as
a crude presence/motion signal with no external sensor wired.

## Layout
- `server/`   — Go HTTP server, receives readings on `POST /readings`
- `firmware/` — NodeMCU ESP8266 Arduino sketch (`firmware/node/node.ino`)
- `Makefile`  — build / flash / monitor / server helpers
- `.env`      — Wi-Fi credentials + server URL (git-ignored, never committed)

## Prerequisites
- [`arduino-cli`](https://arduino.github.io/arduino-cli/) with the ESP8266 core:

      arduino-cli config init
      arduino-cli config add board_manager.additional_urls \
        https://arduino.esp8266.com/stable/package_esp8266com_index.json
      arduino-cli core update-index
      arduino-cli core install esp8266:esp8266

- Go (for the ingest server)
- A NodeMCU / ESP8266 board on a USB serial port

## Configure
Create a `.env` in the repo root (it is git-ignored):

    WIFI_SSID=your-network
    WIFI_PASS=your-password
    POST_URL=http://192.168.1.50:8080/readings

Point `POST_URL` at the LAN IP of the machine running the Go server. The
Makefile injects these as `-D` defines at compile time, so credentials never
live in the source. `make env` prints what the build will use (password
redacted).

## Run the server
    make server          # or: cd server && go run .

Listens on `:8080` and logs each reading:

    got: {"device":"node-1","uptime_ms":16678,"rssi":-47,"heap":49712}

## Build, flash, monitor the node
    make build           # compile with injected creds
    make flash           # compile + upload to the board
    make monitor         # open serial @115200 (Ctrl-] to exit)
    make run             # flash then monitor

Override the serial port if auto-detect misses it:

    make flash PORT=/dev/cu.usbserial-XXXX

## What you'll see
On the serial monitor:

    POST -> 200  (rssi=-40 dBm)

Walk the board around the room: `rssi` climbs toward ~-40 dBm near the router
and drops past -80 dBm across the house. The same `rssi` field lands in the
server log, giving you signal-strength-over-time — a real sensor pipeline with
nothing but the chip itself.
