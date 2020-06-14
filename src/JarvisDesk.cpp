#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
#include "TelnetLogger.h"

// Pinouts for ESP8266 Oak (which uses these "P" numbers)
#define HS0   P9
#define HS1   P8
#define HS2   P7
#define HS3   P5

extern AdafruitIO_WiFi io;

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

struct Util {
  static unsigned int getword(unsigned char a, unsigned char b) {
    return (a << 8) + b;
  }

  static unsigned to_mm(unsigned h) {
    if (h < 600) {
      // Height in inches*10; convert to mm
      h *= 254;  // convert to mm*100
      h += 50;   // round up to nearest whole mm
      h /= 100;  // convert to mm
    }
  }

};

class JarvisDesk_impl {
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
    Log.print("io_set: ", field, "=", value);

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

    void dump_all() {
      auto p = next(tail);
      Log.print("DUMP: ");
      while (p != tail) {
        unsigned ch = get(p);
        Log.print(ch, HEX);
        if (p == tail)
          Log.println();
        else
          Log.print("-");
      }
    }
  };

  ring_buffer desk = {CONTROLLER};
  ring_buffer hs = {HANDSET};

  void set_preset(unsigned char p) {
    if (preset == p) return;
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
    if (height == h) return;
    height = h;
    height_changed.reset(700);

    // publish update to AdafruitIO
    io_set("height", height);
  }

  bool error(const ring_buffer::payload &pb) {
    {
      const char * msg = nullptr;
      if (pb.err == ring_buffer::payload::ERR::eUNDERFLOW)   msg = "UNDERFLOW";
      else if (pb.err == ring_buffer::payload::ERR::CHKSUM) msg = "CHKSUM";
      if (msg) Log.println(msg);
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
      // Not sure what P2 is for.
      unsigned int h = desk.get(p);
      set_height(Util::getword(h, desk.get(p)));
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
    {
      p = pb.head;
      Log.print("Desk: ");
      while (p != pb.tail) {
        unsigned ch = desk.get(p);
        Log.print(ch, HEX);
        if (p == pb.tail)
          Log.println();
        else
          Log.print("-");
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
      Log.print("Memory-set: ", memset, " ", height);
      return;
    }

    //-- Unknown codes
    if (Log.connected()) {
      p = pb.head;
      Log.print("Handset: ");
      while (p != pb.tail) {
        unsigned ch = hs.get(p);
        Log.print(ch, HEX);
        if (p == pb.tail)
          Log.println();
        else
          Log.print("-");
      }
    }
  }

  struct cmdPacket {
    /** Note: Most of these commands are sent only from the desk controller or from
              the handset.  They are collected here in one enum for simplicity.
    **/
    enum command_byte {
      // FAKE
      NONE        = 0x00,  // Unused/never seen; used as default for "Uninitialized"

      // CONTROLLER
      HEIGHT      = 0x01,  // Height report; P0=4 (mm?)
      LIMIT_RESP  = 0x20,  // Max-height set/cleared; response to [21];
      REP_MAX     = 0x21,  // Report max height; Response to SET_MAX
      REP_MIN     = 0x22,  // Report min height; Response to SET_MIN
      LIMIT_STOP  = 0x23,  // Min/Max reached
      RESET       = 0x40,  // Indicates desk in RESET mode; Displays "RESET"
      REP_PRESET  = 0x92,  // Moving to Preset location

      // HANDSET
      UNITS       = 0x0E,  // Set units to cm/inches
      MEM_MODE    = 0x19,  // Set memory mode
      COLL_SENS   = 0x1D,  // Set anti-collision sensitivity  (Sent 1x; no repeats)
      SET_MAX     = 0x21,  // Set max height; Sets max-height to current height
      SET_MIN     = 0x22,  // Set min height; Sets min-height to current height
      LIMIT_CLR   = 0x23,  // Clear min/max height
      PROGMEM_1   = 0x03,  // Set memory position 1 to current height
      PROGMEM_2   = 0x04,  // Set memory position 2 to current height
      PROGMEM_3   = 0x25,  // Set memory position 3 to current height
      PROGMEM_4   = 0x26,  // Set memory position 4 to current height
      WAKE        = 0x29,  // Poll message (??) sent when desk doesn't respond to BREAK messages
      CALIBRATE   = 0x91,  // Height calibration (Repeats 2x)
    };

    cmdPacket(unsigned char addr_) : addr(addr_) {}

    command_byte  cmd = NONE;
    unsigned char addr;
    unsigned char checksum = 99;
    unsigned char argc = 0;
    unsigned char argv[5];
    enum state_t {
      SYNC,    // waiting for addr
      SYNC2,   // waiting for addr2
      CMD,     // waiting for cmd
      LENGTH,  // waiting for argc
      // ARGS4,3,2,1   // collecting args
      ARGS = LENGTH + sizeof(argv),   // collecting args
      CHKSUM,  // waiting for checksum
      ENDMSG,  // waiting for EOM
    } state = SYNC;

    bool error(unsigned char ch) {
      state = static_cast<state_t>(SYNC + (ch == addr));
      cmd = NONE;
      argc = 0;
      memset(argv, 0U, sizeof(argv));
      return false;
    }

    bool put(unsigned char ch) {
      bool complete = false;

      switch (state) {
      case SYNC:
      case SYNC2:
        if (ch != addr) return error(ch);
        break;

      case CMD:
        cmd = static_cast<cmdPacket::command_byte>(checksum = ch);
        break;

      case LENGTH:
        if (ch > sizeof(argv)) return error(ch);
        checksum += (argc = ch);
        state = static_cast<state_t>(CHKSUM - ch - 1);
        break;

      default:    // ARGS
        if (state <= LENGTH || state > ARGS) return error(ch); // assert(ARGS);
        checksum += (argv[argc - (CHKSUM-state)] = ch);
        break;

      case CHKSUM:
        if (ch != checksum) return error(ch);
        complete = true;
        break;

      case ENDMSG:
        return error(ch);  // We do the same here whether it's an error or not
      }
      // Common increment for every state
      state = static_cast<state_t>(state + 1);
      if (state < SYNC || state > ENDMSG) return error(ch); // assert(state);
      return complete;
    }

    void dump() {
      Log.print("Packet: addr=");
      Log.print(addr, HEX);
      Log.print(" state=", state);
      Log.print(" argc=");
      Log.print(argc, HEX);
      Log.print(" argv=[ ");
      for (unsigned i = 0 ; i < argc ; i++) {
        Log.print(argv[i], HEX);
        Log.print(" ");
      }
      Log.print("] checksum=");
      Log.print(checksum, HEX);
      Log.println();
    }
  };

  cmdPacket deskPacket = {CONTROLLER};
  cmdPacket hsPacket = {HANDSET};

  // Decode the serial stream from the desk controller
  void decode_serial() {
    while (deskSerial.available()) {
      auto ch = deskSerial.read();
      auto p = desk.put(ch);
      if (deskPacket.put(ch)) {
        deskPacket.dump();
      }
      if (!error(p))
        decode_desk(p);
      else if (p.err == ring_buffer::payload::ERR::CHKSUM)
        desk.dump_all();
    }

    while (hsSerial.available()) {
      auto ch = hsSerial.read();
      auto p = hs.put(ch);
      if (hsPacket.put(ch)) {
        hsPacket.dump();
      }
      if (!error(p))
        decode_handset(p);
      else if (p.err == ring_buffer::payload::ERR::CHKSUM)
        hs.dump_all();
    }

  }
};

void jarvis_report() {
}

//-- JarvisDesk API interface
JarvisDesk::JarvisDesk() {
  jarvis = new JarvisDesk_impl();
}

void JarvisDesk::begin() {
  jarvis->begin();
}

void JarvisDesk::run() {
  jarvis->run();
}

void JarvisDesk::report() {
  Log.println("Height: ", jarvis->height);
  Log.println("Preset: ", jarvis->preset);
  Log.println("Keys: ", jarvis->getMessage());
}

void JarvisDesk::goto_preset(int p) {
  jarvis->goto_preset(p);
}
