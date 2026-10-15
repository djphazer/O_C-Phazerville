#pragma once

// F32-native: source select, level VCA, and the output routing matrix run on
// float32 blocks (AudioMixerF32 / AudioVCA_F32), so the level CV reaches the
// VCAs at full float precision instead of q15 and the selected source is
// never requantized between stages; peak metering reads the float blocks
// (AudioAnalyzePeakF32). The hardware/USB int16 sources convert to float
// once at dedicated edge adapters, and the chain still sees int16 via the
// HemisphereAudioAppletF32 edge adapters. Params, ranges, and UI are
// unchanged.

#include "../Audio/AudioVCA_F32.h"
#include "../Audio/AudioMixerF32.h"
#include "../Audio/AudioAnalyzePeakF32.h"
#include "../Audio/InterpolatingStreamF32.h"

class InputApplet : public HemisphereAudioAppletF32 {
public:
  const uint64_t applet_id() override {
    return strhash("Input");
  }

  const char* applet_name() {
    return "Input";
  }
  void Start() override {
    if (MONO == Channels()) {
      modesel_ = hemisphere & 1;
    }

    ForEachChannel(ch) {
      PatchCable(OC::AudioIO::InputStream(0), ch, i2s_conv, ch);
      PatchCableF32(i2s_conv, ch, srcmix[ch], 0);
#ifdef AUDIO_INTERFACE
      PatchCable(OC::AudioIO::InputStream(1), ch, usb_conv, ch);
      PatchCableF32(usb_conv, ch, srcmix[ch], 1);
#endif

      attenuations[ch].Method(INTERPOLATION_LINEAR);
      attenuations[ch].Acquire();
      PatchCableF32(srcmix[ch], 0, vcas[ch], 0);
      PatchCableF32(attenuations[ch], 0, vcas[ch], 1);

      PatchCableF32(srcmix[ch], 0, peakmeter[ch], 0);
    }

    for (int i = 0; i < Channels(); ++i) {
      ForEachChannel(ch) {
        PatchCableF32(vcas[ch], 0, mixer[i], ch);
      }
      PatchCableF32(InputF32(), i, mixer[i], 2);
      PatchCableF32(mixer[i], 0, OutputF32(), i);

      mixer[i].gain(0, 0.0); // Left to i
      mixer[i].gain(1, 0.0); // Right to i

      mixer[i].gain(2, 1.0); // passthru
    }
    UpdateMix();
  }
  void Unload() override {
    for (int i = 0; i < Channels(); ++i) {
      attenuations[i].Release();
    }
    AllowRestart();
  }
  void Controller() override {
    ForEachChannel(ch) {
      attenuations[ch].Push(level_cv.InF(1.0));
    }
  }
  void View() override {
    const char* const txt[] = {
      "Left", "Right", "Dual", "Mixed",
#ifdef AUDIO_INTERFACE
      "USB L", "USB R", "USB Dual", "USB Mix"
#endif
    };
    gfxPrint(3, 15, txt[modesel_]);
    if (cursor == CHANNEL_MODE) gfxCursor(3, 23, 31, "Mode");
    gfxPrint(1, 45, "Lvl:");
    gfxPrintDb(level);
    if (cursor == IN_LEVEL) gfxCursor(26, 53, 30, "Gain");
    gfxStartCursor();
    gfxPrint(level_cv);
    gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

    ForEachChannel(ch) {
      if (peakmeter[ch].available()) {
        int peaklvl = peakmeter[ch].read() * 64;
        gfxInvert(ch*61, 64 - peaklvl, 3, peaklvl);
      }
    }

    gfxDisplayInputMapEditor();
  }
  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    // abandoning old data at index 0
    //data[0] = PackPackables(...);
    data[1] = PackPackables(modesel_, level, level_cv);
  }
  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    if (data[0] != 0) {
      // backward compat
      bool mixtomono;
      UnpackPackables(data[0], pack<4>(modesel_), pack(level), pack<1>(mixtomono), level_cv);
      if (mixtomono || modesel_ == 2) modesel_ = MIXED;
      CONSTRAIN(modesel_, 0, MODE_COUNT - 1);
    } else {
      UnpackPackables(data[1], modesel_, level, level_cv);
    }
    UpdateMix();
  }

  // *****
  void OnButtonPress() override {
    if (CheckEditInputMapPress(cursor, IndexedInput(LEVEL_CV, level_cv)))
      return;
    CursorToggle();
  }
  // *****
  void OnEncoderMove(int direction) override {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    if (EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case IN_LEVEL:
        level = constrain(level + direction, LVL_MIN_DB - 1, LVL_MAX_DB);
        break;
      case LEVEL_CV:
        level_cv.ChangeSource(direction);
        break;
      case CHANNEL_MODE:
        modesel_ = constrain(modesel_ + direction, 0, MODE_COUNT - 1);
        if (Channels() == MONO && (modesel_ == DUAL
#ifdef AUDIO_INTERFACE
            || modesel_ == USB_DUAL
#endif
          )) modesel_ += direction;
        break;
    }
    UpdateMix();
  }

  void UpdateMix() {
    float lvl_scalar = level < LVL_MIN_DB ? 0.0f : dbToScalar(level); // * level_cv.InF(1.0f);

    ForEachChannel(ch) {
      switch (modesel_) {
#ifdef AUDIO_INTERFACE
        case USB_DUAL:
        case USB_MIX:
        case USB_L:
        case USB_R:
          srcmix[ch].gain(0, 0.0f); // i2s
          srcmix[ch].gain(1, 1.0f); // usb
          break;
#endif
        case DUAL:
        case MIXED:
        case LEFT:
        case RIGHT:
          srcmix[ch].gain(0, 1.0f); // i2s
          srcmix[ch].gain(1, 0.0f); // usb
          break;
      }
    }
    for (int ch = 0; ch < Channels(); ++ch) {
      switch (modesel_) {
#ifdef AUDIO_INTERFACE
        case USB_DUAL:
#endif
        case DUAL:
          mixer[ch].gain(0, lvl_scalar * (1-ch)); // Left to ch
          mixer[ch].gain(1, lvl_scalar * ch); // Right to ch
          break;
#ifdef AUDIO_INTERFACE
        case USB_MIX:
#endif
        case MIXED:
          // TODO: adjust by 0.5 when mixing to mono?
          mixer[ch].gain(0, lvl_scalar); // Left to ch
          mixer[ch].gain(1, lvl_scalar); // Right to ch
          break;
#ifdef AUDIO_INTERFACE
        case USB_L:
#endif
        case LEFT:
          mixer[ch].gain(0, lvl_scalar); // Left to ch
          mixer[ch].gain(1, 0.0f); // Right to ch
          break;
#ifdef AUDIO_INTERFACE
        case USB_R:
#endif
        case RIGHT:
          mixer[ch].gain(0, 0.0f); // Left to ch
          mixer[ch].gain(1, lvl_scalar); // Right to ch
          break;
      }
    }

  }

protected:
  void SetHelp() override {}

private:
  AudioConvertI16toF32Multi<2> i2s_conv;
#ifdef AUDIO_INTERFACE
  AudioConvertI16toF32Multi<2> usb_conv;
#endif
  std::array<InterpolatingStreamF32<>, 2> attenuations;
  std::array<AudioVCA_F32, 2> vcas;
  AudioMixerF32<2> srcmix[2];
  AudioMixerF32<3> mixer[STEREO];
  AudioAnalyzePeakF32 peakmeter[2];

  int8_t modesel_ = DUAL;
  int8_t level = 0;
  CVInputMap level_cv;

  enum ModeSelect {
    LEFT, RIGHT, DUAL, MIXED,
#ifdef AUDIO_INTERFACE
    USB_L, USB_R, USB_DUAL, USB_MIX,
#endif
    MODE_COUNT
  };
  enum InputCursor { CHANNEL_MODE, IN_LEVEL, LEVEL_CV, MAX_CURSOR = LEVEL_CV };
  int cursor = 0;
};
