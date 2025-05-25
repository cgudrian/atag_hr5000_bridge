# ATAG HR5000 Protocol Documentation

This document describes the RS485 communication protocol used by ATAG HR5000 heating systems, as reverse-engineered from the bridge implementation.

## Physical Layer

- **Interface**: RS485 serial communication
- **Baud Rate**: 9600 bps
- **Data Format**: 8 data bits, 1 stop bit, stick parity
- **Pins**: DE=12, DI=13, RO=14 (ESP8266 NodeMCU)

## Packet Structure

### General Format
All packets follow this structure:
```
[ADDRESS] [DATA...] [CHECKSUM/TERMINATOR]
```

- **Address byte**: Has parity bit set (9th bit) to indicate start of new packet
- **Data bytes**: Variable length payload
- **Packet identification**: Based on address and packet length

### Packet Detection
- New packets are detected when parity bit is set on received byte
- Packet timeout: 100ms (configurable via `packetTimeout`)
- Maximum packet size: 128 bytes

## Known Packet Types

### Type 0x41 (Length: 30 bytes)
Main data packet containing sensor readings and system state.

**Structure:**
```
Byte 0:    Address (0x41)
Bytes 1-17: Unknown/Reserved
Byte 18:   Data Index (determines data type)
Bytes 19-26: Data Values (8 bytes, signed int8)
Bytes 27-29: Unknown/Checksum
```

#### Data Index 0x03 - Temperature Readings
**Data Values (signed int8, °C):**
- `val[0]`: Flow temperature (Vorlauf)
- `val[1]`: Return temperature (Rücklauf) 
- `val[2]`: Hot water temperature (Warmwasser)
- `val[3]`: Outside temperature (Außen)
- `val[4]`: Exhaust temperature (Abgas)
- `val[5]`: Unknown
- `val[6]`: Unknown
- `val[7]`: Nominal temperature (Soll)

**MQTT Topics:**
- `atagbridge/temperature/flow`
- `atagbridge/temperature/return`
- `atagbridge/temperature/difference` (calculated: flow - return)
- `atagbridge/temperature/hotwater`
- `atagbridge/temperature/outside`
- `atagbridge/temperature/exhaust`
- `atagbridge/temperature/nominal`

#### Data Index 0x04 - System State
**Data Values:**
- `val[6]`: System state code (uint8)

**State Codes:**
| Code | State | Description |
|------|-------|-------------|
| 0 | AUS | Off |
| 1 | LZ | Ignition delay |
| 2 | ZZ | Ignition |
| 3 | HB | Heating operation |
| 4 | WW | Hot water operation |
| 5 | KV | Unknown |
| 6 | RT | Unknown |
| 7 | NH | Post-heating |
| 8 | NW | Post-heating water |
| 9 | KT | Unknown |

**MQTT Topic:** `atagbridge/state`

#### Data Index 0x06 - Pressure Reading
**Data Values:**
- `val[7]`: System pressure (uint8, divide by 10 for bar)

**MQTT Topic:** `atagbridge/pressure`

#### Data Index 0x07 - Power and Operation Flags
**Data Values:**
- `val[6]`: Power percentage (0-100%)
- `val[7]`: Operation flags (bitfield)

**Power Calculation:**
```cpp
power_kw = MAX_POWER * val[6] / 100.0f
// MAX_POWER = 21.6 kW
```

**Operation Flags (val[7]):**
- Bit 0: Pump active
- Bit 1: Heating active  
- Bit 2: Hot water active

**MQTT Topics:**
- `atagbridge/power` (calculated power in kW)
- `atagbridge/energy` (integrated energy in kWh)
- `atagbridge/toggles/pump`
- `atagbridge/toggles/heating`
- `atagbridge/toggles/hotwater`

### Type 0x71 (Length: 41 bytes)
**Status:** Packet format identified but not yet decoded.

## Data Processing

### Energy Integration
Power readings are integrated over time to calculate cumulative energy consumption:
```cpp
class Integrator {
    // Linear interpolation integration: ∫(power)dt
    result += 0.5 * (lastValue + value) * deltaT;
}
```

### Raw Data Logging
All raw packet data is logged to InfluxDB:
- `packet` measurement: packet metadata
- `raw` measurement: raw byte values by data index

## Debug Interface

A TCP server on port 4711 provides real-time packet dumps:
```
[41] 00 01 02 ... (hex dump of packets)
```

## Configuration Parameters

- `packetTimeout`: Packet completion timeout (default: 100ms)
- `hostname`: Device hostname for MQTT client ID
- MQTT prefix: `atagbridge` (compile-time constant)

## Implementation Notes

- All temperature values are signed 8-bit integers in Celsius
- Pressure values require division by 10 to get bar units
- Power calculation uses system-specific maximum power rating
- Energy integration assumes linear power changes between readings
- MQTT messages use QoS 0 except for online status (QoS 1, retained)