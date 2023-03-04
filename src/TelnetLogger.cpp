#include "TelnetLogger.h"
#include "UserCommand.h"

// declare telnet server (do NOT put in setup())
WiFiServer telnetServer(23);

TelnetLogger Log;

void TelnetLogger::begin() {
  telnetServer.begin();
  telnetServer.setNoDelay(true);
}

bool TelnetLogger::connected() {
  return serverClient && serverClient.connected();
}

// https://www.rfc-editor.org/rfc/rfc854.html
// https://www.rfc-editor.org/rfc/rfc1143
// https://datatracker.ietf.org/doc/html/rfc857
constexpr unsigned IAC = 255;
constexpr unsigned WILL = 251;
constexpr unsigned WONT = 252;
constexpr unsigned DO = 253;
constexpr unsigned DONT = 254;
constexpr unsigned EC = 247;  // erase char
constexpr unsigned EL = 248;  // erase line
constexpr unsigned NOP = 241; //  No operation.

// Negotiate the telnet connection parameters
// Filters telnet commands and returns byte to be interpreted.
// Returns false to pass char through as-was; true to filter (was a handshake)
bool TelnetLogger::handshake(unsigned ch) {

  if (shake_count == 0) {
    if (ch == IAC) {
      shake_count = 2;
      return true;
    }
  }

  unsigned verb = 0;
  switch (shake_count) {
  case 2: // WILL, WONT, DO, DONT
    if (ch < IAC && ch >= WILL) {
      shake_msg = ch;
      shake_count--;
      return true;
    } else {
      // Unrecognized or quoted-FF; pass unmolested
      shake_count = 0;
      return false;
    }
  case 1:
    switch (ch) {
    case WILL:
    case WONT:
      verb = DONT;
      break;
    case DO:
    case DONT:
      verb = WONT;
      break;
    }
    if (verb) {
      serverClient.write((uint8_t)IAC);
      serverClient.write((uint8_t)verb);
      serverClient.write((uint8_t)ch);
      serverClient.flush();
    }
    shake_count = 0;
    return true;
  }

  return false;
}

void TelnetServer::run() {
  if (telnetServer.hasClient()) {
    if (!connected()) {
      if (serverClient) {
        serverClient.stop();
      }
      serverClient = telnetServer.available();
      serverClient.println("Connected to Jarvis");
    }
  }

  while (serverClient.available()) { // get data from Client
    unsigned ch = serverClient.read();
    if (!handshake(ch))
      if (!cmd.put(ch)) {
        serverClient.stop();
      }
  }
}