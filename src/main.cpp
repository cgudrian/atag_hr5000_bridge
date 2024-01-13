#include <Arduino.h>

#include <configManager.h>
#include <dashboard.h>
#include <InfluxDbClient.h>
#include <LittleFS.h>
#include <MQTT.h>
#include <SoftwareSerial.h>
#include <updater.h>
#include <webServer.h>
#include <WiFiManager.h>

#include <map>

// TODO: turn into configuration option
#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#ifndef MQTT_PREFIX
#define MQTT_PREFIX "atagbridge"
#endif

static auto const MAX_POWER = 21.6f;

static int const RS485_DE = 12;
static int const RS485_DI = 13;
static int const RS485_RO = 14;

static SoftwareSerial RS485;
static InfluxDBClient Influx;
static WiFiServer Dumper(4711);
static MQTTClient mqtt;
static WiFiClient net;

static auto const& cfg = configManager.data;

namespace topics {
static char const* const online = MQTT_PREFIX "/online";
static char const* const flowTemperature = MQTT_PREFIX "/temperature/flow";
static char const* const returnTemperature = MQTT_PREFIX "/temperature/return";
static char const* const differenceTemperature = MQTT_PREFIX "/temperature/difference";
static char const* const hotWaterTemperature = MQTT_PREFIX "/temperature/hotwater";
static char const* const outsideTemperature = MQTT_PREFIX "/temperature/outside";
static char const* const exhaustTemperature = MQTT_PREFIX "/temperature/exhaust";
static char const* const nominalTemperature = MQTT_PREFIX "/temperature/nominal";
static char const* const pressure = MQTT_PREFIX "/pressure";
static char const* const power = MQTT_PREFIX "/power";
static char const* const energy = MQTT_PREFIX "/energy";
static char const* const state = MQTT_PREFIX "/state";
static char const* const pump = MQTT_PREFIX "/toggles/pump";
static char const* const heating = MQTT_PREFIX "/toggles/heating";
static char const* const hotwater = MQTT_PREFIX "/toggles/hotwater";
}  // namespace topics

static void mqttConnect()
{
    Serial.print("Connecting to MQTT broker...");
    if (mqtt.connect(cfg.hostname)) {
        Serial.println("OK.");
        mqtt.publish(topics::online, "1", true, 1);
    } else {
        Serial.println("FAILED.");
    }
}

static void mqttDisconnect()
{
    mqtt.publish(topics::online, "0", true, 1);
    mqtt.clearWill();
    mqtt.disconnect();
}

static void applyConfiguration()
{
    static String lastHostname;
    static String lastMqttHost;
    static int lastMqttPort;

    Serial.print("Applying Configuration...");

    if (lastMqttHost != cfg.mqttBrokerHost || lastMqttPort != cfg.mqttBrokerPort) {
        Serial.print("MQTT...");
        mqttDisconnect();
        mqtt.setHost(cfg.mqttBrokerHost, cfg.mqttBrokerPort);
    }

    if (lastHostname != cfg.hostname) {
        Serial.print("Hostname...");
        mqttDisconnect();
        mqtt.setWill(topics::online, "0", 1, true);
        WiFi.setHostname(cfg.hostname);
    }

    Serial.print("InfluxDB...");
    Influx.setConnectionParams(cfg.influxdbUrl, cfg.influxdbOrg, cfg.influxdbBucket, cfg.influxdbToken);

    lastHostname = cfg.hostname;
    lastMqttHost = cfg.mqttBrokerHost;
    lastMqttPort = cfg.mqttBrokerPort;

    Serial.println("Done.");
}

void setup()
{
    pinMode(LED_BUILTIN_AUX, OUTPUT);
    digitalWrite(LED_BUILTIN_AUX, HIGH);

    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW);

    Serial.begin(115200);
    Serial.println("Booting...");

    RS485.begin(9600, SWSERIAL_8S1, RS485_RO, RS485_DI, false);

    configManager.begin();

    Serial.println("Current Configuration:");
    Serial.println("----------------------");
    Serial.print("Hostname: ");
    Serial.println(cfg.hostname);
    Serial.print("MQTT Broker: ");
    Serial.print(cfg.mqttBrokerHost);
    Serial.print(":");
    Serial.println(cfg.mqttBrokerPort);

    applyConfiguration();
    configManager.setConfigSaveCallback(applyConfiguration);

    WiFiManager.begin(cfg.projectName);
    WiFi.setAutoReconnect(true);

    LittleFS.begin();
    GUI.begin();
    dash.begin();

    Dumper.begin();

    timeSync(TZ_INFO, "de.pool.ntp.org", "pool.ntp.org");

    mqtt.begin(net);
    mqttConnect();

    digitalWrite(LED_BUILTIN_AUX, LOW);
    Serial.println("Ready.");
}

static void influxWrite(Point& p)
{
    if (!Influx.writePoint(p)) {
        Serial.print("InfluxDB Client error: ");
        Serial.println(Influx.getLastErrorMessage());
    }
}

static WiFiClient dumperClient;

static size_t packetSize;
static uint8_t buffer[128];
static unsigned long lastByteTime;
static bool readingPacket;

class Integrator {
  public:
    float update(float value)
    {
        auto now = millis();

        if (!firstTime) {
            auto deltaT = (now - lastT) / 1000.0;
            // integration with linear interpolation
            result += 0.5 * (lastValue + value) * deltaT;
        } else {
            firstTime = false;
        }

        lastValue = value;
        lastT = now;

        return result;
    }

  private:
    bool firstTime{true};
    decltype(millis()) lastT{};
    float lastValue{};
    float result{};
};

static Integrator powerToEnergy;

static void processPacket()
{
    if (packetSize < 2)
        return;

    if (dumperClient) {
        dumperClient.printf("[%02x]", buffer[0]);
        for (size_t i = 1; i < packetSize; ++i)
            dumperClient.printf(" %02x", buffer[i]);
        dumperClient.print('\n');
    }

    {
        Point packet("packet");
        packet.addTag("address", String(buffer[0], 16));
        packet.addTag("lastByte", String(buffer[packetSize - 1], 16));
        packet.addField("size", packetSize);
        influxWrite(packet);
    }

    if (buffer[0] == 0x41 && packetSize == 30) {
        int8_t* val = reinterpret_cast<int8_t*>(&buffer[19]);

        Point raw("raw");
        raw.addTag("index", String(buffer[18]));
        for (int i = 0; i < 8; ++i)
            raw.addField(String("val") + i, val[i]);
        influxWrite(raw);

        switch (buffer[18]) {
            case 0x3: {
                Point temperatures("temperatures");
                temperatures.addField("vorlauf", val[0]);
                temperatures.addField("rücklauf", val[1]);
                temperatures.addField("warmwasser", val[2]);
                temperatures.addField("außen", static_cast<int8_t>(val[3]));
                temperatures.addField("abgas", val[4]);
                influxWrite(temperatures);

                mqtt.publish(topics::flowTemperature, String(val[0]));
                mqtt.publish(topics::returnTemperature, String(val[1]));
                mqtt.publish(topics::differenceTemperature, String(val[0] - val[1]));
                mqtt.publish(topics::hotWaterTemperature, String(val[2]));
                mqtt.publish(topics::outsideTemperature, String(val[3]));
                mqtt.publish(topics::exhaustTemperature, String(val[4]));
                mqtt.publish(topics::nominalTemperature, String(val[7]));

                break;
            }

            case 0x4: {
                static std::map<uint8_t, char const*> VALUE_MAP = {
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

                auto v = val[6];
                auto s = VALUE_MAP[v];
                if (s)
                    mqtt.publish(topics::state, s);
                else
                    mqtt.publish(topics::state, String(v));

                break;
            }

            case 0x6: {
                Point pressures("pressures");
                auto p = val[7] / 10.0f;
                pressures.addField("anlage", p, 1);
                influxWrite(pressures);

                mqtt.publish(topics::pressure, String(p));

                break;
            }

            case 0x7: {
                // power
                auto power = MAX_POWER * val[6] / 100.0f;
                mqtt.publish(topics::power, String(power));

                // integrate and convert to kWh
                auto energy = powerToEnergy.update(power) / 3600.0f;
                mqtt.publish(topics::energy, String(energy));

                // operation state
                auto bits = val[7];
                mqtt.publish(topics::pump, String((bits & 1) ? 1 : 0));
                mqtt.publish(topics::heating, String((bits & 2) ? 1 : 0));
                mqtt.publish(topics::hotwater, String((bits & 4) ? 1 : 0));
                break;
            }
        }
    } else if (buffer[0] == 0x71 && packetSize == 41) {
    }
}

void loop()
{
    static auto const MQTT_RECONNECT_INTERVAL = 1000;
    static auto nextMqttConnectTime = millis() + MQTT_RECONNECT_INTERVAL;

    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();
    mqtt.loop();

    if (!mqtt.connected() && millis() >= nextMqttConnectTime) {
        mqttConnect();
        nextMqttConnectTime = millis() + MQTT_RECONNECT_INTERVAL;
    }

    if (auto newDumperClient = Dumper.accept())
        dumperClient = std::move(newDumperClient);

    while (RS485.available()) {
        auto b = RS485.read();
        lastByteTime = millis();

        // address byte starts new packet
        if (RS485.readParity()) {
            if (readingPacket)
                processPacket();
            readingPacket = true;
            packetSize = 0;
        }

        // add current byte to buffer
        if (readingPacket && packetSize < sizeof(buffer))
            buffer[packetSize++] = b;
    }

    // process buffer after timeout
    if (readingPacket && (millis() - lastByteTime > cfg.packetTimeout)) {
        processPacket();
        packetSize = 0;
        readingPacket = false;
    }
}
