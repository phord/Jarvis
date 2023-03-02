// Protocol for Fully standup desk using desk controller
// model FullyCB2C-A and handset model JCHT35M16-1

#include "ProtocolBase.h"

struct ProtocolUplift : public ProtocolBase {

  ProtocolUplift(unsigned char addr_) : ProtocolBase(addr_) {}

  unsigned char checksum = 99;
  enum state_t {
    SYNC,   // waiting for addr
    SYNC2,  // waiting for addr2
    CMD,    // waiting for cmd
    LENGTH, // waiting for argc
    // ARGS4,3,2,1   // collecting args
    ARGS = LENGTH + sizeof(argv), // collecting args
    CHKSUM,                       // waiting for checksum
    ENDMSG,                       // waiting for EOM
  } state = SYNC;

  // Compensating handler for error bytes.
  // If we get an unexpected char, reset our state and clear any accumulated
  // arguments. But we want to resync with the start of the next possible
  // message as soon as possible. So, after an error we set the state back to
  // SYNC to begin waiting for a new packet.  But if the error byte itself was a
  // sync byte (matches our address), then we should already advance to SYNC2.
  // returns "false" to simplify returning from "put"
  bool error(unsigned char ch) {
    reset();
    state = static_cast<state_t>(SYNC + (ch == addr));
    return false;
  }

  void reset() override {
    state = SYNC;
    cmd = 0;
    argc = 0;
    memset(argv, 0U, sizeof(argv));
  }

  bool put(unsigned char ch) override;
  void decode() override;
};