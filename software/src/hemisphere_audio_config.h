#pragma once

#include "AudioAppletSubapp.h"
#include "audio_applets/DelayApplet.h"
#include "audio_applets/InputApplet.h"
#include "audio_applets/LadderApplet.h"
#include "audio_applets/OscApplet.h"
#include "audio_applets/PassthruApplet.h"
#include "audio_applets/VCAApplet.h"

const size_t NUM_SLOTS = 5;

VcaApplet<MONO> vca;

std::tuple<InputApplet<0>, InputApplet<1>, OscApplet> mono_input_pool[2];
std::tuple<InputApplet<0>, InputApplet<1>> stereo_input_pool;
std::tuple<PassthruApplet<MONO>, DelayApplet, LadderApplet<MONO>, VcaApplet<MONO>>
  mono_processors_pool[2][NUM_SLOTS - 1];
std::tuple<PassthruApplet<STEREO>, LadderApplet<STEREO>, VcaApplet<STEREO>>
  stereo_processors_pool[NUM_SLOTS - 1];

AudioAppletSubapp<NUM_SLOTS, 3, 2, 4, 3> audio_app(
  mono_input_pool,
  stereo_input_pool,
  mono_processors_pool,
  stereo_processors_pool
);
