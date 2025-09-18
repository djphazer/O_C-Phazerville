/*
 * WAV file player applet
 *
 * It looks in the root directory of the SD card for the
 * selected file, by number, in the format:
 *   000.WAV
 *   001.WAV
 *   002.WAV
 *   ...
 *
 * Sync mode will automatically lock tempo with the internal clock.
 *
 */

#include "Audio/AudioPassthrough.h"
#include "CVInputMap.h"
#include "HemisphereAudioApplet.h"
#include "OC_gpio.h"
#include <TeensyVariablePlayback.h>

template <AudioChannels Channels>
class WavPlayerApplet : public HemisphereAudioApplet {
public:
  const uint64_t applet_id() override {
    return strhash("WavPlay");
  }
  const char* applet_name() override {
    return titlestat;
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, mixer[i], 3);
      mixer[i].gain(3, 1.0);
      out_conns[i].connect(mixer[i], 0, output, i);
    }

    hpfilter[0].resonance(1.0);
    hpfilter[1].resonance(1.0);
    SetFilter(0);

    // -- SD card WAV players
    if (!SDcard_Ready) {
      Serial.println("Unable to access the SD card");
    }
    else {
      wavplayer.enableInterpolation(true);
      wavplayer.setBufferInPSRAM(false);
    }
  }
  void Unload() {
    wavplayer.stop();
  }

  void Controller() {
    djfilter_mod = constrain(djfilter + djfilter_cv.InRescaled(128), -64, 64);
    SetFilter(filter_on * djfilter_mod);

    float gain = dbToScalar(level) + level_cv.InF(0.0f);
    if (gain < 0.0f) gain = 0.0f;

    FileLevel(gain);
    if (tempo_sync)
      FileMatchTempo();
    else
      FileRate(0.01f * playrate + playrate_cv.InF(0.0f));

    if (HS::clock_m.EndOfBeat() && FileIsPlaying()) {
      if (loop_length && loop_on) {
        if (++loop_count >= loop_length)
          retrig = true;
      }

      sync_trig = true;
    }

    if (playstop_cv.Clock()) {
      if (FileIsPlaying())
        retrig = true;
      else
        StartPlaying();
    }

    titlestat[7] = FileIsPlaying() ? '*' : ' ';
    titlestat[8] = (filter_on) ? '/' : ' ';

    if (!HS::clock_m.IsRunning() || HS::clock_m.EndOfBeat())
      go_time = true;
  }

  void mainloop() {
    if (SDcard_Ready) {
        if (wavplayer_reload) {
          FileLoad();
          wavplayer_reload = false;
        }

        if (wavplayer_playtrig && (go_time || !tempo_sync)) {
          wavplayer.play();
          loop_count = 0;
          wavplayer_playtrig = false;
        }

        if (syncloopstart && go_time) {
          ToggleLoop();
          syncloopstart = false;
          retrig = false;
        }
        if (retrig) {
          if (wavplayer.available()) {
            wavplayer.retrigger();
            loop_count = 0;
          }
          retrig = false;
        }


        if (tempo_sync && sync_trig) {
          wavplayer.syncTrig();
          sync_trig = false;
        }
    }
  }

  void View() {
    if (!SDcard_Ready) {
      gfxPrint(4, 25, "NO SD CARD!!");
      return;
    }
    size_t y = 13;
    gfxStartCursor(1, y);
    gfxPrintfn(1, y, 0, "%03u", GetFileNum());
    gfxEndCursor(cursor == FILE_NUM);

    gfxIcon(22, y, FileIsPlaying() ? PLAY_ICON : STOP_ICON);
    gfxStartCursor(30, y-1);
    gfxPrint(playstop_cv);
    gfxEndCursor(cursor == PLAYSTOP_GATE_CV, false, playstop_cv.InputName());

    if (wavplayer_ready)
      gfxPrint(37, y, GetFileBPM());
    else
      gfxPrint(37, y, "(--)");

    // filter mod
    gfxStartCursor(56, y);
    gfxPrint(djfilter_cv);
    gfxEndCursor(cursor == FILTER_CV, false, djfilter_cv.InputName());

    // filter meter at the top
    if (filter_on) {
      const int w = abs(djfilter_mod);
      const int x = (djfilter_mod<0)?(63 - w):0;
      gfxInvert(x, y-3, w, 4);
    }

    y += 10;
    if (cursor == FILTER_PARAM) {
      if (EditMode()) {
        if (djfilter > 0) gfxPrint(36, y, "HPF");
        else if (djfilter < 0) gfxPrint(36, y, "LPF");
        else gfxPrint(40, y, "X");
      } else {
        gfxIcon(49, y, RIGHT_ICON);
        gfxIcon(56, y, (filter_on) ? SLEW_ICON : PhzIcons::tuner);
      }
    } else if (FileIsPlaying()) {
      uint32_t tmilli = GetFileTime();
      uint32_t tsec = tmilli / 1000;
      uint32_t tmin = tsec / 60;
      tmilli %= 1000;
      tsec %= 60;

      gfxPos(1, y);
      graphics.printf("%02lu:%02lu.%03lu", tmin, tsec, tmilli);
    }

    y += 10;
    gfxPrint(1, y, "Lvl:");
    gfxStartCursor();
    graphics.printf("%3ddB", level);
    gfxEndCursor(cursor == LEVEL);
    gfxStartCursor();
    gfxPrint(level_cv);
    gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

    y += 10;
    if (tempo_sync) {
      gfxPrint(1, y, "Sync:");
    } else {
      gfxPrint(1, y, "Rate:");
    }
    gfxStartCursor();
    graphics.printf("%3d%%", playrate);
    gfxEndCursor(cursor == PLAYRATE, true);
    gfxStartCursor();
    gfxPrint(playrate_cv);
    gfxEndCursor(cursor == PLAYRATE_CV, false, playrate_cv.InputName());

    y += 10;
    gfxPrint(1, y, "Loop:");
    gfxStartCursor();
    graphics.printf("%2u", loop_length);
    gfxEndCursor(cursor == LOOP_LENGTH);

    gfxIcon(52, y, loop_on ? CHECK_ON_ICON : CHECK_OFF_ICON);
    if (cursor == LOOP_ENABLE)
      gfxFrame(51, y-1, 10, 10);

    gfxDisplayInputMapEditor();
  }

  void AuxButton() {
    if (FILTER_PARAM == cursor) {
      filter_on = !filter_on;
    } else if (PLAYRATE == cursor) {
      tempo_sync ^= 1;
    } else
      ToggleFilePlayer();
  }
  void OnButtonPress() {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(PLAYSTOP_GATE_CV, playstop_cv),
          IndexedInput(FILTER_CV, djfilter_cv),
          IndexedInput(LEVEL_CV, level_cv),
          IndexedInput(PLAYRATE_CV, playrate_cv)
    )) return;

    if (LOOP_ENABLE == cursor) {
      syncloopstart = true;
      go_time = false;
    } else
      CursorToggle();
  };
  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, NUM_PARAMS - 1);
      return;
    }
    if(EditSelectedInputMap(direction)) return;
    switch (cursor) {
      case FILE_NUM:
        ChangeToFile(GetFileNum() + direction);
        break;
      case PLAYSTOP_GATE_CV:
        playstop_cv.ChangeSource(direction);
        break;
      case FILTER_PARAM:
        filter_on = true;
        djfilter = constrain(djfilter + direction, -63, 63);
        break;
      case FILTER_CV:
        djfilter_cv.ChangeSource(direction);
        break;
      case LEVEL:
        level = constrain(level + direction, -90, 90);
        break;
      case LEVEL_CV:
        level_cv.ChangeSource(direction);
        break;
      case PLAYRATE:
        playrate = constrain(playrate + direction, -200, 200);
        break;
      case PLAYRATE_CV:
        playrate_cv.ChangeSource(direction);
        break;
      case LOOP_LENGTH:
        loop_length = constrain(loop_length + direction, 0, 128);
        break;
    }
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    // STOP playback to avoid SD card hangup on preset save
    wavplayer.stop();
    data[0] = PackPackables(level, level_cv, int8_t(playrate), playrate_cv, wavplayer_select, djfilter);
    data[1] = PackPackables(playrate, djfilter_cv, playstop_cv, loop_length);
  }
  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    int8_t old_playrate;
    UnpackPackables(data[0], level, level_cv, old_playrate, playrate_cv, wavplayer_select, djfilter);
    UnpackPackables(data[1], playrate, djfilter_cv, playstop_cv, loop_length);
    if (playrate == 0) playrate = old_playrate;
    if (loop_length == 0) loop_length = 8;
    ChangeToFile(wavplayer_select);
  }

  AudioStream* InputStream() override {
    return &input;
  }
  AudioStream* OutputStream() override {
    return &output;
  }

protected:
  void SetHelp() override {}

private:
  enum WAVCursor {
    FILE_NUM,
    PLAYSTOP_GATE_CV,
    FILTER_PARAM,
    FILTER_CV,
    LEVEL,
    LEVEL_CV,
    PLAYRATE,
    PLAYRATE_CV,
    LOOP_LENGTH,
    LOOP_ENABLE,

    NUM_PARAMS
  };

  char titlestat[10] = "WavPlay  ";

  int cursor = 0;
  int8_t level = -3; // dB
  int8_t djfilter = 0; // positive is hi-pass, negative low-pass
  int8_t djfilter_mod = 0; // for display
  CVInputMap djfilter_cv;
  // flags
  bool lowcut = false;
  bool filter_on = false;
  bool tempo_sync = true;

  DigitalInputMap playstop_cv;
  CVInputMap level_cv;
  int16_t playrate = 100; // TODO: we need 9 bits for +/-200%
  CVInputMap playrate_cv;
  // signals from ISR->loop
  bool go_time = false;
  bool sync_trig = false;
  bool retrig = false;

  AudioPassthrough<Channels> input;
  AudioPlaySdResmp      wavplayer;
  AudioFilterStateVariable hpfilter[2];
  AudioMixer4           mixer[2];
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;

  AudioConnection          patchCordWav1L{wavplayer, 0, hpfilter[0], 0};
  AudioConnection          patchCordWav1R{wavplayer, 1, hpfilter[1], 0};
  AudioConnection          patchCordWav1Ldry{wavplayer, 0, mixer[0], 0};
  AudioConnection          patchCordWav1Rdry{wavplayer, 1, mixer[1], 0};
  AudioConnection          patchCordWavHPF1L{hpfilter[0], 2, mixer[0], 1};
  AudioConnection          patchCordWavHPF1R{hpfilter[1], 2, mixer[1], 1};
  AudioConnection          patchCordWavLPF2L{hpfilter[0], 0, mixer[0], 2};
  AudioConnection          patchCordWavLPF2R{hpfilter[1], 0, mixer[1], 2};

  // SD player vars, copied from other dev branch
  bool wavplayer_reload = true;
  bool wavplayer_playtrig = false;
  bool wavplayer_ready = false;
  uint8_t wavplayer_select = 1;
  uint8_t loop_length = 8;
  int8_t loop_count = 0;
  bool loop_on = false;
  bool syncloopstart = false;

  // SD file player functions
  void FileLoad() {
    char filename[] = "000.WAV";
    filename[1] += wavplayer_select / 10;
    filename[2] += wavplayer_select % 10;
    wavplayer_ready = wavplayer.playWav(filename);
  }
  void StartPlaying() {
    wavplayer_playtrig = true;
    go_time = false;
  }
  bool FileIsPlaying() {
    return wavplayer.isPlaying();
  }
  void ToggleFilePlayer() {
    if (wavplayer.isPlaying()) {
      wavplayer.stop();
      wavplayer.setPlayStart(play_start_sample);
      loop_on = false;
    } else if (SDcard_Ready) {
      StartPlaying();
    }
  }
  void ToggleLoop() {
    if (loop_length && !loop_on) {
      const uint32_t start = wavplayer.isPlaying() ?
                    wavplayer.getPosition() : 0;
      wavplayer.setLoopStart( start );
      wavplayer.setPlayStart(play_start_loop);
      if (wavplayer.available())
        wavplayer.retrigger();
      loop_on = true;
      loop_count = 0;
    } else {
      wavplayer.setPlayStart(play_start_sample);
      loop_on = false;
    }
  }

  static constexpr int FILTER_MAX = (60 << 7); // 5V ~ 14.4khz
  void SetFilter(int scalar) {
    lowcut = (scalar < 0);
    if (scalar < 2 && scalar > -2) {
      for (int ch = 0; ch < Channels; ++ch) {
        hpfilter[ch].frequency(0);
      }
      return;
    }

    float freq = scalar * FILTER_MAX / 64;
    if (lowcut)
      freq = (FILTER_MAX - abs(freq)) / 64;
    else
      freq = abs(freq) / 64;
    freq *= freq;

    for (int ch = 0; ch < Channels; ++ch) {
      hpfilter[ch].frequency(freq);
    }
  }

  void ChangeToFile(int select) {
    wavplayer_select = (uint8_t)constrain(select, 0, 99);
    wavplayer_reload = true;
    if (wavplayer.isPlaying()) {
      StartPlaying();
    }
  }
  uint8_t GetFileNum() {
    return wavplayer_select;
  }
  uint32_t GetFileTime() {
    return wavplayer.positionMillis();
  }
  uint16_t GetFileBPM() {
    return (uint16_t)wavplayer.getBPM();
  }
  void FileMatchTempo() {
    wavplayer.matchTempo(
      HS::clock_m.GetTempoFloat() * (playrate * 0.01f + playrate_cv.InF(0.0f))
    );
  }
  void FileLevel(float lvl) {
    bool dry = (djfilter_mod < 2 && djfilter_mod > -2);
    for (int ch = 0; ch < Channels; ++ch) {
      mixer[ch].gain(0, lvl * dry);
      mixer[ch].gain(1, lvl * (1-lowcut) * (1-dry));
      mixer[ch].gain(2, lvl * lowcut * (1-dry));
    }
  }
  void FileRate(float rate) {
    // bipolar CV has +/- 50% pitch bend
    wavplayer.setPlaybackRate(rate);
  }
};
