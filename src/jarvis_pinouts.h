#pragma once
#include <Arduino.h>

// There are a few different protocols for controllers.
// Uncomment the one you have.
// Uplift Desk V2
#define JCB35N2PA32V2

// Define minimum and maximum desk height in mm
// #define MIN_HEIGHT 0
// #define MAX_HEIGHT 5000
#define MIN_HEIGHT 640
#define MAX_HEIGHT 1300

// You need to define pins for 4 to 6 GPIOs to connect to your Jarvis RJ-45 cable.
// 4 pins go to the handset buttons. These encode the Up, Down, Memory and Presets (1-4) buttons.
// 2 pins go to the serial TX/RX pins between the handset and the desk controller.
//   * Since we only receive on these pins, I call them "desk transmit (DTX)" and handset transmit (HTX)
//   * HTX is optional; I only use this for experimenting with the handset serial commands.
//   * HS3 is optional; it is not needed for controlling the Jarvis Desk, afaict.
//
// RJ-45   Signal   Description
//  pin     ID
//   1      HS3     Handset control line 3
//   2      DTX     Serial control messages from controller to handset
//   3      GND
//   4      HTX     Serial control messages from handset to controller
//   5      VCC 5v
//   6      HS2     Handset control line 2
//   7      HS1     Handset control line 1
//   8      HS0     Handset control line 0

// Pin definition configurations.  Uncomment one of these lines, or define your own with DEFINE_PINS below.
// #define JARVISDESK_DIGISTUMP_OAK_V1
// #define JARVISDESK_WEMOS_D1MINI_V1
// #define JARVISDESK_WEMOS_D1MINI_V2
#define JARVISDESK_NODEMCU_V3

// -----------------------------------------------------------------------------

#define NC -1   // Use NC for pins that are not connected or not used
#define DEFPIN(name, pin) static constexpr int name = pin;
// Helper macro to define 6 pins in one easy list.
#define DEFINE_PINS(p1, p2, p4, p6, p7, p8) \
    DEFPIN(HS3, p1);  \
    DEFPIN(DTX, p2);  \
    DEFPIN(HTX, p4);  \
    DEFPIN(HS2, p6);  \
    DEFPIN(HS1, p7);  \
    DEFPIN(HS0, p8);

#ifdef JARVISDESK_DIGISTUMP_OAK_V1
    // Pinouts for Esp8266 Oak (which uses these "P" numbers)
    // Note: I had HS3=P2 originally, but it wasn't actually connected on my board and it was commented out in the code.
    // RJ45Pin:  1   2   4   6   7   8
    //  Signal: HS3 DTX HTX HS2 HS1 HS0
    DEFINE_PINS(NC, P3, P4, P9, P8, P10);
#endif

#ifdef JARVISDESK_WEMOS_D1MINI_V1
    // Pinouts for ESp8266 Wemos D1 mini Version 1, using protoboard
    // RJ45Pin:  1   2   4   6   7   8
    //  Signal: HS3 DTX HTX HS2 HS1 HS0
    DEFINE_PINS(D1, D2, D3, D5, D0, D6);
#endif

#ifdef JARVISDESK_WEMOS_D1MINI_V2
    // Pinouts for ESp8266 Wemos D1 mini PCB v2.x, using PCB
    // RJ45Pin:  1   2   4   6   7   8
    //  Signal: HS3 DTX HTX HS2 HS1 HS0
    DEFINE_PINS(D5, D2, D1, D0, D6, D7);
#endif

#ifdef JARVISDESK_NODEMCU_V3
    // Pinouts for ESp8266 Wemos D1 mini PCB v2.x, using PCB
    // RJ45Pin:  1   2   4   6   7   8
    //  Signal: HS3 DTX HTX HS2 HS1 HS0
    DEFINE_PINS(D2, D7, D8, D6, D1, D0);
#endif

inline bool is_pin_connected(int pin) { return pin != NC; }