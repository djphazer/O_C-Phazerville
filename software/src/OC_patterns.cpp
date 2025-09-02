#include "OC_patterns.h"

namespace OC {

    Pattern user_patterns[Patterns::PATTERN_USER_COUNT];
    Pattern dummy_pattern;

    const char* const pattern_names_short[] = {
        "SEQ-1",
        "SEQ-2",
        "SEQ-3",
        "SEQ-4",
        "DEFAULT"
    };

    const char* const pattern_names[] = {
        "User-sequence 1",
        "User-sequence 2",
        "User-sequence 3",
        "User-sequence 4",
        "DEFAULT"
    }; 
} // namespace OC
