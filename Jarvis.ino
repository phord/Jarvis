// #include <Ticker.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "local-config.h"
#include "src/JarvisDesk.h"
#include "src/TelnetLogger.h"
#include "src/Ota.h"

#include "AdafruitIO_WiFi.h"
const int LED_PIN = LED_BUILTIN;

AdafruitIO_WiFi io(IO_USERNAME, IO_KEY, WIFI_SSID, WIFI_PASS);
AdafruitIO_Group* jarvis_sub = io.group("jarvis");

JarvisDesk Jarvis;
Ota ota;

// FIXME: Move to flasher.h
void flash(unsigned count = 0, float secs = 0.3);

void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(9600);
  Serial.println("Serial init");

  io.connect();

  Jarvis.begin();

  jarvis_sub->onMessage("preset", handlePreset);

  flash(0, 0.5);
  while (io.status() < AIO_CONNECTED) {
    delay(100);
  }

  flash(10, 0.1);

  ota.begin();

  Log.begin();
  jarvis_sub->get();
}

void loop() {
  // Run the AdafruitIO Service
  io.run();

  if (Serial.available())
    onSerialInput();

  // run the Jarvis desk interface
  Jarvis.run();

  Log.run();

  // Run the OTA Updater Service
  ota.loop();
}

// Handle messages from AdafruitIO
void handlePreset(AdafruitIO_Data* data) {
  Log.println(">MSG: ", data->feedName(), "=", data->toString());

  auto preset = data->toInt();
  flash(preset, 0.25);

  // Press the button
  Jarvis.goto_preset(preset);
  Jarvis.report();
}

void onSerialInput() {
  char ch = Serial.read();

  if (ch == 'r') {
    Log.println(">SMSG: Reset");

    Serial.print("MSG: Reset");

    Jarvis.reset();
    return;
  }

  if (ch == 'm') {
    Log.println(">SMSG: Virtual Memory Press");

    Serial.print("MSG: Virtual Memory Press");

    Jarvis.press_Memory(10000);
    return;
  }

  uint preset = ch - '0';

  if (preset > 0 && preset < 5) { // preset 1-4
      Log.println(">SMSG: Preset ", preset);

    Serial.print(">MSG: Preset ");
    Serial.println(preset);

    flash(preset, 0.25);

    // Press the button
    Jarvis.goto_preset(preset);
  }

  Jarvis.report();
}