#include <Arduino.h>
#include <SoftwareSerial.h>
#include "LittleFS.h"

#include <InfluxDbClient.h>

#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "dashboard.h"

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

#define INFLUXDB_URL "http://coreserv.fritz.box:8086"
#define INFLUXDB_TOKEN "Ha7oxfPrj8MYiWnoJ4gDM7x7ahDGG79TnlKizqQoUxweJTvnWcqspbi8I0DVsDF7nDY2l6Dq86MifLpsLRwVaA=="
#define INFLUXDB_ORG "home"
#define INFLUXDB_BUCKET "test"

static constexpr int RS485_DE = 12;
static constexpr int RS485_DI = 13;
static constexpr int RS485_RO = 14;

SoftwareSerial RS485;
InfluxDBClient client;

void setup()
{
  pinMode(LED_BUILTIN_AUX, OUTPUT);
  digitalWrite(LED_BUILTIN_AUX, HIGH);

  pinMode(RS485_DE, OUTPUT);
  digitalWrite(RS485_DE, LOW);

  Serial.begin(115200);
  RS485.begin(9600, SWSERIAL_8S1, RS485_RO, RS485_DI, false);
  RS485.setTimeout(10);

  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  dash.begin();
  WiFiManager.begin(configManager.data.projectName);

  client.setConnectionParams(
      configManager.data.influxdbUrl,
      configManager.data.influxdbOrg,
      configManager.data.influxdbBucket,
      configManager.data.influxdbToken);

  configManager.setConfigSaveCallback([]()
                                      { client.setConnectionParams(
                                            configManager.data.influxdbUrl,
                                            configManager.data.influxdbOrg,
                                            configManager.data.influxdbBucket,
                                            configManager.data.influxdbToken); });

  timeSync(TZ_INFO, "de.pool.ntp.org", "pool.ntp.org");

  digitalWrite(LED_BUILTIN_AUX, LOW);
  Serial.println("Ready");
}

uint8_t buffer[64];

void influxWrite(Point &p)
{
  if (!client.writePoint(p))
  {
    Serial.print("InfluxDB Client error: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop()
{
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();

  if (auto bytesRead = RS485.readBytes(buffer, sizeof(buffer)))
  {
    if (buffer[0] == 0x41 && bytesRead == 30)
    {
      Point packet("packet");
      packet.addField("size", bytesRead);
      influxWrite(packet);

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

      switch (buffer[18])
      {
      case 0x3:
        Point temperatures("temperatures");
        temperatures.addField("vorlauf", buffer[19]);
        temperatures.addField("rücklauf", buffer[20]);
        temperatures.addField("warmwasser", buffer[21]);
        temperatures.addField("außen", static_cast<int8_t>(buffer[22]));
        temperatures.addField("abgas", buffer[23]);
        influxWrite(temperatures);
        break;
      }
    }
  }
}
