# Lightweight GPS tracker

This project contains firmware for a simple GPS tracker based on the ESP32 (core) and SIMCOM module (network communication).

Firmware has two main functionalities. First is obvious from the name, the firmware allows a device to measure real-time GPS positions and send them through the internet via SIMCOM module to MQTT broker.
Second functionality combines actual position with predefined position - waypoints. When the tracker reaches the waypoint, the music starts to play.

> :information_source: The planned usage of this project is a game. Each team gets GPS the tracker as a guide. Teams will follow its instructions, thanks to that they will visit all waypoints and reach the goal.

## Configuration

The config file is located in `data/config.json`. Example configuration:

```json
{
  "general": {
    "tracker-id": 123,
    "accuracy": 100
  },
  "mqtt": {
    "host": "mqtt.broker.com",
    "port": 8883,
    "username": "tracker",
    "password": "password-tracker",
    "topic": "gps-tracker"
  },
  "gsm": {
    "enable": true,
    "apn": "internet",
    "user": "",
    "password": ""
  },
  "gps": {
    "enable": true,
    "fast-fix": true,
    "sampling-rate": 1000,
    "minimal-accuracy": 5
  },
  "waypoints": [
    {
      "id": 1,
      "lat": 50.1268959,
      "lon": 14.42045593,
      "path": "/moses.mp3"
    }
  ]
}
```

## Build & upload

The project uses the PlatformIO tools. So the easiest way how to compile and upload them is to use PIO commands.

- `pio run -t uploadfs` -- creates a filesystem with files in `data/` folder and upload the whole directory to the microcontroller
- `pio run -t upload` -- compiles the code and upload the resulting binary to the microcontroller
- `pio run` -- just compiles the code