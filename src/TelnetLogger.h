#ifndef TelnetLogger_h
#define TelnetLogger_h

#include <ESP8266WiFi.h>

class TelnetLogger {
public:
    // kluge
    void print(int x, int base = DEC) {
      if (connected()) {
        serverClient.print(x, base);
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

    void begin();
    void run();
    bool connected();

private:
    WiFiClient serverClient;

    template<typename first_t, class ...Args>
    void print_raw(first_t const & arg, Args... args) {
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
};

extern TelnetLogger Log;

#endif