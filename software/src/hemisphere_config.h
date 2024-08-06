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

#include <utility>
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
//#include "applets/EnsOscKey.h"
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
#include "applets/ShiftReg.h"
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
#ifdef ARDUINO_TEENSY41
// #include "applets/Freeverb.h"
#include "applets/Delay.h"
// #include "applets/Polyform.h"
#endif

//
// template <class A>
// Applet CreateApplet(int id, uint8_t categories) {
//     HemisphereApplet* instances[APPLET_SLOTS];
//     for (int i = 0; i < APPLET_SLOTS; i++) {
//         instances[i] = new A();
//     }
//     return HS::Applet{ id, categories, instances };
// }
static std::tuple<
    StaticApplet<ADSREG, 8, 0x01>,
    StaticApplet<ADEG, 34, 0x01>,
    StaticApplet<ASR, 47, 0x09>,
    StaticApplet<AttenuateOffset, 56, 0x10>,
    StaticApplet<Binary, 41, 0x41>,
    StaticApplet<BootsNCat, 55, 0x80>,
    StaticApplet<Brancher, 4, 0x14>,
    StaticApplet<BugCrack, 51, 0x80>,
    StaticApplet<Burst, 31, 0x04>,
    StaticApplet<Button, 65, 0x10>,
    StaticApplet<Calculate, 12, 0x10>,
    StaticApplet<Calibr8, 88, 0x10>,
    StaticApplet<Carpeggio, 32, 0x0a>,
    StaticApplet<Chordinator, 64, 0x08>,
    StaticApplet<ClockDivider, 6, 0x04>,
    StaticApplet<ClockSkip, 28, 0x04>,
    StaticApplet<Compare, 30, 0x10>,
    StaticApplet<Cumulus, 74, 0x40>,
    StaticApplet<CVRecV2, 24, 0x02>,
    StaticApplet<DivSeq, 68, 0x06>,
    StaticApplet<DrLoFi, 16, 0x80>,
    StaticApplet<DrumMap, 57, 0x02>,
    StaticApplet<DualQuant, 9, 0x08>,
    StaticApplet<DualTM, 18, 0x02>,
    StaticApplet<EbbAndLfo, 7, 0x01>,
    StaticApplet<EnigmaJr, 45, 0x02>,
//    StaticApplet<EnsOscKey, 35, 0x08>,
    StaticApplet<EnvFollow, 42, 0x11>,
    StaticApplet<EuclidX, 15, 0x02>,
    StaticApplet<GameOfLife, 22, 0x01>,
    StaticApplet<GateDelay, 29, 0x04>,
    StaticApplet<GatedVCA, 17, 0x50>,
    StaticApplet<Logic, 10, 0x44>,
    StaticApplet<LowerRenz, 21, 0x01>,
    StaticApplet<Metronome, 50, 0x04>,
    StaticApplet<hMIDIIn, 150, 0x20>,
    StaticApplet<hMIDIOut, 27, 0x20>,
    StaticApplet<MixerBal, 33, 0x10>,
    StaticApplet<MultiScale, 73, 0x08>,
    StaticApplet<Palimpsest, 20, 0x02>,
    StaticApplet<Pigeons, 71, 0x02>,
    StaticApplet<PolyDiv, 72, 0x06>,
    StaticApplet<ProbabilityDivider, 59, 0x04>,
    StaticApplet<ProbabilityMelody, 62, 0x04>,
    StaticApplet<ResetClock, 70, 0x14>,
    StaticApplet<RndWalk, 69, 0x01>,
    StaticApplet<RunglBook, 44, 0x01>,
    StaticApplet<ScaleDuet, 26, 0x08>,
    StaticApplet<Schmitt, 40, 0x40>,
    StaticApplet<Scope, 23, 0x80>,
    StaticApplet<Seq32, 75, 0x02>,
    StaticApplet<SeqPlay7, 76, 0x02>,
    StaticApplet<SequenceX, 14, 0x02>,
    StaticApplet<ShiftGate, 48, 0x45>,
    StaticApplet<ShiftReg, 77, 0x45>,
    StaticApplet<Shredder, 58, 0x01>,
    StaticApplet<Shuffle, 36, 0x04>,
    StaticApplet<Slew, 19, 0x01>,
    StaticApplet<Squanch, 46, 0x08>,
    StaticApplet<Stairs, 61, 0x01>,
    StaticApplet<Strum, 74, 0x08>,
    StaticApplet<Switch, 3, 0x10>,
    StaticApplet<SwitchSeq, 38, 0x10>,
    StaticApplet<TB_3PO, 60, 0x02>,
    StaticApplet<TLNeuron, 13, 0x40>,
    StaticApplet<Trending, 37, 0x40>,
    StaticApplet<TrigSeq, 11, 0x06>,
    StaticApplet<TrigSeq16, 25, 0x06>,
    StaticApplet<Tuner, 39, 0x80>,
    StaticApplet<VectorEG, 52, 0x01>,
    StaticApplet<VectorLFO, 49, 0x01>,
    StaticApplet<VectorMod, 53, 0x01>,
    StaticApplet<VectorMorph, 54, 0x01>,
    StaticApplet<Voltage, 43, 0x10>
    #ifdef ARDUINO_TEENSY41
    , StaticApplet<Delay, 76, 0x00>
    // , StaticApplet<Polyform, 76, 0x00>
    // , StaticApplet<Freeverb, 75, 0x00>
    #endif
> applet_tuple;

template <typename Base, typename Tuple, size_t... Is>
std::array<Base *, std::tuple_size<Tuple>::value> tuple_to_array(Tuple &tuple, std::integer_sequence<size_t, Is...>) {
    return std::array<Base *, std::tuple_size<Tuple>::value>{ &std::get<Is>(tuple)... };
}

template <typename Base, typename Tuple>
std::array<Base *, std::tuple_size<Tuple>::value> tuple_to_array(Tuple &tuple) {
    return tuple_to_array<Base>(tuple, std::make_index_sequence<std::tuple_size<Tuple>::value>());
}

namespace HS {
  constexpr static int HEMISPHERE_AVAILABLE_APPLETS = std::tuple_size<decltype(applet_tuple)>();
  static auto available_applets = tuple_to_array<BaseApplet>(applet_tuple);

  // TODO: figure out where to store this
  uint64_t hidden_applets[2] = { 0, 0 };
  bool applet_is_hidden(const int& index) {
    return (hidden_applets[index/64] >> (index%64)) & 1;
  }
  void showhide_applet(const int& index) {
    const int seg = index/64;
    hidden_applets[seg] = hidden_applets[seg] ^ (uint64_t(1) << (index%64));
  }

  int get_applet_index_by_id(const int& id) {
    int index = 0;
    for (int i = 0; i < HEMISPHERE_AVAILABLE_APPLETS; i++)
    {
        if (available_applets[i]->id == id) index = i;
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
