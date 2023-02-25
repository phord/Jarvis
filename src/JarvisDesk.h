#ifndef JarvisDesk_h
#define JarvisDesk_h

#include "Arduino.h"

struct JarvisDesk_impl;
class JarvisDesk {
public:

    JarvisDesk();
    ~JarvisDesk();

    void begin();
    void run();
    void report();
    void goto_preset(int p);
    void reset(int duration); // reset and relevel the desk
    void press_memory(int duration);

private:
    JarvisDesk_impl *jarvis;
};

#endif