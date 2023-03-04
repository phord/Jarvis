#include "Arduino.h"

#include "AdafruitIO_WiFi.h"
#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
#include "ProtocolFully.h"
#include "ProtocolUplift.h"
#include "TelnetLogger.h"
#include "jarvis_pinouts.h"

extern AdafruitIO_WiFi io;

enum JarvisMessage {
  BUTTON_DOWN = 1,
  BUTTON_UP = 2,
  BUTTON_1 = 3,
  BUTTON_2 = 4,
  BUTTON_3 = 5,
  BUTTON_4 = 6,
  BUTTON_MEM = 9
};

SoftwareSerial deskSerial(DTX);
SoftwareSerial hsSerial(HTX);

struct one_shot_timer {
  void reset(unsigned long t_ = 1000) { t = millis() + t_; }
  bool trigger() {
    auto now = millis();
    if (t && now - t <= t - now) {
      t = 0;
      return true;
    }
    return false;
  }

  // Timer is set and has not triggered (will trigger)
  bool pending() { return t && !trigger(); }

  // Timer is not set or has expired (will trigger)
  bool expired() { return !pending(); }

  // Timer is set (won't trigger)
  bool active() { return !!t; }

  unsigned long t = 0;
};

class JarvisDesk_impl {
public:
  void begin() {
    jarvis = io.group("jarvis");
    if (is_pin_connected(DTX))
      deskSerial.begin(9600);
    if (is_pin_connected(HTX))
      hsSerial.begin(9600);

    // Disable pullups turned on by espSoftwareSerial library
    if (is_pin_connected(DTX))
      pinMode(DTX, INPUT);
    if (is_pin_connected(HTX))
      pinMode(HTX, INPUT);
    jarvis->get();
  }

  void run() {
    //-- Process signals from desk controller
    decode_serial();

    //-- Manage any pending preset presses
    if (pending_preset) {
      if (is_moving()) {
        if (!pending_stop) {
          // Press the wake sequence once to try to stop our motion
          press_memory(30);
          pending_stop = true;
        }
      } else {
        latch(pending_preset);
        latch_timer.reset();
        pending_preset = 0;
      }
    } else
      pending_stop = false;

    if (latch_timer.trigger()) {
      unlatch();
    }

    // Publish pending updates periodically to AdafruitIO
    io_send();
  }

  void press_memory(int duration = 30) {
#if not defined(JCB35N2PA32V2) // JCB35N2PA32V2 doesn't use this line when pressing memory
    latch_pin(HS0);
#endif
    latch_pin(HS3);
    latch_timer.reset(duration);
    Log.println("Pressing memory for ", duration, "ms");
  }

  bool goto_preset(int p) {
    if (!p || p > 4)
      return false;
    if (p == preset)
      return false;
    pending_preset = p;
    return true;
  }

  void reset(int duration = 5000) {
    // Press and hold down for duration default 5s
    latch_pin(HS0);
    latch_timer.reset(duration);
    Log.println("Starting reset");
  }

  bool readPin(int pin) {
    if (is_pin_connected(pin)) {
      return digitalRead(pin);
    }
    return true; // pretend NC pins are pulled up
  }

  JarvisMessage getMessage() {
    unsigned btn =
      !readPin(HS0) * 1 +
      !readPin(HS1) * 2 +
      !readPin(HS2) * 4 +
      !readPin(HS3) * 8;

    return static_cast<JarvisMessage>(btn);
  }

  bool is_moving() { return height_changed.pending(); }

  void set_preset(unsigned char p) {
    if (preset == p)
      return;
    preset = p;

    // publish update to AdafruitIO
    io_set("preset", p);
  }

  void set_height(unsigned int h) {
    if (h == 9999 || h == 0) {
      Log.print("Fake-height: ");
      Log.println(h);
      return;
    }

    h = Util::to_mm(h);
    if (height == h || h < MIN_HEIGHT || h > MAX_HEIGHT)
      return; // if out of range, ignore it

    height = h;
    height_changed.reset(700);

    // publish update to AdafruitIO
    io_set("height", height);
  }

  void program_preset(unsigned memset) {
    // Record program setting if we know the height
    if (height) {
      char buf[20];
      sprintf(buf, "Prog_%d", memset);
      io_set(buf, height);
    }
    Log.println("Memory-set: ", memset, " ", height);
  }

  unsigned int height = 0;
  unsigned char preset = 0;

  // Timer for last height change (to detect movement)
  one_shot_timer height_changed; // detect active movement
  one_shot_timer latch_timer;    // unlatch a momentary button press
  one_shot_timer io_timer;       // prevent overloading AdafruitIO

  bool io_pending = false;

private:
  AdafruitIO_Group *jarvis = nullptr;

  // The preset we are commanded to go to next, if any
  unsigned char pending_preset = 0;
  bool pending_stop = false;

  void latch_pin(int pin) {
    if (is_pin_connected(pin)) {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
    }
  }

  void unlatch_pin(int pin) {
    if (is_pin_connected(pin)) {
      pinMode(pin, INPUT);
      digitalWrite(pin, HIGH);
    }
  }

  void io_set(const char *field, unsigned long value) {
    Log.println("io_set: ", field, "=", value);

    io_pending = true;
    jarvis->set(field, value);

    // Send right away, if possible.
    io_send();
  }

  bool io_send() {
    if (io_pending && io_timer.expired()) {
      Log.print("save:");

      auto data = jarvis->data;
      while (data) {
        Log.print(" ", data->feedName(), "=", data->toString());
        data = data->next_data;
      }
      Log.println();

      jarvis->save();
      io_pending = false;
      io_timer.reset(2000);
      return true;
    }
    return false;
  }

  void unlatch() {
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
#if defined(JCB35N2PA32V2)
  UpliftController deskPacket;
  UpliftController hsPacket;  // unused
#else
  FullyController deskPacket;
  FullyHandset hsPacket;
#endif

  // Decode the serial stream from the desk controller
  void decode_serial() {
    static int msg = 0;

    auto m = getMessage();
    if (m != msg) {
      msg = m;
      LogHs.println("Buttons: ", msg);
    }
    if (is_pin_connected(DTX)) {
      while (deskSerial.available()) {
        auto ch = deskSerial.read();
        LogDesk.print("<");
        LogDesk.print_hex(ch);
        LogDesk.print(">");
        if (deskPacket.put(ch)) {
          deskPacket.decode();
          deskPacket.reset();
          LogDesk.println();
        }
      }
    }

    if (is_pin_connected(HTX)) {
      while (hsSerial.available()) {
        auto ch = hsSerial.read();
        LogHs.print("{");
        LogHs.print_hex(ch);
        LogHs.print("}");
        if (hsPacket.put(ch)) {
          hsPacket.decode();
          LogHs.println();
        }
      }
    }
  }
};

//-- JarvisDesk API interface
JarvisDesk::JarvisDesk() { jarvis = new JarvisDesk_impl(); }

JarvisDesk::~JarvisDesk() { delete jarvis; }

void JarvisDesk::begin() { jarvis->begin(); }

void JarvisDesk::run() { jarvis->run(); }
void JarvisDesk::reset(int duration) { jarvis->reset(duration); }

void JarvisDesk::report() {
  Log.println("Height: ", jarvis->height);
  Log.println("Preset: ", jarvis->preset);
  Log.println("Keys: ", jarvis->getMessage());
}

void JarvisDesk::goto_preset(int p) { jarvis->goto_preset(p); }

void JarvisDesk::set_preset(unsigned char p) { jarvis->set_preset(p); }

void JarvisDesk::set_height(unsigned int h) { jarvis->set_height(h); }

void JarvisDesk::program_preset(unsigned memset) {
  jarvis->program_preset(memset);
}

void JarvisDesk::press_memory(int duration) {
  jarvis->press_memory(duration);
}