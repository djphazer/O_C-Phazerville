#pragma once

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

#ifdef ARDUINO_TEENSY41
// Teensy 4.1 can run four applets in Quadrasphere
#define CREATE_APPLET(class_name) \
class_name class_name ## _instance[4]

#define DECLARE_APPLET(id, categories, class_name) \
{ id, categories, { \
  &class_name ## _instance[0], &class_name ## _instance[1], \
  &class_name ## _instance[2], &class_name ## _instance[3] \
  } }

#else
#define CREATE_APPLET(class_name) \
class_name class_name ## _instance[2]

#define DECLARE_APPLET(id, categories, class_name) \
{ id, categories, { &class_name ## _instance[0], &class_name ## _instance[1] } }
#endif

#include "applets/ADSREG.h"
#include "applets/ADEG.h"
#include "applets/ASR.h"
#include "applets/AttenuateOffset.h"
#include "applets/Binary.h"
#include "applets/BootsNCat.h"
#include "applets/Brancher.h"
#include "applets/BugCrack.h"
#include "applets/Burst.h"
#include "applets/Button.h"
#include "applets/Cumulus.h"
#include "applets/CVRecV2.h"
#include "applets/Calculate.h"
#include "applets/Calibr8.h"
#include "applets/Carpeggio.h"
#include "applets/Chordinator.h"
#include "applets/ClockDivider.h"
#ifdef ARDUINO_TEENSY41
#include "applets/ClockSetupT4.h"
#else
#include "applets/ClockSetup.h"
#endif
#include "applets/ClockSkip.h"
#include "applets/Compare.h"
#include "applets/DivSeq.h"
#include "applets/DrumMap.h"
#include "applets/DualQuant.h"
#include "applets/DualTM.h"
#include "applets/EbbAndLfo.h"
#include "applets/EnigmaJr.h"
#include "applets/EnsOscKey.h"
#include "applets/EnvFollow.h"
#include "applets/EuclidX.h"
#include "applets/GameOfLife.h"
#include "applets/GateDelay.h"
#include "applets/GatedVCA.h"
#include "applets/DrLoFi.h"
#include "applets/Logic.h"
#include "applets/LowerRenz.h"
#include "applets/Metronome.h"
#include "applets/MixerBal.h"
#include "applets/MultiScale.h"
#include "applets/Palimpsest.h"
#include "applets/Pigeons.h"
#include "applets/PolyDiv.h"
#include "applets/ProbabilityDivider.h"
#include "applets/ProbabilityMelody.h"
#include "applets/ResetClock.h"
#include "applets/RndWalk.h"
#include "applets/RunglBook.h"
#include "applets/ScaleDuet.h"
#include "applets/Schmitt.h"
#include "applets/Scope.h"
#include "applets/SequenceX.h"
#include "applets/Seq32.h"
#include "applets/SeqPlay7.h"
#include "applets/ShiftGate.h"
#include "applets/Shredder.h"
#include "applets/Shuffle.h"
#include "applets/Slew.h"
#include "applets/Squanch.h"
#include "applets/Stairs.h"
#include "applets/Strum.h"
#include "applets/Switch.h"
#include "applets/SwitchSeq.h"
#include "applets/TB3PO.h"
#include "applets/TLNeuron.h"
#include "applets/Trending.h"
#include "applets/TrigSeq.h"
#include "applets/TrigSeq16.h"
#include "applets/Tuner.h"
#include "applets/VectorEG.h"
#include "applets/VectorLFO.h"
#include "applets/VectorMod.h"
#include "applets/VectorMorph.h"
#include "applets/Voltage.h"
#include "applets/hMIDIIn.h"
#include "applets/hMIDIOut.h"


CREATE_APPLET(Cumulus);
CREATE_APPLET(ADSREG);
CREATE_APPLET(ADEG);
CREATE_APPLET(AttenuateOffset);
CREATE_APPLET(BugCrack);
CREATE_APPLET(DrumMap);
CREATE_APPLET(DualTM);
CREATE_APPLET(EbbAndLfo);
CREATE_APPLET(EuclidX);
CREATE_APPLET(hMIDIIn);
CREATE_APPLET(hMIDIOut);
CREATE_APPLET(Pigeons);
CREATE_APPLET(Stairs);
CREATE_APPLET(TB_3PO);
CREATE_APPLET(Voltage);

CREATE_APPLET(Calibr8);
CREATE_APPLET(DivSeq);
CREATE_APPLET(PolyDiv);
CREATE_APPLET(Strum);

CREATE_APPLET(ASR);
CREATE_APPLET(Binary);
CREATE_APPLET(BootsNCat);
CREATE_APPLET(Brancher);
CREATE_APPLET(Burst);
CREATE_APPLET(Button);
CREATE_APPLET(Calculate);
CREATE_APPLET(Carpeggio);
CREATE_APPLET(Chordinator);
CREATE_APPLET(ClockDivider);
CREATE_APPLET(ClockSkip);
CREATE_APPLET(Compare);
CREATE_APPLET(CVRecV2);
CREATE_APPLET(DualQuant);
CREATE_APPLET(EnigmaJr);
CREATE_APPLET(EnsOscKey);
CREATE_APPLET(EnvFollow);
CREATE_APPLET(GameOfLife);
CREATE_APPLET(GateDelay);
CREATE_APPLET(GatedVCA);
CREATE_APPLET(DrLoFi);
CREATE_APPLET(Logic);
CREATE_APPLET(LowerRenz);
CREATE_APPLET(Metronome);
CREATE_APPLET(MixerBal);
CREATE_APPLET(MultiScale);
CREATE_APPLET(Palimpsest);
CREATE_APPLET(ProbabilityDivider);
CREATE_APPLET(ProbabilityMelody);
CREATE_APPLET(ResetClock);
CREATE_APPLET(RndWalk);
CREATE_APPLET(RunglBook);
CREATE_APPLET(ScaleDuet);
CREATE_APPLET(Schmitt);
CREATE_APPLET(Scope);
CREATE_APPLET(SequenceX);
CREATE_APPLET(Seq32);
CREATE_APPLET(SeqPlay7);
CREATE_APPLET(ShiftGate);
CREATE_APPLET(Shredder);
CREATE_APPLET(Shuffle);
CREATE_APPLET(Slew);
CREATE_APPLET(Squanch);
CREATE_APPLET(Switch);
CREATE_APPLET(SwitchSeq);
CREATE_APPLET(TLNeuron);
CREATE_APPLET(Trending);
CREATE_APPLET(TrigSeq);
CREATE_APPLET(TrigSeq16);
CREATE_APPLET(Tuner);
CREATE_APPLET(VectorEG);
CREATE_APPLET(VectorLFO);
CREATE_APPLET(VectorMod);
CREATE_APPLET(VectorMorph);

//////////////////  id  cat   class name
#define HEMISPHERE_APPLETS { \
    DECLARE_APPLET(  8, 0x01, ADSREG), \
    DECLARE_APPLET( 34, 0x01, ADEG), \
    DECLARE_APPLET( 47, 0x09, ASR), \
    DECLARE_APPLET( 56, 0x10, AttenuateOffset), \
    DECLARE_APPLET( 41, 0x41, Binary), \
    DECLARE_APPLET( 55, 0x80, BootsNCat), \
    DECLARE_APPLET(  4, 0x14, Brancher), \
    DECLARE_APPLET( 51, 0x80, BugCrack), \
    DECLARE_APPLET( 31, 0x04, Burst), \
    DECLARE_APPLET( 65, 0x10, Button), \
    DECLARE_APPLET( 12, 0x10, Calculate),\
    DECLARE_APPLET( 88, 0x10, Calibr8), \
    DECLARE_APPLET( 32, 0x0a, Carpeggio), \
    DECLARE_APPLET( 64, 0x08, Chordinator), \
    DECLARE_APPLET(  6, 0x04, ClockDivider), \
    DECLARE_APPLET( 28, 0x04, ClockSkip), \
    DECLARE_APPLET( 30, 0x10, Compare), \
    DECLARE_APPLET( 74, 0x40, Cumulus), \
    DECLARE_APPLET( 24, 0x02, CVRecV2), \
    DECLARE_APPLET( 68, 0x06, DivSeq), \
    DECLARE_APPLET( 16, 0x80, DrLoFi), \
    DECLARE_APPLET( 57, 0x02, DrumMap), \
    DECLARE_APPLET(  9, 0x08, DualQuant), \
    DECLARE_APPLET( 18, 0x02, DualTM), \
    DECLARE_APPLET(  7, 0x01, EbbAndLfo), \
    DECLARE_APPLET( 45, 0x02, EnigmaJr), \
    DECLARE_APPLET( 35, 0x08, EnsOscKey), \
    DECLARE_APPLET( 42, 0x11, EnvFollow), \
    DECLARE_APPLET( 15, 0x02, EuclidX), \
    DECLARE_APPLET( 22, 0x01, GameOfLife), \
    DECLARE_APPLET( 29, 0x04, GateDelay), \
    DECLARE_APPLET( 17, 0x50, GatedVCA), \
    DECLARE_APPLET( 10, 0x44, Logic), \
    DECLARE_APPLET( 21, 0x01, LowerRenz), \
    DECLARE_APPLET( 50, 0x04, Metronome), \
    DECLARE_APPLET(150, 0x20, hMIDIIn), \
    DECLARE_APPLET( 27, 0x20, hMIDIOut), \
    DECLARE_APPLET( 33, 0x10, MixerBal), \
    DECLARE_APPLET( 73, 0x08, MultiScale), \
    DECLARE_APPLET( 20, 0x02, Palimpsest), \
    DECLARE_APPLET( 71, 0x02, Pigeons), \
    DECLARE_APPLET( 72, 0x06, PolyDiv), \
    DECLARE_APPLET( 59, 0x04, ProbabilityDivider), \
    DECLARE_APPLET( 62, 0x04, ProbabilityMelody), \
    DECLARE_APPLET( 70, 0x14, ResetClock), \
    DECLARE_APPLET( 69, 0x01, RndWalk), \
    DECLARE_APPLET( 44, 0x01, RunglBook), \
    DECLARE_APPLET( 26, 0x08, ScaleDuet), \
    DECLARE_APPLET( 40, 0x40, Schmitt), \
    DECLARE_APPLET( 23, 0x80, Scope), \
    DECLARE_APPLET( 75, 0x02, Seq32), \
    DECLARE_APPLET( 76, 0x02, SeqPlay7), \
    DECLARE_APPLET( 14, 0x02, SequenceX), \
    DECLARE_APPLET( 48, 0x45, ShiftGate), \
    DECLARE_APPLET( 58, 0x01, Shredder), \
    DECLARE_APPLET( 36, 0x04, Shuffle), \
    DECLARE_APPLET( 19, 0x01, Slew), \
    DECLARE_APPLET( 46, 0x08, Squanch), \
    DECLARE_APPLET( 61, 0x01, Stairs), \
    DECLARE_APPLET( 74, 0x08, Strum), \
    DECLARE_APPLET(  3, 0x10, Switch), \
    DECLARE_APPLET( 38, 0x10, SwitchSeq), \
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
    DECLARE_APPLET( 43, 0x10, Voltage), \
}
/*
    DECLARE_APPLET(127, 0x80, DIAGNOSTIC), \
*/


namespace HS {
  static constexpr Applet available_applets[] = HEMISPHERE_APPLETS;
  static constexpr int HEMISPHERE_AVAILABLE_APPLETS = ARRAY_SIZE(available_applets);

  // TODO: needs to be larger than 64 bits...
  // TODO: also figure out where to store this
  uint64_t hidden_applets = 0;
  bool applet_is_hidden(const int& index) {
    return (hidden_applets >> index) & 1;
  }
  void showhide_applet(const int& index) {
    hidden_applets = hidden_applets ^ (uint64_t(1) << index);
  }

  constexpr int get_applet_index_by_id(const int& id) {
    int index = 0;
    for (int i = 0; i < HEMISPHERE_AVAILABLE_APPLETS; i++)
    {
        if (available_applets[i].id == id) index = i;
    }
    return index;
  }

  int get_next_applet_index(int index, const int dir) {
    do {
      index += dir;
      if (index >= HEMISPHERE_AVAILABLE_APPLETS) index = 0;
      if (index < 0) index = HEMISPHERE_AVAILABLE_APPLETS - 1;
    } while (applet_is_hidden(index));

    return index;
  }
}
