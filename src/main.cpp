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

#include "Integrator.h"
#include "AtagPacketProcessor.h"

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

// Packet processor for handling ATAG protocol data
static AtagPacketProcessor* packetProcessor = nullptr;

// MQTT topics are now handled by AtagPacketProcessor
namespace {
    constexpr const char* onlineTopic = MQTT_PREFIX "/online";
}

static void mqttConnect()
{
    Serial.print("Connecting to MQTT broker...");
    if (mqtt.connect(cfg.hostname)) {
        Serial.println("OK.");
        mqtt.publish(onlineTopic, "1", true, 1);
    } else {
        Serial.println("FAILED.");
    }
}

static void mqttDisconnect()
{
    mqtt.publish(onlineTopic, "0", true, 1);
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
        mqtt.setWill(onlineTopic, "0", 1, true);
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

    timeSync(configManager.data.timezone, "de.pool.ntp.org", "pool.ntp.org");

    mqtt.begin(net);
    mqttConnect();

    // Initialize packet processor
    packetProcessor = new AtagPacketProcessor(mqtt, Influx, MAX_POWER);

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

    // Use the packet processor to handle data extraction and publishing
    if (packetProcessor) {
        packetProcessor->processPacket(buffer, packetSize);
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
