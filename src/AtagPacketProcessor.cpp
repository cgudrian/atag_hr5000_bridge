#include "AtagPacketProcessor.h"

// Static member initialization
const std::map<uint8_t, const char*> AtagPacketProcessor::stateMap = {
    {0, "AUS"},
    {1, "LZ"},
    {2, "ZZ"},
    {3, "HB"},
    {4, "WW"},
    {5, "KV"},
    {6, "RT"},
    {7, "NH"},
    {8, "NW"},
    {9, "KT"},
};

AtagPacketProcessor::AtagPacketProcessor(MQTTClient& mqttClient, InfluxDBClient& influxClient, float maxPower)
    : mqtt(mqttClient), influx(influxClient), maxPower(maxPower) {
}

void AtagPacketProcessor::processPacket(const uint8_t* buffer, size_t size) {
    if (buffer[0] == 0x41 && size == 30) {
        // Extract data values (signed 8-bit integers)
        const int8_t* val = reinterpret_cast<const int8_t*>(&buffer[19]);
        uint8_t dataIndex = buffer[18];

        // Store raw data in InfluxDB
        Point raw("raw");
        raw.addTag("index", String(dataIndex, HEX));
        for (int i = 0; i < 8; i++) {
            raw.addField("val" + String(i), val[i]);
        }
        influxWrite(raw);

        // Process data based on index
        switch (dataIndex) {
            case 0x03:
                processTemperatures(val);
                break;
            case 0x04:
                processSystemState(val);
                break;
            case 0x06:
                processPressure(val);
                break;
            case 0x07:
                processPower(val);
                break;
        }
    } else if (buffer[0] == 0x71 && size == 41) {
        // 0x71 packets are not yet decoded
        // Placeholder for future implementation
    }
}

void AtagPacketProcessor::processTemperatures(const int8_t* val) {
    Point temperatures("temperatures");
    temperatures.addField("vorlauf", val[0]);
    temperatures.addField("rücklauf", val[1]);
    temperatures.addField("warmwasser", val[2]);
    temperatures.addField("außen", static_cast<int8_t>(val[3]));
    temperatures.addField("abgas", val[4]);
    influxWrite(temperatures);

    mqtt.publish(Topics::flowTemperature, String(val[0]));
    mqtt.publish(Topics::returnTemperature, String(val[1]));
    mqtt.publish(Topics::differenceTemperature, String(val[0] - val[1]));
    mqtt.publish(Topics::hotWaterTemperature, String(val[2]));
    mqtt.publish(Topics::outsideTemperature, String(val[3]));
    mqtt.publish(Topics::exhaustTemperature, String(val[4]));
    mqtt.publish(Topics::nominalTemperature, String(val[7]));
}

void AtagPacketProcessor::processSystemState(const int8_t* val) {
    auto v = val[6];
    auto it = stateMap.find(v);
    
    if (it != stateMap.end()) {
        mqtt.publish(Topics::state, it->second);
    } else {
        mqtt.publish(Topics::state, String(v));
    }
}

void AtagPacketProcessor::processPressure(const int8_t* val) {
    Point pressures("pressures");
    auto p = val[7] / 10.0f;
    pressures.addField("anlage", p, 1);
    influxWrite(pressures);

    mqtt.publish(Topics::pressure, String(p));
}

void AtagPacketProcessor::processPower(const int8_t* val) {
    // Calculate power in kW
    auto power = maxPower * val[6] / 100.0f;
    mqtt.publish(Topics::power, String(power));

    // Integrate power to calculate energy and convert to kWh
    auto energy = powerToEnergy.update(power) / 3600.0f;
    mqtt.publish(Topics::energy, String(energy));

    // Process operation state flags
    auto bits = val[7];
    mqtt.publish(Topics::pump, String((bits & 1) ? 1 : 0));
    mqtt.publish(Topics::heating, String((bits & 2) ? 1 : 0));
    mqtt.publish(Topics::hotwater, String((bits & 4) ? 1 : 0));
}

void AtagPacketProcessor::influxWrite(const Point& point) {
    // InfluxDB writePoint expects non-const reference, so we need to cast
    Point& nonConstPoint = const_cast<Point&>(point);
    if (!influx.writePoint(nonConstPoint)) {
        Serial.print("InfluxDB write failed: ");
        Serial.println(influx.getLastErrorMessage());
    }
}