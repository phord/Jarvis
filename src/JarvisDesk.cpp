#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "AdafruitIO_WiFi.h"
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
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
    return (static_cast<unsigned>(a) << 8) + b;
  }

  static unsigned to_mm(unsigned h) {
    if (h < 600) {
      // Height in inches*10; convert to mm
      h *= 254; // convert to mm*100
      h += 50;  // round up to nearest whole mm
      h /= 100; // convert to mm
    }
    return h;
  }

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
        if (!pending_stop)  {
          // Press the memory once to try to stop our motion
          press_Memory(30);
          pending_stop = true;
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

  void press_Memory(int duration = 30) {
#if not defined(JCB35N2PA32V2) // JCB35N2PA32V2 doesn't use this line when pressing memory
    latch_pin(HS0);
#endif
    latch_pin(HS3);
    latch_timer.reset(duration);
    Log.println("Pressing memory for ", duration, "ms");
  }

  bool goto_preset(int p) {
    if (!p || p > 4) return false;
    if (p == preset) return false;
    pending_preset = p;
    Log.println("Goto preset ", p);
    return true;
  }

  void reset() {
    // Press and hold down for 10 seconds
    latch_pin(HS0);
    latch_timer.reset(5000);
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

  bool is_moving() {
    return height_changed.pending();
  }

  unsigned int height = 0;
  unsigned char preset = 0;

  // Timer for last height change (to detect movement)
  one_shot_timer height_changed; // detect active movement
  one_shot_timer latch_timer;    // unlatch a momentary button press
  one_shot_timer io_timer;       // prevent overloading AdafruitIO

  bool io_pending = false;

  private:
  AdafruitIO_Group* jarvis = nullptr;

  // The preset we are commanded to go to next, if any
  unsigned char pending_preset = 0;
  bool pending_stop = false;
  bool pending_reset = false;

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

  void io_set(const char* field, unsigned long value) {
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
#define CONTROLLER 0x01

#else
#define CONTROLLER 0xF2
#endif

#define HANDSET 0xF1

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
    if (height == h || h < 640 || h > 1300) return; // if it out of range, ignore it

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

  struct cmdPacket {
    /** Note: Most of these commands are sent only from the desk controller or from
              the handset.  They are collected here in one enum for simplicity.
    **/

#if defined(JCB35N2PA32V2)
    enum command_byte {
      // FAKE
      NONE = 0x00, // Unused/never seen; used as default for "Uninitialized"

      // CONTROLLER
      HEIGHT = 0x01, // Height report
      ERROR = 0x02,
      RESET = 0x04, // Indicates desk in RESET mode; Displays "RESET"/"ASR"
      PRGM = 0x06, // Programming code
    };
#else
    enum command_byte {
      // FAKE
      NONE = 0x00, // Unused/never seen; used as default for "Uninitialized"

      // CONTROLLER
      HEIGHT = 0x01,     // Height report; P0=4 (mm?)
      LIMIT_RESP = 0x20, // Max-height set/cleared; response to [21];
      REP_MAX = 0x21,    // Report max height; Response to SET_MAX
      REP_MIN = 0x22,    // Report min height; Response to SET_MIN
      LIMIT_STOP = 0x23, // Min/Max reached
      RESET = 0x40,      // Indicates desk in RESET mode; Displays "RESET"
      REP_PRESET = 0x92, // Moving to Preset location

      // HANDSET
      UNITS = 0x0E,     // Set units to cm/inches
      MEM_MODE = 0x19,  // Set memory mode
      COLL_SENS = 0x1D, // Set anti-collision sensitivity  (Sent 1x; no repeats)
      SET_MAX = 0x21,   // Set max height; Sets max-height to current height
      SET_MIN = 0x22,   // Set min height; Sets min-height to current height
      LIMIT_CLR = 0x23, // Clear min/max height
      PROGMEM_1 = 0x03, // Set memory position 1 to current height
      PROGMEM_2 = 0x04, // Set memory position 2 to current height
      PROGMEM_3 = 0x25, // Set memory position 3 to current height
      PROGMEM_4 = 0x26, // Set memory position 4 to current height
      WAKE = 0x29,      // Poll message (??) sent when desk doesn't respond to BREAK messages
      CALIBRATE = 0x91, // Height calibration (Repeats 2x)
    };
#endif

    cmdPacket(unsigned char addr_): addr(addr_) {}

    command_byte cmd = NONE;
    unsigned char addr;
    unsigned char checksum = 99;
    unsigned char argc = 0;
    unsigned char argv[5];
    enum state_t {
      SYNC,   // waiting for addr
      SYNC2,  // waiting for addr2
      CMD,    // waiting for cmd
      LENGTH, // waiting for argc
      // ARGS4,3,2,1   // collecting args
      ARGS = sizeof(argv), // collecting args
      CHKSUM,              // waiting for checksum
      ENDMSG,              // waiting for EOM
    } state = SYNC;

    bool error(unsigned char ch) {
      state = static_cast<state_t>(SYNC + (ch == addr));
      cmd = NONE;
      argc = 0;
      memset(argv, 0U, sizeof(argv));
      return false;
    }

    void reset() {
      state = SYNC;
      cmd = NONE;
      argc = 0;
      memset(argv, 0U, sizeof(argv));
    }
#if defined(JCB35N2PA32V2)
    bool put(unsigned char ch) {
      bool complete = false;

      switch (state) {
        case SYNC:
          if (ch != addr){
            Log.print("Bad Sync: ");
            Log.println(ch);
            return error(ch);
          }
          break;

        case CMD:
          if (ch == 5) // end of stream
            return error(ch);
          if (ch != 1 && ch != 2 && ch != 4 && ch != 6) {
            Log.println("Bad cmd: ", ch);
            return error(ch);
          }
          cmd = static_cast<cmdPacket::command_byte>(ch); // was checksum = ch
          break;

        default: // ARGS, state increased by 2 each time
          if (state < 4 || state > 6) { // only 2 args seen
            Log.println("Arg mismatch");
            Log.println("cmd: ", cmd);
            for (int i = 0; i <= state - 2; i++)
              Log.println("argv[", i, "]: ", argv[i]);

            return error(ch);
          }
          argv[argc++] = ch;

          if (argc == 2)
            complete = true;
          
          break;
      }
      state = static_cast<state_t>(state + 2);

      return complete;
    }
#else
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

        default: // ARGS
          if (state <= LENGTH || state > ARGS) return error(ch); // assert(ARGS);
          checksum += (argv[argc - (CHKSUM - state)] = ch);
          break;

        case CHKSUM:
          if (ch != checksum) return error(ch);
          complete = true;
          break;

        case ENDMSG:
          return error(ch); // We do the same here whether it's an error or not
      }
      // Common increment for every state
      state = static_cast<state_t>(state + 1);
      if (state < SYNC || state > ENDMSG) return error(ch); // assert(state);
      return complete;
    }
#endif

    void print_choice(int n, std::vector<const char *> args) {
      if (n < args.size()) Log.println(args[n]);
      else {
        Log.println("UNKNOWN[P0=", n, "]");
        dump();
      }
    }

    template <class... Args>
    void config(const char* field, Args... args) {
      Log.print(field, ": ");
      if (!argc) Log.println("No args?");
      else print_choice(argv[0], { args... });
    }
#if defined(JCB35N2PA32V2)
    void decode(JarvisDesk_impl& parent) {
      switch (cmd) {
        case NONE:
          break;

          // CONTROLLER commands
        case HEIGHT:
          if (argc == 2) {
            parent.set_height(Util::getword(argv[0], argv[1]));
            return;
          }
          Log.println("set-height: not enough args?");
          break;

        case ERROR:
          if (argc != 2) {
            Log.println("Error: unknown");
            return;
          }
          if (argv[0] == 0 && argv[1] == 0x80)
            Log.println("Desk Height Locked");

          if (argv[0] == 0x80 && argv[1] == 0)
            Log.println("Error: E08");

          return;

        //This is for ack and programming notice
        // Setting memory for locations 1,2,3,4
        //1-6-x-0 where x is 1,2,4,8 
        case PRGM: 
          if (argc != 2) {
            Log.println("Input: not enough args?");
            return;
          }
          if((argv[0] & 0b1111)>0 && argv[1] == 0){
            if(argv[0] & 0b1100)
              parent.program_preset((argv[0]&0b1000)?4:3);
            else 
              parent.program_preset(argv[0]);
            return;
          }
            
          if(argv[0] == 0 && argv[1] == 0) 
            Log.println("Pending programming input");
          
          return;

        case RESET:
          if (argc != 2) {
            Log.println("ASR Reset: not enough args?");
            return;
          }
          if (argv[0] == 1 && argv[1] == 0xAA)
            Log.println("ASR Reset");
          break;

          // Unrecognized:
        default:
          Log.print("UNKNOWN COMMAND: ");
          break;
      }
      dump();
    }
#else
    void decode(JarvisDesk_impl& parent) {
      switch (cmd) {
        case NONE:  break;

          // CONTROLLER commands
        case HEIGHT:
          if (argc >= 2) {
            parent.set_height(Util::getword(argv[0], argv[1]));
            return;
          }
          Log.println("set-height: not enough args?");
          break;

        case REP_MAX:
          if (argc >= 2) {
            auto h = Util::to_mm(Util::getword(argv[0], argv[1]));
            Log.println("Max-height set to ", h, "mm");
            return;
          }
          break;

        case REP_MIN:
          if (argc >= 2) {
            auto h = Util::to_mm(Util::getword(argv[0], argv[1]));
            Log.println("Min-height set to ", h, "mm");
            return;
          }
          break;

        case LIMIT_RESP:
          if (argc == 1) {
            Log.print("Height limit: ");
            Log.print_hex(argv[0]); // TBD: Meaning of {0, 1, 2, 16}
            Log.println();
            return;
          }
          break;

        case LIMIT_STOP:
          if (argc) {
            Log.println("Height limit stop: ",
              (argv[0] == 0x01) ? "MAX " : 
              
              (argv[0] == 0x02) ? "MIN": "???");
            if (!(argv[0] & ~0x03)) return;
          }
          break;

        case RESET:
          Log.println("RESET");
          return;

        case REP_PRESET:
          // Variant results; wtf?
          // 1,2,3,4 = {4, 8, 16, 32}
          // OR
          // 1,2,3,4 = {3, 4, 0x25, 0x26}
        {
          auto preset =
            argv[0] == 4 ? 1 : 
            argv[0] == 8 ? 2 :
            argv[0] == 16 ? 3 :
            argv[0] == 32 ? 4 : 0;
          Log.println("Moving to preset: ", preset);
          if (preset) return;
        }
        break;

        // HANDSET commands
        case UNITS:
          config("Units", "inches", "centimeters");
          return;

        case MEM_MODE:
          config("Memory mode", "One-touch", "Constant touch");
          return;

        case COLL_SENS:
          config("Collision sensitivity", "???", "High", "Medium", "Low");
          return;

          // TBD
          // case SET_MAX:  // See REP_MAX
          // case SET_MIN:  // See REP_MIN
          // case LIMIT_CLR: // See LIMIT_STOP
          return;

        case PROGMEM_1:
          parent.program_preset(1);
          return;

        case PROGMEM_2:
          parent.program_preset(2);
          return;

        case PROGMEM_3:
          parent.program_preset(3);
          return;

        case PROGMEM_4:
          parent.program_preset(4);
          return;

        case WAKE:
          Log.println("WAKE");
          return;

        case CALIBRATE:
          Log.println("Calibrate min-height");
          return;

          // Unrecognized:
        default:
          Log.print("UNKNOWN COMMAND: ");
          break;
      }
      dump();
    }
#endif

    void dump() {
      Log.print_hex(addr);
      Log.print(": ");
      Log.print_hex(cmd);
      if (!argc) {
        Log.println();
        return;
      }
      Log.print("[");
      for (unsigned i = 0; i < argc; i++) {
        Log.print_hex(argv[i]);
        if (i + 1 < argc) Log.print(" ");
      }
      Log.println("]");
    }
  };

  cmdPacket deskPacket = { CONTROLLER };
  cmdPacket hsPacket = { HANDSET };

  // Decode the serial stream from the desk controller
  void decode_serial() {
    if (is_pin_connected(DTX)) {
      while (deskSerial.available()) {
        auto ch = deskSerial.read();
        if (deskPacket.put(ch)) {
          deskPacket.decode(*this);
          deskPacket.reset();
        }
      }
    }

    if (is_pin_connected(HTX)) {
      while (hsSerial.available()) {
        auto ch = hsSerial.read();
        if (hsPacket.put(ch)) {
          hsPacket.decode(*this);
        }
        yield();
      }
    }
  }

};

//-- JarvisDesk API interface
JarvisDesk::JarvisDesk() {
  jarvis = new JarvisDesk_impl();
}

JarvisDesk::~JarvisDesk() {
  delete jarvis;
}

void JarvisDesk::begin() {
  jarvis->begin();
}

void JarvisDesk::run() {
  jarvis->run();
}
void JarvisDesk::reset() {
  jarvis->reset();
}

void JarvisDesk::report() {
  Log.println("Height: ", jarvis->height);
  Log.println("Preset: ", jarvis->preset);
  Log.println("Keys: ", jarvis->getMessage());
}

void JarvisDesk::goto_preset(int p) {
  jarvis->goto_preset(p);
}

void JarvisDesk::press_Memory(int duration = 30) {
  jarvis->press_Memory(duration);
}