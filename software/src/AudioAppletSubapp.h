#pragma once

#include "AudioIO.h"
#include "HemisphereAudioApplet.h"
#include "OC_ui.h"
#include "PhzConfig.h"
#include "elapsedMillis.h"
#include "waveheaderparser.h"
#include "src/UI/ui_events.h"
#include "util/util_math.h"
#include "util/util_tuples.h"

#define ForEachSide(ch) for (HEM_SIDE ch : {LEFT_HEMISPHERE, RIGHT_HEMISPHERE})

using std::array, std::tuple;

template <class T, size_t N>
class Slot {};

template <
  uint_fast8_t Slots,
  size_t NumMonoProcessors,
  size_t NumStereoProcessors,
  typename MonoReg,
  typename StereoReg>
class AudioAppletSubapp {
public:
  MonoReg &mono_applets;
  StereoReg &stereo_applets;

  const array<RegID, NumMonoProcessors> mono_appletIds;
  const array<RegID, NumStereoProcessors> stereo_appletIds;

  static constexpr size_t NumStereoSources = NumStereoProcessors;
  static constexpr size_t NumMonoSources = NumMonoProcessors;

  AudioAppletSubapp(MonoReg &mono_applets_, StereoReg &stereo_applets_)
    : mono_applets(mono_applets_)
    , stereo_applets(stereo_applets_)
    , mono_appletIds(mono_applets.getIds())
    , stereo_appletIds(stereo_applets.getIds())
  {
    selected_mono_applets[0].fill(0);
    selected_mono_applets[1].fill(0);
    selected_stereo_applets.fill(0);

    // Input applet for top slots
    selected_mono_applets[0][0] = 1;
    selected_mono_applets[1][0] = 1;
    selected_stereo_applets[0] = 1;
  }

  void Init() {
    for (size_t slot = 0; slot < Slots; slot++) {
      if (IsStereo(slot)) {
        get_selected_stereo_applet(slot).BaseStart(LEFT_HEMISPHERE);
        ForEachSide(side) ConnectStereoToNext(side, slot);
      } else {
        ForEachSide(side) {
          get_selected_mono_applet(side, slot).BaseStart(side);
          ConnectMonoToNext(side, slot);
        }
      }
    }
    peak_conns[0][0].connect(OC::AudioIO::InputStream(), 0, peaks[0][0], 0);
    peak_conns[1][0].connect(OC::AudioIO::InputStream(), 1, peaks[1][0], 0);
  }

  void ReInit() {
    for (size_t slot = 0; slot < Slots; slot++) {
      // reset to defaults here
      if (IsStereo(slot)) {
        ChangeStereoApplet(LEFT_HEMISPHERE, slot, slot ? 0 : 1);
        stereo ^= 1 << slot; // change it back to dual mono
        SwapMonoStereo(slot);
      }
      ForEachSide(side) {
        ChangeMonoApplet(side, slot, slot ? 0 : 1); // Input for top slots
      }
    }
  }

  void Controller() {
    AudioNoInterrupts();
    for (size_t i = 0; i < Slots; i++) {
      if (IsStereo(i)) {
        get_selected_stereo_applet(i).Controller();
      } else {
        get_selected_mono_applet(LEFT_HEMISPHERE, i).Controller();
        get_selected_mono_applet(RIGHT_HEMISPHERE, i).Controller();
      }
    }
    AudioInterrupts();
  }

  void mainloop() {
    for (size_t slot = 0; slot < Slots; slot++) {
      if (IsStereo(slot)) {
        get_selected_stereo_applet(slot).mainloop();
      } else {
        get_selected_mono_applet(LEFT_HEMISPHERE, slot).mainloop();
        get_selected_mono_applet(RIGHT_HEMISPHERE, slot).mainloop();
      }
    }
  }

  void View() {
    gfxDottedLine(0, 0, 127, 0);

    const bool forcemenu = state[0] != EDIT_APPLET && state[1] != EDIT_APPLET;

    if (forcemenu || (state[0] != EDIT_APPLET && menutimer[0] < MENU_TIMEOUT)
                  || (state[1] != EDIT_APPLET && menutimer[1] < MENU_TIMEOUT)) {
      for (size_t i = 0; i < Slots; i++) {
        print_applet_line(i);
      }
    }

    ForEachSide(side) {
      if (state[side] == EDIT_APPLET) {
        HemisphereAudioApplet& applet = get_selected_applet(side);
        applet.SetDisplaySide(static_cast<HEM_SIDE>(side + AUDIO_SLOT_L));
        const bool full = (state[1 - side] != EDIT_APPLET)
          && menutimer[1 - side] > MENU_TIMEOUT + MENU_ANIMATE;
        applet.BaseView(full, full);
        continue;
      }

      if (forcemenu || menutimer[side] < MENU_TIMEOUT) {
        int y = cursor[side] * 10 + 14;
        if (state[side] == SWITCH_APPLET) {
          int x = IsStereo(cursor[side]) && state[1 - side] != EDIT_APPLET
            ? 32
            : 64 * side;
          gfxInvert(x, y, 64, 9);
        } else {
          gfxIcon(120 * side, y + 1, side ? LEFT_ICON : RIGHT_ICON);
        }
        for (uint_fast8_t slot = 0; slot < Slots + 1; slot++) {
          draw_peak(side, slot);
        }

        gfxPos(1 + 64 * side, 2);
        if (side) graphics.printf("MEM%3d%%) R", mem_percent);
        else graphics.printf("L (CPU%3d%%", cpu_percent);
      } else if (menutimer[side] < MENU_TIMEOUT + MENU_ANIMATE) {
        // a smooth wipe transition
        int x = Proportion(menutimer[side] - MENU_TIMEOUT, MENU_ANIMATE, 62);
        if (side) x += 64;
        else x = 63 - x;
        gfxLine(x, 11, x, 63);
        if (side) ++x;
        else --x;
        gfxDottedLine(x, 11, x, 63);
      }
    }

    if (last_stats_update > STATS_TIMEOUT) {
      last_stats_update = 0;
      mem_percent = static_cast<int16_t>(
        100 * static_cast<float>(AudioMemoryUsageMax())
        / OC::AudioIO::AUDIO_MEMORY
      );
      cpu_percent = static_cast<int16_t>(AudioProcessorUsageMax());
      AudioProcessorUsageMaxReset();
      AudioMemoryUsageMaxReset();
    }
  }

  // returns true to exit
  bool HandleButtonEvent(const UI::Event& event) {
    if (event.type == UI::EVENT_BUTTON_PRESS) {
      switch (event.control) {
        case OC::CONTROL_BUTTON_A:
          if (MOVE_CURSOR == state[0]) return true;
          state[0] = MOVE_CURSOR;
          menutimer[0] = 0;
          break;
        case OC::CONTROL_BUTTON_B:
          if (MOVE_CURSOR == state[1]) return true;
          state[1] = MOVE_CURSOR;
          menutimer[1] = 0;
          break;
        case OC::CONTROL_BUTTON_X:
          if (MOVE_CURSOR != state[0])
            get_selected_applet(LEFT_HEMISPHERE).AuxButton();
          break;
        case OC::CONTROL_BUTTON_Y:
          if (MOVE_CURSOR != state[1])
            get_selected_applet(RIGHT_HEMISPHERE).AuxButton();
          break;
        default:
          break;
      }
    }
    return false;
  }

  void SwapMonoStereo(int c) {
    if (IsStereo(c)) {
      get_selected_stereo_applet(c).BaseStart(HEM_SIDE(LEFT_HEMISPHERE + AUDIO_SLOT_L));
      ForEachSide(side) {
        get_selected_mono_applet(side, c).Disconnect();
        get_selected_mono_applet(side, c).Unload();
        ConnectStereoToNext(side, c);
        if (c > 0) ConnectSlotToNext(side, c - 1);
      }
    } else {
      get_selected_stereo_applet(c).Disconnect();
      get_selected_stereo_applet(c).Unload();
      ForEachSide(side) {
        get_selected_mono_applet(side, c).BaseStart(HEM_SIDE(side + AUDIO_SLOT_L));
        ConnectMonoToNext(side, c);
        if (c > 0) ConnectSlotToNext(side, c - 1);
      }
    }
  }

  void HandleEncoderButtonEvent(const UI::Event& event) {
    if (event.mask == (OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R)) {
      // check ready_for_press to suppress double events on button combos
      if (cursor[0] == cursor[1] && ready_for_press) {
        int c = cursor[0];
        stereo ^= 1 << c;

        SwapMonoStereo(c);
      }
      // Prevent press detection when doing a button combo
      ready_for_press = false;
    } else if (event.type == UI::EVENT_BUTTON_PRESS && ready_for_press) {
      if (event.control == OC::CONTROL_BUTTON_L)
        HandleEncoderPress(LEFT_HEMISPHERE);
      if (event.control == OC::CONTROL_BUTTON_R)
        HandleEncoderPress(RIGHT_HEMISPHERE);
    } else if (event.type == UI::EVENT_BUTTON_DOWN) {
      ready_for_press = true;
    }
  }

  void HandleEncoderPress(HEM_SIDE side) {
    int c = cursor[side];
    switch (state[side]) {
      case MOVE_CURSOR:
        candidate[side] = IsStereo(c) ? selected_stereo_applets[c]
                                      : selected_mono_applets[side][c];
        state[side] = SWITCH_APPLET;
        break;
      case SWITCH_APPLET:
        if (IsStereo(c)) ChangeStereoApplet(side, c, candidate[side]);
        else ChangeMonoApplet(side, c, candidate[side]);
        if (candidate[side])
          state[side] = EDIT_APPLET;
        else // don't edit the PassthruApplet (index 0)
          state[side] = MOVE_CURSOR;
        break;
      case EDIT_APPLET:
        get_selected_applet(side).OnButtonPress();
        break;
    }
  }

  void ChangeStereoApplet(HEM_SIDE side, size_t slot, int ix) {
    int& sel = selected_stereo_applets[slot];
    if (ix == sel) return;
    get_selected_stereo_applet(slot).Disconnect();
    get_selected_stereo_applet(slot).Unload();
    sel = ix;
    auto& app = get_selected_stereo_applet(slot);
    app.BaseStart(HEM_SIDE(side + AUDIO_SLOT_L));
    ForEachSide(side) ConnectStereoToNext(side, slot);
    if (slot > 0) {
      ForEachSide(side) ConnectSlotToNext(side, slot - 1);
    }
  }

  void ChangeMonoApplet(HEM_SIDE side, size_t slot, int ix) {
    int& sel = selected_mono_applets[side][slot];
    if (ix == sel) return;
    get_selected_mono_applet(side, slot).Disconnect();
    get_selected_mono_applet(side, slot).Unload();
    sel = ix;
    auto& app = get_selected_mono_applet(side, slot);
    app.BaseStart(HEM_SIDE(side + AUDIO_SLOT_L));
    ConnectMonoToNext(side, slot);
    if (slot > 0) ConnectSlotToNext(side, slot - 1);
  }

  void ForwardEncoderMove(HEM_SIDE side, size_t slot, int dir) {
    auto& app = IsStereo(slot) ? get_selected_stereo_applet(slot)
                               : get_selected_mono_applet(side, slot);
    app.OnEncoderMove(dir);
  }

  void HandleEncoderEvent(const UI::Event& event) {
    int dir = event.value;
    if (event.mask & (OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R)) {
      // push-and-turn for coarse adjustments
      dir *= 10;
      OC::ui.SetButtonIgnoreMask();
    }
    if (event.control == OC::CONTROL_ENCODER_L)
      HandleEncoderEvent(LEFT_HEMISPHERE, dir);
    if (event.control == OC::CONTROL_ENCODER_R)
      HandleEncoderEvent(RIGHT_HEMISPHERE, dir);
  }

  void HandleEncoderEvent(HEM_SIDE side, int dir) {
    int& c = cursor[side];
    switch (state[side]) {
      case MOVE_CURSOR:
        c = constrain(c + dir, 0, static_cast<int>(Slots) - 1);
        menutimer[side] = 0;
        break;
      case SWITCH_APPLET: {
        int n = IsStereo(c) ? (c == 0 ? NumStereoSources : NumStereoProcessors)
                            : (c == 0 ? NumMonoSources : NumMonoProcessors);
        candidate[side] = constrain(candidate[side] + dir, 0, n - 1);
        menutimer[side] = 0;
        break;
      }
      case EDIT_APPLET:
        ForwardEncoderMove(side, c, dir);
        break;
    }
  }

  void ConnectSlotToNext(HEM_SIDE side, size_t slot) {
    if (IsStereo(slot)) {
      ConnectStereoToNext(side, slot);
    } else {
      ConnectMonoToNext(side, slot);
    }
  }

  void ConnectMonoToNext(HEM_SIDE side, size_t slot) {
    AudioStream* stream = get_selected_mono_applet(side, slot).OutputStream();
    AudioConnection& conn = conns[side][slot];
    conn.disconnect();
    if (slot + 1 < Slots && !IsStereo(slot + 1)) {
      conn.connect(
        *stream, 0, *get_selected_mono_applet(side, slot + 1).InputStream(), 0
      );
    } else {
      AudioStream* next_stream = slot + 1 < Slots
        ? get_selected_stereo_applet(slot + 1).InputStream()
        : &OC::AudioIO::OutputStream();
      conn.connect(*stream, 0, *next_stream, side);
    }
    peak_conns[side][slot + 1].disconnect();
    peak_conns[side][slot + 1].connect(*stream, 0, peaks[side][slot + 1], 0);
  }

  void ConnectStereoToNext(HEM_SIDE side, size_t slot) {
    AudioStream* stream = get_selected_stereo_applet(slot).OutputStream();
    AudioConnection& conn = conns[side][slot];
    conn.disconnect();
    if (slot + 1 < Slots && !IsStereo(slot + 1)) {
      AudioStream* next_stream
        = get_selected_mono_applet(side, slot + 1).InputStream();
      conn.connect(*stream, side, *next_stream, 0);
    } else {
      AudioStream* next_stream = slot + 1 < Slots
        ? get_selected_stereo_applet(slot + 1).InputStream()
        : &OC::AudioIO::OutputStream();
      conn.connect(*stream, side, *next_stream, side);
    }
    peak_conns[side][slot + 1].disconnect();
    peak_conns[side][slot + 1].connect(*stream, side, peaks[side][slot + 1], 0);
  }

  // 3 bits, cannot be 0
  enum AudioConfigSections : uint8_t {
    MAIN = 1,
    MONO_APPLETS = 2,
    STEREO_APPLETS = 3,
    MONO_APPLET_PARAMS = 4,
    STEREO_APPLET_PARAMS = 5,
  };

  enum AudioConfigMainKeys : uint8_t { STEREO_MODE_FLAGS };

  constexpr uint16_t key(uint8_t section, uint8_t key) const {
    return (section << 8) | key;
  }

  // Returns a reference to the filtered peak dB value for a given side and slot. useful if we want to stop processing at some gated value without instantiating a whole applet.
  float& get_peak_db(HEM_SIDE side, size_t slot) {
    return lpf_peak_db[side][slot];
  }

  void LoadPreset(int id) {
    // preset id is upper 5 bits - 32 presets per bank
    uint16_t preset_key = id << 11;

    uint64_t data = 0;
    uint64_t oldstereo = stereo;
    PhzConfig::getValue(preset_key | key(MAIN, STEREO_MODE_FLAGS), data);
    stereo = data & 0xFFFFFFFF;

    for (size_t slot = 0; slot < Slots; ++slot) {

      if (IsStereo(slot)) {
        if (0 == ((oldstereo >> slot) & 1)) {
          SwapMonoStereo(slot);
        }
        data = 0; // Default to input/passthrough if nothing is found.
        // stereo applets
        PhzConfig::getValue(preset_key | key(STEREO_APPLETS, slot), data);
        int ix = get_stereo_applet_ix_by_id(slot, data, 0);
        Serial.printf("\n%lu: Loading applet id=", slot);
        Serial.print(data, HEX);
        Serial.printf(" ix=%d", ix);
        ChangeStereoApplet(LEFT_HEMISPHERE, slot, ix);

        if (data) {
          LoadAppletData(
            preset_key | key(STEREO_APPLET_PARAMS, slot * APPLET_CONFIG_SIZE),
            get_selected_stereo_applet(slot),
            slot
          );
        }
      } else {
        if ((oldstereo >> slot) & 1) {
          SwapMonoStereo(slot);
        }
        // mono applets
        ForEachSide(ch) {
          uint8_t slot_key = slot + ch * Slots;
          data = 0; // Default to input/passthrough if nothing is found.
          PhzConfig::getValue(preset_key | key(MONO_APPLETS, slot_key), data);
          int ix = get_mono_applet_ix_by_id(ch, slot, data, 0);
          Serial.printf("\n%lu, %d: Loading applet id=", slot, ch);
          Serial.print(data, HEX);
          Serial.printf(" ix=%d", ix);
          ChangeMonoApplet(ch, slot, ix);

          if (data) {
            LoadAppletData(
              preset_key | key(MONO_APPLET_PARAMS, slot_key * APPLET_CONFIG_SIZE),
              get_selected_mono_applet(ch, slot),
              slot
            );
          }
        }
      }
    }
  }

  void SavePreset(int id) {
    //PhzConfig::clear_config();
    // We assume the config file is already loaded somewhere else,
    // we're just injecting our keys into it.

    // preset id is upper 5 bits - 32 presets per bank
    uint16_t preset_key = id << 11;

    PhzConfig::setValue(preset_key | key(MAIN, STEREO_MODE_FLAGS), (uint64_t)stereo); // bitset
    uint64_t applet_id = 0;
    for (size_t slot = 0; slot < Slots; ++slot) {
      auto& stereo_applet = get_selected_stereo_applet(slot);
      applet_id = stereo_applet.applet_id();
      Serial.printf("\n%lu: Saving applet id=", slot);
      Serial.print(applet_id, HEX);
      PhzConfig::setValue(preset_key | key(STEREO_APPLETS, slot), applet_id);

      SaveAppletData(
        preset_key | key(STEREO_APPLET_PARAMS, slot * APPLET_CONFIG_SIZE),
        stereo_applet,
        slot
      );

      ForEachSide(ch) {
        uint8_t slot_key = slot + ch * Slots;
        auto& mono_applet = get_selected_mono_applet(ch, slot);
        applet_id = mono_applet.applet_id();
        Serial.printf("\n%lu, %d: Saving applet id=", slot, ch);
        Serial.print(applet_id, HEX);
        PhzConfig::setValue(preset_key | key(MONO_APPLETS, slot_key), applet_id);

        SaveAppletData(
          preset_key | key(MONO_APPLET_PARAMS, slot_key * APPLET_CONFIG_SIZE),
          mono_applet,
          slot
        );
      }
    }
  }

  void LoadAppletData(uint16_t key, HemisphereAudioApplet& applet, size_t slot) {
    array<uint64_t, APPLET_CONFIG_SIZE> data;
    for (uint_fast8_t i = 0; i < APPLET_CONFIG_SIZE; ++i) {
      if (PhzConfig::getValue(key + i, data[i])) {
        Serial.printf(" | data[%u]=", i);
        Serial.print(data[i], HEX);
      } else {
        Serial.printf(" | no data[%u]", i);
        data[i] = 0;
      }
    }
    applet.SetSlot(slot);
    applet.OnDataReceive(data);
  }

  void SaveAppletData(uint16_t key, HemisphereAudioApplet& applet, size_t slot) {
    array<uint64_t, APPLET_CONFIG_SIZE> data = {0};
    applet.SetSlot(slot);
    applet.OnDataRequest(data);
    for (uint_fast8_t i = 0; i < APPLET_CONFIG_SIZE; ++i) {
      // We default to 0, so may as well skip them to save space
      if (data[i]) PhzConfig::setValue(key + i, data[i]);
      else PhzConfig::deleteKey(key + i); // clears old unused data
      Serial.printf(" | data[%u]=", i);
      Serial.print(data[i], HEX);
    }
  }

protected:
  inline bool IsStereo(size_t slot) {
    return (stereo >> slot) & 1;
  }

private:
  static const size_t APPLET_CONFIG_SIZE = HemisphereAudioApplet::CONFIG_SIZE;

  uint32_t stereo = 0; // bitset

  // indexes
  array<array<int, Slots>, 2> selected_mono_applets;
  array<int, Slots> selected_stereo_applets;

  array<array<AudioConnection, Slots + 1>, 2> conns;
  array<array<AudioAnalyzePeak, Slots + 1>, 2> peaks;
  array<array<AudioConnection, Slots + 1>, 2> peak_conns;
  array<array<float, Slots + 1>, 2> lpf_peak_db;

  bool ready_for_press = false;
  size_t total, user, free;

  uint32_t last_update = 0;

  int16_t mem_percent = 0;
  int16_t cpu_percent = 0;

  elapsedMillis last_stats_update = 0;
  static constexpr int STATS_TIMEOUT = 250;

  // enum ViewState { NORMAL, RECORD_CONFIG, MASTER_FX };
  // ViewState view_state = NORMAL;

  enum EditState {
    MOVE_CURSOR,
    SWITCH_APPLET,
    EDIT_APPLET,
  };

  // cursor movement to show menu
  elapsedMillis menutimer[2];
  static constexpr int MENU_TIMEOUT = 5000;
  static constexpr int MENU_ANIMATE = 500;

  EditState state[2];
  int cursor[2]; // selected slot for each side
  // candidate applet for each side, referenced by index into applets arrays
  int candidate[2];

  PassthruApplet<MONO> dummy_mono;
  PassthruApplet<STEREO> dummy_stereo;

  HemisphereAudioApplet& get_mono_applet(
    HEM_SIDE side, size_t slot, size_t ix
  ) {
    HemisphereAudioApplet* target = mono_applets.get(mono_appletIds[ix], side * Slots + slot);
    if (target) return *target;

    static uint8_t counter = 0;
    if (counter < 10) {
      ++counter;
      SERIAL_PRINTLN("nullptr from mono_applets - side: %u slot: %u ix: %u", side, slot, ix);
    }
    return dummy_mono;
  }

  HemisphereAudioApplet& get_stereo_applet(size_t slot, size_t ix) {
    HemisphereAudioApplet* target = stereo_applets.get(stereo_appletIds[ix], slot);
    if (target) return *target;

    static uint8_t counter = 0;
    if (counter < 10) {
      ++counter;
      SERIAL_PRINTLN("nullptr from stereo_applets - slot: %u ix: %u", slot, ix);
    }
    return dummy_stereo;
  }

  HemisphereAudioApplet& get_selected_mono_applet(HEM_SIDE side, size_t slot) {
    return get_mono_applet(side, slot, selected_mono_applets[side][slot]);
  }

  HemisphereAudioApplet& get_selected_stereo_applet(size_t slot) {
    return get_stereo_applet(slot, selected_stereo_applets[slot]);
  }

  HemisphereAudioApplet& get_listed_mono_applet(HEM_SIDE side, int slot) {
    if (cursor[side] == slot && state[side] == SWITCH_APPLET) {
      return get_mono_applet(side, slot, candidate[side]);
    }
    return get_selected_mono_applet(side, slot);
  }

  template <size_t N>
  int get_applet_ix_by_id(array<RegID, N> appletIds, RegID id, int default_value = 0) {
    for (size_t i = 0; i < appletIds.size(); i++) {
      if (appletIds[i] == id) return static_cast<int>(i);
    }
    return default_value;
  }

  int get_mono_applet_ix_by_id(
    HEM_SIDE side, int slot, RegID id, int default_value = 0
  ) {
    return get_applet_ix_by_id(mono_appletIds, id, default_value);
  }

  int get_stereo_applet_ix_by_id(
    int slot, RegID id, int default_value = 0
  ) {
      return get_applet_ix_by_id(stereo_appletIds, id, default_value);
  }

  HemisphereAudioApplet& get_listed_stereo_applet(int slot) {
    ForEachSide(side) {
      if (cursor[side] == slot && state[side] == SWITCH_APPLET) {
        return get_stereo_applet(slot, candidate[side]);
      }
    }
    return get_selected_stereo_applet(slot);
  }

  HemisphereAudioApplet& get_selected_applet(HEM_SIDE side) {
    int c = cursor[side];
    if (IsStereo(c)) {
      return get_selected_stereo_applet(c);
    } else {
      return get_selected_mono_applet(side, c);
    }
  }

  void print_applet_line(int slot) {
    int y = 15 + 10 * slot;
    if (IsStereo(slot)) {
      const char* name = get_listed_stereo_applet(slot).applet_name();
      const int l = static_cast<int>(strlen(name));
      if (state[0] != EDIT_APPLET && state[1] != EDIT_APPLET) {
        gfxPrint(64 - l * 3, y, name);
      } else {
        ForEachSide(side) {
          if (state[side] != EDIT_APPLET && cursor[1 - side] != slot) {
            gfxPrint(64 - (1 - side) * (1 + l * 6), y, name);
          }
        }
      }
    } else {
      ForEachSide(side) {
        if (state[side] != EDIT_APPLET) {
          const char* name = get_listed_mono_applet(side, slot).applet_name();
          const int l = static_cast<int>(strlen(name));
          gfxPrint(8 + side * (110 - l * 6), y, name);
        }
      }
    }
  }

  int peak_width(HEM_SIDE side, int slot) {
    AudioAnalyzePeak& p = peaks[side][slot];
    float& db = lpf_peak_db[side][slot];
    if (p.available()) {
      ONE_POLE(db, scalarToDb(p.read()), 0.25f);
      if (db < -48.0f) db = -48.0f;
    }
    return static_cast<int>((db + 48.0f) / 48.0f * 64);
  }
  void draw_peak(HEM_SIDE side, int slot, int y = -1) {
    int w = peak_width(side, slot);
    if (y < 0) y = slot * 10 + 13;
    gfxInvert(side ? 64 : 64 - w, y, w, 1);
  }
};
