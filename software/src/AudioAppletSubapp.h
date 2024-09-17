#pragma once

#include "HSUtils.h"
#include "HemisphereAudioApplet.h"
#include "OC_ui.h"
#include "util/util_tuples.h"

using std::array, std::tuple;
// using std::tuple;

template <size_t Slots, size_t NumMonoSources, size_t NumStereoSources,
          size_t NumMonoProcessors, size_t NumStereoProcessors>
class AudioAppletSubapp {
public:
  template <class... MIs, class... SIs, class... MPs, class... SPs>
  AudioAppletSubapp(tuple<MIs...> (&mono_source_pool)[2],
                    tuple<SIs...> &stereo_source_pool,
                    tuple<MPs...> (&mono_processor_pool)[Slots - 1][2],
                    tuple<SPs...> (&stereo_processor_pool)[Slots - 1])
      : mono_input_applets{util::tuple_to_ptr_arr<HemisphereAudioApplet>(
                               mono_source_pool[0]),
                           util::tuple_to_ptr_arr<HemisphereAudioApplet>(
                               mono_source_pool[1])},
        stereo_input_applets(
            util::tuple_to_ptr_arr<HemisphereAudioApplet>(stereo_source_pool)) {
    for (size_t slot = 0; slot < Slots - 1; slot++) {
      mono_processor_applets[slot] = {
          util::tuple_to_ptr_arr<HemisphereAudioApplet>(
              mono_processor_pool[slot][0]),
          util::tuple_to_ptr_arr<HemisphereAudioApplet>(
              mono_processor_pool[slot][1])};

      stereo_processor_applets[slot] =
          util::tuple_to_ptr_arr<HemisphereAudioApplet>(
              stereo_processor_pool[slot]);
    }
  }

  void View() {
    gfxPrint("Audio Pipeline");
    gfxDottedLine(0, 10, 123, 10);
    for (size_t i = 0; i < Slots; i++) {
      print_applet_line(i);
    }
    if (edit_state == EDIT_LEFT) {
      HemisphereApplet *applet = IsStereo(left_cursor)
                                     ? get_stereo_applet(left_cursor)
                                     : get_mono_applet(0, left_cursor);
      gfxPrint(64, 15, applet->applet_name());
      gfxPrint(64, 25, "appears");
      gfxPrint(64, 35, "here");
    } else if (edit_state == EDIT_RIGHT) {
      HemisphereApplet *applet = IsStereo(right_cursor)
                                     ? get_stereo_applet(right_cursor)
                                     : get_mono_applet(1, right_cursor);
      gfxPrint(0, 15, applet->applet_name());
      gfxPrint(0, 25, "appears");
      gfxPrint(0, 35, "here");
    }
  }

  void HandleButtonEvent(const UI::Event &event) {
    parent_app->isEditing = !parent_app->isEditing;
    edit_state =
        parent_app->isEditing
            ? (event.control == OC::CONTROL_BUTTON_L ? EDIT_LEFT : EDIT_RIGHT)
            : EDIT_NONE;
  }

  void HandleEncoderEvent(const UI::Event &event) {
    int dir = event.value;
    if (event.control == OC::CONTROL_ENCODER_L) {
      switch (edit_state) {
      case EDIT_LEFT:
        if (IsStereo(left_cursor)) {
          int &sel = selected_stereo_applets[left_cursor];
          int n = left_cursor == 0 ? NumStereoSources : NumStereoProcessors;
          sel = constrain(sel + dir, 0, n - 1);
        } else {
          int &sel = selected_mono_applets[left_cursor][0];
          int n = left_cursor == 0 ? NumMonoSources : NumMonoProcessors;
          sel = constrain(sel + dir, 0, n - 1);
        }
        break;
      case EDIT_RIGHT:
        break;
      case EDIT_NONE:
        left_cursor = constrain(left_cursor + dir, 0, (int)Slots - 1);
        break;
      }
    } else if (event.control == OC::CONTROL_ENCODER_R) {
      switch (edit_state) {
      case EDIT_RIGHT:
        if (IsStereo(right_cursor)) {
          int &sel = selected_stereo_applets[right_cursor];
          int n = right_cursor == 0 ? NumStereoSources : NumStereoProcessors;
          sel = constrain(sel + dir, 0, n - 1);
        } else {
          int &sel = selected_mono_applets[right_cursor][1];
          int n = right_cursor == 0 ? NumMonoSources : NumMonoProcessors;
          sel = constrain(sel + dir, 0, n - 1);
        }
        break;
      case EDIT_LEFT:
        break;
      case EDIT_NONE:
        right_cursor = constrain(right_cursor + dir, 0, (int)Slots - 1);
        break;
      }
    }
  }

  void SetParentApp(HSApplication *app) { parent_app = app; }

protected:
  inline bool IsStereo(size_t slot) { return (stereo >> slot) & 1; }

private:
  HSApplication *parent_app;
  array<array<HemisphereAudioApplet *, NumMonoSources>, 2> mono_input_applets;
  array<HemisphereAudioApplet *, NumStereoSources> stereo_input_applets;
  array<array<array<HemisphereAudioApplet *, NumMonoProcessors>, 2>, Slots - 1>
      mono_processor_applets;
  array<array<HemisphereAudioApplet *, NumStereoProcessors>, Slots - 1>
      stereo_processor_applets;
  uint32_t stereo; // bitset

  array<array<int, 2>, Slots> selected_mono_applets;
  array<int, Slots> selected_stereo_applets;

  enum EditState {
    EDIT_NONE,
    EDIT_LEFT,
    EDIT_RIGHT,
  };

  EditState edit_state = EDIT_NONE;
  int left_cursor = 0;
  int right_cursor = 0;

  HemisphereApplet *get_mono_applet(size_t side, size_t slot) {
    const size_t ix = selected_mono_applets[slot][side];
    return slot == 0 ? mono_input_applets[side][ix]
                     : mono_processor_applets[slot - 1][side][ix];
  }

  HemisphereApplet *get_stereo_applet(size_t slot) {
    const size_t ix = selected_stereo_applets[slot];
    return slot == 0 ? stereo_input_applets[ix]
                     : stereo_processor_applets[slot - 1][ix];
  }

  void print_applet_line(int slot) {
    int y = 13 + 10 * slot;
    if ((stereo >> slot) & 1) {
      int xs[] = {32, 0, 64};

      // int x = edit_state == EDIT_LEFT ? 0 : edit_state == EDIT_RIGHT ? 64 :
      // 32;
      int x = xs[edit_state];
      gfxPrint(x, y, get_stereo_applet(slot)->applet_name());
      if (left_cursor == slot || right_cursor == slot) {
        parent_app->gfxCursor(x, y + 8, 63);
      }
    } else {
      if (edit_state != EDIT_RIGHT) {
        gfxPrint(0, y, get_mono_applet(0, slot)->applet_name());
        if (left_cursor == slot)
          parent_app->gfxCursor(0, y + 8, 63);
      }
      if (edit_state != EDIT_LEFT) {
        gfxPrint(64, y, get_mono_applet(1, slot)->applet_name());
        if (right_cursor == slot)
          parent_app->gfxCursor(64, y + 8, 63);
      }
    }
  }
};
