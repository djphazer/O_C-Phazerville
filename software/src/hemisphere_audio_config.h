#pragma once

#include "AudioAppletSubapp.h"
#include "audio_applets/DelayApplet.h"
#include "audio_applets/InputApplet.h"
#include "audio_applets/LadderApplet.h"
#include "audio_applets/FilterFolderApplet.h"
#include "audio_applets/OscApplet.h"
#include "audio_applets/PassthruApplet.h"
#include "audio_applets/VCAApplet.h"
#include "audio_applets/WAVPlayerApplet.h"
#include "audio_applets/UpsampledApplet.h"

const size_t NUM_SLOTS = 5;

std::tuple<InputApplet<MONO>, UpsampledApplet> mono_input_pool[2];
std::tuple<InputApplet<STEREO>, WavPlayerApplet<STEREO>> stereo_input_pool;
std::tuple<PassthruApplet<MONO>, OscApplet, DelayApplet, LadderApplet<MONO>, FilterFolderApplet<MONO>, VcaApplet<MONO>>
  mono_processors_pool[2][NUM_SLOTS - 1];
std::tuple<PassthruApplet<STEREO>, LadderApplet<STEREO>, FilterFolderApplet<STEREO>, VcaApplet<STEREO>, WavPlayerApplet<STEREO>>
  stereo_processors_pool[NUM_SLOTS - 1];

AudioAppletSubapp<NUM_SLOTS, 2, 2, 6, 5> audio_app(
  mono_input_pool,
  stereo_input_pool,
  mono_processors_pool,
  stereo_processors_pool
);
