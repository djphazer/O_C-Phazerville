/*
 * OneShot Sample Player applet
 *    by Tom Whiston
 *    based on WAVPlayerApplet and TeensyVariablePlayback library by Nic Newdigate
 *
 * A oneshot sample player designed for triggering individual samples from a
 * folder on the SD card with CV control over sample selection, playback speed,
 * and a gate-controlled envelope for clean playback termination.
 *
 * SD Card Structure:
 *   /oneshot/00/
 *   /oneshot/01/
 *   /oneshot/02/
 *   ...
 *
 * Inside each folder, WAV files are loaded alphabetically.
 * All sampling rates should work, but they must be 16-bit.
 *
 * A dot indicator shows if the folder exists and if a sample is loaded.
 *
 */

#include <TeensyVariablePlayback.h>

template <AudioChannels Channels>
class OneShotPlayerApplet : public HemisphereAudioApplet {
public:
  const uint64_t applet_id() override {
    return strhash("OneShot");
  }
  const char* applet_name() override {
    return titlestat;
  }

  void Start() {
    // Setup audio connections
    for (int i = 0; i < Channels; i++) {
      // Input passthrough
      PatchCable(input, i, mixer[i], 1);
      mixer[i].gain(1, 1.0);

      // Sample playback direct to mixer
      PatchCable(wavplayer, i, mixer[i], 0);

      // Output
      PatchCable(mixer[i], 0, output, i);
    }

    // Initialize SD card player
    if (!SDcard_Ready) {
      Serial.println("Unable to access the SD card");
    } else {
      wavplayer.enableInterpolation(true);
      wavplayer.setBufferInPSRAM(false);
    }
  }

  void Unload() {
    wavplayer.stop();
    AllowRestart();
  }

  void Reset() override {
    trigger_cv.Reset();
  }

  void Controller() {
    bool current_gate = trigger_cv.Gate();
    bool trig = trigger_cv.Clock();

    // Handle trigger/gate modes
    if (trig_mode == TRIG_MODE_TRIGGER) {
      // Trigger mode: on rising edge, play entire sample with attack envelope
      if (trig) {
        sample_playtrig = true;
        env_level = 0.0f;  // Start attack from zero
        env_stage = ENV_ATTACK;
        // If attack is 0, jump straight to sustain
        if (attack == 0) {
          env_level = 1.0f;
          env_stage = ENV_SUSTAIN;
        }
      }
    } else {
      // Gate mode: play while gate high, release when gate goes low
      if (trig) {
        // Rising edge - start playback with attack
        sample_playtrig = true;
        env_level = 0.0f;  // Start attack from zero
        env_stage = ENV_ATTACK;
        if (attack == 0) {
          env_level = 1.0f;
          env_stage = ENV_SUSTAIN;
        }
      } else if (prev_gate_state && !current_gate) {
        // Falling edge - start release
        env_stage = ENV_RELEASE;
      }
    }
    prev_gate_state = current_gate;

    // Process envelope
    // Controller runs at ~16.667kHz (once per ms approximately)
    // Using HEMISPHERE_CLOCK_TICKS = 17 ticks per ms
    const float tick_ms = 1.0f / HEMISPHERE_CLOCK_TICKS; // ~0.059ms per tick

    switch (env_stage) {
      case ENV_ATTACK:
        if (attack > 0) {
          float attack_inc = tick_ms / (attack * 10);  // attack is in 10ms units
          env_level += attack_inc;
          if (env_level >= 1.0f) {
            env_level = 1.0f;
            env_stage = ENV_SUSTAIN;
          }
        } else {
          env_level = 1.0f;
          env_stage = ENV_SUSTAIN;
        }
        break;

      case ENV_SUSTAIN:
        // Gate mode: release when gate goes low
        if (trig_mode == TRIG_MODE_GATE && !current_gate) {
          env_stage = ENV_RELEASE;
        }
        // When sample finishes, go idle
        if (!sample_playtrig && !wavplayer.isPlaying()) {
          env_stage = ENV_IDLE;
          env_level = 0.0f;
        }
        break;

      case ENV_RELEASE:
        if (release > 0) {
          float release_dec = tick_ms / (release * 10);  // release is in 10ms units
          env_level -= release_dec;
          if (env_level <= 0.0f) {
            env_level = 0.0f;
            env_stage = ENV_IDLE;
            // Stop playback when release completes
            if (wavplayer.isPlaying()) {
              wavplayer.stop();
            }
          }
        } else {
          env_level = 0.0f;
          env_stage = ENV_IDLE;
          if (wavplayer.isPlaying()) {
            wavplayer.stop();
          }
        }
        break;

      case ENV_IDLE:
      default:
        env_level = 0.0f;
        break;
    }

    // Process sample CV - offset the GUI selection
    if (folder_file_count > 0) {
      int cv_sample = sample_cv.InRescaled(folder_file_count);
      sample_index_mod = constrain(sample_index + cv_sample, 0, folder_file_count - 1);
    } else {
      sample_index_mod = 0;
    }

    // Process playback rate CV
    float rate = 0.01f * playrate + playrate_cv.InF(0.0f);
    if (wavplayer_ready) {
      wavplayer.setPlaybackRate(rate);
    }

    // Process level CV and apply envelope
    float gain = dbToScalar(level) + level_cv.InF(0.0f);
    if (gain < 0.0f) gain = 0.0f;
    gain *= env_level; // Apply envelope
    SetLevel(gain);

    // Update title status
    titlestat[7] = wavplayer.isPlaying() ? '*' : ' ';
    titlestat[8] = current_gate ? '|' : ' ';
  }

  void mainloop() {
    if (!SDcard_Ready) return;

    // Handle folder changes
    if (folder_changed) {
      ScanFolder();
      folder_changed = false;
      sample_reload = true;
      loaded_sample_index = -1; // Invalidate so the new folder's sample gets loaded

      // Clamp sample_index to valid range for new folder
      if (folder_file_count > 0 && sample_index >= folder_file_count) {
        sample_index = folder_file_count - 1;
      } else if (folder_file_count == 0) {
        sample_index = 0;
      }
    }

    // Detect sample change (sample-and-hold behavior)
    if (loaded_sample_index != sample_index_mod) {
      sample_reload = true;
    }

    // Handle sample loading - only when not playing (sample-and-hold)
    if (sample_reload && folder_file_count > 0 && !wavplayer.isPlaying()) {
      LoadSample(sample_index_mod);
      loaded_sample_index = sample_index_mod;
      sample_reload = false;
    }

    // Handle sample trigger
    if (sample_playtrig) {
      // If sample has changed while playing, stop and load the new one
      if (wavplayer.isPlaying() && loaded_sample_index != sample_index_mod) {
        wavplayer.stop();
        LoadSample(sample_index_mod);
        loaded_sample_index = sample_index_mod;
        sample_reload = false;
      } else if (wavplayer.isPlaying()) {
        // Same sample, just retrigger from beginning
        wavplayer.stop();
      }
      wavplayer.play();
      sample_playtrig = false;
    }
  }

  void View() {
    if (!SDcard_Ready) {
      gfxPrint(4, 25, "NO SD CARD!!");
      return;
    }

    // Line 1: Folder and Sample selection (y=15)
    gfxPrint(1, 15, "F");
    gfxStartCursor();
    graphics.printf("%02u", folder_num);
    gfxEndCursor(cursor == FOLDER_NUM);
    gfxPrint(folder_exists ? "." : " ");

    gfxPrint("S");
    gfxStartCursor();
    graphics.printf("%03u", sample_index_mod);
    gfxEndCursor(cursor == SAMPLE_NUM);
    gfxPrint(wavplayer_ready ? "." : " ");
    gfxStartCursor();
    gfxPrint(sample_cv);
    gfxEndCursor(cursor == SAMPLE_CV, false, sample_cv.InputName());

    // Line 2: Playback rate (y=25)
    gfxPrint(1, 25, "Rate");
    gfxStartCursor();
    graphics.printf("%4d%%", playrate);
    gfxEndCursor(cursor == PLAYRATE);
    gfxStartCursor();
    gfxPrint(playrate_cv);
    gfxEndCursor(cursor == PLAYRATE_CV, false, playrate_cv.InputName());

    // Line 3: Level (y=35)
    gfxPrint(1, 35, "Lvl");
    gfxStartCursor();
    graphics.printf("%4ddB", level);
    gfxEndCursor(cursor == LEVEL);
    gfxStartCursor();
    gfxPrint(level_cv);
    gfxEndCursor(cursor == LEVEL_CV, false, level_cv.InputName());

    // Line 4: Trigger input and Mode (y=45)
    gfxPrint(1, 45, "In");
    gfxStartCursor();
    gfxPrint(trigger_cv);
    gfxEndCursor(cursor == TRIGGER_INPUT, false, trigger_cv.InputName());

    gfxPrint(" ");
    gfxStartCursor();
    gfxPrint(trig_mode == TRIG_MODE_TRIGGER ? "Trg" : "Gte");
    gfxEndCursor(cursor == TRIG_MODE);

    // Line 5: Attack and Release (y=55)
    gfxPrint(1, 55, "A");
    gfxStartCursor();
    graphics.printf("%3d", attack);
    gfxEndCursor(cursor == ATTACK);

    gfxPrint(" R");
    gfxStartCursor();
    graphics.printf("%3d", release);
    gfxEndCursor(cursor == RELEASE);

    gfxDisplayInputMapEditor();
  }

  void AuxButton() {
    // Manual trigger
    if (SDcard_Ready && folder_file_count > 0) {
      sample_playtrig = true;
      env_level = 0.0f;  // Start attack from zero
      env_stage = ENV_ATTACK;
      if (attack == 0) {
        env_level = 1.0f;
        env_stage = ENV_SUSTAIN;
      }
    }
  }

  void OnButtonPress() {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(SAMPLE_CV, sample_cv),
          IndexedInput(PLAYRATE_CV, playrate_cv),
          IndexedInput(LEVEL_CV, level_cv),
          IndexedInput(TRIGGER_INPUT, trigger_cv)
        ))
      return;

    CursorToggle();
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, NUM_PARAMS - 1);
      return;
    }
    if (EditSelectedInputMap(direction)) return;

    switch (cursor) {
      case FOLDER_NUM:
        folder_num = constrain(folder_num + direction, 0, 99);
        folder_changed = true;
        break;
      case SAMPLE_NUM:
        if (folder_file_count > 0) {
          sample_index = constrain(sample_index + direction, 0, folder_file_count - 1);
          sample_reload = true;
        }
        break;
      case SAMPLE_CV:
        sample_cv.ChangeSource(direction);
        break;
      case PLAYRATE:
        playrate = constrain(playrate + direction, -200, 200);
        break;
      case PLAYRATE_CV:
        playrate_cv.ChangeSource(direction);
        break;
      case LEVEL:
        level = constrain(level + direction, -90, 12);
        break;
      case LEVEL_CV:
        level_cv.ChangeSource(direction);
        break;
      case TRIGGER_INPUT:
        trigger_cv.ChangeSource(direction);
        break;
      case TRIG_MODE:
        trig_mode = (trig_mode + 1) % 2;
        break;
      case ATTACK:
        attack = constrain(attack + direction, 0, 200);
        break;
      case RELEASE:
        release = constrain(release + direction, 0, 200);
        break;
    }
  }

  void OnDataRequest(std::array<uint64_t, CONFIG_SIZE>& data) override {
    // Stop playback to avoid SD card hangup on preset save
    wavplayer.stop();

    data[0] = PackPackables(
      folder_num, // 8 bits
      sample_index, // 16 bits
      sample_cv, // 16 bits
      playrate // 16 bits
    );
    data[1] = PackPackables(
      playrate_cv, // 16 bits
      level, // 8 bits
      level_cv, // 16 bits
      trigger_cv // 16 bits
    );
    data[2] = PackPackables(
      trig_mode,  // 8 bits
      attack,     // 8 bits
      release     // 8 bits
    );
  }

  void OnDataReceive(const std::array<uint64_t, CONFIG_SIZE>& data) override {
    UnpackPackables(data[0], folder_num, sample_index, sample_cv, playrate);
    UnpackPackables(data[1], playrate_cv, level, level_cv, trigger_cv);
    UnpackPackables(data[2], trig_mode, attack, release);

    folder_changed = true;
    sample_reload = true;
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
  enum OneShotCursor {
    FOLDER_NUM,
    SAMPLE_NUM,
    SAMPLE_CV,
    PLAYRATE,
    PLAYRATE_CV,
    LEVEL,
    LEVEL_CV,
    TRIGGER_INPUT,
    TRIG_MODE,
    ATTACK,
    RELEASE,

    NUM_PARAMS
  };

  enum TriggerMode {
    TRIG_MODE_TRIGGER = 0,
    TRIG_MODE_GATE = 1
  };

  char titlestat[10] = "OneShot  ";

  int cursor = 0;

  // Parameters
  uint8_t folder_num = 0;
  int16_t sample_index = 0;
  int16_t sample_index_mod = 0;
  int16_t playrate = 100;
  int8_t level = 0; // dB
  uint8_t trig_mode = TRIG_MODE_TRIGGER;
  uint8_t attack = 0;   // Attack time in 10ms units (0-200 = 0-2000ms)
  uint8_t release = 5;  // Release time in 10ms units (0-200 = 0-2000ms)

  // CV Input maps
  CVInputMap sample_cv;
  CVInputMap playrate_cv;
  CVInputMap level_cv;
  DigitalInputMap trigger_cv;

  // State
  bool folder_changed = true;
  bool sample_reload = true;
  bool sample_playtrig = false;
  bool wavplayer_ready = false;
  bool folder_exists = false;
  uint16_t folder_file_count = 0;
  int16_t loaded_sample_index = -1; // Track currently loaded sample

  // Envelope state
  enum EnvelopeStage { ENV_IDLE, ENV_ATTACK, ENV_SUSTAIN, ENV_RELEASE };
  EnvelopeStage env_stage = ENV_IDLE;
  float env_level = 0.0f;        // Current envelope level (0.0 to 1.0)
  bool prev_gate_state = false;  // For edge detection in trigger mode

  // Audio objects
  AudioPassthrough<Channels> input;
  AudioPlaySdResmp wavplayer;
  AudioMixer4 mixer[2];
  AudioPassthrough<Channels> output;

  void SetLevel(float gain) {
    for (int ch = 0; ch < Channels; ch++) {
      mixer[ch].gain(0, gain); // Sample channel
    }
  }

  void ScanFolder() {
    // Build folder path: /oneshot/00/
    char folder_path[24];
    strcpy(folder_path, "/oneshot/");
    folder_path[9] = '0' + (folder_num / 10);
    folder_path[10] = '0' + (folder_num % 10);
    folder_path[11] = '/';
    folder_path[12] = '\0';

    // Reset state
    folder_file_count = 0;
    folder_exists = false;

    if (!SD.exists(folder_path)) {
      return;
    }

    File dir = SD.open(folder_path);
    if (!dir) {
      return;
    }

    folder_exists = true;

    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;

      if (!entry.isDirectory()) {
        const char* name = entry.name();
        size_t len = strlen(name);
        if (len > 4
            && (strcasecmp(name + len - 4, ".wav") == 0 || strcasecmp(name + len - 4, ".WAV") == 0)) {
          folder_file_count++;
        }
      }
      entry.close();
    }
    dir.close();
  }

  void LoadSample(int index) {
    if (folder_file_count == 0) {
      wavplayer_ready = false;
      return;
    }

    // Build folder path: /oneshot/00/
    char folder_path[24];
    strcpy(folder_path, "/oneshot/");
    folder_path[9] = '0' + (folder_num / 10);
    folder_path[10] = '0' + (folder_num % 10);
    folder_path[11] = '/';
    folder_path[12] = '\0';

    File dir = SD.open(folder_path);
    if (!dir) {
      wavplayer_ready = false;
      return;
    }

    // Find the Nth WAV file
    int current_index = 0;
    char filepath[64];
    bool found = false;

    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;

      if (!entry.isDirectory()) {
        const char* name = entry.name();
        size_t len = strlen(name);
        if (len > 4
            && (strcasecmp(name + len - 4, ".wav") == 0 || strcasecmp(name + len - 4, ".WAV") == 0)) {
          if (current_index == index) {
            // Build full path
            strcpy(filepath, folder_path);
            strcat(filepath, name);
            found = true;
            entry.close();
            break;
          }
          current_index++;
        }
      }
      entry.close();
    }
    dir.close();

    if (found) {
      wavplayer_ready = wavplayer.playWav(filepath);
    } else {
      wavplayer_ready = false;
    }
  }
};
