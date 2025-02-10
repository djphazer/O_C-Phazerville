#ifndef OC_PATTERNS_H_
#define OC_PATTERNS_H_

#include <Arduino.h>

namespace OC {

  struct Pattern {
    // length + mask is stored within app
    int16_t notes[16];
  };

  // TODO: this is dumb
  static const Pattern patterns[] = {
    // default
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
  };

  static constexpr int kMaxPatternLength = 16;
  static constexpr int kMinPatternLength = 2;

  namespace Patterns {
    enum {
      PATTERN_USER_0_1,
      PATTERN_USER_1_1,
      PATTERN_USER_2_1,
      PATTERN_USER_3_1,
      PATTERN_USER_0_2,
      PATTERN_USER_1_2,
      PATTERN_USER_2_2,
      PATTERN_USER_3_2, 
      PATTERN_USER_COUNT,
    };

    static const int PATTERN_NONE = -1;
    static const int NUM_PATTERNS_PER_CHAN = PATTERN_USER_COUNT / 2;

    static const int kMin = kMinPatternLength;
    static const int kMax = kMaxPatternLength;
  };

  extern const char *const pattern_names[];
  extern const char *const pattern_names_short[];
  extern Pattern user_patterns[Patterns::PATTERN_USER_COUNT];
  extern Pattern dummy_pattern;

};

#endif // OC_PATTERNS_H_
