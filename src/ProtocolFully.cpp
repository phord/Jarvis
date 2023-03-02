#include <Arduino.h>
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
#include "TelnetLogger.h"
#include "jarvis_pinouts.h"

#include "ProtocolFully.h"

extern JarvisDesk Jarvis;

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



// returns true when a message is decoded and ready to parse in {cmd, argc, argv}
bool ProtocolFully::put(unsigned char ch) {
  bool complete = false;
  switch (state) {
  case SYNC:
  case SYNC2:
    if (ch != addr)
      return error(ch);
    break;
  case CMD:
    cmd = ch;
    checksum = ch;
    break;
  case LENGTH:
    if (ch > sizeof(argv))
      return error(ch);
    checksum += (argc = ch);
    state = static_cast<state_t>(CHKSUM - ch - 1);
    break;
  default: // ARGS
    if (state <= LENGTH || state > ARGS)
      return error(ch); // assert(ARGS);
    checksum += (argv[argc - (CHKSUM - state)] = ch);
    break;
  case CHKSUM:
    if (ch != checksum)
      return error(ch);
    complete = true;
    break;
  case ENDMSG:
    return error(ch); // We do the same here whether it's an error or not
  }
  // Common increment for every state
  state = static_cast<state_t>(state + 1);
  if (state < SYNC || state > ENDMSG)
    return error(ch); // assert(state);
  return complete;
}

void ProtocolFully::decode() {
  switch (static_cast<command_byte>(cmd)) {
  case NONE:
    break;
  // CONTROLLER commands
  case HEIGHT:
    if (argc >= 2) {
      Jarvis.set_height(Util::getword(argv[0], argv[1]));
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
      Log.println("Height limit stop: ", (argv[0] == 0x01)   ? "MAX "
                                         : (argv[0] == 0x02) ? "MIN"
                                                             : "???");
      if (!(argv[0] & ~0x03))
        return;
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
      auto preset = argv[0] == 4    ? 1
                    : argv[0] == 8  ? 2
                    : argv[0] == 16 ? 3
                    : argv[0] == 32 ? 4
                                    : 0;
      Log.println("Moving to preset: ", preset);
      if (preset)
        return;
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
    Jarvis.program_preset(1);
    return;
  case PROGMEM_2:
    Jarvis.program_preset(2);
    return;
  case PROGMEM_3:
    Jarvis.program_preset(3);
    return;
  case PROGMEM_4:
    Jarvis.program_preset(4);
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
