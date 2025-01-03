#pragma once

#include "AudioIO.h"
#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "OC_ui.h"
#include "UI/ui_events.h"
#include "util/util_tuples.h"
#include <Audio.h>
#include <cstdint>

#define ForEachSide(ch) for(HEM_SIDE ch : {LEFT_HEMISPHERE, RIGHT_HEMISPHERE})

using std::array, std::tuple;

template <class T, size_t N>
class Slot {};

template <
  uint_fast8_t Slots,
  size_t NumMonoSources,
  size_t NumStereoSources,
  size_t NumMonoProcessors,
  size_t NumStereoProcessors>
class AudioAppletSubapp {
public:
  template <class... MIs, class... SIs, class... MPs, class... SPs>
  AudioAppletSubapp(
    tuple<MIs...> (&mono_source_pool)[2],
    tuple<SIs...>& stereo_source_pool,
    tuple<MPs...> (&mono_processor_pool)[2][Slots - 1],
    tuple<SPs...> (&stereo_processor_pool)[Slots - 1]
  )
    : mono_input_applets{
        util::tuple_to_ptr_arr<HemisphereAudioApplet>(mono_source_pool[0]),
        util::tuple_to_ptr_arr<HemisphereAudioApplet>(mono_source_pool[1]),
      }
    , stereo_input_applets(
        util::tuple_to_ptr_arr<HemisphereAudioApplet>(stereo_source_pool)
      ) {
    for (size_t slot = 0; slot < Slots - 1; slot++) {
      mono_processor_applets[0][slot]
        = util::tuple_to_ptr_arr<HemisphereAudioApplet>(
          mono_processor_pool[0][slot]
        );
      mono_processor_applets[1][slot]
        = util::tuple_to_ptr_arr<HemisphereAudioApplet>(
          mono_processor_pool[1][slot]
        );

      stereo_processor_applets[slot]
        = util::tuple_to_ptr_arr<HemisphereAudioApplet>(
          stereo_processor_pool[slot]
        );
    }
    selected_mono_applets[0].fill(0);
    selected_mono_applets[1].fill(0);
    selected_stereo_applets.fill(0);
  }

  void Init() {
    for (size_t slot = 0; slot < Slots; slot++) {
      if (IsStereo(slot)) {
        get_stereo_applet(slot).BaseStart(LEFT_HEMISPHERE);
        ForEachSide(side) ConnectStereoToNext(side, slot);
      } else {
        ForEachSide(side) {
          get_mono_applet(side, slot).BaseStart(side);
          ConnectMonoToNext(side, slot);
        }
      }
    }
    peak_conns[0][0].connect(OC::AudioIO::InputStream(), 0, peaks[0][0], 0);
    peak_conns[1][0].connect(OC::AudioIO::InputStream(), 1, peaks[1][0], 0);
  }

  void Controller() {
    AudioNoInterrupts();
    // Call Controller instead of BaseController so we don't trigger
    // cursor_countdown multiple times. This is a stupid hack and we should do
    // something smarter.
    for (size_t i = 0; i < Slots; i++) {
      if (IsStereo(i)) {
        get_stereo_applet(i).Controller();
      } else {
        get_mono_applet(LEFT_HEMISPHERE, i).Controller();
        get_mono_applet(RIGHT_HEMISPHERE, i).Controller();
      }
    }
    AudioInterrupts();
  }

  void mainloop() {
    for (size_t slot = 0; slot < Slots; slot++) {
      if (IsStereo(slot)) {
        get_stereo_applet(slot).mainloop();
      } else {
        get_mono_applet(LEFT_HEMISPHERE, slot).mainloop();
        get_mono_applet(RIGHT_HEMISPHERE, slot).mainloop();
      }
    }
  }

  void View() {
    for (size_t i = 0; i < Slots; i++) {
      print_applet_line(i);
    }

    ForEachSide(side) {
      HEM_SIDE s = static_cast<HEM_SIDE>(side);
      if (state[side] == EDIT_APPLET) {
        HemisphereApplet* applet = get_selected_applet(s);
        applet->SetDisplaySide(s);
        applet->BaseView();
      } else {
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
          draw_peak(s, slot);
        }

        gfxPos(1 + 64 * side, 2);
        if (side) graphics.printf("MEM%3d%%) R", mem_percent);
        else graphics.printf("L (CPU%3d%%", cpu_percent);
      }
    }

    // Recall that unsigned substraction rolls over correclty, so when millis()
    // rolls over, this will still work.
    if (millis() - last_stats_update > 250) {
      last_stats_update = millis();
      mem_percent = static_cast<int16_t>(
        100 * static_cast<float>(AudioMemoryUsageMax())
        / OC::AudioIO::AUDIO_MEMORY
      );
      cpu_percent = static_cast<int16_t>(AudioProcessorUsageMax());
      AudioProcessorUsageMaxReset();
      AudioMemoryUsageMaxReset();
    }
  }

  void HandleAuxButtonEvent(const UI::Event& event) {
    if (event.type == UI::EVENT_BUTTON_PRESS) {
      if (event.control == OC::CONTROL_BUTTON_X) {
        HemisphereApplet *applet = get_selected_applet(LEFT_HEMISPHERE);
        if (EDIT_APPLET == state[0] && applet->EditMode())
          applet->AuxButton();
        else
          state[0] = MOVE_CURSOR; // close applet view
      }
      if (event.control == OC::CONTROL_BUTTON_Y) {
        HemisphereApplet *applet = get_selected_applet(RIGHT_HEMISPHERE);
        if (EDIT_APPLET == state[1] && applet->EditMode())
          applet->AuxButton();
        else
          state[1] = MOVE_CURSOR; // close applet view
      }
    }
  }

  void HandleEncoderButtonEvent(const UI::Event& event) {
    if ((event.mask & OC::CONTROL_BUTTON_L)
        && (event.mask & OC::CONTROL_BUTTON_R)) {
      // check ready_for_press to suppress double events on button combos
      if (cursor[0] == cursor[1] && ready_for_press) {
        int c = cursor[0];
        stereo ^= 1 << c;
        if (IsStereo(c)) {
          get_stereo_applet(c).BaseStart(LEFT_HEMISPHERE);
          ForEachSide(side) {
            get_mono_applet(side, c).Unload();
            ConnectStereoToNext(side, c);
            if (c > 0) ConnectSlotToNext(side, c - 1);
          }
        } else {
          get_stereo_applet(c).Unload();
          ForEachSide(side) {
            get_mono_applet(side, c).BaseStart(side);
            ConnectMonoToNext(side, c);
            if (c > 0) ConnectSlotToNext(side, c - 1);
          }
        }
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
    switch (state[side]) {
      case MOVE_CURSOR:
        state[side] = SWITCH_APPLET;
        break;
      case SWITCH_APPLET:
        state[side] = EDIT_APPLET;
        break;
      case EDIT_APPLET:
        get_selected_applet(side)->OnButtonPress();
        break;
    }
  }

  void ChangeStereoApplet(HEM_SIDE side, size_t slot, int dir) {
    int& sel = selected_stereo_applets[slot];
    int n = slot == 0 ? NumStereoSources : NumStereoProcessors;
    get_stereo_applet(slot).Unload();
    sel = constrain(sel + dir, 0, n - 1);
    auto& app = get_stereo_applet(slot);
    app.BaseStart(side);
    ForEachSide(side) ConnectStereoToNext(side, slot);
    if (slot > 0) {
      ForEachSide(side) ConnectSlotToNext(side, slot - 1);
    }
  }

  void ChangeMonoApplet(HEM_SIDE side, size_t slot, int dir) {
    int& sel = selected_mono_applets[side][slot];
    int n = slot == 0 ? NumMonoSources : NumMonoProcessors;
    get_mono_applet(side, slot).Unload();
    sel = constrain(sel + dir, 0, n - 1);
    auto& app = get_mono_applet(side, slot);
    app.BaseStart(side);
    ConnectMonoToNext(side, slot);
    if (slot > 0) ConnectSlotToNext(side, slot - 1);
  }

  void ForwardEncoderMove(HEM_SIDE side, size_t slot, int dir) {
    auto& app
      = IsStereo(slot) ? get_stereo_applet(slot) : get_mono_applet(side, slot);
    app.OnEncoderMove(dir);
  }

  void HandleEncoderEvent(const UI::Event& event) {
    int dir = event.value;
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
        break;
      case SWITCH_APPLET:
        if (IsStereo(c)) ChangeStereoApplet(side, c, dir);
        else ChangeMonoApplet(side, c, dir);
        break;
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
    AudioStream* stream = get_mono_applet(side, slot).OutputStream();
    AudioConnection& conn = conns[side][slot];
    conn.disconnect();
    if (slot + 1 < Slots && !IsStereo(slot + 1)) {
      conn.connect(
        *stream, 0, *get_mono_applet(side, slot + 1).InputStream(), 0
      );
    } else {
      AudioStream* next_stream = slot + 1 < Slots
        ? get_stereo_applet(slot + 1).InputStream()
        : &OC::AudioIO::OutputStream();
      conn.connect(*stream, 0, *next_stream, side);
    }
    peak_conns[side][slot + 1].disconnect();
    peak_conns[side][slot + 1].connect(*stream, 0, peaks[side][slot + 1], 0);
  }

  void ConnectStereoToNext(HEM_SIDE side, size_t slot) {
    AudioStream* stream = get_stereo_applet(slot).OutputStream();
    AudioConnection& conn = conns[side][slot];
    conn.disconnect();
    if (slot + 1 < Slots && !IsStereo(slot + 1)) {
      AudioStream* next_stream = get_mono_applet(side, slot + 1).InputStream();
      conn.connect(*stream, side, *next_stream, 0);
    } else {
      AudioStream* next_stream = slot + 1 < Slots
        ? get_stereo_applet(slot + 1).InputStream()
        : &OC::AudioIO::OutputStream();
      conn.connect(*stream, side, *next_stream, side);
    }
    peak_conns[side][slot + 1].disconnect();
    peak_conns[side][slot + 1].connect(*stream, side, peaks[side][slot + 1], 0);
  }

protected:
  inline bool IsStereo(size_t slot) {
    return (stereo >> slot) & 1;
  }

private:
  array<array<HemisphereAudioApplet*, NumMonoSources>, 2> mono_input_applets;
  array<HemisphereAudioApplet*, NumStereoSources> stereo_input_applets;
  array<array<array<HemisphereAudioApplet*, NumMonoProcessors>, Slots - 1>, 2>
    mono_processor_applets;
  array<array<HemisphereAudioApplet*, NumStereoProcessors>, Slots - 1>
    stereo_processor_applets;
  uint32_t stereo = 0; // bitset

  array<array<int, Slots>, 2> selected_mono_applets;
  array<int, Slots> selected_stereo_applets;

  array<array<AudioConnection, Slots + 1>, 2> conns;
  array<array<AudioAnalyzePeak, Slots + 1>, 2> peaks;
  array<array<AudioConnection, Slots + 1>, 2> peak_conns;

  bool ready_for_press = false;
  size_t total, user, free;

  uint32_t last_update = 0;

  int16_t mem_percent = 0;
  int16_t cpu_percent = 0;
  uint32_t last_stats_update = 0;

  enum EditState {
    MOVE_CURSOR,
    SWITCH_APPLET,
    EDIT_APPLET,
  };

  EditState state[2];
  int cursor[2];

  int cursor_countdown;

  HemisphereAudioApplet& get_mono_applet(HEM_SIDE side, size_t slot) {
    const size_t ix = selected_mono_applets[side][slot];
    return slot == 0 ? *mono_input_applets[side][ix]
                     : *mono_processor_applets[side][slot - 1][ix];
  }

  HemisphereAudioApplet& get_stereo_applet(size_t slot) {
    const size_t ix = selected_stereo_applets[slot];
    return slot == 0 ? *stereo_input_applets[ix]
                     : *stereo_processor_applets[slot - 1][ix];
  }

  HemisphereAudioApplet* get_selected_applet(HEM_SIDE side) {
    int c = cursor[side];
    if (IsStereo(c)) {
      return &get_stereo_applet(c);
    } else {
      return &get_mono_applet(side, c);
    }
  }

  void print_applet_line(int slot) {
    int y = 15 + 10 * slot;
    if (IsStereo(slot)) {
      const char* name = get_stereo_applet(slot).applet_name();
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
          const char* name
            = get_mono_applet(static_cast<HEM_SIDE>(side), slot).applet_name();
          const int l = static_cast<int>(strlen(name));
          gfxPrint(8 + side * (110 - l * 6), y, name);
        }
      }
    }
  }

  int peak_width(HEM_SIDE side, int slot) {
    AudioAnalyzePeak& p = peaks[side][slot];
    if (p.available()) {
      float db = scalarToDb(p.read());
      if (db < -48.0f) db = -48.0f;
      return static_cast<int>((db + 48.0f) / 48.0f * 64);
    } else {
      return 0;
    }
  }
  void draw_peak(HEM_SIDE side, int slot, int y = -1) {
    int w = peak_width(side, slot);
    if (y < 0) y = slot * 10 + 13;
    gfxInvert(side ? 64 : 64 - w, y, w, 1);
  }
};
