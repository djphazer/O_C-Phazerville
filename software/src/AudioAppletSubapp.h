#pragma once

#include "AudioIO.h"
#include "HSApplication.h"
#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "OC_ui.h"
#include "smalloc.h"
#include "UI/ui_events.h"
#include "util/util_tuples.h"
#include <Audio.h>

using std::array, std::tuple;

template <class T, size_t N>
class Slot {};

template <
  size_t Slots,
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
    selected_mono_applets[1][0] = 1;
    selected_stereo_applets.fill(0);
  }

  void Init() {
    for (size_t slot = 0; slot < Slots; slot++) {
      if (IsStereo(slot)) {
        get_stereo_applet(slot).BaseStart(LEFT_HEMISPHERE);
        ConnectStereoToNext(slot);
      } else {
        get_mono_applet(LEFT_HEMISPHERE, slot).BaseStart(LEFT_HEMISPHERE);
        get_mono_applet(RIGHT_HEMISPHERE, slot).BaseStart(RIGHT_HEMISPHERE);
        ConnectMonoToNext(LEFT_HEMISPHERE, slot);
        ConnectMonoToNext(RIGHT_HEMISPHERE, slot);
      }
    }
  }

  void Controller() {
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
  }

  void View() {
    for (size_t i = 0; i < Slots; i++) {
      print_applet_line(i);
    }
    if (edit_state == EDIT_LEFT) {
      HemisphereAudioApplet& applet = IsStereo(left_cursor)
        ? get_stereo_applet(left_cursor)
        : get_mono_applet(LEFT_HEMISPHERE, left_cursor);
      applet.BaseView();
      gfxFrame(64, 0, 64, 64);
    } else {
      gfxPrint(65, 2, "R Channel");
      gfxDottedLine(64, 10, 126, 10);
    }
    if (edit_state == EDIT_RIGHT) {
      HemisphereAudioApplet& applet = IsStereo(right_cursor)
        ? get_stereo_applet(right_cursor)
        : get_mono_applet(RIGHT_HEMISPHERE, right_cursor);
      applet.BaseView();
      gfxFrame(0, 0, 64, 64);
    } else {
      gfxPrint(1, 2, "L Channel");
      gfxDottedLine(0, 10, 62, 10);
    }
  }

  void HandleEncoderButtonEvent(const UI::Event& event) {
    if ((event.mask & OC::CONTROL_BUTTON_L)
        && (event.mask & OC::CONTROL_BUTTON_R)) {
      // check ready_for_press to suppress double events on button combos
      if (left_cursor == right_cursor && ready_for_press) {
        stereo ^= 1 << left_cursor;
        if (IsStereo(left_cursor)) {
          get_mono_applet(LEFT_HEMISPHERE, left_cursor).Unload();
          get_mono_applet(RIGHT_HEMISPHERE, left_cursor).Unload();
          get_stereo_applet(left_cursor).BaseStart(LEFT_HEMISPHERE);
          ConnectStereoToNext(left_cursor);
          if (left_cursor > 0) ConnectSlotToNext(left_cursor - 1);
        } else {
          get_stereo_applet(left_cursor).Unload();
          get_mono_applet(LEFT_HEMISPHERE, left_cursor)
            .BaseStart(LEFT_HEMISPHERE);
          get_mono_applet(RIGHT_HEMISPHERE, left_cursor)
            .BaseStart(RIGHT_HEMISPHERE);
          ConnectMonoToNext(LEFT_HEMISPHERE, left_cursor);
          ConnectMonoToNext(RIGHT_HEMISPHERE, left_cursor);
          if (left_cursor > 0) ConnectSlotToNext(left_cursor - 1);
        }
      }
      // Prevent press detection when doing a button combo
      ready_for_press = false;
    } else if (event.type == UI::EVENT_BUTTON_PRESS && ready_for_press) {
      bool forwardPress
        = (event.control == OC::CONTROL_BUTTON_R && edit_state == EDIT_LEFT)
        || (event.control == OC::CONTROL_BUTTON_L && edit_state == EDIT_RIGHT);
      if (forwardPress) {
        get_selected_applet()->OnButtonPress();
      } else {
        parent_app->isEditing = !parent_app->isEditing;
        edit_state = parent_app->isEditing
          ? (event.control == OC::CONTROL_BUTTON_L ? EDIT_LEFT : EDIT_RIGHT)
          : EDIT_NONE;
        if (edit_state != EDIT_NONE) {
          get_selected_applet()->SetDisplaySide(
            event.control == OC::CONTROL_BUTTON_L ? RIGHT_HEMISPHERE
                                                  : LEFT_HEMISPHERE
          );
        }
      }
    } else if (event.type == UI::EVENT_BUTTON_DOWN) {
      ready_for_press = true;
    }
  }

  void ChangeStereoApplet(HEM_SIDE side, size_t slot, int dir) {
    int& sel = selected_stereo_applets[slot];
    int n = slot == 0 ? NumStereoSources : NumStereoProcessors;
    get_stereo_applet(slot).Unload();
    sel = constrain(sel + dir, 0, n - 1);
    auto& app = get_stereo_applet(slot);
    app.BaseStart(side);
    app.SetDisplaySide(
      side == LEFT_HEMISPHERE ? RIGHT_HEMISPHERE : LEFT_HEMISPHERE
    );
    ConnectStereoToNext(slot);
    if (slot > 0) ConnectSlotToNext(slot - 1);
  }

  void ChangeMonoApplet(HEM_SIDE side, size_t slot, int dir) {
    int& sel = selected_mono_applets[side][slot];
    int n = slot == 0 ? NumMonoSources : NumMonoProcessors;
    get_mono_applet(side, slot).Unload();
    sel = constrain(sel + dir, 0, n - 1);
    auto& app = get_mono_applet(side, slot);
    app.BaseStart(side);
    app.SetDisplaySide(
      side == LEFT_HEMISPHERE ? RIGHT_HEMISPHERE : LEFT_HEMISPHERE
    );
    ConnectMonoToNext(side, slot);
    if (slot > 0) ConnectSlotToNext(slot - 1);
  }

  void ForwardEncoderMove(HEM_SIDE side, size_t slot, int dir) {
    auto& app
      = IsStereo(slot) ? get_stereo_applet(slot) : get_mono_applet(side, slot);
    app.OnEncoderMove(dir);
  }

  void HandleEncoderEvent(const UI::Event& event) {
    int dir = event.value;
    if (event.control == OC::CONTROL_ENCODER_L) {
      switch (edit_state) {
        case EDIT_LEFT:
          if (IsStereo(left_cursor)) {
            ChangeStereoApplet(LEFT_HEMISPHERE, left_cursor, dir);
          } else {
            ChangeMonoApplet(LEFT_HEMISPHERE, left_cursor, dir);
          }
          break;
        case EDIT_RIGHT: {
          ForwardEncoderMove(RIGHT_HEMISPHERE, right_cursor, dir);
          break;
        }
        case EDIT_NONE:
          left_cursor = constrain(left_cursor + dir, 0, (int)Slots - 1);
          break;
      }
    } else if (event.control == OC::CONTROL_ENCODER_R) {
      switch (edit_state) {
        case EDIT_RIGHT:
          if (IsStereo(right_cursor)) {
            ChangeStereoApplet(RIGHT_HEMISPHERE, right_cursor, dir);
          } else {
            ChangeMonoApplet(RIGHT_HEMISPHERE, right_cursor, dir);
          }
          break;
        case EDIT_LEFT: {
          ForwardEncoderMove(LEFT_HEMISPHERE, left_cursor, dir);
          break;
        }
        case EDIT_NONE:
          right_cursor = constrain(right_cursor + dir, 0, (int)Slots - 1);
          break;
      }
    }
  }

  void SetParentApp(HSApplication* app) {
    parent_app = app;
  }

  void ConnectSlotToNext(size_t slot) {
    if (IsStereo(slot)) {
      ConnectStereoToNext(slot);
    } else {
      ConnectMonoToNext(LEFT_HEMISPHERE, slot);
      ConnectMonoToNext(RIGHT_HEMISPHERE, slot);
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
  }

  void ConnectStereoToNext(size_t slot) {
    AudioStream* stream = get_stereo_applet(slot).OutputStream();
    AudioConnection& lconn = conns[LEFT_HEMISPHERE][slot];
    AudioConnection& rconn = conns[RIGHT_HEMISPHERE][slot];
    lconn.disconnect();
    rconn.disconnect();
    if (slot + 1 < Slots && !IsStereo(slot + 1)) {
      AudioStream* lstream
        = get_mono_applet(LEFT_HEMISPHERE, slot + 1).InputStream();
      AudioStream* rstream
        = get_mono_applet(RIGHT_HEMISPHERE, slot + 1).InputStream();
      lconn.connect(*stream, 0, *lstream, 0);
      rconn.connect(*stream, 1, *rstream, 0);
    } else {
      AudioStream* next_stream = slot + 1 < Slots
        ? get_stereo_applet(slot + 1).InputStream()
        : &OC::AudioIO::OutputStream();
      lconn.connect(*stream, 0, *next_stream, 1);
      rconn.connect(*stream, 1, *next_stream, 1);
    }
  }

protected:
  inline bool IsStereo(size_t slot) {
    return (stereo >> slot) & 1;
  }

private:
  HSApplication* parent_app = nullptr;
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

  bool ready_for_press = false;
  size_t total, user, free;

  enum EditState {
    EDIT_NONE,
    EDIT_LEFT,
    EDIT_RIGHT,
  };

  EditState edit_state = EDIT_NONE;
  int left_cursor = 0;
  int right_cursor = 0;

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

  HemisphereAudioApplet* get_selected_applet() {
    if (edit_state == EDIT_NONE) return nullptr;
    int cursor = edit_state == EDIT_LEFT ? left_cursor : right_cursor;
    if (IsStereo(cursor)) {
      return &get_stereo_applet(cursor);
    } else {
      return &get_mono_applet(
        edit_state == EDIT_LEFT ? LEFT_HEMISPHERE : RIGHT_HEMISPHERE, cursor
      );
    }
  }

  void print_applet_line(int slot) {
    int y = 15 + 10 * slot;
    if ((stereo >> slot) & 1) {
      int xs[] = {32, 0, 64};
      int x = xs[edit_state];
      gfxPrint(x, y, get_stereo_applet(slot).applet_name());
      if (left_cursor == slot || right_cursor == slot) {
        parent_app->gfxCursor(x, y + 8, 63);
      }
    } else {
      if (edit_state != EDIT_RIGHT) {
        gfxPrint(0, y, get_mono_applet(LEFT_HEMISPHERE, slot).applet_name());
        if (left_cursor == slot) parent_app->gfxCursor(0, y + 8, 63);
      }
      if (edit_state != EDIT_LEFT) {
        gfxPrint(64, y, get_mono_applet(RIGHT_HEMISPHERE, slot).applet_name());
        if (right_cursor == slot) parent_app->gfxCursor(64, y + 8, 63);
      }
    }
  }
};
