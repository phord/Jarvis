// File: TelnetServer.ino for ESP8266 NodeMCU
// 2015-12-07 Rudolf Reuter www.rudiswiki.de/wiki9 (search for "wifi")
// 2015-12-17 RR, structure copied from example WiFiTelnetToSerial.ino
//
// Developed for debug purpose, use like the Arduino Serial Monitor.
// Needs Arduino 1.6.5/6 to compile.
//
/*
 *   This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <ESP8266WiFi.h>
#include <Arduino.h>


// declare telnet server (do NOT put in setup())
WiFiServer telnetServer(23);
WiFiClient serverClient;

boolean debug = false;  // true = more messages
//boolean debug = true;

unsigned long startTime = millis();

void telnet_setup() {

//  Serial.print("Chip ID: 0x");
//  Serial.println(ESP.getChipId(), HEX);

//  if (WiFi.status() == WL_CONNECTED) {
//    Serial.print("WiFi mode: ");
//    Serial.println(str_mode[WiFi.getMode()]);
//    Serial.print ( "Status: " );
//    Serial.println (str_status[WiFi.status()]);
//    // signal WiFi connect
//    digitalWrite(LED_BUILTIN, LOW);
//    delay(300); // ms
//    digitalWrite(LED_BUILTIN, HIGH);      
//  } else {
//    Serial.println("");
//    Serial.println("WiFi connect failed, push RESET button.");
//    signalError();
//  }

  telnetServer.begin();
  telnetServer.setNoDelay(true);
//  Serial.println("Please connect Telnet Client, exit with ^] and 'quit'");
//
//  Serial.print("Free Heap[B]: ");
//  Serial.println(ESP.getFreeHeap());
} // setup()


void telnet_loop() {
  // look for Client connect trial
  if (telnetServer.hasClient()) {
    if (!serverClient || !serverClient.connected()) {
      if (serverClient) {
        serverClient.stop();
//        Serial.println("Telnet Client Stop");
      }
      serverClient = telnetServer.available();
//      Serial.println("New Telnet client");
      serverClient.println("Connected to Jarvis");
      serverClient.flush();  // clear input buffer, else you get strange characters 
    }
  }

  while(serverClient.available()) {  // get data from Client
    serverClient.print((unsigned)serverClient.read(), HEX);
    serverClient.print(" ");
  }

  if (millis() - startTime > 2000) { // run every 2000 ms
    startTime = millis();

    if (serverClient && serverClient.connected()) {  // send data to Client
      serverClient.print("Telnet Test, millis: ");
      serverClient.println(millis());
      serverClient.print("Free Heap RAM: ");
      serverClient.println(ESP.getFreeHeap());
    }
  }
  delay(10);  // to avoid strange characters left in buffer

}
