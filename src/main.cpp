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
Point sensor("wifi_status");

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

  client.setConnectionParams(
      configManager.data.influxdbUrl,
      configManager.data.influxdbOrg,
      configManager.data.influxdbBucket,
      configManager.data.influxdbToken);

  timeSync(TZ_INFO, "de.pool.ntp.org", "pool.ntp.org");

  sensor.addTag("device", "ESP8266");
  sensor.addTag("SSID", WiFi.SSID());

  digitalWrite(LED_BUILTIN_AUX, LOW);
  Serial.println("Ready");
}

void loop()
{
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();

  sensor.clearFields();

#if 0
  while (RS485.available())
  {
    uint8_t c = RS485.read();
    if (RS485.readParity())
    {
      dash.data.address_bytes += 1;
      Serial.printf("\n*%02x", c);
    }
    else
    {
      dash.data.data_bytes += 1;
      Serial.printf(" %02x", c);
    }
  }
#else
  sensor.addField("rssi", WiFi.RSSI());

  Serial.print("Writing: ");
  Serial.println(sensor.toLineProtocol());

  if (!client.writePoint(sensor))
  {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(10000);
#endif
}
