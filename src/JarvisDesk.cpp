#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <SoftwareSerial.h>

// Pinouts for ESP8266 Oak (which uses these "P" numbers)
#define HS0   P9
#define HS1   P8
#define HS2   P7
#define HS3   P5

extern WiFiClient serverClient;
extern AdafruitIO_WiFi io;
extern AdafruitIO_Group *jarvis_sub;

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

SoftwareSerial deskSerial(P3);
SoftwareSerial hsSerial(P4);

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
    jarvis = io.group("jarvis");
    deskSerial.begin(9600);
    hsSerial.begin(9600);

    // Disable pullups turned on my espSoftwareSerial library
    pinMode(P3, INPUT);
    pinMode(P4, INPUT);
    jarvis->get();
  }

  void run() {
    //-- Process signals from desk controller
    decode_serial();

    //-- Manage any pending preset presses
    if (pending_preset) {
      if (is_moving()) {
        if (!pending_stop)  {
          // Press the memory once to try to stop our motion
          latch_pin(HS0);
          latch_pin(HS3);
          pending_stop = true;
          latch_timer.reset(30);
        }
      } else {
        latch(pending_preset);
        latch_timer.reset();
        pending_preset = 0;
      }
    }
    else pending_stop = false;

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
      !digitalRead(HS0) * 1 +
      !digitalRead(HS1) * 2 +
      !digitalRead(HS2) * 4;
      // + !digitalRead(HS3) * 8;  Not wired in yet

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
  AdafruitIO_Group *jarvis = nullptr;

  // The preset we are commanded to go to next, if any
  unsigned char pending_preset = 0;
  bool pending_stop = false;

  void latch_pin(int pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  void unlatch_pin(int pin) {
    pinMode(pin, INPUT);
    digitalWrite(pin, HIGH);
  }

  void io_set(const char * field, unsigned long value) {
    if (serverClient && serverClient.connected()) {
      serverClient.print("io_set: ");
      serverClient.print(field);
      serverClient.print("=");
      serverClient.println(value);
    }

    io_pending = true;
    jarvis_sub->set(field, value);

    // Send right away, if possible.
    io_send();
  }

  bool io_send() {
    if (io_pending && io_timer.expired()) {
      if (serverClient && serverClient.connected()) {
        serverClient.print("save:");

        auto data = jarvis_sub->data;
        while (data) {
          serverClient.print(" ");
          serverClient.print(data->feedName());
          serverClient.print("=");
          serverClient.print(data->toString());
          data = data->next_data;
        }
        serverClient.println();
      }
      jarvis_sub->save();
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
  #define EOM           0x7E

  struct ring_buffer {
    unsigned char tail = 0;
    unsigned char serial_buffer[BUFSIZE];
    unsigned char addr = 0;

    ring_buffer(unsigned char address) : addr(address) {}

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

    struct payload {
      unsigned char head = 0, tail = 0;
      enum ERR {NONE = 0, WAITING, CHKSUM, eUNDERFLOW};
      ERR err = NONE;

      payload(unsigned char start, unsigned char end) : head(start), tail(end) {}
      payload(ERR e) : err(e) {}
    };

    payload put(unsigned char ch) {
      serial_buffer[tail] = ch;

      payload p = {payload::ERR::WAITING};
      if (ch == EOM)
        p = parse_packet();

      tail = next(tail);
      return p;
    }

    payload parse_packet() {
      unsigned char head = prev(tail);

      //-- Header: starts with controller address 2x
      while ((head = prev(head)) != tail) {
        if (serial_buffer[head] == addr)
          if (serial_buffer[next(head)] == addr)
            break;
      }

      //-- Underflow
      if (head == tail) return {payload::ERR::eUNDERFLOW};

      //-- Payload
      unsigned char p = head = next(next(head));
      unsigned char e = prev(tail);        // ptr to chksum byte
      unsigned char chksum = 0;
      while (p != e) {
        chksum += get(p);
      }

      //-- Bad checksum
      if (chksum != get(p)) return {payload::ERR::CHKSUM};

      return {head, e};
    }

  };

  ring_buffer desk = {CONTROLLER};
  ring_buffer hs = {HANDSET};

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
    if (h == 9999 || h == 0) {
      if (serverClient && serverClient.connected()) {
        serverClient.print("Fake-height: ");
        serverClient.print(h);
        serverClient.println("mm");
      }
      return;
    }

    height = h;
    height_changed.reset(700);

    // publish update to AdafruitIO
    io_set("height", height);

    // if (serverClient && serverClient.connected()) {
    //   serverClient.print("Height: ");
    //   serverClient.print(height);
    //   serverClient.println("mm");
    // }
  }

  bool error(const ring_buffer::payload &pb) {
    if (serverClient && serverClient.connected()) {
      const char * msg = nullptr;
      if (pb.err == ring_buffer::payload::ERR::eUNDERFLOW)   msg = "UNDERFLOW";
      else if (pb.err == ring_buffer::payload::ERR::CHKSUM) msg = "CHKSUM";
      if (msg) serverClient.println(msg);
    }
    return !!pb.err;
  }

  // decode the packet from head..tail and verify checksum
  void decode_desk(ring_buffer::payload const &pb) {
    unsigned char p;

    //-- Height announcement
    //   F2 F2 1 3 HI LO 7 CHK
    //           =       =
    p = pb.head;
    if (desk.get(p) == 0x01 && desk.get(p) == 0x03) {
      // Not sure what 3 and 7 are for.  Units?
      unsigned int h = desk.get(p) * 0x100;
      h += desk.get(p);
      set_height(h);
      return;
    }


    //-- Move to preset position
    //   F2-F2-92-1-10-A3-7E
    //              ==
    p = pb.head;
    if (desk.get(p) == 0x92 && desk.get(p) == 0x01) {
      unsigned char ps = 0;
      unsigned char ch = desk.get(p);
      switch (ch) {
        case 0x04: ps = 1; break;
        case 0x08: ps = 2; break;
        case 0x10: ps = 3; break;
        case 0x20: ps = 4; break;
        default: break; // error?
      }
      if (ps) {
        set_preset(ps);
        return;
      }
    }

    //-- Unknown codes
    if (serverClient && serverClient.connected()) {
      p = pb.head;
      serverClient.print("Desk: ");
      while (p != pb.tail) {
        unsigned ch = desk.get(p);
        serverClient.print(ch, HEX);
        if (p == pb.tail)
          serverClient.println();
        else
          serverClient.print("-");
      }
    }
  }

  // decode the packet from head..tail and verify checksum
  void decode_handset(ring_buffer::payload const &pb) {
    unsigned char p;

    //-- Program preset
    //   F1 F1 26 0 CHK
    //         == =
    // Note these commands nearly match the binary patterns that pset buttons 1-4 send
    // on the HS0..HS3 signal lines.  Surely not a coincidence?  Are more presets possible?
    enum {
      MSET_1 = 0x03,
      MSET_2 = 0x04,
      MSET_3 = 0x25,
      MSET_4 = 0x26
    };
    p = pb.head;
    auto cmd = hs.get(p);
    if ((cmd == MSET_1 || cmd == MSET_2 || cmd == MSET_3 || cmd == MSET_4) && hs.get(p) == 0x00) {
      // Convert 3..6 => 1..4
      auto memset = (cmd & 0x0F) - 2;
      // Record program setting if we know the height
      if (height) {
        char buf[20];
        sprintf(buf, "Prog_%d", memset);
        io_set(buf, height);
      }
      if (serverClient && serverClient.connected()) {
        serverClient.print("Memory-set: ");
        serverClient.print(memset);
        serverClient.print(" ");
        serverClient.println(height);
      }
      return;
    }

    //-- Unknown codes
    if (serverClient && serverClient.connected()) {
      p = pb.head;
      serverClient.print("Handset: ");
      while (p != pb.tail) {
        unsigned ch = hs.get(p);
        serverClient.print(ch, HEX);
        if (p == pb.tail)
          serverClient.println();
        else
          serverClient.print("-");
      }
    }
  }

  // Decode the serial stream from the desk controller
  void decode_serial() {
    while (deskSerial.available()) {
      auto ch = deskSerial.read();
      auto p = desk.put(ch);
      if (!error(p))
        decode_desk(p);

      // // HACK: where's my serials?
      // if (serverClient && serverClient.connected()) {
      //   serverClient.print(ch, HEX);
      //   serverClient.print("-");
      //   if (ch == 0x7E)
      //     serverClient.println();
      // }
    }

    while (hsSerial.available()) {
      auto ch = hsSerial.read();
      auto p = hs.put(ch);
      if (!error(p))
        decode_handset(p);

      // // HACK: where's my serials?
      // if (serverClient && serverClient.connected()) {
      //   serverClient.print(ch, HEX);
      //   serverClient.print("-");
      //   if (ch == 0x7E)
      //     serverClient.println();
      // }
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

void jarvis_report() {
  if (serverClient && serverClient.connected()) {
      serverClient.print("Height: ");
      serverClient.println(Jarvis.height);
      serverClient.print("Preset: ");
      serverClient.println(Jarvis.preset);
      serverClient.print("Keys: ");
      serverClient.println(Jarvis.getMessage());
  }
}