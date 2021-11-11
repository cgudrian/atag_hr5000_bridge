#include <Arduino.h>
#include <SoftwareSerial.h>
#include "LittleFS.h"

#include "WiFiManager.h"
#include "webServer.h"
#include "updater.h"
#include "configManager.h"
#include "timeSync.h"

SoftwareSerial softSerial;

void setup()
{
  Serial.begin(115200);
  softSerial.begin(9600, SWSERIAL_8S1, 13, 12, false);

  softSerial.readParity();

  LittleFS.begin();
  GUI.begin();
  configManager.begin();
  WiFiManager.begin(configManager.data.projectName);
  timeSync.begin();

  Serial.println("Ready");
}

void loop()
{
  WiFiManager.loop();
  updater.loop();
  configManager.loop();

  while (softSerial.available())
  {
    uint8_t c = softSerial.read();
    if (softSerial.readParity())
    {
      Serial.printf("\n*%02x", c);
    }
    else
    {
      Serial.printf(" %02x", c);
    }
  }
}
