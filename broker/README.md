# broker

Local MQTT broker (Mosquitto) for the node-mcu MQTT sketch.

    cd broker
    docker compose up -d          # start on :1883
    docker compose logs -f        # watch it
    docker compose down           # stop

Anonymous access on 0.0.0.0:1883 (LAN test setup). Point the firmware at this
host's LAN IP via MQTT_HOST in ../.env. For production, add a password file and
set allow_anonymous false in config/mosquitto.conf.
