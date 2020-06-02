#include <Arduino.h>
#include <Ticker.h>

// Pinouts for ESP8266 Oak (which uses these "P" numbers)
#define HS0   P9
#define HS1   P8
#define HS2   P7
#define HS3   P5

Ticker latch_timer;

unsigned latch_value = 0;

void latch_pin(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void unlatch_pin(int pin) {
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);
}

void unlatch() 
{
  unlatch_pin(HS0);
  unlatch_pin(HS1);
  unlatch_pin(HS2);
  unlatch_pin(HS3);
}

void latch(unsigned preset)
{
  if (!preset || preset > 4) return;

  // pinout pattern has preset+2
  preset += 2;
  
  unlatch();
  if (preset & 1) latch_pin(HS0);
  if (preset & 2) latch_pin(HS1);
  if (preset & 4) latch_pin(HS2);
  if (preset & 8) latch_pin(HS3);
}

void momentary_off();
void momentary_on() 
{
  latch(latch_value);
  latch_timer.detach();
  latch_timer.attach(1, momentary_off);
}

void momentary_off() 
{
  unlatch();
  latch_timer.detach();
}

// TODO: Add check for buttons already pressed (user interacting)
// TODO: Handle case where first press stopped an action
void momentary(unsigned preset) 
{
//  Serial.print("Pressing preset button ");
//  Serial.println(preset);
  latch_value = preset;
  momentary_on();
}
