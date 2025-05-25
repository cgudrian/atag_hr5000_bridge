# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Git Configuration

- **Main branch for PRs**: `claude` (not `master`)
- Always target the `claude` branch when creating pull requests

## Build and Development Commands

- Build project: `platformio run`
- Upload to device: `platformio run --target upload`
- Monitor serial output: `platformio device monitor`
- Clean build: `platformio run --target clean`
- **Run tests**: `platformio test -e test`

## Project Architecture

This is an ESP8266-based bridge that interfaces with ATAG HR5000 heating systems using RS485 communication. The project bridges ATAG boiler data to MQTT and InfluxDB for monitoring and home automation.

### Core Components

- **RS485 Communication**: Uses SoftwareSerial on pins 12-14 to communicate with ATAG boiler via RS485
- **MQTT Publishing**: Publishes sensor data to MQTT topics under `atagbridge/` prefix
- **InfluxDB Integration**: Stores time-series data for historical tracking
- **Web Interface**: Built using ESP8266 IoT Framework for configuration
- **Configuration Management**: Uses JSON-based config (config.json) for runtime settings

### Data Flow

1. RS485 packets received from ATAG boiler (addresses 0x41, 0x71)
2. Packet parsing extracts temperatures, pressure, power, and operation states
3. Data published to MQTT topics and written to InfluxDB
4. Power integration calculates cumulative energy consumption
5. Web dumper on port 4711 provides raw packet debugging

### Key Libraries

- ESP8266 IoT Framework: Web interface and configuration management
- ESPAsyncWebServer: HTTP server
- ESP8266 Influxdb: Time-series database client
- MQTT: Message broker communication
- SoftwareSerial: RS485 communication

### Configuration Files

- `config.json`: Runtime configuration schema (hostname, MQTT broker, InfluxDB credentials)
- `dashboard.json`: Dashboard widget definitions
- `platformio.ini`: Build configuration and dependencies