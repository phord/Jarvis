#pragma once
// Command interface for TelnetLogger

#include <string>
struct UserCommand {
  std::string cmd;
  bool put(char ch);
};
