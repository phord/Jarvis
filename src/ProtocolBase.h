// Protocol base class and helper utilities

struct ProtocolBase {

  unsigned char cmd;
  unsigned char addr;
  unsigned char argc = 0;
  unsigned char argv[5];

  ProtocolBase(unsigned char addr_) : addr(addr_) {}

  // returns true when a message is decoded and ready to parse in {cmd, argc, argv}
  virtual bool put(unsigned char ch) = 0;

  // Decode a completed packet and react accordingly
  virtual void decode() = 0;

  // Reset this structure defaults
  virtual void reset() = 0;

  // Helpers

  // Print the interpreted configuration setting from a list of options using
  // argv[0] as the index.
  template<class ...Args>
  void config(const char * field, Args... args) {
    int n = argc ? argv[0] : -1;
    print_choice(field, n, {args...});
  }

  void print_choice(const char *field, int n, std::vector<const char *> args);
  void dump();
};


struct Util {
  static unsigned int getword(unsigned char a, unsigned char b) {
    return (static_cast<unsigned>(a) << 8) + b;
  }

  static unsigned to_mm(unsigned h) {
    if (h < 600) {
      // Height in inches*10; convert to mm
      h *= 254;  // convert to mm*100
      h += 50;   // round up to nearest whole mm
      h /= 100;  // convert to mm
    }
    return h;
  }
};
