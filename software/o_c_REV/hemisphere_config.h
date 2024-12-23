// Categories*:
// 0x01 = Modulator
// 0x02 = Sequencer
// 0x04 = Clocking
// 0x08 = Quantizer
// 0x10 = Utility
// 0x20 = MIDI
// 0x40 = Logic
// 0x80 = Other
//
// * Category filtering is deprecated at 1.8, but I'm leaving the per-applet categorization
// alone to avoid breaking forked codebases by other developers.

//////////////////  id  cat   class name


#define HEMISPHERE_APPLETS { \
    DECLARE_APPLET( 34, 0x01, ADEG), \
    DECLARE_APPLET( 47, 0x09, ASR), \
    DECLARE_APPLET( 56, 0x10, AttenuateOffset), \
    DECLARE_APPLET( 41, 0x41, Binary), \
    DECLARE_APPLET( 55, 0x80, BootsNCat), \
    DECLARE_APPLET( 51, 0x80, BugCrack), \
    DECLARE_APPLET( 31, 0x04, Burst), \
    DECLARE_APPLET( 65, 0x10, Button), \
    DECLARE_APPLET( 12, 0x10, Calculate), \
    DECLARE_APPLET(  6, 0x04, ClockDivider), \
    DECLARE_APPLET( 28, 0x04, ClockSkip), \
    DECLARE_APPLET( 30, 0x10, Compare), \
    DECLARE_APPLET(  9, 0x08, DualQuant), \
    DECLARE_APPLET( 18, 0x02, DualTM), \
    DECLARE_APPLET(  7, 0x01, EbbAndLfo), \
    DECLARE_APPLET( 42, 0x11, EnvFollow), \
    DECLARE_APPLET( 15, 0x02, EuclidX), \
    DECLARE_APPLET( 10, 0x44, Logic), \
    DECLARE_APPLET( 50, 0x04, Metronome), \
    DECLARE_APPLET( 73, 0x08, MultiScale), \
    DECLARE_APPLET( 71, 0x02, Pigeons), \
    DECLARE_APPLET( 59, 0x04, ProbabilityDivider), \
    DECLARE_APPLET( 62, 0x04, ProbabilityMelody), \
    DECLARE_APPLET (65, 0x01, Relabi), \
    DECLARE_APPLET( 70, 0x14, ResetClock), \
    DECLARE_APPLET( 69, 0x01, RndWalk), \
    DECLARE_APPLET( 44, 0x01, RunglBook), \
    DECLARE_APPLET( 40, 0x40, Schmitt), \
    DECLARE_APPLET( 23, 0x80, Scope), \
    DECLARE_APPLET( 14, 0x02, SequenceX), \
    DECLARE_APPLET( 48, 0x45, ShiftGate), \
    DECLARE_APPLET( 19, 0x01, Slew), \
    DECLARE_APPLET( 61, 0x01, Stairs), \
    DECLARE_APPLET(  3, 0x10, Switch), \
    DECLARE_APPLET( 13, 0x40, TLNeuron), \
    DECLARE_APPLET( 39, 0x80, Tuner), \
    DECLARE_APPLET( 43, 0x10, Voltage), \
}
/*
    DECLARE_APPLET(127, 0x80, DIAGNOSTIC), \

    DECLARE_APPLET(  8, 0x01, ADSREG), \
    DECLARE_APPLET( 34, 0x01, ADEG), \
    DECLARE_APPLET( 47, 0x09, ASR), \
    DECLARE_APPLET( 56, 0x10, AttenuateOffset), \
    DECLARE_APPLET( 41, 0x41, Binary), \
    DECLARE_APPLET( 55, 0x80, BootsNCat), \
    DECLARE_APPLET( 51, 0x80, BugCrack), \
    DECLARE_APPLET( 31, 0x04, Burst), \
    DECLARE_APPLET( 65, 0x10, Button), \
    DECLARE_APPLET( 12, 0x10, Calculate),\
    DECLARE_APPLET( 32, 0x0a, Carpeggio), \
    DECLARE_APPLET( 64, 0x08, Chordinator), \
    DECLARE_APPLET(  6, 0x04, ClockDivider), \
    DECLARE_APPLET( 28, 0x04, ClockSkip), \
    DECLARE_APPLET( 30, 0x10, Compare), \
    DECLARE_APPLET( 24, 0x02, CVRecV2), \
    DECLARE_APPLET( 57, 0x02, DrumMap), \
    DECLARE_APPLET(  9, 0x08, DualQuant), \
    DECLARE_APPLET( 18, 0x02, DualTM), \
    DECLARE_APPLET(  7, 0x01, EbbAndLfo), \
    DECLARE_APPLET( 45, 0x02, EnigmaJr), \
    DECLARE_APPLET( 42, 0x11, EnvFollow), \
    DECLARE_APPLET( 15, 0x02, EuclidX), \
    DECLARE_APPLET( 22, 0x01, GameOfLife), \
    DECLARE_APPLET( 29, 0x04, GateDelay), \
    DECLARE_APPLET( 17, 0x50, GatedVCA), \
    DECLARE_APPLET( 16, 0x80, LoFiPCM), \
    DECLARE_APPLET( 10, 0x44, Logic), \
    DECLARE_APPLET( 21, 0x01, LowerRenz), \
    DECLARE_APPLET( 50, 0x04, Metronome), \
    DECLARE_APPLET(150, 0x20, hMIDIIn), \
    DECLARE_APPLET( 27, 0x20, hMIDIOut), \
    DECLARE_APPLET( 33, 0x10, MixerBal), \
    DECLARE_APPLET( 73, 0x08, MultiScale), \
    DECLARE_APPLET( 20, 0x02, Palimpsest), \
    DECLARE_APPLET( 71, 0x02, Pigeons), \
    DECLARE_APPLET( 59, 0x04, ProbabilityDivider), \
    DECLARE_APPLET( 62, 0x04, ProbabilityMelody), \
    DECLARE_APPLET( 70, 0x14, ResetClock), \
    DECLARE_APPLET( 69, 0x01, RndWalk), \
    DECLARE_APPLET( 44, 0x01, RunglBook), \
    DECLARE_APPLET( 26, 0x08, ScaleDuet), \
    DECLARE_APPLET( 40, 0x40, Schmitt), \
    DECLARE_APPLET( 23, 0x80, Scope), \
    DECLARE_APPLET( 14, 0x02, SequenceX), \
    DECLARE_APPLET( 48, 0x45, ShiftGate), \
    DECLARE_APPLET( 58, 0x01, Shredder), \
    DECLARE_APPLET( 36, 0x04, Shuffle), \
    DECLARE_APPLET( 19, 0x01, Slew), \
    DECLARE_APPLET( 46, 0x08, Squanch), \
    DECLARE_APPLET( 61, 0x01, Stairs), \
    DECLARE_APPLET(  3, 0x10, Switch), \
    DECLARE_APPLET( 60, 0x02, TB_3PO), \
    DECLARE_APPLET( 13, 0x40, TLNeuron), \
    DECLARE_APPLET( 37, 0x40, Trending), \
    DECLARE_APPLET( 11, 0x06, TrigSeq), \
    DECLARE_APPLET( 25, 0x06, TrigSeq16), \
    DECLARE_APPLET( 39, 0x80, Tuner), \
    DECLARE_APPLET( 52, 0x01, VectorEG), \
    DECLARE_APPLET( 49, 0x01, VectorLFO), \
    DECLARE_APPLET( 53, 0x01, VectorMod), \
    DECLARE_APPLET( 54, 0x01, VectorMorph), \
    DECLARE_APPLET( 43, 0x10, Voltage);
*/
