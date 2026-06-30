#pragma once

#include "../HemisphereApplet.h"

using namespace HS;

// hacks to effectively rewrite part of the applet boilerplate,
// making names and icons static
#define applet_name applet_name() final { return applet_name_(); } \
  static constexpr const char* applet_name_

#define applet_icon applet_icon() final { return applet_icon_(); } \
  static constexpr const uint8_t* applet_icon_

#include "ADSREG.h"
#include "ADEG.h"
#include "ASR.h"
#include "AttenuateOffset.h"
#include "Binary.h"
#include "BootsNCat.h"
#include "Brancher.h"
#include "BugCrack.h"
#include "Burst.h"
#include "Button.h"
#include "BitBeat.h"
#include "Cumulus.h"
#include "CVRecV2.h"
#include "Calculate.h"
#include "TruthCat3.h"
#include "TruthCat4.h"
#include "Calibr8.h"
#include "Carpeggio.h"
#include "Chordinator.h"
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
#include "CVSeq.h"
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
#include "Ponglet.h"
#include "ProbabilityDivider.h"
#include "ProbabilityMelody.h"
#include "Relabi.h"
#include "ResetClock.h"
#include "RndWalk.h"
#include "RunglBook.h"
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
#include "Trending.h"
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

#include "../AppletRegistry.h"

constexpr Registry reg = Registry<HemisphereApplet, HS::APPLET_SLOTS
    , DeclareApplet<ADSREG, 8, CAT_MODULATOR>
    , DeclareApplet<ADEG, 34, CAT_MODULATOR>
    , DeclareApplet<MiniASR, 47, CAT_MODULATOR | CAT_QUANTIZER>
    , DeclareApplet<AttenuateOffset, 56, CAT_UTILITY>
    , DeclareApplet<Binary, 41, CAT_LOGIC | CAT_MODULATOR>
    , DeclareApplet<BitBeat, 79, CAT_MODULATOR>
    , DeclareApplet<BootsNCat, 55, CAT_OTHER>
    , DeclareApplet<Brancher, 4, CAT_UTILITY | CAT_CLOCKING>
    , DeclareApplet<BugCrack, 51, CAT_OTHER>
    , DeclareApplet<Burst, 31, CAT_CLOCKING>
    , DeclareApplet<Button, 65, CAT_UTILITY>
    , DeclareApplet<Calculate, 12, CAT_UTILITY>
    , DeclareApplet<Calibr8, 88, CAT_UTILITY>
    , DeclareApplet<Carpeggio, 32, CAT_SEQUENCER | CAT_QUANTIZER>
    , DeclareApplet<Chordinator, 64, CAT_QUANTIZER>
    , DeclareApplet<ClockDivider, 6, CAT_CLOCKING>
    , DeclareApplet<ClkToGate, 78, CAT_CLOCKING>
    , DeclareApplet<ClockSkip, 28, CAT_CLOCKING>
    , DeclareApplet<Combin8, 82, CAT_UTILITY>
    , DeclareApplet<Compare, 30, CAT_UTILITY>
    , DeclareApplet<Cumulus, 5, CAT_LOGIC>
    , DeclareApplet<CVRecV2, 24, CAT_SEQUENCER>
    , DeclareApplet<CVSeq, 92, CAT_SEQUENCER>
    , DeclareApplet<DivSeq, 68, CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<DivSeq10, 80, CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<DrLoFi, 16, CAT_OTHER>
    , DeclareApplet<DrumMap, 57, CAT_SEQUENCER>
    , DeclareApplet<DualQuant, 9, CAT_QUANTIZER>
#ifdef PEWPEWPEW
    , DeclareApplet<OffsetQuant, 90, CAT_QUANTIZER>
#endif
#if !defined(CUSTOM_BUILD) || defined(PEWPEWPEW)
    , DeclareApplet<DuoTET, 63, CAT_QUANTIZER>
#endif
    , DeclareApplet<EbbAndLfo, 7, CAT_MODULATOR>
#ifdef ENABLE_APP_ENIGMA
    , DeclareApplet<EnigmaJr, 45, CAT_SEQUENCER>
#endif
#ifdef PEWPEWPEW
    , DeclareApplet<EnsOscKey, 35, CAT_QUANTIZER>
#endif
    , DeclareApplet<EnvFollow, 42, CAT_UTILITY | CAT_MODULATOR>
#ifdef __IMXRT1062__
    , DeclareApplet<EnvSeq, 91, CAT_SEQUENCER>
#endif
    , DeclareApplet<EuclidO, 83, CAT_SEQUENCER>
    , DeclareApplet<EuclidX, 15, CAT_SEQUENCER>
#ifdef PEWPEWPEW
    , DeclareApplet<GameOfLife, 22, CAT_MODULATOR>
#endif
    , DeclareApplet<GateDelay, 29, CAT_CLOCKING>
    , DeclareApplet<GatedVCA, 17, CAT_UTILITY | CAT_LOGIC>
    , DeclareApplet<Logic, 10, CAT_LOGIC | CAT_CLOCKING>
    , DeclareApplet<LowerRenz, 21, CAT_MODULATOR>
    , DeclareApplet<Metronome, 50, CAT_CLOCKING>
#ifdef __IMXRT1062__
    , DeclareApplet<MidiLoop, 81, CAT_MIDI>
#endif
    , DeclareApplet<MarkoV, 93, CAT_SEQUENCER>
    , DeclareApplet<MarkovPerc, 94, CAT_OTHER>
    , DeclareApplet<hMIDIIn, 150, CAT_MIDI>
    , DeclareApplet<hMIDIOut, 27, CAT_MIDI>
#ifdef PEWPEWPEW
    , DeclareApplet<MultiScale, 73, CAT_QUANTIZER>
#endif
    , DeclareApplet<Palimpsest, 20, CAT_SEQUENCER>
    , DeclareApplet<Pigeons, 71, CAT_SEQUENCER>
    , DeclareApplet<PolyDiv, 72, CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<Ponglet, 99, CAT_OTHER>
    , DeclareApplet<ProbabilityDivider, 59, CAT_CLOCKING>
    , DeclareApplet<ProbabilityMelody, 62, CAT_CLOCKING>
    , DeclareApplet<Relabi, 89, CAT_MODULATOR>
    , DeclareApplet<ResetClock, 70, CAT_UTILITY | CAT_CLOCKING>
    , DeclareApplet<RndWalk, 69, CAT_MODULATOR>
    , DeclareApplet<RunglBook, 44, CAT_MODULATOR>
    , DeclareApplet<ScaleDuet, 26, CAT_QUANTIZER>
    , DeclareApplet<Schmitt, 40, CAT_LOGIC>
    , DeclareApplet<Scope, 23, CAT_OTHER>
    , DeclareApplet<Seq32, 75, CAT_SEQUENCER>
    , DeclareApplet<SeqPlay7, 76, CAT_SEQUENCER>
    , DeclareApplet<SequenceX, 14, CAT_SEQUENCER>
    , DeclareApplet<ShiftGate, 48, CAT_LOGIC | CAT_MODULATOR | CAT_CLOCKING>
#ifdef PEWPEWPEW
    , DeclareApplet<ShiftReg, 77, CAT_LOGIC | CAT_MODULATOR | CAT_CLOCKING>
#endif
    , DeclareApplet<Shredder, 58, CAT_MODULATOR>
    , DeclareApplet<Shuffle, 36, CAT_CLOCKING>
    , DeclareApplet<Slew, 19, CAT_MODULATOR>
    , DeclareApplet<Squanch, 46, CAT_QUANTIZER>
    , DeclareApplet<Stairs, 61, CAT_MODULATOR>
    , DeclareApplet<Strum, 74, CAT_QUANTIZER>
    , DeclareApplet<Switch, 3, CAT_UTILITY>
    , DeclareApplet<SwitchSeq, 38, CAT_UTILITY>
    , DeclareApplet<TB_3PO, 60, CAT_SEQUENCER>
    , DeclareApplet<TLNeuron, 13, CAT_LOGIC>
    , DeclareApplet<Trending, 37, CAT_LOGIC>
    , DeclareApplet<TrigSeq, 11, CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<TrigSeq16, 25, CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<TruthCat3, 85, CAT_LOGIC | CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<TruthCat4, 84, CAT_LOGIC | CAT_SEQUENCER | CAT_CLOCKING>
    , DeclareApplet<Tuner, 39, CAT_OTHER>
    , DeclareApplet<TwoRings, 18, CAT_SEQUENCER>
    , DeclareApplet<VectorEG, 52, CAT_MODULATOR>
    , DeclareApplet<VectorLFO, 49, CAT_MODULATOR>
    , DeclareApplet<VectorMod, 53, CAT_MODULATOR> // awkward middle child
    , DeclareApplet<VectorMorph, 54, CAT_MODULATOR>
    , DeclareApplet<Voltage, 43, CAT_UTILITY>
#ifdef PEWPEWPEW
    , DeclareApplet<WTVCO, 67, CAT_OTHER>
#endif
    , DeclareApplet<Xfader, 33, CAT_UTILITY>
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

  HemisphereApplet * get_applet(const int index, HEM_SIDE slot = LEFT_HEMISPHERE) {
    return reg.get(appletIds[index], slot);
  }

  const char * get_applet_name(const int index) {
    return reg.getName(index);
  }

  const uint8_t * get_applet_icon(const int index) {
    return reg.getIcon(index);
  }

  constexpr int get_applet_index_by_id(const RegID id) {
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
