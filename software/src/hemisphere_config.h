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

template<class A>
struct DeclareApplet {
  const int id;
  const uint8_t categories;
};

template <class... AppletClasses> struct AppletRegistry {
  // Must be static or template specialization stick around. Must be inline or
  // you get linker errors.
  inline static std::tuple<AppletClasses...> pool[APPLET_SLOTS];
  std::array<Applet, sizeof...(AppletClasses)> applets;

  // Constructor *must* be constexpr or all the template specializations will
  // cause code and memory size to increase
  constexpr AppletRegistry(DeclareApplet<AppletClasses>... applets)
      : applets{Applet{applets.id, applets.categories,
                       PtrArray<AppletClasses, APPLET_SLOTS>()}...} {}

private:
  template <class C, size_t N, size_t... Is>
  static constexpr std::array<HemisphereApplet *, sizeof...(Is)>
  PtrArray(std::index_sequence<Is...>) {
    return std::array<HemisphereApplet *, sizeof...(Is)>{&std::get<C>(pool[Is])...};
  }

  template <class C, size_t N>
  static constexpr std::array<HemisphereApplet *, N> PtrArray() {
    return PtrArray<C, N>(std::make_index_sequence<N>());
  }
};

AppletRegistry reg{
    DeclareApplet<ADSREG>{8, 0x01},
    DeclareApplet<ADEG>{34, 0x01},
    DeclareApplet<ASR>{47, 0x09},
    DeclareApplet<AttenuateOffset>{56, 0x10},
    DeclareApplet<Binary>{41, 0x41},
    DeclareApplet<BootsNCat>{55, 0x80},
    DeclareApplet<Brancher>{4, 0x14},
    DeclareApplet<BugCrack>{51, 0x80},
    DeclareApplet<Burst>{31, 0x04},
    DeclareApplet<Button>{65, 0x10},
    DeclareApplet<Calculate>{12, 0x10},\
    DeclareApplet<Calibr8>{88, 0x10},
    DeclareApplet<Carpeggio>{32, 0x0a},
    DeclareApplet<Chordinator>{64, 0x08},
    DeclareApplet<ClockDivider>{6, 0x04},
    DeclareApplet<ClockSkip>{28, 0x04},
    DeclareApplet<Compare>{30, 0x10},
    DeclareApplet<Cumulus>{74, 0x40},
    DeclareApplet<CVRecV2>{24, 0x02},
    DeclareApplet<DivSeq>{68, 0x06},
    DeclareApplet<DrLoFi>{16, 0x80},
    DeclareApplet<DrumMap>{57, 0x02},
    DeclareApplet<DualQuant>{9, 0x08},
    DeclareApplet<DualTM>{18, 0x02},
    DeclareApplet<EbbAndLfo>{7, 0x01},
    DeclareApplet<EnigmaJr>{45, 0x02},
    DeclareApplet<EnvFollow>{42, 0x11},
    DeclareApplet<EuclidX>{15, 0x02},
    DeclareApplet<GameOfLife>{22, 0x01},
    DeclareApplet<GateDelay>{29, 0x04},
    DeclareApplet<GatedVCA>{17, 0x50},
    DeclareApplet<Logic>{10, 0x44},
    DeclareApplet<LowerRenz>{21, 0x01},
    DeclareApplet<Metronome>{50, 0x04},
    DeclareApplet<hMIDIIn>{150, 0x20},
    DeclareApplet<hMIDIOut>{27, 0x20},
    DeclareApplet<MixerBal>{33, 0x10},
    DeclareApplet<MultiScale>{73, 0x08},
    DeclareApplet<Palimpsest>{20, 0x02},
    DeclareApplet<Pigeons>{71, 0x02},
    DeclareApplet<PolyDiv>{72, 0x06},
    DeclareApplet<ProbabilityDivider>{59, 0x04},
    DeclareApplet<ProbabilityMelody>{62, 0x04},
    DeclareApplet<ResetClock>{70, 0x14},
    DeclareApplet<RndWalk>{69, 0x01},
    DeclareApplet<RunglBook>{44, 0x01},
    DeclareApplet<ScaleDuet>{26, 0x08},
    DeclareApplet<Schmitt>{40, 0x40},
    DeclareApplet<Scope>{23, 0x80},
    DeclareApplet<Seq32>{75, 0x02},
    DeclareApplet<SeqPlay7>{76, 0x02},
    DeclareApplet<SequenceX>{14, 0x02},
    DeclareApplet<ShiftGate>{48, 0x45},
    DeclareApplet<ShiftReg>{77, 0x45},
    DeclareApplet<Shredder>{58, 0x01},
    DeclareApplet<Shuffle>{36, 0x04},
    DeclareApplet<Slew>{19, 0x01},
    DeclareApplet<Squanch>{46, 0x08},
    DeclareApplet<Stairs>{61, 0x01},
    DeclareApplet<Strum>{74, 0x08},
    DeclareApplet<Switch>{3, 0x10},
    DeclareApplet<SwitchSeq>{38, 0x10},
    DeclareApplet<TB_3PO>{60, 0x02},
    DeclareApplet<TLNeuron>{13, 0x40},
    DeclareApplet<Trending>{37, 0x40},
    DeclareApplet<TrigSeq>{11, 0x06},
    DeclareApplet<TrigSeq16>{25, 0x06},
    DeclareApplet<Tuner>{39, 0x80},
    DeclareApplet<VectorEG>{52, 0x01},
    DeclareApplet<VectorLFO>{49, 0x01},
    DeclareApplet<VectorMod>{53, 0x01},
    DeclareApplet<VectorMorph>{54, 0x01},
    DeclareApplet<Voltage>{43, 0x10},
};


namespace HS {
  static constexpr auto & available_applets = reg.applets;
  static constexpr int HEMISPHERE_AVAILABLE_APPLETS = ARRAY_SIZE(available_applets);

  // TODO: figure out where to store this
  uint64_t hidden_applets[2] = { 0, 0 };
  bool applet_is_hidden(const int& index) {
    return (hidden_applets[index/64] >> (index%64)) & 1;
  }
  void showhide_applet(const int& index) {
    const int seg = index/64;
    hidden_applets[seg] = hidden_applets[seg] ^ (uint64_t(1) << (index%64));
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
