#pragma once
#include <ESP8266WiFi.h>

class TelnetLogger;

class TelnetServer {
public:
  void begin();
  void run();
  bool connected();

protected:
  // kluge
  void print_hex(unsigned x) {
    serverClient.print(x, HEX);
  }

  template<typename first_t, class ...Args>
  void print_raw(first_t const& arg, Args... args) {
    serverClient.print(arg);
    print_raw(args...);
  }

  void print_raw() {}

  template<class ...Args>
  void print_raw(char arg, Args... args) {
    if (arg == '\n')
      serverClient.println();
    else
      serverClient.print(arg);
    print_raw(args...);
  }

private:
  WiFiClient serverClient;

  unsigned shake_count;
  unsigned shake_msg;

  // Telnet connection options negotiation
  bool handshake(unsigned ch);
  friend TelnetLogger;
};

class TelnetLogger {
public:
  TelnetLogger(TelnetServer & srv, bool on = true) : server(srv), enabled(on) {}

  // kluge
  void print_hex(unsigned x) {
    if (connected()) {
      server.print_hex(x);
    }
  }

  template<class ...Args>
  void print(Args... args) {
    if (connected()) {
      print_raw(args...);
    }
  }

  template<class ...Args>
  void println(Args... args) {
    print(args..., '\n');
  }

  bool connected() { return enabled && server.connected(); }

  void toggle() { enabled = !enabled; }
  bool is_enabled() { return enabled; }
  void enable() { enabled = true; }
  void disable() { enabled = false; }

private:
  TelnetServer & server;
  bool enabled;

  template<class ...Args>
  void print_raw(Args... args) {
    server.print_raw(args...);
  }
};

extern TelnetServer LogServer;
extern TelnetLogger Log;
