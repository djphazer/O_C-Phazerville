#pragma once

#include "src/Audio/filter_variable2.h"
#include "AudioAppletSubapp.h"
#include "audio_applets/CrosspanApplet.h"
#include "audio_applets/DelayApplet.h"
#include "audio_applets/DynamicsApplet.h"
#include "audio_applets/FilterFolderApplet.h"
#include "audio_applets/InputApplet.h"
#include "audio_applets/LadderApplet.h"
#include "audio_applets/MidSideApplet.h"
#include "audio_applets/OscApplet.h"
#include "audio_applets/PassthruApplet.h"
#include "audio_applets/UpsampledApplet.h"
#include "audio_applets/VCAApplet.h"
#include "audio_applets/WAVPlayerApplet.h"
#include "audio_applets/OneShotPlayerApplet.h"
#include "audio_applets/HandSawApplet.h"
#include "audio_applets/FreeverbApplet.h"
#include "audio_applets/SamverbApplet.h"
#include "audio_applets/PhaserApplet.h"
#include "audio_applets/ThreeBandz.h"
#include "audio_applets/TuneTrackerApplet.h"
#include "audio_applets/FMDrumApplet.h"
#include "audio_applets/GlitchApplet.h"
#include "audio_applets/GritApplet.h"
#include "audio_applets/MistierApplet.h"
#include "audio_applets/AdvKrpsStrngApplet.h"
#include "audio_applets/ModalResonatorApplet.h"
#include "audio_applets/WAVRecorderApplet.h"
#include "audio_applets/WTVCOApplet.h"
#include "audio_applets/HarmOscApplet.h"

const size_t NUM_SLOTS = 5;

Factory<AudioEffectReverbSchroeder, 8> HemisphereAudioApplet::bung_factory;
Factory<AudioEffectFreeverb, 8> HemisphereAudioApplet::verb_factory;

DMAMEM std::tuple<
  PassthruApplet<MONO>,
  InputApplet<MONO>,
  HandSawApplet,
  HarmOscApplet,
  UpsampledApplet<MONO>,
  OscApplet,
  FMDrumApplet,
  WavPlayerApplet<MONO>,
  OneShotPlayerApplet<MONO>,
  AdvKrpsStrngApplet,
  WTVCOApplet,
  ModalResonatorApplet<MONO>>
    mono_input_pool[2];
DMAMEM std::tuple<
  PassthruApplet<STEREO>,
  InputApplet<STEREO>,
  WavPlayerApplet<STEREO>,
  OneShotPlayerApplet<STEREO>,
  UpsampledApplet<STEREO>>
    stereo_input_pool;
DMAMEM std::tuple<
  PassthruApplet<MONO>,
  InputApplet<MONO>,
  OscApplet,
  HandSawApplet,
  HarmOscApplet,
  FMDrumApplet,
  WavPlayerApplet<MONO>,
  OneShotPlayerApplet<MONO>,
  VcaApplet<MONO>,
  LadderApplet<MONO>,
  FilterFolderApplet<MONO>,
  DelayApplet<MONO>,
  PhazerApplet,
  ReverbApplet,
  BungverbApplet,
  DynamicsApplet<MONO>,
  TuneTrackerApplet<MONO>,
  UpsampledApplet<MONO>,
  GlitchApplet<MONO>,
  GritApplet<MONO>,
  MistierApplet<MONO>,
  AdvKrpsStrngApplet,
  ModalResonatorApplet<MONO>,
  WTVCOApplet
#ifndef USB_AUDIO
  , WavRecorderApplet<MONO>
#endif
  >
    mono_processors_pool[2][NUM_SLOTS - 1];
DMAMEM std::tuple<
  PassthruApplet<STEREO>,
  CrosspanApplet,
  MidSideApplet,
  DynamicsApplet<STEREO>,
  ThreeBandzApplet,
  InputApplet<STEREO>,
  DelayApplet<STEREO>,
  LadderApplet<STEREO>,
  VcaApplet<STEREO>,
  FilterFolderApplet<STEREO>,
  WavPlayerApplet<STEREO>,
  OneShotPlayerApplet<STEREO>,
  UpsampledApplet<STEREO>,
  ModalResonatorApplet<STEREO>
#ifndef USB_AUDIO
  , WavRecorderApplet<STEREO>
#endif
  >
    stereo_processors_pool[NUM_SLOTS - 1];

// Helper to extract the tuple type from an array... thanks ChatGPT...
template <typename ArrayType>
using Unwrap = typename std::remove_reference<
  typename std::remove_extent<ArrayType>::type>::type;

// Compute sizes using deduced tuple types
constexpr size_t MONO_INPUT_POOL_SIZE
  = std::tuple_size<Unwrap<decltype(mono_input_pool)>>::value;
constexpr size_t STEREO_INPUT_POOL_SIZE
  = std::tuple_size<Unwrap<decltype(stereo_input_pool)>>::value;
constexpr size_t MONO_PROCESSORS_POOL_SIZE
  = std::tuple_size<Unwrap<Unwrap<decltype(mono_processors_pool)>>>::value;
constexpr size_t STEREO_PROCESSORS_POOL_SIZE
  = std::tuple_size<Unwrap<decltype(stereo_processors_pool)>>::value;

AudioAppletSubapp<
  NUM_SLOTS,
  MONO_INPUT_POOL_SIZE,
  STEREO_INPUT_POOL_SIZE,
  MONO_PROCESSORS_POOL_SIZE,
  STEREO_PROCESSORS_POOL_SIZE>
  audio_app(
    mono_input_pool,
    stereo_input_pool,
    mono_processors_pool,
    stereo_processors_pool
  );
