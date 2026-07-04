#pragma once

#include "../src/Audio/filter_variable2.h"
#include "../AudioIO.h"
#include <FS.h>
#include "../HSUtils.h"
#include "../HemisphereAudioApplet.h"
#include "../dsputils_arm.h"
#include "../Audio/AudioMixer.h"
#include "../Audio/AudioPassthrough.h"
#include "../Audio/AudioVCA.h"
#include "../Audio/InterpolatingStream.h"

// hacks to effectively rewrite part of the applet boilerplate,
// making names and icons static
#define applet_name applet_name() override { return applet_name_(); } \
  static constexpr const char* applet_name_

#define applet_icon applet_icon() override { return applet_icon_(); } \
  static constexpr const uint8_t* applet_icon_

// actual applets
#include "CrosspanApplet.h"
#include "DelayApplet.h"
#include "DynamicsApplet.h"
#include "FilterFolderApplet.h"
#include "InputApplet.h"
#include "LadderApplet.h"
#include "MidSideApplet.h"
#include "OscApplet.h"
#include "PassthruApplet.h"
#include "UpsampledApplet.h"
#include "VCAApplet.h"
#include "WAVPlayerApplet.h"
#include "OneShotPlayerApplet.h"
#include "HandSawApplet.h"
#include "FreeverbApplet.h"
#include "SamverbApplet.h"
#include "PhaserApplet.h"
#include "ThreeBandz.h"
#include "TuneTrackerApplet.h"
#include "FMDrumApplet.h"
#include "GlitchApplet.h"
#include "GritApplet.h"
#include "MistierApplet.h"
#include "AdvKrpsStrngApplet.h"
#include "ModalResonatorApplet.h"
#include "WAVRecorderApplet.h"
#include "WTVCOApplet.h"
#include "HarmOscApplet.h"

#undef applet_name
#undef applet_icon

const size_t NUM_SLOTS = 5;

Factory<AudioEffectReverbSchroeder, 8> HemisphereAudioApplet::bung_factory;
Factory<AudioEffectFreeverb, 8> HemisphereAudioApplet::verb_factory;
Factory<AudioEffectDynamics, 20> HemisphereAudioApplet::compressor_factory;

// TODO: categories
constexpr Registry mono_applets = Registry<HemisphereAudioApplet, NUM_SLOTS * 2
    , DeclareFancyApplet<PassthruApplet<MONO>>
    , DeclareFancyApplet<InputApplet<MONO>>
    , DeclareFancyApplet<OscApplet>
    , DeclareFancyApplet<HandSawApplet>
    , DeclareFancyApplet<HarmOscApplet>
    , DeclareFancyApplet<FMDrumApplet>
    , DeclareFancyApplet<WavPlayerApplet<MONO>>
    , DeclareFancyApplet<OneShotPlayerApplet<MONO>>
    , DeclareFancyApplet<VcaApplet<MONO>>
    , DeclareFancyApplet<LadderApplet<MONO>>
    , DeclareFancyApplet<FilterFolderApplet<MONO>>
    , DeclareFancyApplet<DelayApplet<MONO>>
    , DeclareFancyApplet<PhazerApplet>
    , DeclareFancyApplet<ReverbApplet>
    , DeclareFancyApplet<BungverbApplet>
    , DeclareFancyApplet<DynamicsApplet<MONO>>
    , DeclareFancyApplet<TuneTrackerApplet<MONO>>
    , DeclareFancyApplet<UpsampledApplet<MONO>>
    , DeclareFancyApplet<GlitchApplet<MONO>>
    , DeclareFancyApplet<GritApplet<MONO>>
    , DeclareFancyApplet<MistierApplet<MONO>>
    , DeclareFancyApplet<AdvKrpsStrngApplet>
    , DeclareFancyApplet<ModalResonatorApplet<MONO>>
    , DeclareFancyApplet<WTVCOApplet>
#ifndef USB_AUDIO
    , DeclareFancyApplet<WavRecorderApplet<MONO>>
#endif
>{};

constexpr Registry stereo_applets = Registry<HemisphereAudioApplet, NUM_SLOTS
  , DeclareFancyApplet<PassthruApplet<STEREO>>
  , DeclareFancyApplet<InputApplet<STEREO>>
  , DeclareFancyApplet<CrosspanApplet>
  , DeclareFancyApplet<MidSideApplet>
  , DeclareFancyApplet<DynamicsApplet<STEREO>>
  , DeclareFancyApplet<ThreeBandzApplet>
  , DeclareFancyApplet<DelayApplet<STEREO>>
  , DeclareFancyApplet<LadderApplet<STEREO>>
  , DeclareFancyApplet<VcaApplet<STEREO>>
  , DeclareFancyApplet<FilterFolderApplet<STEREO>>
  , DeclareFancyApplet<WavPlayerApplet<STEREO>>
  , DeclareFancyApplet<OneShotPlayerApplet<STEREO>>
  , DeclareFancyApplet<UpsampledApplet<STEREO>>
  , DeclareFancyApplet<ModalResonatorApplet<STEREO>>
#ifndef USB_AUDIO
  , DeclareFancyApplet<WavRecorderApplet<STEREO>>
#endif
>{};

static constexpr auto mono_appletIds = mono_applets.getIds();
constexpr int MONO_POOL_SIZE = mono_appletIds.size();

static constexpr auto stereo_appletIds = stereo_applets.getIds();
constexpr int STEREO_POOL_SIZE = stereo_appletIds.size();

/*DMAMEM std::tuple<*/
/*  PassthruApplet<MONO>,*/
/*  InputApplet<MONO>,*/
/*  HandSawApplet,*/
/*  HarmOscApplet,*/
/*  UpsampledApplet<MONO>,*/
/*  OscApplet,*/
/*  FMDrumApplet,*/
/*  WavPlayerApplet<MONO>,*/
/*  OneShotPlayerApplet<MONO>,*/
/*  AdvKrpsStrngApplet,*/
/*  WTVCOApplet,*/
/*  ModalResonatorApplet<MONO>,*/
/*  >*/
/*    mono_input_pool[2];*/
/*DMAMEM std::tuple<*/
/*  PassthruApplet<STEREO>,*/
/*  InputApplet<STEREO>,*/
/*  WavPlayerApplet<STEREO>,*/
/*  OneShotPlayerApplet<STEREO>,*/
/*  UpsampledApplet<STEREO>>*/
/*    stereo_input_pool;*/

// Helper to extract the tuple type from an array... thanks ChatGPT...
template <typename ArrayType>
using Unwrap = typename std::remove_reference<
  typename std::remove_extent<ArrayType>::type>::type;

// Compute sizes using deduced tuple types
/*constexpr size_t MONO_INPUT_POOL_SIZE = std::tuple_size<Unwrap<decltype(mono_input_pool)>>::value;*/
/*constexpr size_t STEREO_INPUT_POOL_SIZE = std::tuple_size<Unwrap<decltype(stereo_input_pool)>>::value;*/
/*constexpr size_t MONO_PROCESSORS_POOL_SIZE = std::tuple_size<Unwrap<Unwrap<decltype(mono_processors_pool)>>>::value;*/
/*constexpr size_t STEREO_PROCESSORS_POOL_SIZE = std::tuple_size<Unwrap<decltype(stereo_processors_pool)>>::value;*/

#include "../AudioAppletSubapp.h"

AudioAppletSubapp<NUM_SLOTS, MONO_POOL_SIZE, STEREO_POOL_SIZE, decltype(mono_applets), decltype(stereo_applets)> audio_app(
  mono_applets, stereo_applets
);
