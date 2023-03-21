#include <Arduino.h>
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
#include "TelnetLogger.h"
#include "jarvis_pinouts.h"

#include "ProtocolUplift.h"

extern JarvisDesk Jarvis;

#define CONTROLLER 0x01
UpliftController::UpliftController() : ProtocolUplift(CONTROLLER) {}

enum command_byte {
  // FAKE
  NONE = 0x00, // Unused/never seen; used as default for "Uninitialized"

  // CONTROLLER
  HEIGHT = 0x01, // Height report
  ERROR  = 0x02,  // Error reporting and desk lockout
  RESET  = 0x04,  // Indicates desk in RESET mode; Displays "RESET"/"ASR"
  PRGM   = 0x06, // Programming code. Used when memory is pressed and in start up.
};

// returns true when a message is decoded and ready to parse in {cmd, argc,
// argv}
bool ProtocolUplift::put(unsigned char ch) {
  bool complete = false;

  switch (state) {
  case SYNC:
    if (ch != addr) {
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
    cmd = static_cast<command_byte>(ch); // was checksum = ch
    break;

  default:                        // ARGS, state increased by 2 each time
    if (state < 4 || state > 6) { // only 2 args seen
      Log.println("Arg mismatch");
      Log.println("cmd: ", cmd);
      for (int i = 0; i <= 2; i++)
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

void ProtocolUplift::decode() {
  switch (static_cast<command_byte>(cmd)) {
  case NONE:
    break;
  // CONTROLLER commands
  case HEIGHT:
    if (argc == 2) {
      Jarvis.set_height(Util::getword(argv[0], argv[1]));
      return;
    }
    Log.println("set-height: not enough args?");
    break;

  case ERROR:
    if (argc != 2) {
      Log.println("Error: unknown");
      return;
    }
    if (argv[0] == 0 ){
        if(argv[1] == 0x80) // Desk is locked
          Log.println("Desk Height Locked");
        if(argv[1]) = 0x40) // E13 Leg uneven error? Not Documented by Uplift
          Log.println("Error: E13");
      }

    if (argv[0] == 0x80 && argv[1] == 0) // E08
      Log.println("Error: E08");


    return;

  // This is for ack and programming notice
  //  Setting memory for locations 1,2,3,4
  // 1-6-x-0 where x is 1,2,4,8
  case PRGM:
    if (argc != 2) {
      Log.println("Input: not enough args?");
      return;
    }
    if ((argv[0] & 0b1111) > 0 && argv[1] == 0) {
      if (argv[0] & 0b1100)
        Jarvis.program_preset((argv[0] & 0b1000) ? 4 : 3);
      else
        Jarvis.program_preset(argv[0]);
      return;
    }

    if (argv[0] == 0 && argv[1] == 0)
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
