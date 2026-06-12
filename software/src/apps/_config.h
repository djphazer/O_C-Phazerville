// workaround
namespace menu = OC::menu;

#ifndef NO_HEMISPHERE

#ifdef ARDUINO_TEENSY41
#include "apps/Quadrants.h"
#else
#include "apps/Hemisphere.h"
#endif

#endif

#include "apps/Calibr8or.h"
#include "apps/Scenery.h"
#include "apps/ASR.h"
#ifdef ENABLE_APP_H1200
#include "apps/H1200.h"
#endif
#ifdef ENABLE_APP_AUTOMATONNETZ
#include "apps/Automatonnetz.h"
#endif
#include "apps/Sequins.h"
#include "apps/QQ.h"
#include "apps/DQ.h"
#include "apps/Quadraturia.h"
#include "apps/Lorenz.h"
#include "apps/Piqued.h"
#include "apps/BBGEN.h"
#include "apps/Viznutcracker.h"
#include "apps/Chords.h"
#ifdef ENABLE_APP_REFERENCES
#include "apps/References.h"
#endif
// #include "apps/Passencore.h"
#include "apps/CaptainMIDI.h"
#include "apps/TheDarkestTimeline.h"
#include "apps/Enigma.h"
#ifdef ENABLE_APP_NEURAL_NETWORK
#include "apps/NeuralNetwork.h"
#endif
#include "apps/ScaleEditor.h"
#include "apps/WaveformEditor.h"
#include "apps/PongGame.h"
#include "apps/Backup.h"
#include "apps/SETTINGS.h"


namespace OC {

/*
// The order in the AppContainer is not inconsequential.
// Each app's Start() method is called in sequence.
// For example, the default quantizer settings from Hemisphere
// are overwritten when Calibr8or loads its settings
*/

// Instantiate the available apps below.
// Any type not listed here should not exist, i.e. the linker should be able to
// triage all code (minus any dangling static parts).

static AppContainer<void // this space intentionally left blank
  , AppSettings
#ifndef NO_HEMISPHERE
  #ifdef ARDUINO_TEENSY41
  , AppQuadrants
  #else
  , AppHemisphere
  #endif
#endif
#ifdef ENABLE_APP_CALIBR8OR
  , AppCalibr8or
#endif
#ifdef ENABLE_APP_SCENES
  , AppScenery
#endif
#ifdef ENABLE_APP_MIDI
  , AppCaptainMIDI
#endif
#ifdef ENABLE_APP_DARKEST_TIMELINE
  , TheDarkestTimeline
#endif
#ifdef ENABLE_APP_ENIGMA
  , AppEnigma
#endif
#ifdef ENABLE_APP_NEURAL_NETWORK
  , AppNeuralNetwork
#endif
#ifdef ENABLE_APP_PASSENCORE
  // , AppPassencore
#endif
#ifdef ENABLE_APP_ASR
  , AppASR
#endif
#ifdef ENABLE_APP_H1200
  , AppH1200
#endif
#ifdef ENABLE_APP_AUTOMATONNETZ
  , AppAutomatonnetz
#endif
#ifdef ENABLE_APP_QUANTERMAIN
  , AppQuadQuantizer
#endif
#ifdef ENABLE_APP_METAQ
  , AppDualQuantizer
#endif
#ifdef ENABLE_APP_POLYLFO
  , AppPolyLfo
#endif
#ifdef ENABLE_APP_LORENZ
  , AppLorenzGenerator
#endif
#ifdef ENABLE_APP_PIQUED
  , AppQuadEnvelopeGenerator
#endif
#ifdef ENABLE_APP_SEQUINS
  , AppDualSequencer
#endif
#ifdef ENABLE_APP_BBGEN
  , AppQuadBouncingBalls
#endif
#ifdef ENABLE_APP_BYTEBEATGEN
  , AppQuadByteBeats
#endif
#ifdef ENABLE_APP_CHORDS
  , AppChordQuantizer
#endif
#ifdef ENABLE_APP_REFERENCES
  , AppReferences
#endif
#ifdef ENABLE_APP_PONG
  , AppPong
#endif
  , AppScaleEditor
#ifndef NO_HEMISPHERE
  , AppWaveformEditor
#endif
  , AppBackup
> app_container;

static_assert(decltype(app_container)::TotalAppDataStorageSize() < AppData::kAppDataSize,
              "Apps use too much EEPROM space!");

static constexpr int DEFAULT_APP_INDEX = 1;
static constexpr uint16_t DEFAULT_APP_ID = decltype(app_container)::GetAppIDAtIndex<DEFAULT_APP_INDEX>();

}
