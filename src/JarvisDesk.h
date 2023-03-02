#ifndef JarvisDesk_h
#define JarvisDesk_h

#include <Arduino.h>

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

  // Desk reactions for Protocols

  // Announce: User commanded move to a preset
  void set_preset(unsigned char p);

  // Announce: the desk height
  void set_height(unsigned int h);

  // Announce: Store the current height in a given preset memory
  void program_preset(unsigned memset);

private:
  JarvisDesk_impl *jarvis;
};

#endif