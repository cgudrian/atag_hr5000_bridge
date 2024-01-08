#include <Arduino.h>
#include <SoftwareSerial.h>
#include "LittleFS.h"

#include <InfluxDbClient.h>

#include "configManager.h"
#include "dashboard.h"
#include "updater.h"
#include "webServer.h"
#include "WiFiManager.h"

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

static constexpr int RS485_DE = 12;
static constexpr int RS485_DI = 13;
static constexpr int RS485_RO = 14;

SoftwareSerial RS485;
InfluxDBClient Influx;
WiFiServer Dumper(4711);

void applyConfiguration()
{
    Influx.setConnectionParams(configManager.data.influxdbUrl,
                               configManager.data.influxdbOrg,
                               configManager.data.influxdbBucket,
                               configManager.data.influxdbToken);

    if (strlen(configManager.data.hostname))
        WiFi.setHostname(configManager.data.hostname);
}

void setup()
{
    pinMode(LED_BUILTIN_AUX, OUTPUT);
    digitalWrite(LED_BUILTIN_AUX, HIGH);

    pinMode(RS485_DE, OUTPUT);
    digitalWrite(RS485_DE, LOW);

    Serial.begin(115200);
    RS485.begin(9600, SWSERIAL_8S1, RS485_RO, RS485_DI, false);

    LittleFS.begin();
    GUI.begin();
    configManager.begin();
    dash.begin();

    WiFiManager.begin(configManager.data.projectName);
    WiFi.setAutoConnect(true);

    applyConfiguration();
    configManager.setConfigSaveCallback(applyConfiguration);

    Dumper.begin();

    timeSync(TZ_INFO, "de.pool.ntp.org", "pool.ntp.org");

    digitalWrite(LED_BUILTIN_AUX, LOW);
    Serial.println("Ready");
}

size_t packetSize;
uint8_t buffer[128];
unsigned long lastByteTime;
bool readingPacket;

void influxWrite(Point& p)
{
    if (!Influx.writePoint(p)) {
        Serial.print("InfluxDB Client error: ");
        Serial.println(Influx.getLastErrorMessage());
    }
}

WiFiClient dumperClient;

void processPacket()
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
        Point raw("raw");
        raw.addTag("index", String(buffer[18]));
        raw.addField("val0", buffer[19]);
        raw.addField("val1", buffer[20]);
        raw.addField("val2", buffer[21]);
        raw.addField("val3", buffer[22]);
        raw.addField("val4", buffer[23]);
        raw.addField("val5", buffer[24]);
        raw.addField("val6", buffer[25]);
        raw.addField("val7", buffer[26]);
        influxWrite(raw);

        switch (buffer[18]) {
            case 0x3: {
                Point temperatures("temperatures");
                temperatures.addField("vorlauf", buffer[19]);
                temperatures.addField("rücklauf", buffer[20]);
                temperatures.addField("warmwasser", buffer[21]);
                temperatures.addField("außen", static_cast<int8_t>(buffer[22]));
                temperatures.addField("abgas", buffer[23]);
                influxWrite(temperatures);
                break;
            }

            case 0x6: {
                Point pressures("pressures");
                pressures.addField("anlage", buffer[26] / 10.0f, 1);
                influxWrite(pressures);
                break;
            }
        }
    } else if (buffer[0] == 0x71 && packetSize == 41) {
    }
}

void loop()
{
    WiFiManager.loop();
    updater.loop();
    configManager.loop();
    dash.loop();

    if (auto newDumperClient = Dumper.available())
        dumperClient = newDumperClient;

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
    if (readingPacket && (millis() - lastByteTime > configManager.data.packetTimeout)) {
        processPacket();
        packetSize = 0;
        readingPacket = false;
    }
}
