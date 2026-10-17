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
#define applet_name applet_name() const final { return applet_name_(); } \
  static constexpr const char* applet_name_

#define applet_icon applet_icon() const final { return applet_icon_(); } \
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
// #include "FreeverbApplet.h"
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
#include "AbyssApplet.h"
#include "AnimorfApplet.h"

#undef applet_name
#undef applet_icon

const size_t NUM_SLOTS = 5;

Factory<AudioEffectReverbSchroederF32, 8> HemisphereAudioApplet::bung_factory;
Factory<AudioEffectFreeverbF32, 8> HemisphereAudioApplet::verb_factory;
Factory<AudioEffectDynamics, 20> HemisphereAudioApplet::compressor_factory;
Factory<AudioEffectDynamicsF32, 10> HemisphereAudioApplet::compressor_f32_factory;

// TODO: categories
constexpr Registry mono_applets = Registry<HemisphereAudioApplet, NUM_SLOTS * 2
    , DeclareFancyApplet<PassthruApplet>
    , DeclareFancyApplet<InputApplet>
    , DeclareFancyApplet<UpsampledApplet>
    , DeclareFancyApplet<OscApplet>
    , DeclareFancyApplet<HandSawApplet>
    , DeclareFancyApplet<HarmOscApplet>
    , DeclareFancyApplet<FMDrumApplet>
    , DeclareFancyApplet<WavPlayerApplet>
    , DeclareFancyApplet<OneShotPlayerApplet>
    , DeclareFancyApplet<VcaApplet>
    , DeclareFancyApplet<LadderApplet>
    , DeclareFancyApplet<FilterFolderApplet>
    , DeclareFancyApplet<DelayApplet>
    , DeclareFancyApplet<AbyssApplet>
    , DeclareFancyApplet<AnimorfApplet>
    , DeclareFancyApplet<PhazerApplet>
    // , DeclareFancyApplet<ReverbApplet>
    , DeclareFancyApplet<BungverbApplet>
    , DeclareFancyApplet<DynamicsApplet>
    , DeclareFancyApplet<ThreeBandzApplet>
    , DeclareFancyApplet<TuneTrackerApplet>
    , DeclareFancyApplet<GlitchApplet>
    , DeclareFancyApplet<GritApplet>
    , DeclareFancyApplet<MistierApplet>
    , DeclareFancyApplet<AdvKrpsStrngApplet>
    , DeclareFancyApplet<WTVCOApplet>
    , DeclareFancyApplet<ModalResonatorApplet>
#ifndef AUDIO_INTERFACE
    , DeclareFancyApplet<WavRecorderApplet>
#endif
>{};

constexpr Registry stereo_applets = Registry<HemisphereAudioApplet, NUM_SLOTS
  , DeclareFancyApplet<PassthruApplet>
  , DeclareFancyApplet<InputApplet>
  , DeclareFancyApplet<UpsampledApplet>
  , DeclareFancyApplet<CrosspanApplet>
  , DeclareFancyApplet<MidSideApplet>
  , DeclareFancyApplet<VcaApplet>
  , DeclareFancyApplet<DynamicsApplet>
  , DeclareFancyApplet<ThreeBandzApplet>
  , DeclareFancyApplet<DelayApplet>
  , DeclareFancyApplet<AbyssApplet>
  , DeclareFancyApplet<AnimorfApplet>
  , DeclareFancyApplet<LadderApplet>
  , DeclareFancyApplet<FilterFolderApplet>
  , DeclareFancyApplet<WavPlayerApplet>
  , DeclareFancyApplet<OneShotPlayerApplet>
  , DeclareFancyApplet<ModalResonatorApplet>
#ifndef AUDIO_INTERFACE
  , DeclareFancyApplet<WavRecorderApplet>
#endif
>{};

static constexpr auto mono_appletIds = mono_applets.getIds();
constexpr int MONO_POOL_SIZE = mono_appletIds.size();

static constexpr auto stereo_appletIds = stereo_applets.getIds();
constexpr int STEREO_POOL_SIZE = stereo_appletIds.size();


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
