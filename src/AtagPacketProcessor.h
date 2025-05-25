#pragma once

#include <Arduino.h>
#include <MQTT.h>
#include <InfluxDbClient.h>
#include <map>
#include "Integrator.h"

/**
 * @brief Processes ATAG HR5000 packets and publishes data to MQTT/InfluxDB
 * 
 * This class handles the parsing and processing of different ATAG packet types,
 * extracting sensor data and publishing it to both MQTT topics and InfluxDB.
 */
class AtagPacketProcessor {
public:
    /**
     * @brief Construct a new ATAG Packet Processor
     * 
     * @param mqttClient Reference to MQTT client for publishing
     * @param influxClient Reference to InfluxDB client for data storage
     * @param maxPower Maximum power rating of the boiler in kW
     */
    AtagPacketProcessor(MQTTClient& mqttClient, InfluxDBClient& influxClient, float maxPower);

    /**
     * @brief Process a complete ATAG packet
     * 
     * @param buffer Packet data buffer
     * @param size Size of the packet in bytes
     */
    void processPacket(const uint8_t* buffer, size_t size);

    /**
     * @brief Get the current integrated energy value
     * 
     * @return Current energy in kWh
     */
    float getCurrentEnergy() const { return powerToEnergy.getResult() / 3600.0f; }

    /**
     * @brief Reset the energy integrator
     */
    void resetEnergyIntegrator() { powerToEnergy.reset(); }

private:
    // MQTT topics namespace
    struct Topics {
        static constexpr const char* online = "atagbridge/online";
        static constexpr const char* flowTemperature = "atagbridge/temperature/flow";
        static constexpr const char* returnTemperature = "atagbridge/temperature/return";
        static constexpr const char* differenceTemperature = "atagbridge/temperature/difference";
        static constexpr const char* hotWaterTemperature = "atagbridge/temperature/hotwater";
        static constexpr const char* outsideTemperature = "atagbridge/temperature/outside";
        static constexpr const char* exhaustTemperature = "atagbridge/temperature/exhaust";
        static constexpr const char* nominalTemperature = "atagbridge/temperature/nominal";
        static constexpr const char* pressure = "atagbridge/pressure";
        static constexpr const char* power = "atagbridge/power";
        static constexpr const char* energy = "atagbridge/energy";
        static constexpr const char* state = "atagbridge/state";
        static constexpr const char* pump = "atagbridge/toggles/pump";
        static constexpr const char* heating = "atagbridge/toggles/heating";
        static constexpr const char* hotwater = "atagbridge/toggles/hotwater";
    };

    /**
     * @brief Process temperature data (data index 0x03)
     * 
     * @param data Packet data values
     */
    void processTemperatures(const int8_t* data);

    /**
     * @brief Process system state data (data index 0x04)
     * 
     * @param data Packet data values
     */
    void processSystemState(const int8_t* data);

    /**
     * @brief Process pressure data (data index 0x06)
     * 
     * @param data Packet data values
     */
    void processPressure(const int8_t* data);

    /**
     * @brief Process power and operation flags (data index 0x07)
     * 
     * @param data Packet data values
     */
    void processPower(const int8_t* data);

    /**
     * @brief Write data point to InfluxDB
     * 
     * @param point Data point to write
     */
    void influxWrite(const Point& point);

    MQTTClient& mqtt;
    InfluxDBClient& influx;
    float maxPower;
    Integrator powerToEnergy;

    // System state mapping
    static const std::map<uint8_t, const char*> stateMap;
};