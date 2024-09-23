#pragma once

#include "AudioAppletSubapp.h"
#include "audio_applets/DelayApplet.h"
#include "audio_applets/InputApplet.h"
#include "audio_applets/LPFApplet.h"
#include "audio_applets/OscApplet.h"
#include "audio_applets/PassthruApplet.h"
#include "audio_applets/VCAApplet.h"

const size_t NUM_SLOTS = 5;

InputApplet<0> leftIn;
std::tuple<InputApplet<0>, InputApplet<1>, OscApplet> mono_input_pool[2];
std::tuple<InputApplet<0>, InputApplet<1>> stereo_input_pool;
std::tuple<PassthruApplet, DelayApplet, LpfApplet<MONO>, VcaApplet<MONO>>
  mono_processors_pool[2][NUM_SLOTS - 1];
std::tuple<VcaApplet<STEREO>> stereo_processors_pool[NUM_SLOTS - 1];

AudioAppletSubapp<5, 3, 2, 4, 1> audio_app(
  mono_input_pool,
  stereo_input_pool,
  mono_processors_pool,
  stereo_processors_pool
);
