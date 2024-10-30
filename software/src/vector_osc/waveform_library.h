namespace HS {

#define VO_TOC 255
const byte WAVEFORM_LIBRARY_COUNT = 32;

// Waveform Library Names
enum {
    Triangle = 32,
    Sawtooth,
    Ramp,
    Sine,
	Cosine,
    Square,
    Pulse2,
    Pulse3,
    Pulse4,
    Trapezoid,
    TriPeak,
    UpAndDown,
    Logarithmic,
    Exponential,
    Diminishing,
    Step,
	EG1 = 48,
    EG2,
    EG3,
    PositiveTriangle,
    PositiveSawtooth,
    PositiveRamp,
    PositiveSquare,
    PositiveSine,
    HalfRectifiedSine,
    Morph1 = 57,
    Morph2,
    Morph3,
    Morph4,
    Morph5,
    Morph6,
    Morph7,
};

VOSegment library_waveforms[] = {
    // Triangle
    VOSegment {2, VO_TOC}, // 255=TOC marker, and 2=Number of segments in this waveform
    VOSegment {255, 1},
    VOSegment {0, 1},

    // Sawtooth
    VOSegment {2, VO_TOC},
    VOSegment {255, 0},
    VOSegment {0, 1},

    // Ramp
    VOSegment {2, VO_TOC},
    VOSegment {0, 0},
    VOSegment {255, 1},

    // Sine
    VOSegment {12, VO_TOC},
    VOSegment {191, 1},
    VOSegment {238, 1},
    VOSegment {255, 1},
    VOSegment {238, 1},
    VOSegment {191, 1},
    VOSegment {128, 1},
    VOSegment {64, 1},
    VOSegment {17, 1},
    VOSegment {0, 1},
    VOSegment {17, 1},
    VOSegment {64, 1},
    VOSegment {128, 1},

    // Cosine
    VOSegment {12, VO_TOC},
    VOSegment {64, 1},
    VOSegment {18, 1},
    VOSegment {0, 1},
    VOSegment {18, 1},
    VOSegment {64, 1},
    VOSegment {128, 1},
    VOSegment {192, 1},
    VOSegment {238, 1},
    VOSegment {255, 1},
    VOSegment {238, 1},
    VOSegment {192, 1},
    VOSegment {128, 1},

	// Square
    VOSegment {4, VO_TOC},
    VOSegment {255, 0},
    VOSegment {255, 1},
    VOSegment {0, 0},
    VOSegment {0, 1},

    // Pulse 2:1
    VOSegment {4, VO_TOC},
    VOSegment {255, 0},
    VOSegment {255, 2},
    VOSegment {0, 0},
    VOSegment {0, 1},

    // Pulse 3:1
    VOSegment {4, VO_TOC},
    VOSegment {255, 0},
    VOSegment {255, 3},
    VOSegment {0, 0},
    VOSegment {0, 1},

    // Pulse 4:1
    VOSegment {4, VO_TOC},
    VOSegment {255, 0},
    VOSegment {255, 4},
    VOSegment {0, 0},
    VOSegment {0, 1},

    // Trapezoid
    VOSegment {3, VO_TOC},
    VOSegment {255, 1},
    VOSegment {255, 3},
    VOSegment {0, 1},

	// Tri Peak LFO
	VOSegment {11, VO_TOC},
	VOSegment {255, 1},
	VOSegment {128, 1},
	VOSegment {176, 1},
	VOSegment {128, 1},
	VOSegment {255, 1},
	VOSegment {0, 1},
	VOSegment {128, 1},
	VOSegment {80, 1},
	VOSegment {128, 1},
	VOSegment {0, 1},
	VOSegment {128,1},

	// Up and Down
	VOSegment {10, VO_TOC},
	VOSegment {160, 0},
	VOSegment {160, 1},
	VOSegment {30, 0},
	VOSegment {30, 1},
	VOSegment {255, 0},
	VOSegment {255, 1},
	VOSegment {100, 0},
	VOSegment {100, 1},
	VOSegment {0, 0},
	VOSegment {0, 1},

    // Log
    VOSegment {12, VO_TOC},
    VOSegment {255, 0},
    VOSegment {254, 1},
    VOSegment {251, 1},
    VOSegment {243, 1},
    VOSegment {237, 1},
    VOSegment {226, 1},
    VOSegment {207, 1},
    VOSegment {179, 1},
    VOSegment {141, 1},
    VOSegment {84, 1},
    VOSegment {0, 1},
    VOSegment {0, 0},

    // Exp
    VOSegment {12, VO_TOC},
    VOSegment {255, 0},
    VOSegment {186, 1},
    VOSegment {139, 1},
    VOSegment {101, 1},
    VOSegment {75, 1},
    VOSegment {53, 1},
    VOSegment {35, 1},
    VOSegment {18, 1},
    VOSegment {7, 1},
    VOSegment {4, 1},
    VOSegment {0, 1},
    VOSegment {0, 0},

    // Diminishing Saw
    VOSegment {8, VO_TOC},
    VOSegment {255, 1},
    VOSegment {0, 0},
    VOSegment {192, 1},
    VOSegment {0, 0},
    VOSegment {128, 1},
    VOSegment {0, 0},
    VOSegment {64, 1},
    VOSegment {0, 0},

    // Step
    VOSegment {9, VO_TOC},
    VOSegment {255, 0},
    VOSegment {255, 1},
    VOSegment {192, 0},
    VOSegment {192, 1},
    VOSegment {128, 0},
    VOSegment {128, 1},
    VOSegment {64, 0},
    VOSegment {64, 1},
    VOSegment {0, 0},

    // EG1
    VOSegment {4, VO_TOC},
    VOSegment {255, 0},
    VOSegment {128, 2},
    VOSegment {128, 1},
    VOSegment {0, 2},

    // EG2
    VOSegment {4, VO_TOC},
    VOSegment {255, 1},
    VOSegment {128, 2},
    VOSegment {128, 1},
    VOSegment {0, 2},

    // EG3
    VOSegment {5, VO_TOC},
    VOSegment {218, 0},
    VOSegment {255, 1},
    VOSegment {128, 1},
    VOSegment {192, 1},
    VOSegment {0, 1},

    // Positive Triangle
    VOSegment {2, VO_TOC},
    VOSegment {255, 1},
    VOSegment {128, 1},

    // Positive Sawtooth
    VOSegment {2, VO_TOC},
    VOSegment {255, 0},
    VOSegment {128, 1},

    // Positive Ramp
    VOSegment {2, VO_TOC},
    VOSegment {128, 0},
    VOSegment {255, 1},

    // Positive Square
    VOSegment {4, VO_TOC},
    VOSegment {255, 0},
    VOSegment {255, 1},
    VOSegment {128, 0},
    VOSegment {128, 1},

    // Positive Sine
    VOSegment {12, VO_TOC},
    VOSegment {128, 1},
    VOSegment {137, 1},
    VOSegment {160, 1},
    VOSegment {192, 1},
    VOSegment {224, 1},
    VOSegment {247, 1},
    VOSegment {255, 1},
    VOSegment {247, 1},
    VOSegment {224, 1},
    VOSegment {192, 1},
    VOSegment {160, 1},
    VOSegment {137, 1},

    // Half-Rectified Sine
    VOSegment {7, VO_TOC},
    VOSegment {192, 1},
    VOSegment {238, 1},
    VOSegment {255, 1},
    VOSegment {238, 1},
    VOSegment {191, 1},
    VOSegment {128, 1},
    VOSegment {128, 6},

	// Morph Wave 1
	VOSegment {3, VO_TOC},
	VOSegment {255, 1},
	VOSegment {128, 1},
	VOSegment {128, 1},

	// Morph Wave 2
	VOSegment {3, VO_TOC},
	VOSegment {255, 1},
	VOSegment {128, 1},
	VOSegment {128, 2},

	// Morph Wave 3
	VOSegment {3, VO_TOC},
	VOSegment {255, 1},
	VOSegment {128, 1},
	VOSegment {128, 4},

	// Morph Wave 4
	VOSegment {3, VO_TOC},
	VOSegment {255, 2},
	VOSegment {128, 2},
	VOSegment {128, 1},

	// Morph Wave 5
	VOSegment {5, VO_TOC},
	VOSegment {255, 1},
	VOSegment {128, 1},
	VOSegment {220, 1},
	VOSegment {128, 1},
	VOSegment {128, 4},

	// Morph Wave 6
	VOSegment {3, VO_TOC},
	VOSegment {255, 3},
	VOSegment {128, 1},
	VOSegment {128, 2},

	// Morph Wave 7: This... MEANS... something!
	VOSegment {6, VO_TOC},
	VOSegment {148, 0},
	VOSegment {180, 5},
	VOSegment {250, 2},
	VOSegment {255, 1},
	VOSegment {180, 2},
	VOSegment {148, 5},
};

}; // namespace HS
