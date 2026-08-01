# node-mcu

ESP8266 (NodeMCU) sensor node posting JSON over Wi-Fi to a Go ingest server.
First step of a Rust-firmware + Go/Weft controller pipeline.

## Layout
- `server/`   — Go HTTP server that receives readings (`go run .`)
- `firmware/` — NodeMCU ESP8266 Arduino sketch

## Run the server
    cd server && go run .

## Flash the node
    arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 firmware/node
    arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp8266:esp8266:nodemcuv2 firmware/node
