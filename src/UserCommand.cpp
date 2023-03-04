#include <Arduino.h>
#include <SoftwareSerial.h>

#include "JarvisDesk.h"
#include "ProtocolFully.h"
#include "TelnetLogger.h"
#include "UserCommand.h"

extern JarvisDesk Jarvis;
extern SoftwareSerial deskSerial;
extern SoftwareSerial hsSerial;

#define CMD_MAX 256

bool execute(std::string & cmd);

bool UserCommand::put(char ch) {
  switch (ch) {
  case '\n':
  case '\r':
    return execute(cmd);
  case 8: // backspace
    if (!cmd.empty()) {
      cmd.pop_back();
    }
  default:
    break;
  }
  if (ch >= ' ') {
    if (cmd.size() < CMD_MAX)
      cmd.push_back(ch);
  }
  return true;
};

bool execute(std::string & s) {
  std::string cmd;
  std::swap(s, cmd);

  if (cmd.empty())
    return true;

  if (cmd == "exit" || cmd == "quit")
    return false;

  return true;
}