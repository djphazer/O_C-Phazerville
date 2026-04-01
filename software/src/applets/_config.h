#pragma once

#include "HemisphereApplet.h"

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
// *currently unused, but may be useful again someday

using namespace HS;

// hacks to effectively rewrite part of the applet boilerplate,
// making names and icons static
#define applet_name applet_name() override { return applet_name_(); } \
  static constexpr const char* applet_name_

#define applet_icon applet_icon() override { return applet_icon_(); } \
  static constexpr const uint8_t* applet_icon_

#include "ADSREG.h"
#include "ADEG.h"
#include "ASR.h"
#include "AttenuateOffset.h"
#ifdef PEWPEWPEW
#include "Binary.h"
#endif
#include "BootsNCat.h"
#include "Brancher.h"
#include "BugCrack.h"
#include "Burst.h"
#include "Button.h"
#include "BitBeat.h"
#include "Cumulus.h"
#include "CVRecV2.h"
#include "Calculate.h"
#include "Calibr8.h"
#include "Carpeggio.h"
#ifdef PEWPEWPEW
#include "Chordinator.h"
#endif
#include "ClockDivider.h"
#include "ClkToGate.h"
#ifdef ARDUINO_TEENSY41
#include "ClockSetupT4.h"
#else
#include "ClockSetup.h"
#endif
#include "ClockSkip.h"
#include "Combin8.h"
#include "Compare.h"
#include "DivSeq.h"
#include "DivSeq10.h"
#include "DrumMap.h"
#include "DualQuant.h"
#ifdef PEWPEWPEW
#include "OffsetQuant.h"
#endif
#include "TwoRings.h"
#if !defined(CUSTOM_BUILD) || defined(PEWPEWPEW)
#include "DuoTET.h"
#endif
#include "EbbAndLfo.h"
#ifdef ENABLE_APP_ENIGMA
#include "EnigmaJr.h"
#endif
#ifdef PEWPEWPEW
#include "EnsOscKey.h"
#endif
#include "EnvFollow.h"
#include "EnvSeq.h"
#include "EuclidO.h"
#include "EuclidX.h"
#ifdef PEWPEWPEW
#include "GameOfLife.h"
#endif
#include "GateDelay.h"
#include "GatedVCA.h"
#include "DrLoFi.h"
#include "Logic.h"
#include "LowerRenz.h"
#include "Metronome.h"
#ifdef __IMXRT1062__
#include "MidiLoop.h"
#endif
#ifdef PEWPEWPEW
#include "MultiScale.h"
#endif
#include "Palimpsest.h"
#include "Pigeons.h"
#include "PolyDiv.h"
#include "ProbabilityDivider.h"
#include "ProbabilityMelody.h"
#include "Relabi.h"
#include "ResetClock.h"
#include "RndWalk.h"
#ifdef PEWPEWPEW
#include "RunglBook.h"
#endif
#include "ScaleDuet.h"
#include "Schmitt.h"
#include "Scope.h"
#include "SequenceX.h"
#include "Seq32.h"
#include "SeqPlay7.h"
#include "ShiftGate.h"
#ifdef PEWPEWPEW
#include "ShiftReg.h"
#endif
#include "Shredder.h"
#include "Shuffle.h"
#include "Slew.h"
#include "Squanch.h"
#include "Stairs.h"
#include "Strum.h"
#include "Switch.h"
#include "SwitchSeq.h"
#include "TB3PO.h"
#include "TLNeuron.h"
#ifdef PEWPEWPEW
#include "Trending.h"
#endif
#include "TrigSeq.h"
#include "TrigSeq16.h"
#include "Tuner.h"
#include "VectorEG.h"
#include "VectorLFO.h"
#include "VectorMod.h"
#include "VectorMorph.h"
#include "Voltage.h"
#include "MarkoV.h"
#include "MarkovPerc.h"
#ifdef PEWPEWPEW
#include "WTVCO.h"
#endif
#include "Xfader.h"
#include "hMIDIIn.h"
#include "hMIDIOut.h"

#undef applet_name
#undef applet_icon

#include "AppletRegistry.h"

constexpr Registry reg = Registry<HemisphereApplet, 200 // max ID
    , DeclareApplet<ADSREG, 8, 0x01>
    , DeclareApplet<ADEG, 34, 0x01>
    , DeclareApplet<MiniASR, 47, 0x09>
    , DeclareApplet<AttenuateOffset, 56, 0x10>
#ifdef PEWPEWPEW
    , DeclareApplet<Binary, 41, 0x41>
#endif
    , DeclareApplet<BitBeat, 79, 0x01>
    , DeclareApplet<BootsNCat, 55, 0x80>
    , DeclareApplet<Brancher, 4, 0x14>
    , DeclareApplet<BugCrack, 51, 0x80>
    , DeclareApplet<Burst, 31, 0x04>
    , DeclareApplet<Button, 65, 0x10>
    , DeclareApplet<Calculate, 12, 0x10>
    , DeclareApplet<Calibr8, 88, 0x10>
    , DeclareApplet<Carpeggio, 32, 0x0a>
#ifdef PEWPEWPEW
    , DeclareApplet<Chordinator, 64, 0x08>
#endif
    , DeclareApplet<ClockDivider, 6, 0x04>
    , DeclareApplet<ClkToGate, 78, 0x04>
    , DeclareApplet<ClockSkip, 28, 0x04>
    , DeclareApplet<Combin8, 82, 0x10>
    , DeclareApplet<Compare, 30, 0x10>
    , DeclareApplet<Cumulus, 5, 0x40>
    , DeclareApplet<CVRecV2, 24, 0x02>
    , DeclareApplet<DivSeq, 68, 0x06>
    , DeclareApplet<DivSeq10, 80, 0x06>
    , DeclareApplet<DrLoFi, 16, 0x80>
    , DeclareApplet<DrumMap, 57, 0x02>
    , DeclareApplet<DualQuant, 9, 0x08>
#ifdef PEWPEWPEW
    , DeclareApplet<OffsetQuant, 90, 0x08>
#endif
#if !defined(CUSTOM_BUILD) || defined(PEWPEWPEW)
    , DeclareApplet<DuoTET, 63, 0x08>
#endif
    , DeclareApplet<EbbAndLfo, 7, 0x01>
#ifdef ENABLE_APP_ENIGMA
    , DeclareApplet<EnigmaJr, 45, 0x02>
#endif
#ifdef PEWPEWPEW
    , DeclareApplet<EnsOscKey, 35, 0x08>
#endif
    , DeclareApplet<EnvFollow, 42, 0x11>
#ifdef __IMXRT1062__
    , DeclareApplet<EnvSeq, 91, 0x02>
#endif
    , DeclareApplet<EuclidO, 83, 0x02>
    , DeclareApplet<EuclidX, 15, 0x02>
#ifdef PEWPEWPEW
    , DeclareApplet<GameOfLife, 22, 0x01>
#endif
    , DeclareApplet<GateDelay, 29, 0x04>
#ifdef PEWPEWPEW
    , DeclareApplet<GatedVCA, 17, 0x50>
#endif
    , DeclareApplet<Logic, 10, 0x44>
    , DeclareApplet<LowerRenz, 21, 0x01>
    , DeclareApplet<Metronome, 50, 0x04>
#ifdef __IMXRT1062__
    , DeclareApplet<MidiLoop, 81, 0x20>
#endif
    , DeclareApplet<MarkoV, 93, 0x02>
    , DeclareApplet<MarkovPerc, 94, 0x80>
    , DeclareApplet<hMIDIIn, 150, 0x20>
    , DeclareApplet<hMIDIOut, 27, 0x20>
#ifdef PEWPEWPEW
    , DeclareApplet<MultiScale, 73, 0x08>
#endif
    , DeclareApplet<Palimpsest, 20, 0x02>
    , DeclareApplet<Pigeons, 71, 0x02>
    , DeclareApplet<PolyDiv, 72, 0x06>
    , DeclareApplet<ProbabilityDivider, 59, 0x04>
    , DeclareApplet<ProbabilityMelody, 62, 0x04>
    , DeclareApplet<Relabi, 89, 0x01>
    , DeclareApplet<ResetClock, 70, 0x14>
    , DeclareApplet<RndWalk, 69, 0x01>
#ifdef PEWPEWPEW
    , DeclareApplet<RunglBook, 44, 0x01>
#endif
    , DeclareApplet<ScaleDuet, 26, 0x08>
    , DeclareApplet<Schmitt, 40, 0x40>
    , DeclareApplet<Scope, 23, 0x80>
    , DeclareApplet<Seq32, 75, 0x02>
    , DeclareApplet<SeqPlay7, 76, 0x02>
    , DeclareApplet<SequenceX, 14, 0x02>
    , DeclareApplet<ShiftGate, 48, 0x45>
#ifdef PEWPEWPEW
    , DeclareApplet<ShiftReg, 77, 0x45>
#endif
    , DeclareApplet<Shredder, 58, 0x01>
    , DeclareApplet<Shuffle, 36, 0x04>
    , DeclareApplet<Slew, 19, 0x01>
    , DeclareApplet<Squanch, 46, 0x08>
    , DeclareApplet<Stairs, 61, 0x01>
    , DeclareApplet<Strum, 74, 0x08>
    , DeclareApplet<Switch, 3, 0x10>
    , DeclareApplet<SwitchSeq, 38, 0x10>
    , DeclareApplet<TB_3PO, 60, 0x02>
    , DeclareApplet<TLNeuron, 13, 0x40>
#ifdef PEWPEWPEW
    , DeclareApplet<Trending, 37, 0x40>
#endif
    , DeclareApplet<TrigSeq, 11, 0x06>
    , DeclareApplet<TrigSeq16, 25, 0x06>
    , DeclareApplet<Tuner, 39, 0x80>
    , DeclareApplet<TwoRings, 18, 0x02>
    , DeclareApplet<VectorEG, 52, 0x01>
    , DeclareApplet<VectorLFO, 49, 0x01>
//    , DeclareApplet<VectorMod, 53, 0x01> // awkward middle child
    , DeclareApplet<VectorMorph, 54, 0x01>
    , DeclareApplet<Voltage, 43, 0x10>
#ifdef PEWPEWPEW
    , DeclareApplet<WTVCO, 67, 0x80>
#endif
    , DeclareApplet<Xfader, 33, 0x10>
>{};


namespace HS {
  static constexpr auto appletIds = reg.getIds();
  constexpr int HEMISPHERE_AVAILABLE_APPLETS = appletIds.size();

  uint64_t hidden_applets[2] = { 0, 0 };
  bool applet_is_hidden(const int& index) {
    return (hidden_applets[index/64] >> (index%64)) & 1;
  }
  void showhide_applet(const int& index) {
    const int seg = index/64;
    hidden_applets[seg] = hidden_applets[seg] ^ (uint64_t(1) << (index%64));
  }

  HemisphereApplet * get_applet(const int index, HEM_SIDE slot = 0) {
    return reg.get(appletIds[index], slot);
  }

  const char * get_applet_name(const int index) {
    return reg.getName(index);
  }

  const uint8_t * get_applet_icon(const int index) {
    return reg.getIcon(index);
  }

  constexpr int get_applet_index_by_id(const int& id) {
    int index = 0;
    for (int i = 0; i < HEMISPHERE_AVAILABLE_APPLETS; i++)
    {
        if (appletIds[i] == id) index = i;
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
