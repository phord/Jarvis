#include <Arduino.h>
#include "ProtocolBase.h"
#include "TelnetLogger.h"

void ProtocolBase::print_choice(const char *field, int n,
                                std::vector<const char *> args) {
  Log.print(field, ": ");
  if (n == -1)
    Log.println("No args?");
  else if (n < args.size())
    Log.println(args[n]);
  else {
    Log.println("UNKNOWN[P0=", n, "]");
    dump();
  }
}

void ProtocolBase::dump() {
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
    if (i + 1 < argc)
      Log.print(" ");
  }
  Log.println("]");
}