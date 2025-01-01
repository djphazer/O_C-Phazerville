#pragma once

#include "AudioAppletSubapp.h"
#include "audio_applets/CrosspanApplet.h"
#include "audio_applets/DelayApplet.h"
#include "audio_applets/DynamicsApplet.h"
#include "audio_applets/FilterFolderApplet.h"
#include "audio_applets/InputApplet.h"
#include "audio_applets/LadderApplet.h"
#include "audio_applets/OscApplet.h"
#include "audio_applets/PassthruApplet.h"
#include "audio_applets/UpsampledApplet.h"
#include "audio_applets/VCAApplet.h"
#include "audio_applets/WAVPlayerApplet.h"

const size_t NUM_SLOTS = 5;

DMAMEM std::tuple<InputApplet<MONO>, UpsampledApplet<MONO>, OscApplet>
  mono_input_pool[2];
DMAMEM std::
  tuple<InputApplet<STEREO>, WavPlayerApplet<STEREO>, UpsampledApplet<STEREO>>
    stereo_input_pool;
DMAMEM std::tuple<
  PassthruApplet<MONO>,
  DynamicsApplet<MONO>,
  InputApplet<MONO>,
  OscApplet,
  DelayApplet<MONO>,
  LadderApplet<MONO>,
  FilterFolderApplet<MONO>,
  VcaApplet<MONO>,
  UpsampledApplet<MONO>>
  mono_processors_pool[2][NUM_SLOTS - 1];
DMAMEM std::tuple<
  PassthruApplet<STEREO>,
  CrosspanApplet,
  DynamicsApplet<STEREO>,
  InputApplet<STEREO>,
  DelayApplet<STEREO>,
  LadderApplet<STEREO>,
  VcaApplet<STEREO>,
  FilterFolderApplet<STEREO>,
  WavPlayerApplet<STEREO>,
  UpsampledApplet<STEREO>>
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
