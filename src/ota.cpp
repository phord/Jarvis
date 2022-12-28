#include"Ota.h"

#include <ArduinoOTA.h>

// FIXME: Move to flasher.h
void flash(unsigned count = 0, float secs = 0.3);

void Ota::begin() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("jarvis");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    //    Serial.println("Start");
    flash(0, 0.25);
    });
  ArduinoOTA.onEnd([]() {
    //    Serial.println("\nEnd");
    });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    //    flipper.attach(1.1 - (progress/total), flip);
    flash(0, 1.1 - (progress / total));
    });

  ArduinoOTA.onError([](ota_error_t error) {
    //    digitalWrite(LED_PIN, HIGH);
    //    Serial.printf("Error[%u]: ", error);
    //    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    //    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    //    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    //    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    //    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    flash(100, 0.05);
    });

  ArduinoOTA.begin();
}

void Ota::loop() {
  // Run the OTA Updater Service
  ArduinoOTA.handle();
}