#include "TelnetLogger.h"

// declare telnet server (do NOT put in setup())
WiFiServer telnetServer(23);

TelnetLogger Log;

void TelnetLogger::begin()
{
  telnetServer.begin();
  telnetServer.setNoDelay(true);
}

bool TelnetLogger::connected()
{
  return serverClient && serverClient.connected();
}

void TelnetLogger::run()
{
  if (telnetServer.hasClient())
  {
    if (!connected())
    {
      if (serverClient)
      {
        serverClient.stop();
      }
      serverClient = telnetServer.available();
      serverClient.println("Connected to Jarvis");
    }
  }

  while (serverClient.available())
  { // get data from Client
    unsigned ch = serverClient.read();
    // serverClient.print(ch, HEX);
    // serverClient.print(" ");
  }
}