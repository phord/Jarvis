### Jarvis Standup Desk interface

Technical notes

The Jarvis desk controller I have is the touchpanel interface with memories.  You can surely use
the desk without this interface if you design your own, but you need to know the protocols.  I
try to publish what I discovered through reverse-engineering here.

Handset model: JCHT35M16-1
Desk controller model: FullyCB2C-A

The interface from the handset to the controller is via an RJ-45 8-pin connector (like ethernet).
The pins in the RJ-45 jack are numbered 1 to 8. Following are their usage on the desk:

   1 - HS3 - Handset control line 3 [1]
   2 - Tx  - Serial control messages from controller to handset [2]
   3 - GND - Ground
   4 - Rx  - Serial control messages from handset to controller [2]
   5 - GND - Vcc (5vdc) supply from desk controller [3]
   6 - HS2 - Handset control line 2 [1]
   7 - HS1 - Handset control line 1 [1]
   8 - HS0 - Handset control line 0 [1]

[1] The pins (HS0, HS1, HS2, HS3) make up the "button" presses mux lines. See HSx for description
[2] Serial port UART is 9600bps, 8 data bits, No parity, 1 stop bit
[3] 5VDC supply appears capable of driving at least 300ma
[4] All the signal lines have 5v pullups provided by the desk controller

## HSx control lines
The desk provides 5v pullups on all these signal lines.

On the mechanical desk control buttons, when you press the up button, one of these lines is
pulled low.  Similarly when you press the down button, another line is pulled low.  When you
use a smart controller interface like the touch-screen model, multiple lines may be pulled
low simultaneously to indicate up to 7 different buttons.  If you combine the first three
lines' inverted logical signals, you will find a "control number" between 0 and 7 is possible.
HS3 seems to be a special line used for Memory signalling.

Zero means no button is being pressed.  This is "open".

         Down  Up  1  2  3  4  M
    HS3                        X
    HS2               X  X  X
    HS1         X  X        X
    HS0    X       X     X     X

Translated into binary, these buttons send these codes:
    "Open"  0
    Down    1
    Up      2
    1       3
    2       4
    3       5
    4       6
    M       9  (this is always sent twice)

## UART control lines

The Jarvis UART signaling protocol sends checksummed control packets consisting of between 6
and 9 (??) bytes.

     ADDRESS  [2 bytes]
     PAYLOAD  [2..5 bytes]
     CHECKSUM [1 byte]
     EOM 0x7E [1 byte]

The address byte is repeated two times.  The desk controller always sends 0xF2 0xF2.  The handset
always sends 0xF1 0xF1.

The payload consists of 2 to 5 bytes.  I haven't decoded all the meaning here. WIP

The checksum is the low-byte sum of the all the payload bytes.  In code,

     CHECKSUM = sum(PAYLOAD) % 0xFF

The EOM byte is always 0x7E.

I will use a shorthand to refer to these packets later. The desk controller packets will be
given as CONTROLLER(byte, byte, ...) and the handset packets will be given as HANDSET(byte, byte...).


## Startup
Smart handset sends a NULL (0x00) byte on first startup.  If no response is received from the
desk controller, it sends a 230ms BREAK signal followed by another 0x00 byte. If still no
response is received, it begins sending HANDSET(0x29, 0x00) repeatedly.

## Height report

The desk controller responds to the handset by sending a display message containing the current
desk height. It sends this several dozen times while the display is on.  The height is encoded
as a two-byte word (HI, LOW) which make up the desk's height in mm.  The units are mm regardless
of whether the display is in inches or centimeters.  The packet looks like this:

     CONTROLLER(0x01, 0x02, HI, LOW, 0x07)

For example, to indicate 128.6cm (1286mm), the desk sends CONTROLLER(0x01, 0x02, 0x05, 06, 0x07),
because 0x0506 = 1286 in decimal.

## TBD Other codes
How is the length determined?
What are the other bytes in the payload?
What are the other commands it can send?

## Memory setting

## Configuration commands