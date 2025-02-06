#include "Audio/AudioPassthrough.h"
#include "HemisphereAudioApplet.h"
#include <TeensyVariablePlayback.h>

template <AudioChannels Channels>
class WavPlayerApplet : public HemisphereAudioApplet {
public:
  const char* applet_name() {
    return titlestat;
  }

  void Start() {
    for (int i = 0; i < Channels; i++) {
      in_conns[i].connect(input, i, mixer[i], 3);
      out_conns[i].connect(mixer[i], 0, output, i);
    }

    hpfilter[0][0].resonance(0.7);
    hpfilter[0][1].resonance(0.7);
    hpfilter[1][0].resonance(0.7);
    hpfilter[1][1].resonance(0.7);
    FileHPF(0, 0);
    FileHPF(1, 0);

    // -- SD card WAV players
    if (!HS::wavplayer_available) {
      Serial.println("Unable to access the SD card");
    }
    else {
      wavplayer[0].enableInterpolation(true);
      //wavplayer[1].enableInterpolation(true);
      wavplayer[0].setBufferInPSRAM(false);
      //wavplayer[1].setBufferInPSRAM(true);
    }
  }

  void Controller() {
    float gain = dbToScalar(level)
      * static_cast<float>(level_cv.In(HEMISPHERE_MAX_INPUT_CV))
      / HEMISPHERE_MAX_INPUT_CV;

    // TODO: we probably don't need 2 in one applet
    // orrrr, we could have dynamic number of players...
    for (int i = 0; i < 1; i++) {
      FileLevel(i, gain);
      const int speed_cv = 0; // TODO:
      if (speed_cv > 0)
        FileRate(i, 0.01f * playrate);
      else
        FileMatchTempo(i);

      if (HS::clock_m.EndOfBeat()) {
        if (loop_length[i] && loop_on[i]) {
          if (++loop_count[i] >= loop_length[i])
            FileHotCue(i);
        }

        wavplayer[i].syncTrig();
      }
    }

    titlestat[7] = FileIsPlaying() ? '*' : ' ';
    titlestat[8] = lowcut[0] ? '/' : ' ';

    if (!HS::clock_m.IsRunning() || HS::clock_m.EndOfBeat())
      go_time = true;
  }

  void mainloop() {
    if (HS::wavplayer_available) {
      for (int ch = 0; ch < 1; ++ch) {
        if (wavplayer_reload[ch]) {
          FileLoad(ch);
          wavplayer_reload[ch] = false;
        }

        if (wavplayer_playtrig[ch] && go_time) {
          wavplayer[ch].play();
          wavplayer_playtrig[ch] = false;
        }

        if (syncloopstart && go_time) {
          ToggleLoop();
          syncloopstart = false;
        }
      }
    }
  }

  void View() {
    size_t y = 13;
    gfxStartCursor(1, y);
    gfxPrintfn(1, y, 0, "%03u", GetFileNum());
    gfxEndCursor(cursor == FILE_NUM);

    gfxIcon(25, y, FileIsPlaying() ? PLAY_ICON : STOP_ICON);
    if (cursor == PLAYSTOP_BUTTON)
      gfxFrame(24, y-1, 10, 10);

    if (cursor == HPF_TOGGLE) {
      gfxIcon(35, y, RIGHT_ICON);
      gfxIcon(45, y, lowcut[0] ? BD_ICON : MOD_ICON);
      if (lowcut[0]) gfxPrint(45, y, "X");
    }

    if (FileIsPlaying()) {
      if (cursor != HPF_TOGGLE) {
        gfxPrint(34, y, GetFileBPM());
      }

      uint32_t tmilli = GetFileTime(0);
      uint32_t tsec = tmilli / 1000;
      uint32_t tmin = tsec / 60;
      tmilli %= 1000;
      tsec %= 60;

      gfxPos(1, y+10);
      graphics.printf("%02lu:%02lu.%03lu", tmin, tsec, tmilli);
    }

    y += 20;
    gfxPrint(1, y, "Lvl:");
    gfxStartCursor();
    graphics.printf("%3ddB", level);
    gfxEndCursor(cursor == LEVEL);
    gfxStartCursor();
    gfxPrintIcon(level_cv.Icon());
    gfxEndCursor(cursor == LEVEL_CV);

    y += 10;
    gfxPrint(1, y, "Rate:");
    gfxStartCursor();
    graphics.printf("%3d%%", playrate);
    gfxEndCursor(cursor == PLAYRATE);
    gfxStartCursor();
    gfxPrintIcon(playrate_cv.Icon());
    gfxEndCursor(cursor == PLAYRATE_CV);

    y += 10;
    gfxPrint(1, y, "Loop:");
    gfxStartCursor();
    graphics.printf("%2u", loop_length[0]);
    gfxEndCursor(cursor == LOOP_LENGTH);

    gfxIcon(52, y, loop_on[0] ? CHECK_ON_ICON : CHECK_OFF_ICON);
    if (cursor == LOOP_ENABLE)
      gfxFrame(51, y-1, 10, 10);
  }

  void AuxButton() {
    ToggleFilePlayer();
  }
  void OnButtonPress() {
    if (PLAYSTOP_BUTTON == cursor)
      ToggleFilePlayer();
    else if (HPF_TOGGLE == cursor)
      ToggleLowCut();
    else if (LOOP_ENABLE == cursor) {
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
    switch (cursor) {
      case FILE_NUM:
        ChangeToFile(0, GetFileNum() + direction);
        break;
      case PLAYSTOP_BUTTON:
        // shouldn't happen
        CursorToggle();
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
        loop_length[0] = constrain(loop_length[0] + direction, 0, 128);
        break;
    }
  }

  uint64_t OnDataRequest() {
    return 0;
  }
  void OnDataReceive(uint64_t data) {}

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
    PLAYSTOP_BUTTON,
    HPF_TOGGLE,
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
  CVInputMap level_cv;
  int playrate = 100;
  CVInputMap playrate_cv;
  bool go_time;

  AudioPassthrough<Channels> input;
  AudioPlaySdResmp      wavplayer[1];
  AudioFilterStateVariable hpfilter[2][2];
  AudioMixer4           mixer[2];
  AudioPassthrough<Channels> output;

  std::array<AudioConnection, Channels> in_conns;
  std::array<AudioConnection, Channels> out_conns;

  AudioConnection          patchCordWav1L{wavplayer[0], 0, hpfilter[0][0], 0};
  AudioConnection          patchCordWav1R{wavplayer[0], 1, hpfilter[0][1], 0};
  //AudioConnection          patchCordWav2L{wavplayer[1], 0, hpfilter[1][0], 0};
  //AudioConnection          patchCordWav2R{wavplayer[1], 1, hpfilter[1][1], 0};
  AudioConnection          patchCordWavHPF1L{hpfilter[0][0], 2, mixer[0], 0};
  AudioConnection          patchCordWavHPF1R{hpfilter[0][1], 2, mixer[1], 0};
  //AudioConnection          patchCordWavHPF2L{hpfilter[1][0], 2, mixer[0], 1};
  //AudioConnection          patchCordWavHPF2R{hpfilter[1][1], 2, mixer[1], 1};

  // SD player vars, copied from other dev branch
  bool wavplayer_reload[2] = {true, true};
  bool wavplayer_playtrig[2] = {false};
  uint8_t wavplayer_select[2] = { 1, 10 };
  uint8_t loop_length[2] = { 8, 8 };
  int8_t loop_count[2] = { 0, 0 };
  bool loop_on[2] = { false, false };
  bool lowcut[2] = { false, false };
  bool syncloopstart = false;

  // SD file player functions
  void FileLoad(int ch = 0) {
    char filename[] = "000.WAV";
    filename[1] += wavplayer_select[ch] / 10;
    filename[2] += wavplayer_select[ch] % 10;
    wavplayer[ch].playWav(filename);
  }
  void StartPlaying(int ch = 0) {
    wavplayer_playtrig[ch] = true;
    loop_count[ch] = -1;
    go_time = false;
  }
  bool FileIsPlaying(int ch = 0) {
    return wavplayer[ch].isPlaying();
  }
  void ToggleFilePlayer(int ch = 0) {
    if (wavplayer[ch].isPlaying()) {
      wavplayer[ch].stop();
    } else if (HS::wavplayer_available) {
      StartPlaying(ch);
    }
  }
  void FileHotCue(int ch = 0) {
    if (wavplayer[ch].available()) {
      wavplayer[ch].retrigger();
      loop_count[ch] = 0;
    }
  }
  void ToggleLoop(int ch = 0) {
    if (loop_length[ch] && !loop_on[ch]) {
      const uint32_t start = wavplayer[ch].isPlaying() ?
                    wavplayer[ch].getPosition() : 0;
      wavplayer[ch].setLoopStart( start );
      wavplayer[ch].setPlayStart(play_start_loop);
      if (wavplayer[ch].available())
        wavplayer[ch].retrigger();
      loop_on[ch] = true;
      loop_count[ch] = 0;
    } else {
      wavplayer[ch].setPlayStart(play_start_sample);
      loop_on[ch] = false;
    }
  }

  void FileHPF(int ch, int cv) {
    float freq = abs(cv) / 64;
    freq *= freq;

    hpfilter[ch][0].frequency(freq);
    hpfilter[ch][1].frequency(freq);
  }
  void ToggleLowCut(int ch = 0) {
    lowcut[ch] = !lowcut[ch];
    FileHPF(ch, lowcut[ch] * 1000);
  }

  // simple hooks for beat-sync callbacks
  void FileToggle1() { ToggleFilePlayer(0); }
  void FileToggle2() { ToggleFilePlayer(1); }
  void FilePlay1() { StartPlaying(0); }
  void FilePlay2() { StartPlaying(1); }
  void FileLoopToggle1() { ToggleLoop(0); }
  void FileLoopToggle2() { ToggleLoop(1); }

  void ChangeToFile(int ch, int select) {
    wavplayer_select[ch] = (uint8_t)constrain(select, 0, 99);
    wavplayer_reload[ch] = true;
    if (wavplayer[ch].isPlaying()) {
      StartPlaying(ch);
    }
  }
  uint8_t GetFileNum(int ch = 0) {
    return wavplayer_select[ch];
  }
  uint32_t GetFileTime(int ch = 0) {
    return wavplayer[ch].positionMillis();
  }
  uint16_t GetFileBPM(int ch = 0) {
    return (uint16_t)wavplayer[ch].getBPM();
  }
  void FileMatchTempo(int ch = 0) {
    wavplayer[ch].matchTempo(HS::clock_m.GetTempoFloat() * playrate * 0.01f);
  }
  void FileLevel(int ch, float lvl) {
    mixer[0].gain(0 + ch, lvl);
    mixer[1].gain(0 + ch, lvl);
  }
  void FileRate(int ch, float rate) {
    // bipolar CV has +/- 50% pitch bend
    wavplayer[ch].setPlaybackRate(rate);
  }
};
