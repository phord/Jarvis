#include <Ticker.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "local-config.h"
#include "AdafruitIO_WiFi.h"
const int LED_PIN = LED_BUILTIN;

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Group *jarvis_sub = io.group("jarvis");



// FIXME: Move to flasher.h
void flash(unsigned count = 0, float secs = 0.3);
void momentary(unsigned preset);

void jarvis_begin();
void jarvis_run();
void jarvis_goto(int p);

void telnet_setup();
void telnet_loop();

void setup() {
  pinMode(LED_PIN, OUTPUT);

  io.connect();

  jarvis_begin();

  jarvis_sub->onMessage("preset", handlePreset);
  jarvis_sub->onMessage(handleMessage);

  flash(0, 0.5);
  while(io.status() < AIO_CONNECTED) {
    delay(100);
  }

  flash(10, 0.1);


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
// Arduino OTA setup
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("jarvis");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
//    Serial.println("Start");
    flash(0,0.25);
  });
  ArduinoOTA.onEnd([]() {
//    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
//    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
//    flipper.attach(1.1 - (progress/total), flip);
    flash(0,1.1 - (progress/total));
  });

  ArduinoOTA.onError([](ota_error_t error) {
//    digitalWrite(LED_PIN, HIGH);
//    Serial.printf("Error[%u]: ", error);
//    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
//    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
//    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
//    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
//    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    flash(100,0.05);
  });

  ArduinoOTA.begin();
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  telnet_setup();
  jarvis_sub->get();

}

void loop() {
  // Run the AdafruitIO Service
  io.run();

  // run the Jarvis desk interface
  jarvis_run();

  telnet_loop();

  // Run the OTA Updater Service
  ArduinoOTA.handle();

}

    // Handle messages from AdafruitIO
    void handleMessage(AdafruitIO_Data *data)
    {
    //  Serial.print("received <- ");
    //  Serial.print(data->value());
    //  Serial.print(" from ");
    //  Serial.println(data->feedName());
    //  Serial.println(data->toCSV());
    }

extern WiFiClient serverClient;
// Handle messages from AdafruitIO
void handlePreset(AdafruitIO_Data *data)
{
  if (serverClient && serverClient.connected()) {  // send data to Client
    serverClient.print(">MSG:");

    serverClient.print(" ");
    serverClient.print(data->feedName());
    serverClient.print("=");
    serverClient.print(data->toString());
    serverClient.println();
  }

  auto preset = data->toInt();
  flash(preset, 0.25);

  // Press the button
  jarvis_goto(preset);
}
