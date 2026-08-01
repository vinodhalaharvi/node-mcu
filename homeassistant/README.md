# homeassistant

Home Assistant (Container) for viewing the node-mcu MQTT sensors.

    cd homeassistant
    docker compose up -d          # first boot takes ~1-2 min (large image pull)
    # open http://localhost:8123  -> create your account

Then add the MQTT integration:
  Settings -> Devices & Services -> Add Integration -> MQTT
    Broker:   192.168.68.139   (this host's LAN IP, same broker the node uses)
    Port:     1883
    Username/Password: leave blank (anonymous broker)

The retained discovery configs the node published make a "NodeMCU node-1"
device appear automatically with RSSI / Free heap / Uptime sensors.

Runtime config under config/ is gitignored.
