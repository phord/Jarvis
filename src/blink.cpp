#include <Arduino.h>
#include <Ticker.h>

Ticker flipper;

#define FOREVER 9999

void flip() {
  int state = digitalRead(LED_BUILTIN); // get the current state of LED pin
  digitalWrite(LED_BUILTIN, !state);    // set pin to the opposite state
}

unsigned flasher_count = 0;

void flasher() {
  if (!flasher_count) {
    flipper.detach();
    return;
  }

  if (!digitalRead(LED_BUILTIN)) {
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);
  if (flasher_count != FOREVER)
    --flasher_count;
}

void flash(unsigned count = 0, float secs = 0.3) {
  if (count == 0)
    count = FOREVER;
  flasher_count = count;
  flipper.attach(secs, flasher);
}
