#include <Arduino.h>
#include <SoftwareSerial.h>
#include "LittleFS.h"

#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "dashboard.h"
#include "timeSync.h"

SoftwareSerial rs485;

void setup()
{
  Serial.begin(115200);
  rs485.begin(9600, SWSERIAL_8S1, 13, 12, false);

  rs485.readParity();

  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  dash.begin();
  WiFiManager.begin(configManager.data.projectName);
  timeSync.begin();

  Serial.println("Ready");
}

void loop()
{
  WiFiManager.loop();
  updater.loop();
  configManager.loop();
  dash.loop();

  if (configManager.data.huxlgrimmraschid)
    Serial.println("Flag is enabled.");

  while (rs485.available())
  {
    uint8_t c = rs485.read();
    if (rs485.readParity())
    {
      Serial.printf("\n*%02x", c);
    }
    else
    {
      Serial.printf(" %02x", c);
    }
  }

  dash.data.temperature = timeSync.isSynced() ? (configManager.data.huxlgrimmraschid ? 5 : 3) : -3;
}
