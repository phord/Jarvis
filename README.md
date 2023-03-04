**Me:** Ok, Google. Stand up.<br/>
**Google Assistant:**  Ok. Raising your Jarvis desk.<br/>
**Desk:** _begins moving obediently_<br/>

# IoT Jarvis standup desk interface

**Notice:** This project probably will void your warranty.  Proceed at your own risk.

This is a project to add a computer interface to my [Jarvis stand-up desk from fully.com](https://www.fully.com/standing-desks/jarvis.html).
I reverse-engineered the interface between the desk handset and the desk controller box
to find out how they communicate. Then I wired an ESP8266 wifi module (like the [Huzzah
from Adafruit](https://www.adafruit.com/product/2471)) to a couple of RJ-45
jacks and plugged it all together.  Now I can snoop on the messages being sent between
the desk and the handset, inject my own messages to either one, and monitor the desk position
from the internet using the AdafruitIO MQTT dashboard interface.

It's pretty cool, but honestly, my motivation was to be able to lower the desk without getting
up to see the handset controller. The desk sits next to my couch at home. When I want to set
a drink on it while watching TV, it's inconvenient if I left it standing up.  "Ok, Google. Sit down"
The desk lowers, and I can put my drink down and continue watching Netflix. To be fair, I didn't need to
decode the serial interface for this because the buttons used for memory presets are sent on separate
signal lines. The serial protocol decoding was just for fun.

As a useless side-benefit, you can look back at my [desk's height history](https://io.adafruit.com/phord/feeds/jarvis.height) and see every time it moved up or down and [what preset was used](https://io.adafruit.com/phord/feeds/jarvis.preset) in the last 30 days.
And I can control my desk from anywhere on the internet, so I can lower it even if I'm not home, though that
seems like a bad idea.

![Jarvis desk position history graph](docs/Jarvi-desk-position-history.png)

Features so far:
* Reverse engineered the UART interface and most control messages
* Snoop on the messages being sent between the desk and the handset
* Read the handset buttons
* Telnet wifi interface for debug logging
* Simulate handset buttons (e.g. press 3)
* Monitor desk configuration
* Connected the desk to AdafruitIO's MQTT dashboard for monitoring
* Can control the desk position via the AdafruitIO dashboard
* Interfaced IFTTT to the AdafruitIO feed controlling my desk
* Added a Google Assistant command phrase to raise and lower my desk

Future things:
* Inject control messages to the UART to control and configure the desk
* Inject control messages to the UART to control the handset display
* Explore the handset to see what else it might be hiding
* Keep the handset display awake so buttons are always visible[1]
* Explore the _other_ UART control protocol the desk sometimes falls into
* Expose all handset controls so people can add memory presets to their cheap-model handsets.
* Expand the telnet interface to permit different log levels, message injection, etc.

[1] Sadly, you can't display random text on your display module, as far as I know.
But you could make a nice little 3-digit clock or something there, if you wanted.

# Software notes:

## Security configuration (REQUIRED)

You will need to edit the `local-config.h` file to include your own WiFi access credentials and your Adafruit IO
username and API key.

    // AdafruitIO credentials
    #define IO_USERNAME "your-adafruitio-id"
    #define IO_KEY "your-adafruitio-api-key-hex"

    // Wifi AP credentials
    #define WIFI_SSID "wifi-ssid"
    #define WIFI_PASS "wifi-password"

For your safety (and mine) this file is in the `.gitignore` file for this project to help prevent us from accidentally
committing password information to the project and sharing it with the world. If you want to keep this config file
history in your own local git repository, please be aware of this limitation.

## Board configuration (REQUIRED)

You will need to edit the pin definitions in `jarvis_pinouts.h` to match the GPIOs you used on your board. You will need
6 pins defined. You can use one of the existing layouts defined there by uncommenting the correct #define directive, or
you may choose your own pin layout by adding your own customized version with the `DEFINE_PINS` macro.

## Building the code

This code should be able to build with the [Arduino IDE](https://www.arduino.cc/en/software) or the [arduino-cli](https://www.arduino.cc/pro/cli) tool. You will need extra libraries installed to support your target board. This code is written for the ESP8266, so to
build it as-is, you would need the ESP8266 toolkit, for example. If you use the Adafruit.IO feed you will
also need to install the Adafruit IO library and its dependencies.

To build with arduino-cli, do this:

    arduino-cli compile -b esp8266:esp8266:oak .


## OTA Software Upload

The code here supports Over-the-Air (OTA) program upload using the ESP8266 "espota.py" tool or the ArduinoIDE.
Once a version of this code is booted and running on the board, future updates can be uploaded with the OTA update.

In the Arduino IDE, OTA upload can be done by changing the port from the serial port to the Jarvis (network) port
here:
    ArduinoIDE > Tools > Port : Jarvis
If the device is online and connected to your network, it should show up in the Port menu under "Network ports".

To upload using the espota utility:

    $HOME/.arduino15/packages/esp8266/hardware/esp8266/3.0.2/tools/espota.py -i jarvis.local -p 8266 -f Jarvis.ino.bin


## Telnet interface

The code runs a telnet interface for debug output. This is mostly for convenience, but it also frees up the serial port
pins for use as GPIO pins to connect to the desk.

Telnet to `jarvis.local` on port 23 to see the debug logs.

    telnet jarvis.local

## Board wiring

**WARNING: This is the part that probably voids your warranty.**

The 6 GPIO pins you chose on the ESP8266 board should be wired to the RJ-45 signal pins on your desk.
In addition, you will need to wire the RJ-45 GND pin (pin 3) to your controller ground, and for convenience
you can wire the RJ-45 VCC (pin 5) to your controller's VCC to power it when it's not connected to external power.

See [this discussion](https://github.com/phord/Jarvis/discussions/8) for notes on how I wired up my board.

# Technical notes
## Hardware

The Jarvis desk handset style I have is the touchpanel interface with memories.  You can surely use
the desk without this interface if you design your own, but you need to know the protocols.  I
will try to publish what I discovered through reverse-engineering here.

* Handset model: JCHT35M16-1
* Desk controller model: FullyCB2C-A

Note: Other desk owners have mentioned that the simple 2-button handset interface uses a different
desk controller (model: JCB35N2) but has the same electrical interface. However the serial interface uses
a different protocol. You can find some details in the [JCB35N2 controller protocol discussion](https://github.com/phord/Jarvis/discussions/4).
It's a bit exciting that the basic and advanced models both seem to support both protocols in some form, but so far
I don't know how to switch between them on either controller.

## Physical interface (RJ-45)

The interface from the handset to the controller is via an RJ-45 8-pin connector (like an ethernet
connector). The pins in the RJ-45 jack are numbered 1 to 8.

_Note: I don't know any official names for these signal lines, so I made these up myself._
_Any mistakes here about the meaning or use of these lines are mine._

Following are their usage on the desk:

| Pin | Label | Description
| --- | ----- | --------------------------------------
|  1  |  HS3  | Handset control line 3 [1]
|  2  |  DTX  | Serial control messages from controller to handset [2]
|  3  |  GND  | Ground
|  4  |  HTX  | Serial control messages from handset to controller [2]
|  5  |  VCC  | Vcc (5vdc) supply from desk controller [3]
|  6  |  HS2  | Handset control line 2 [1]
|  7  |  HS1  | Handset control line 1 [1]
|  8  |  HS0  | Handset control line 0 [1]

[1] The pins (HS0, HS1, HS2, HS3) make up the "button-press" mux lines. See _HSx control lines_ below<br/>
[2] Serial port UART is 9600bps, 8 data bits, No parity, 1 stop bit<br/>
[3] 5VDC supply appears capable of driving at least 300ma<br/>
[4] All the signal lines have 5v pullups provided by the desk controller

![Interface signal showing "Memory" and "3" being pressed](docs/Wake-3.png)

_In this capture, I pressed "Memory" to wake the screen up, and then 3 to move the desk to position 3._

## Physical interface (RJ-12)

Some controllers such as the Jiecang JCB36N2CA fitted in recent Fully Jarvis models also offer an RJ-12 6-pin
connector (like a telephone jack), which is used for an [optional Bluetooth add-on](https://fccid.io/2ANKDJCP35NBLT/Internal-Photos/Internal-Photos-3727739).
Of the 6-pin connector, only the center 4 pins are required for the Bluetooth add-on, meaning an RJ-11 connector,
which is physically idential but has only 4 conductors populated, will work just fine.

The pins in the RJ-12 jack are numbered 1 to 6. Following are their usage on the desk:

| Pin | Label | Description
| --- | ----- | --------------------------------------
|  1  |  NC   | (unpopulated in Bluetooth add-on, pulled high by controller)
|  2  |  GND  | Ground
|  3  |  DTX  | Serial control messages from controller to handset
|  4  |  VCC  | Vcc (5vdc) supply from desk controller
|  5  |  HTX  | Serial control messages from handset to controller
|  6  |  NC   | (unpopulated in Bluetooth add-on, pulled high by controller)


## Interfacing with 5 volts

The desk provides 5v pullups on all six signal lines. A "mark" is signaled by pulling the line low.

The ESP8266 microcontroller is only capable of outputting 3.3v on its GPIOs. But I don't need to pull
any of these pins high to 5v because the desk does that for me. I only need to pull these signals low
to 0.0v.

It is safe to _sink_ 5v on these GPIO pins on the ESP8266, so long as the carrier board doesn't
mind the extra voltage. Opinions vary on this topic, though. You may feel safer using a level shifter
or some transistor or diode assistance to pull these 5v signals low on your board. Doing this is beyond
my expertise, though, so I'm afraid I can't help you with this part if you choose to go this route.

## HSx control lines

On the mechanical desk control buttons, when you press the up button, one of the HS[0-2] lines is
pulled low. Similarly when you press the down button, another line is pulled low. When you
use a smart controller interface like the touch-screen model, multiple lines may be pulled
low simultaneously to indicate up to 7 different buttons.  If you combine the first three
lines' inverted logical signals, you will find a "button number" between 0 and 7 is possible.

Zero means no button is being pressed.  This is "open".

Note: HS0+HS3 is used to wake up the controller to send height information.

         Down  Up  1  2  3  4  M
    HS3                        X
    HS2               X  X  X
    HS1         X  X        X
    HS0    X       X     X     X

Translated into binary, these buttons send these codes:
| Button | Value | Notes
| ------ | ----- | ------
| "Open" |   0   |
| Down   |   1   |
| Up     |   2   |
| 1      |   3   |
| 2      |   4   |
| 3      |   5   |
| 4      |   6   |
| *any*  |   9   |  Sent when display is touched while asleep. Always sent as two 30ms pulses.

## UART protocol

The Jarvis UART signaling protocol sends checksummed control packets consisting of between 6
and 9 bytes. All data is sent using 9600 bps, 8 data bits, no parity (9600/8N1). The desk controller
sends data on DTX; the handset sends data on HTX. Command packets are the same format on both.

    [address] [command] [length] [params] [checksum] [eom]

| Field    | Octets  | Description
| -------- | ------- | ---------------------------------
| ADDRESS  |      2  | Same value in both bytes; 0xF1 for Handset, 0xF2 for controller
| COMMAND  |      1  | See *KNOWN COMMANDS*
| LENGTH   |      1  | Length of PARAMS bytes that follow (0 to 4 (??))
| PARAMS   | LENGTH  | Omitted if LENGTH=0
| CHECKSUM |      1  | 8-bit sum of {COMMAND, LENGTH, PARAMS}
| EOM 0x7E |      1  | End of message (0x7E)

The address byte is repeated two times.  The desk controller always sends 0xF2 0xF2.
The handset always sends 0xF1 0xF1.

The PARAMS field consists of 0 to 3 bytes.  I haven't decoded all the meaning here. WIP

The checksum is the low-byte sum of the all the payload bytes.  In pseudocode,

     CHECKSUM = sum(COMMAND, LENGTH, PARAMS) % 0xFF

The EOM byte is always 0x7E.

I will use a shorthand to refer to these packets later. The desk controller packets will be
given as `CONTROLLER(COMMAND, PARAMS, ...)` and the handset packets will be given as
`HANDSET(COMMAND, PARAMS...)`.  `PARAMS` are given as `P0` for the first byte, `P1` for the second,
etc.  16-bit params are indicated like `{P0,P1}`. Optional params are shown in brackets.

For example, the `HANDSET(0x29)` message is sent from the handset to the desk.  It represents
this sequence of 6 bytes:

    F1  F1     29       00       29          7E
    ADDRESS    COMMAND  LENGTH   CHECKSUM    EOM

## Startup sequence

Smart handset sends a `NULL (0x00)` byte on first startup.  If no response is received from the
desk controller, it sends a 230ms BREAK signal followed by another `NULL` byte. If still no
response is received, it begins sending `HANDSET(0x29)` repeatedly.

## Known Commands (Handset)

_Note: I don't know any official names for these commands, so I made these up myself._
_Any mistakes here about the command usage or meaning are mine._
_All transmitted byte values are given in hex._


| Name        | CMD  | Params | Description
| ---------   | ---- | ------ | ----------------------------------------
| RAISE       |  01  |    0   | Raise desk by one step
|             |      |        |
| LOWER       |  02  |    0   | Lower desk by one step
|             |      |        |
| PROGMEM\_1  |  03  |    0   | Set memory position 1 to current height
| PROGMEM\_2  |  04  |    0   | Set memory position 2 to current height
|             |      |        |
| MOVE\_1     |  05  |    0   | Move desk to memory position 1
| MOVE\_2     |  06  |    0   | Move desk to memory position 2
|             |      |        |
| SETTINGS    |  07  |    0   | Request all memory heights and settings;
|             |      |        |   On RJ-45, desk will respond with [25] [26] [27] [28] [0E] [19] [17] [1D] and [01]
|             |      |        |   On RJ-12, desk will respond with [25] [26] [27] [28] and [01]
|             |      |        |
| UNKNOWN\_08 |  08  |    0   | Unknown. Desk will respond with [05]
|             |      |        |
| UNKNOWN\_09 |  09  |    0   | Unknown. Desk will respond with [06]
|             |      |        |
| UNKNOWN\_0C |  0C  |    0   | Maybe: Request physical limits of motion. Desk will respond with [07]
|             |      |        |
| UNITS       |  0E  |    1   | Set units to cm/inches
|             |      |        |   P0=0x00  Set units to CM
|             |      |        |   P0=0x01  Set units to IN
|             |      |        |
| MEM\_MODE   |  19  |    1   | Set memory mode
|             |      |        |   P0=0x00  One-touch mode
|             |      |        |   P0=0x01  Constant touch mode
|             |      |        |
| UNKNOWN\_1C |  1C  |    0   | Unknown. Desk will respond with [1C]
|             |      |        |
| COLL\_SENS  |  1D  |    1   | Set anti-collision sensitivity  (Sent 1x; no repeats)
|             |      |        |   P0=0x01  High
|             |      |        |   P0=0x02  Medium
|             |      |        |   P0=0x03  Low
|             |      |        |
| UNKNOWN\_1F |  1F  |    0   | Unknown. Desk will respond with [1F]
|             |      |        |
| LIMITS      |  20  |    0   | Request limit report, desk will respond with [20] and if set [21] and [22]
|             |      |        |
| SET\_MAX    |  21  |    0   | Set max height; Sets max-height to current height
|             |      |        |
| SET\_MIN    |  22  |    0   | Set min height; Sets min-height to current height
|             |      |        |
| LIMIT\_CLR  |  23  |    1   | Clear min/max height
|             |      |        |   P0=0x01  Clear Max-height
|             |      |        |   P0=0x02  Clear Min-height
|             |      |        |
| PROGMEM\_3  |  25  |    0   | Set memory position 3 to current height
| PROGMEM\_4  |  26  |    0   | Set memory position 4 to current height
|             |      |        |
| MOVE\_3     |  27  |    0   | Move desk to memory position 3
| MOVE\_4     |  28  |    0   | Move desk to memory position 4
|             |      |        |
| WAKE        |  29  |    0   | Poll message sent when desk doesn't respond to BREAK messages
|             |      |        |
| CALIBRATE   |  91  |    0   | Height calibration (Repeats 2x)
|             |      |        |   Desk must be at lowest position
|             |      |        |   Desk enters RESET mode after this
|             |      |        |

## Known Responses (Desk)

_Note: I don't know any official names for these commands, so I made these up myself._
_Any mistakes here about the command usage or meaning are mine._
_All transmitted byte values are given in hex._


| Name        | CMD  | Params | Description
| ---------   | ---- | ------ | ----------------------------------------
| HEIGHT      |  01  |    3   | Height report; P0=4 (mm?)
|             |      |        |   {P0,P1} = height in mm or tenths of inches
|             |      |        |   P2 = ??? (0x7 or 0xF seen so far)  Not units
|             |      |        |   Height from 240..530 is in inches
|             |      |        |   Height from 650..1290 is in mm
|             |      |        |
| UNKNOWN\_05 |  05  |    2   | Unknown response to [08]. P0 and P1 have been observed as 00 00 or FF FF
|             |      |        |
| UNKNOWN\_06 |  06  |    1   | Unknown response to [09]. P0 has been observed as 01
|             |      |        |
| UNKNOWN\_07 |  07  |    4   | Maybe: Physical limits of motion. Response to [0C].
|             |      |        | {P0,P1} = upper limit, {P2,P3} = lower limit (in mm)
|             |      |        | Params have been observed as 05 14 02 8A, which would be 1300mm, 650mm
|             |      |        |
| UNITS       |  0E  |    1   | Report units setting
|             |      |        |   P0=0x00  Units set to CM
|             |      |        |   P0=0x01  Units set to IN
|             |      |        |
| UNKNOWN\_17 |  17  |    1   | Unknown response to [07]. P0 has been observed as 01
|             |      |        |
| MEM\_MODE   |  19  |    1   | Report memory mode
|             |      |        |   P0=0x00  One-touch mode
|             |      |        |   P0=0x01  Constant touch mode
|             |      |        |
| UNKNOWN\_1C |  1C  |    1   | Unknown response to [1C]. P0 has been observed as 0A
|             |      |        |
| COLL\_SENS  |  1D  |    1   | Report anti-collision sensitivity
|             |      |        |   P0=0x01  High
|             |      |        |   P0=0x02  Medium
|             |      |        |   P0=0x03  Low
|             |      |        |
| UNKNOWN\_1F |  1F  |    1   | Unknown response to [1F]. P0 has been observed as 00
|             |      |        |
| LIMITS      |  20  |    1   | Report which limits are set; response to [20] through [23]
|             |      |        |   P0=0x00  "Min/max cleared"
|             |      |        |   P0=0x01  "Only max-height set"
|             |      |        |   P0=0x10  "Only min-height set"
|             |      |        |   P0=0x11  "Min and max set"
|             |      |        |
| SET\_MAX    |  21  |    2   | Report max-height; [P0,P1] = max-height
|             |      |        |
| SET\_MIN    |  22  |    2   | Report min-height; [P0,P1] = min-height
|             |      |        |
| LIMIT\_STOP |  23  |    1   | Min/Max reached. Not sent on RJ-12.
|             |      |        |   P0=0x01  "Max-height reached"
|             |      |        |   P0=0x02  "Min-height reached"
|             |      |        |
| POSITION\_1 |  25  |    2   | Memory position 1 height. Response to [07].
| POSITION\_2 |  26  |    2   | Memory position 2 height. Response to [07].
| POSITION\_3 |  27  |    2   | Memory position 3 height. Response to [07].
| POSITION\_4 |  28  |    2   | Memory position 4 height. Response to [07].
|             |      |        |   {P0, P1} = height in mm or tenths of inches
|             |      |        |
| RESET       |  40  |    0   | Indicates desk in RESET mode; Displays "RESET"
|             |      |        |
| REP\_PRESET |  92  |    1   | Moving to Preset location. Not sent on RJ-12.
|             |      |        |   P0=[4,8,10,20] mapping to presets [1,2,3,4]

## Height report

The desk controller responds to the handset by sending a display message containing the current
desk height. It sends this several dozen times while the display is on.  The height is encoded
as a two-byte word (HI, LOW) which make up the desk's height.  The units are in millimeters or
tenths of inches depending on the configuration UNITS setting. The packet looks like this:

    CONTROLLER(0x01, HI, LOW, 0x07)

For example, to indicate 128.6cm (1286mm), the desk sends `CONTROLLER(0x01, 0x05, 0x06, 0x07)`,
because 0x0506 = 1286 in decimal.  I don't know what the third parameter is here (0x07).  Sometimes
it's 0x0F.  More experimenting is needed.

## Memory setting

Memories are programmed by sending one of the `PROGMEM_*` commands to the controller.  Each
command takes no parameters but sets the memory to the current height of the desk.

The desk does not acknowledge the setting.

## Preset buttons observation

When a Preset button is pressed (not in a programming mode) the lines `HS0`..`HS3` are signaled as
shown above in *HSx control lines*. Notice that the binary pattern flagged in those pins maps
preset button 1 to 0x03, 2 to 0x04, 3 to 0x05 and 4 to 0x06.

The command codes to set these presets are mapped similarly, but not identically:

    PROGMEM_1 = 0x03, PROGMEM_2 = 0x04, PROGMEM_3 = 0x25 and PROGMEM_4 = 0x26

Why not the same?  Is it because buttons 3 and 4 were added on later, but commands 0x05/0x06 were already used?
Is bit 5 (0x20) used to signal something special?

## Configuration commands

See the table above for the short version.  Each command may or may not be sent multiple times and
have a different stream of responses from the other side.  I'll try to document my findings about
these anomalies here later.

# DigiStump Oak notes

Robodesk Jarvis v1 was built with a DigiStump Oak. I don't recommend these boards since they seem to have stopped
making them a few years ago. But I leave these notes here for myself:

Install using the ESP8266 Community package here: https://arduino.esp8266.com/stable/package_esp8266com_index.json

First time programming: Connect P2 to GND to start Oak in serial program mode

Thereafter, use the OTA Software Upload described above.

# Related projects

These other projects also aim to decode the Jarvis and/or Uplift protocols.  Some of them got their start from
this project, but they may have added more info and other desks since the time I wrote this.

[Upsy Desky](https://github.com/tjhorner/upsy-desky) (originally [wifi standing desk controller](https://www.tindie.com/products/tjhorner/wifi-standing-desk-controller/))
A [nice custom build](https://www.youtube.com/watch?v=DTJSREjWH7Y) by David Zhang.
A [thread on integrating ESPHome](https://community.home-assistant.io/t/desky-standing-desk-esphome-works-with-desky-uplift-jiecang-assmann-others/383790/16) with Jarvis/Desky/Uplift, etc.
[RMcGibbo's home build](https://github.com/rmcgibbo/Jarvis)
