#include <Arduino.h>
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
#include "TelnetLogger.h"
#include "jarvis_pinouts.h"

#include "ProtocolFully.h"

extern JarvisDesk Jarvis;

// End-of-message marker
#define EOM 0x7E

#define HANDSET 0xF1
#define CONTROLLER 0xF2

FullyHandset::FullyHandset() : ProtocolFully(HANDSET) {}
FullyController::FullyController() : ProtocolFully(CONTROLLER) {}

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

// Craft a message for Fully desk
std::vector<unsigned char> ProtocolFully::create_msg(unsigned char cmd, std::initializer_list<unsigned char> args) {
  unsigned char checksum = cmd;
  std::vector<unsigned char> out = {addr, addr, cmd, static_cast<unsigned char>(args.size())};
  checksum += args.size();
  for (auto const &p : args) {
    checksum += p;
    out.push_back(p);
  }
  out.push_back(checksum);
  out.push_back(EOM);
  return out;
}

void ProtocolFully::decode() {
  switch (static_cast<fully_command_byte>(cmd)) {
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
      Log.println("Height limit stop: ", (argv[0] == 0x01) ? "MAX " : (argv[0] == 0x02) ? "MIN" : "???");
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
