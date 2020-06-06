#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"

// Pinouts for ESP8266 Oak (which uses these "P" numbers)
#define HS0   P9
#define HS1   P8
#define HS2   P7
#define HS3   P5

extern WiFiClient serverClient;
extern AdafruitIO_WiFi io;
extern AdafruitIO_Group *jarvis;

enum JarvisMessage {
  BUTTON_DOWN = 1,
  BUTTON_UP = 2,
  BUTTON_1 = 3,
  BUTTON_2 = 4,
  BUTTON_3 = 5,
  BUTTON_4 = 6,
  BUTTON_MEM = 9
  // WIP: Add serial comms messages
};

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
  bool pending() {
    return t && !trigger();
  }

  // Timer is not set or has expired (will trigger)
  bool expired() {
    return !pending();
  }

  // Timer is set (won't trigger)
  bool active() {
    return !!t;
  }

  unsigned long t = 0;
};


class JarvisDesk {
  public:
  void begin() {
    Serial.begin(9600);
    while(! Serial);
  }

  void run() {
    //-- Process signals from desk controller
    decode_serial();

    //-- Manage any pending preset presses
    if (pending_preset && !is_moving()) {
      latch(pending_preset);
      latch_timer.reset();
      pending_preset = 0;
    }

    if (latch_timer.trigger()) {
      unlatch();
    }

    // Publish pending updates periodically to AdafruitIO
    io_send();
  }

  bool goto_preset(int p) {
    if (!p || p >4) return false;
    if (p == preset) return false;
    pending_preset = p;
    return true;
  }

  JarvisMessage getMessage() {
    unsigned btn =
      digitalRead(HS0) * 1 +
      digitalRead(HS1) * 2 +
      digitalRead(HS2) * 4 +
      digitalRead(HS3) * 8;

    return static_cast<JarvisMessage>(btn);
  }

  bool is_moving() {
    return height_changed.pending();
  }

  unsigned int height = 0;
  unsigned char preset = 0;

  // Timer for last height change (to detect movement)
  one_shot_timer height_changed;    // detect active movement
  one_shot_timer latch_timer;       // unlatch a momentary button press
  one_shot_timer io_timer;          // prevent overloading AdafruitIO

  bool io_pending = false;

private:

  // The preset we are commanded to go to next, if any
  unsigned char pending_preset = 0;

  void latch_pin(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  void unlatch_pin(int pin) {
    pinMode(pin, INPUT);
    digitalWrite(pin, HIGH);
  }

  void io_set(const char * field, unsigned long value) {
    if (serverClient && serverClient.connected()) {  // send data to Client
      serverClient.print("io_set: ");
      serverClient.print(field);
      serverClient.print("=");
      serverClient.println(value);
    }

    io_pending = true;
    jarvis->set(field, value);

    // Send right away, if possible.
    io_send();
  }

  bool io_send() {
    if (io_pending && io_timer.expired()) {
      if (serverClient && serverClient.connected()) {  // send data to Client
        serverClient.print("save:");

        auto data = jarvis->data;
        while (data) {
          serverClient.print(" ");
          serverClient.print(data->feedName());
          serverClient.print("=");
          serverClient.print(data->toString());
          data = data->next_data;
        }
        serverClient.println();
      }
      jarvis->save();
      io_pending = false;
      io_timer.reset(2000);
      return true;
    }
    return false;
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

  #define BUFSIZE       20
  #define CONTROLLER    0xF2
  #define HANDSET       0xF1

  unsigned char tail = 0;
  unsigned char serial_buffer[BUFSIZE];

  unsigned char prev(unsigned char p) {
      return (p-1 + BUFSIZE) % BUFSIZE;
  }

  unsigned char next(unsigned char p) {
      return (p+1) % BUFSIZE;
  }

  unsigned char get(unsigned char &p) {
      if (p == tail) return 0;
      unsigned char ch = serial_buffer[p];
      p = next(p);
      return ch;
  }

  void set_preset(unsigned char p) {
    if (preset == p) return;
    preset = p;

    // publish update to AdafruitIO
    io_set("preset", p);

    // if (serverClient && serverClient.connected()) {
    //   serverClient.print("Preset: ");
    //   serverClient.println(preset);
    // }
  }

  void set_height(unsigned int h) {
    if (height == h) return;
    height = h;
    height_changed.reset(2000);

    // publish update to AdafruitIO
    io_set("height", height);

    // if (serverClient && serverClient.connected()) {
    //   serverClient.print("Height: ");
    //   serverClient.print(height);
    //   serverClient.println("mm");
    // }
  }

  bool error(const char * msg) {
    if (serverClient && serverClient.connected())
        serverClient.println(msg);
    return false;
  }

  // decode the packet from head..tail and verify checksum
  bool parse_packet() {
    unsigned char head = prev(tail);

    //-- Header: starts with controller address 2x
    while ((head = prev(head)) != tail) {
      if (serial_buffer[head] == CONTROLLER)
        if (serial_buffer[next(head)] == CONTROLLER)
          break;
    }

    //-- Underflow
    if (head == tail) return error("UNDERFLOW");

    //-- Payload
    unsigned char p = next(next(head));
    unsigned char e = prev(tail);        // ptr to chksum byte
    unsigned char chksum = 0;
    while (p != e) {
      chksum += get(p);
    }

    //-- Bad checksum
    if (chksum != get(p)) return error("CHKSUM");

    // Payload = [head+2..tail-2]

    //-- Height announcement
    //   F2 F2 1 3 HI LO 7 CHK
    //           =       =
    p = next(next(head));
    if (get(p) == 0x01 && get(p) == 0x03) {
      // Not sure what 3 and 7 are for.  Units?
      unsigned int h = get(p) * 0x100;
      h += get(p);
      set_height(h);
      return true;
    }


    //-- Move to preset position
    //   F2-F2-92-1-10-A3-7E
    //              ==
    p = next(next(head));
    if (get(p) == 0x92 && get(p) == 0x01) {
      unsigned char ps = 0;
      unsigned char ch = get(p);
      switch (ch) {
        case 0x04: ps = 1; break;
        case 0x08: ps = 2; break;
        case 0x10: ps = 3; break;
        case 0x20: ps = 4; break; // assumed.  Haven't seen it
        default: break; // error?
      }
      if (ps) {
        set_preset(ps);
        return true;
      }
    }

    //-- Unknown codes
    if (serverClient && serverClient.connected()) {  // send data to Client
      unsigned char p = head;
      serverClient.print("Unknown: ");
      while (p != tail) {
        unsigned ch = get(p);
        serverClient.print(ch, HEX);
        if (p == tail)
          serverClient.println();
        else
          serverClient.print("-");
      }
    }


    return true;
  }


  // Decode the serial stream from the desk controller
  void decode_serial() {
    while (Serial.available()) {
      unsigned char ch = serial_buffer[tail] = Serial.read();

      if (ch == 0x7E)
        parse_packet();

      tail = next(tail);
    }
  }
};

// FIXME: Make this interface work like a library?
JarvisDesk Jarvis;

void jarvis_begin();
void jarvis_run();
void jarvis_goto(int p);
//

void jarvis_begin() {
  Jarvis.begin();
}

void jarvis_run() {
  Jarvis.run();
}

void jarvis_goto(int p) {
  Jarvis.goto_preset(p);
}
