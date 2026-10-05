// Copyright (c) 2026, Benjamin Mulkey

//

// Permission is hereby granted, free of charge, to any person obtaining a copy

// of this software and associated documentation files (the "Software"), to deal

// in the Software without restriction, including without limitation the rights

// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell

// copies of the Software, and to permit persons to whom the Software is

// furnished to do so, subject to the following conditions:

//

// The above copyright notice and this permission notice shall be included in

// all copies or substantial portions of the Software.

//

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR

// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,

// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE

// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER

// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,

// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE

// SOFTWARE.


static const uint16_t FROG_BITMAP[9] = {
  0x4F2,
  0xD6B,
  0x5FA,
  0x3FC,
  0x1F8,
  0x3FC,
  0x5FA,
  0xDFB,
  0x4F2
};

class FrogSeq : public HemisphereApplet {

public:

  static constexpr int FROGSEQ_STEPS = 16;

  const char * applet_name() {
    return "FrogSeq";
  }

  const uint8_t* applet_icon() {
    return INVERT_CAR_ICON;
  }

  enum Page {
    MAIN_PAGE,
    NOTE_SEQ_PAGE
  };

  enum MainCursor {
    FROG_SELECT,
    SEMITONE_SELECT,
    RANDOM_SELECT,
    MAIN_CURSOR_LAST = RANDOM_SELECT
  };

  enum NoteSeqCursor {
    NOTE_STEP_FIRST = 0,
    NOTE_STEP_LAST = FROGSEQ_STEPS - 1,

    NOTE_STEPS = FROGSEQ_STEPS,

    LANE1_DIRECTION,
    LANE1_SPEED,
    LANE1_FLOW,

    LANE2_DIRECTION,
    LANE2_SPEED,
    LANE2_FLOW,

    LANE3_DIRECTION,
    LANE3_SPEED,
    LANE3_FLOW,

    NOTE_SEQ_CURSOR_LAST = LANE3_FLOW
  };

private:


  Page page = MAIN_PAGE;

  int cursor = FROG_SELECT;
  bool q_select = false;
  int qselect = 0;

  int8_t frog_x = 26;
  int8_t frog_y = 14;

  int frog_x_reference = 26;
  int frog_y_position = 0;

  bool frog_horizontal = false;

  // Frog positions: Safe Zone, Lane 1, Lane 2, Lane 3.
  static constexpr int FROG_Y[4] = {13, 27, 40, 53};
  int frog_lane = 0;
  int frog_y_reference = 0;


  static constexpr int TRAFFIC_LANES = 3;
  int sequence_length = 16;

  static constexpr int SAVED_SEQUENCES = 8;
  static constexpr int FROGSEQ_DATA_START = 0;
  static constexpr int FROGSEQ_DATA_SLOTS = SAVED_SEQUENCES * 2;

  static constexpr int FROGSEQ_RESTORE_SLOT_1 =
    FROGSEQ_DATA_START + FROGSEQ_DATA_SLOTS;
  static constexpr int FROGSEQ_RESTORE_SLOT_2 =
    FROGSEQ_RESTORE_SLOT_1 + 1;


  uint8_t current_sequence = 0;
  bool sequence_menu = false;

  enum SequenceMenuMode {
    SEQUENCE_LOAD = 0,
    SEQUENCE_MOVE,
    SEQUENCE_RESET
  };

  uint8_t sequence_menu_mode = SEQUENCE_LOAD;
  bool sequence_reset_confirm = false;
  bool sequence_reset_yes = false;
  bool sequence_slot_mode = false;
  uint8_t selected_sequence = 0;

  static constexpr int TRAFFIC_OBJECTS = 3;
  static constexpr int TRAFFIC_WIDTH = 10;
  static constexpr int TRAFFIC_MOVE_PIXELS = 4;
  static constexpr int TRAFFIC_MIN_SPAWN_GAP = 8;
  static constexpr int TRAFFIC_LEFT_BOUNDARY = -TRAFFIC_WIDTH;
  static constexpr int TRAFFIC_RIGHT_BOUNDARY = 64;

  enum TrafficRate {
    TRAFFIC_DIV_2,
    TRAFFIC_X1,
    TRAFFIC_X2,
    TRAFFIC_X3,
    TRAFFIC_X4
  };
  enum TrafficModifier {
    MODIFIER_NOTE,
    MODIFIER_GATE,
    MODIFIER_RATCHET
  };



  struct TrafficObject {
    int x;
    bool active;
    TrafficModifier modifier;
  };

  TrafficObject traffic[TRAFFIC_LANES][TRAFFIC_OBJECTS];

  TrafficModifier RandomTrafficModifier(int lane);

  bool lane_reverse[TRAFFIC_LANES] = {
    false,
    true,
    false
  };

  TrafficRate lane_rate[TRAFFIC_LANES] = {
    TRAFFIC_X4,
    TRAFFIC_X1,
    TRAFFIC_DIV_2
  };

  int lane_flow[TRAFFIC_LANES] = {
    3,
    3,
    3
  };

  uint32_t traffic_last_clock_tick = 0;
  uint32_t traffic_last_move_tick[TRAFFIC_LANES] = {0, 0, 0};
  uint32_t traffic_clock_ticks = 1;
  bool traffic_clock_valid = false;

  int8_t sequence_notes[FROGSEQ_STEPS];
  bool sequence_mutes[FROGSEQ_STEPS];
  int step = 0;
  bool reset = true;

  int GetFrogNote(int step) {
    return sequence_notes[step];
  }

  void SetFrogNote(int note, int step) {
    sequence_notes[step] = constrain(note, -24, 35);
  }

  bool muted(int step) {
    return sequence_mutes[step];
  }

  void SetMute(int step, bool on) {
    sequence_mutes[step] = on;
  }

  void ToggleMute(int step) {
    sequence_mutes[step] = !sequence_mutes[step];
  }

  void PlayCurrentNote() {
    int play_cv = MIDIQuantizer::CV(current_note + 36);
    play_cv = HS::GetQuantEngine(qselect).Process(play_cv, 0, 0);
    Out(0, play_cv);
  }


  int current_note = 0;

  uint32_t collision_display_until = 0;
  uint32_t collision_blink_until = 0;
  uint32_t modifier_display_tick = 0;

  static constexpr uint32_t COLLISION_DISPLAY_TICKS =
    150 * HEMISPHERE_CLOCK_TICKS;
  static constexpr uint32_t COLLISION_BLINK_TICKS =
    50 * HEMISPHERE_CLOCK_TICKS;
  int modifier_step = -1;
  int modifier_value = 0;
  bool modifier_gate = false;
  const uint8_t *modifier_icon = nullptr;
  bool collision_ratchet_armed = false;
  int collision_ratchet_note = 0;  // Ratchet captures the queued step's note, even if muted.
  uint8_t collision_ratchets_to_go = 0;       // Actual Ratchets remaining
  uint8_t collision_ratchets_display = 0;
  uint8_t collision_ratchet_count = 0;
  uint32_t collision_ratchet_next_tick = 0;

  bool collision_ratchet_zap = false;
  uint32_t collision_ratchet_spacing = 0;


  int SafeZoneModifierX() {
    return 1;
  }

  int SafeZoneModifierY() {
    return FROG_Y[0];
  }

  FLASHMEM void DrawFrog() {

    const uint32_t now = OC::CORE::ticks;

    if (now < collision_display_until &&
        now >= collision_blink_until) {
      gfxIcon(frog_x + 2, frog_y + 1, BURST_ICON);
      return;
    }

    for (int y = 0; y < 9; ++y) {
      for (int x = 0; x < 12; ++x) {

        if (FROG_BITMAP[y] & (1 << (11 - x)))
          gfxPixel(frog_x + x, frog_y + y);

      }
    }
  }

  bool FrogCollides(int lane, int car_x) {

    if (frog_lane != lane + 1)
      return false;

    const int frog_left = frog_x + 2;
    const int frog_right = frog_x + 10;
    // Full truck body is the collision zone.
    const int truck_left = car_x;
    const int truck_right = car_x + 10;

    return frog_left < truck_right && frog_right > truck_left;
  }

  void ApplyCollisionModifier(
    int lane,
    TrafficModifier modifier,
    bool clocked
  ) {
    modifier_step = clocked ? step : step + 1;
    if (modifier_step >= sequence_length)
      modifier_step = 0;

    const bool can_ratchet =
      lane_rate[lane] == TRAFFIC_DIV_2 ||
      lane_rate[lane] == TRAFFIC_X2 ||
      lane_rate[lane] == TRAFFIC_X3 ||
      lane_rate[lane] == TRAFFIC_X4;

    // The car already chose its modifier when it spawned.
    // ×1: Note or Gate.
    // ÷2, ×2, ×3 and ×4: Ratchet, Note or Gate.

    if (modifier == MODIFIER_RATCHET && can_ratchet) {
      modifier_icon = nullptr;

      // Replace queued Ratchets, but never interrupt one firing.
      if (!collision_ratchet_armed || collision_ratchet_count == 0) {
        collision_ratchet_armed = true;
        collision_ratchet_count = 0;
        collision_ratchet_next_tick = 0;

        // On the master clock, use the current step.
        // Between clocks, queue the upcoming step.
        // This can "see through" a mute: normal sequence playback still
        // leaves current_note unchanged on a muted step.
        collision_ratchet_note = GetFrogNote(modifier_step);

        const uint32_t ratchet_clock_ticks = max(1u, traffic_clock_ticks);

        switch (lane_rate[lane]) {
          case TRAFFIC_DIV_2:
            // /2 ratchet plays the captured note on two consecutive master clocks.
            collision_ratchets_to_go = 2;
            collision_ratchets_display = 2;
            collision_ratchet_spacing =
              max(1u, traffic_clock_ticks);
            break;

          case TRAFFIC_X2:
            collision_ratchets_to_go = 2;
            collision_ratchets_display = 2;
            collision_ratchet_spacing =
              max(1u, ratchet_clock_ticks / 2);
            break;

          case TRAFFIC_X3:
            collision_ratchets_to_go = 3;
            collision_ratchets_display = 3;
            collision_ratchet_spacing =
              max(1u, ratchet_clock_ticks / 3);
            break;

          case TRAFFIC_X4:
            collision_ratchets_to_go = 4;
            collision_ratchets_display = 4;
            collision_ratchet_spacing =
              max(1u, ratchet_clock_ticks / 4);
            break;

          default:
            collision_ratchet_armed = false;
            break;
        }
      }

      modifier_gate = false;
    }
    else if (modifier == MODIFIER_NOTE) {
      collision_ratchets_display = 0;
      collision_ratchet_zap = false;
      modifier_gate = false;
      modifier_icon = NOTE_ICON;
      modifier_value = random(1, 13);
      if (random(2))
        modifier_value = -modifier_value;
      SetFrogNote(
        GetFrogNote(modifier_step) + modifier_value,
        modifier_step
      );

      if (muted(modifier_step))
        ToggleMute(modifier_step);
    }
    else {
      collision_ratchets_display = 0;
      collision_ratchet_zap = false;
      ToggleMute(modifier_step);
      modifier_gate = true;
      modifier_icon = GATE_ICON;
      modifier_value = muted(modifier_step) ? 0 : 1;
    }

    modifier_display_tick = OC::CORE::ticks;
  }

  void CheckTrafficCollisions(bool clocked) {

    if (frog_lane == 0)
      return;

    for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {
      for (int i = 0; i < TRAFFIC_OBJECTS; ++i) {

        if (!traffic[lane][i].active)
          continue;

        if (FrogCollides(lane, traffic[lane][i].x)) {
          traffic[lane][i].active = false;

          const uint32_t now = OC::CORE::ticks;
          const bool collision_display_active =
            now < collision_display_until;

          // Restart the visual collision response.
          // The first collision shows immediately. A tightly packed
          // collision briefly blanks the Ratchet, then brings it back.
          if (collision_display_active)
            collision_blink_until = now + COLLISION_BLINK_TICKS;
          else
            collision_blink_until = now;

          collision_display_until = now + COLLISION_DISPLAY_TICKS;

          // The car already chose its modifier when it spawned.
          ApplyCollisionModifier(
            lane,
            traffic[lane][i].modifier,
            clocked
          );
          return;
        }
      }
    }
  }

  void MoveTraffic(bool clocked) {
    const uint32_t now = OC::CORE::ticks;
    if (clocked) {
      traffic_clock_ticks = max(1u, ClockCycleTicks(0));
      traffic_clock_valid = true;
      traffic_last_clock_tick = now;
    }

    if (!traffic_clock_valid)
      return;

    // Stop traffic when the external clock stops.
    if (!clocked &&
        now - traffic_last_clock_tick >= traffic_clock_ticks) {
      traffic_clock_valid = false;
      return;
    }

    for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {
      uint32_t move_ticks = traffic_clock_ticks;

      switch (lane_rate[lane]) {
        case TRAFFIC_X4:
          move_ticks = max(1u, traffic_clock_ticks / 4);
          break;
        case TRAFFIC_X3:
          move_ticks = max(1u, traffic_clock_ticks / 3);
          break;
        case TRAFFIC_X2:
          move_ticks = max(1u, traffic_clock_ticks / 2);
          break;
        case TRAFFIC_X1:
          break;
        case TRAFFIC_DIV_2:
          move_ticks = traffic_clock_ticks * 2;
          break;
      }

      if (traffic_last_move_tick[lane] == 0)
        traffic_last_move_tick[lane] = now - move_ticks;

      if (now - traffic_last_move_tick[lane] < move_ticks)
        continue;

      traffic_last_move_tick[lane] = now;

      for (int i = 0; i < TRAFFIC_OBJECTS; ++i) {
        if (!traffic[lane][i].active)
          continue;

        if (lane_reverse[lane]) {
          traffic[lane][i].x -= TRAFFIC_MOVE_PIXELS;
          if (traffic[lane][i].x + TRAFFIC_WIDTH <= 0)
            traffic[lane][i].active = false;
        }
        else {
          traffic[lane][i].x += TRAFFIC_MOVE_PIXELS;
          if (traffic[lane][i].x >= TRAFFIC_RIGHT_BOUNDARY)
            traffic[lane][i].active = false;
        }
      }

      const int spawn_x = lane_reverse[lane]
        ? TRAFFIC_RIGHT_BOUNDARY
        : TRAFFIC_LEFT_BOUNDARY;

      bool spawn_clear = true;
      for (int i = 0; i < TRAFFIC_OBJECTS; ++i) {
        if (!traffic[lane][i].active)
          continue;

        const int car_left = traffic[lane][i].x;
        const int car_right = traffic[lane][i].x + TRAFFIC_WIDTH;

        if (lane_reverse[lane]) {
          if (car_right > TRAFFIC_RIGHT_BOUNDARY - TRAFFIC_MIN_SPAWN_GAP) {
            spawn_clear = false;
            break;
          }
        }
        else {
          if (car_left < TRAFFIC_MIN_SPAWN_GAP) {
            spawn_clear = false;
            break;
          }
        }
      }
      if (spawn_clear &&
          random(100) < (uint32_t)FLOW_CHANCE[lane_flow[lane]]) {

        for (int i = 0; i < TRAFFIC_OBJECTS; ++i) {
          if (!traffic[lane][i].active) {
            traffic[lane][i].active = true;
            traffic[lane][i].x = spawn_x;

            traffic[lane][i].modifier =
              RandomTrafficModifier(lane);

            break;
          }
        }
      }
    }

    CheckTrafficCollisions(clocked);
  }

  static constexpr int FLOW_CHANCE[7] = {
    0,
    2,
    5,
    12,
    25,
    50,
    100
  };

  void ResetTraffic() {
    for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {

      for (int i = 0; i < TRAFFIC_OBJECTS; ++i) {
        traffic[lane][i].active = (i == 0);
        traffic[lane][i].x =
          lane_reverse[lane] ? 64 : -TRAFFIC_WIDTH;

        traffic[lane][i].modifier =
          RandomTrafficModifier(lane);
      }
    }
  }

  FLASHMEM void DrawTrafficIcon(int x, int y, const uint8_t *icon) {
    for (int col = 0; col < 8; ++col) {
      for (int row = 0; row < 8; ++row) {
        const int px = x + col;

        if (px < 0 || px >= 64)
          continue;

        if (icon[col] & (1 << row))
          gfxPixel(px, y + row);
      }
    }
  }

  FLASHMEM void DrawTrafficObject(int lane, int x, int object) {

    const int y = FROG_Y[lane + 1] + 1;

    if (traffic[lane][object].modifier == MODIFIER_RATCHET) {
      const int draw_x = max(x, 0);
      const int draw_right = min(x + TRAFFIC_WIDTH, 64);
      const int draw_width = draw_right - draw_x;

      if (draw_width > 0)
        gfxFrame(draw_x, y, draw_width, 5);

      const int wheel1_x = x + 2;
      const int wheel2_x = x + 8;

      if (wheel1_x >= 0 && wheel1_x < 64)
        gfxPixel(wheel1_x, y + 5);

      if (wheel2_x >= 0 && wheel2_x < 64)
        gfxPixel(wheel2_x, y + 5);

      return;
    }

    const uint8_t *icon =
      traffic[lane][object].modifier == MODIFIER_GATE
        ? (lane_reverse[lane] ? INVERT_CAR_ICON : INVERT_CAR_RIGHT_ICON)
        : (lane_reverse[lane] ? CAR_ICON : CAR_RIGHT_ICON);

    DrawTrafficIcon(x, y, icon);
  }

  FLASHMEM void DrawStepCounter() {

    const int x0 = 1;
    const int y = 25;
    const int gap = 4;

    for (int i = 0; i < FROGSEQ_STEPS; ++i) {

      if (i == step) {
        gfxRect(x0 + i * gap - 1, y - 2, 3, 5);
      }
      else if (!muted(i)) {
        gfxPixel(x0 + i * gap, y);
      }

    }
  }
  FLASHMEM void DrawCurrentNote() {
    if (q_select) {
      char q_label[] = "Q1";
      q_label[1] = '1' + qselect;
      gfxPrint(42, 15, q_label);
      return;
    }

    const int semitone = (current_note % 12 + 12) % 12;
    const int notenum = current_note + 36;

    const int octave = (notenum / 12) - 3;

    gfxBitmap(42, 13, 8, NOTE_NAMES + semitone * 8);

    if (octave == -2)
      gfxBitmap(51, 16, 3, SUB_TWO);   // C1-B1
    else if (octave == -1)
      gfxBitmap(51, 19, 3, SUP_ONE);   // C2-B2
    else if (octave == 1)
      gfxBitmap(51, 11, 3, SUP_ONE);   // C4-B4
    else if (octave == 2)
      gfxBitmap(51, 8, 3, SUB_TWO);    // C5-B5
  }

  FLASHMEM void DrawMainCursor() {

    if (cursor == FROG_SELECT) {

      if (!EditMode() && CursorBlink()) {
        gfxLine(frog_x, frog_y + 10, frog_x + 11, frog_y + 10);
        gfxPixel(frog_x, frog_y + 8);
        gfxPixel(frog_x + 11, frog_y + 8);
      }

    }
    else if (cursor == SEMITONE_SELECT) {

      if (q_select) {
        gfxSpicyCursor(42, 23, 12);
        SetLabel("Q-engine");
        SetAux(true);
      } else {
        gfxCursor(42, 23, 12);
      }

    }
    else if (cursor == RANDOM_SELECT) {

      gfxCursor(54, 23, 10);

    }

  }

  FLASHMEM void DrawMainPage() {
    if (q_select) {
      SetLabel("Q-engine");
    } else {
      SetLabel("");
    }

    if (cursor == FROG_SELECT && EditMode()) {
      if (frog_horizontal) {
        gfxIcon(25, 1, LEFT_ICON);
        gfxIcon(35, 1, RIGHT_ICON);
      } else {
        gfxIcon(25, 1, UP_ICON);
        gfxIcon(35, 1, DOWN_ICON);
      }
    }

    SetAux(
      cursor == FROG_SELECT ||
      q_select ||
      (page == NOTE_SEQ_PAGE &&
       cursor >= 0 &&
       cursor < FROGSEQ_STEPS)
    );

    DrawFrog();

    // Runtime Ratchets display in the Safe Zone.
    // Queued: squares for all Ratchets.
    // Running: ZAP replaces the current hit; squares show hits remaining after it.
    if (modifier_icon == nullptr &&
        (collision_ratchets_display > 0 || collision_ratchet_zap)) {
      const int x = SafeZoneModifierX();
      const int y = SafeZoneModifierY();

      const int display_ratchets =
        constrain(collision_ratchets_display, 0, 4);

      if (collision_ratchet_zap) {
        gfxIcon(
          max(0, x + 1 + (display_ratchets * 5) - 2),
          y,
          ZAP_ICON
        );
      }

      for (int i = 0; i < display_ratchets; i++)
        gfxFrame(x + 1 + (i * 5), y + 3, 3, 3);
    }
    else if (modifier_icon &&
             OC::CORE::ticks - modifier_display_tick <
          HEMISPHERE_CURSOR_TICKS * 6 &&
        modifier_step >= 0) {

      const int x = SafeZoneModifierX();
      const int y = SafeZoneModifierY();

      const int step_number = modifier_step + 1;
      gfxBitmap(x, y, 8, TEENS_8X8 + step_number * 8);

      gfxIcon(x + 9, y, modifier_icon);

      if (modifier_gate) {
        gfxIcon(x + 18, y, modifier_value ? CHECK_ON_ICON : CHECK_OFF_ICON);
      }
      else {
        // Negative values are inverted.
        const int amount = constrain(abs(modifier_value), 0, 19);
        const int value_x = x + 18;

        gfxBitmap(
          value_x,
          y,
          8,
          TEENS_8X8 + amount * 8
        );

        if (modifier_value < 0)
          gfxInvert(value_x, y, 8, 8);
      }
    }

    for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {
      for (int i = 0; i < TRAFFIC_OBJECTS; ++i) {
        if (traffic[lane][i].active)
          DrawTrafficObject(lane, traffic[lane][i].x, i);
      }
    }


    DrawCurrentNote();
    gfxIcon(56, 13, RANDOM_ICON);

DrawStepCounter();

    DrawMainCursor();



    for (int x = 0; x < 64; x += 8)
      gfxLine(x, 38, x + 3, 38);

    for (int x = 0; x < 64; x += 8)
      gfxLine(x, 51, x + 3, 51);

  }

  FLASHMEM void DrawNoteSequencerStep(int step_index) {
    const int col = step_index & 7;
    const int row = step_index >> 3;

    const int x = 1 + col * 8;
    const int y = 16 + row * 12;

    const int note = GetFrogNote(step_index);
    const int height = constrain((note + 32) / 8, 1, 6);

    if (!muted(step_index))
      gfxFrame(x, y + 6 - height, 6, height);

    if (step == step_index)
      gfxIcon(x + 1, y - 8, DOWN_BTN_ICON);

    if (cursor == step_index) {
      gfxFrame(x - 1, y - 1, 8, 8);
      if (EditMode())
        gfxInvert(x - 1, y - 1, 8, 8);
    }
  }

  FLASHMEM void DrawLaneCursor(int lane, int y) {

    const int direction_cursor = LANE1_DIRECTION + lane * 3;
    const int speed_cursor = LANE1_SPEED + lane * 3;
    const int flow_cursor = LANE1_FLOW + lane * 3;

    if (cursor == direction_cursor) {
      SetLabel(lane_reverse[lane] ? "Left" : "Right");
      gfxLine(12, y + 8, 20, y + 8);

      if (EditMode())
        gfxInvert(12, y, 10, 8);
    }
    else if (cursor == speed_cursor) {
      SetLabel("Speed");
      gfxLine(24, y + 8, 43, y + 8);

      if (EditMode())
        gfxInvert(32, y, 15, 8);
    }
    else if (cursor == flow_cursor) {
      SetLabel("Flow");
      gfxLine(48, y + 8, 63, y + 8);

      if (EditMode())
        gfxInvert(57, y, 7, 8);
    }
  }

  FLASHMEM void DrawSequenceMenu() {

    const int px = 5;
    const int py = 13;
    const int pw = 54;
    const int ph = 38;

    gfxRect(px, py, pw, ph);
    gfxFrame(px, py, pw, ph);

    gfxPrint(px + 5, py + 5, "Load");
    gfxPrint(px + 5, py + 15, "Move");
    gfxPrint(px + 5, py + 25, "Reset");

    if (sequence_menu_mode == SEQUENCE_LOAD)
      gfxIcon(px + 38, py + 5, LEFT_ICON);
    else if (sequence_menu_mode == SEQUENCE_MOVE)
      gfxIcon(px + 38, py + 15, LEFT_ICON);
    else
      gfxIcon(px + 38, py + 25, LEFT_ICON);
  }

  FLASHMEM void DrawNoteSequencerPage() {
  SetAux(cursor >= 0 && (cursor <= sequence_length || sequence_menu || sequence_slot_mode || sequence_reset_confirm));

  for (int s = 0; s < sequence_length; ++s)
    DrawNoteSequencerStep(s);

  static const char *speed_labels[] = {
    "/2", "x1", "x2", "x3", "x4"
  };

  static const int lane_y[TRAFFIC_LANES] = {
    36, 45, 54
  };

  for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {
    const int y = lane_y[lane];

    char lane_label[] = { 'L', char('1' + lane), '\0' };
    gfxPrint(0, y + 1, lane_label);

    gfxIcon(13, y,
            lane_reverse[lane] ? ROTATE_L_ICON : ROTATE_R_ICON);

    gfxIcon(24, y, GAUGE_ICON);
    gfxPrint(33, y + 1, speed_labels[lane_rate[lane]]);

    gfxIcon(48, y, MOD_ICON);
    gfxPrint(58, y + 1, lane_flow[lane]);
  }


  for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {
    DrawLaneCursor(lane, lane_y[lane]);
  }

  if (sequence_menu) {

    if (sequence_menu_mode == SEQUENCE_LOAD)
      SetLabel("Load");
    else if (sequence_menu_mode == SEQUENCE_MOVE)
      SetLabel("Move");
    else
      SetLabel("Reset");

  }
  else if (sequence_slot_mode) {
    static char label[12];

    if (sequence_menu_mode == SEQUENCE_LOAD)
      snprintf(label, sizeof(label),
               "Load(%d)", selected_sequence + 1);
    else
      snprintf(label, sizeof(label),
               "Move(%d)", selected_sequence + 1);

    SetLabel(label);
  }
  else if (sequence_reset_confirm) {
    SetLabel(sequence_reset_yes ? "Reset(Y)" : "Reset(N)");
  }
  else if (cursor == sequence_length) {

    SetLabel("Steps");



  }

  if (cursor == sequence_length) {

    gfxFrame(0, 15, 64, 22);

    if (EditMode() || sequence_menu || sequence_slot_mode || sequence_reset_confirm)

      gfxInvert(1, 16, 62, 20);

  }
}

  FLASHMEM void DrawInterface() {

    if (page == MAIN_PAGE)
      DrawMainPage();
    else
      DrawNoteSequencerPage();

  }


  FLASHMEM void MoveFrog(int direction) {

    if (frog_horizontal) {

      if (frog_lane == 0) {
        frog_x_reference = 29;
      }
      else {
        frog_x_reference = constrain(
          frog_x_reference + direction,
          0,
          52
        );
      }

    } else {

      // Keep the reference position for CV modulation.
      frog_y_reference = constrain(frog_y_reference + direction, 0, 3);
      frog_lane = frog_y_reference;
      frog_y = FROG_Y[frog_lane];

      if (frog_lane == 0) {
        frog_x = 29;
        frog_x_reference = 29;
        modifier_icon = nullptr;
        modifier_value = 0;
        modifier_gate = false;
      }

    }

  }

  FLASHMEM void ToggleFrogAxis() {
    frog_horizontal = !frog_horizontal;
  }

  // --------------------------------------------------------------------------


  FLASHMEM void ResetSequence() {

    step = 0;
    reset = true;

    current_note = GetFrogNote(0);
  }

  void AdvanceSequence() {

    if (reset) {

      reset = false;

    }
    else {

      ++step;

      if (step >= sequence_length)
        step = 0;

    }

    if (!muted(step))
      current_note = GetFrogNote(step);
  }

  FLASHMEM void RandomizeSequence() {

    for (int s = 0; s < FROGSEQ_STEPS; ++s) {

      SetFrogNote(random(-24, 36), s);
      SetMute(s, random(2));
    }

    ResetSequence();
  }

  FLASHMEM void EditSequenceNote(int direction) {
    SetFrogNote(GetFrogNote(cursor) + direction, cursor);

    if (cursor == step)
      current_note = GetFrogNote(step);

    int notenum = GetFrogNote(cursor) + 36;

    SetLabel(midi_note_numbers[notenum]);


  }

  FLASHMEM void ToggleSequenceMute() {

    ToggleMute(cursor);

  }

public:


  void Start();


  void Controller();


  FLASHMEM void View();


  void OnEncoderMove(int direction);


  void OnButtonPress() override;


  void AuxButton();


  uint64_t OnDataRequest();

  uint64_t PackSequenceSteps(int first_step);

  void UnpackSequenceSteps(uint64_t data, int first_step);

  void SaveRestoreSnapshot();

  bool RestoreSequence();

  void SaveSequenceMemory(uint8_t sequence);

  bool LoadSequenceMemory(uint8_t sequence);

  void OnDataReceive(uint64_t data);


  void SetHelp();

};



void FLASHMEM FrogSeq::OnEncoderMove(int direction) {

    if (page == MAIN_PAGE) {
      if (q_select) {
        qselect = constrain(qselect + direction, 0, 7);
        return;
      }

      if (EditMode()) {
        if (cursor == FROG_SELECT)
          MoveFrog(direction);
        return;
      }

      if (direction > 0 && cursor == MAIN_CURSOR_LAST) {
        page = NOTE_SEQ_PAGE;
        cursor = 0;
        CancelEdit();
        return;
      }

      MoveCursor(cursor, direction, MAIN_CURSOR_LAST);
return;
    }

    if (page == NOTE_SEQ_PAGE) {
      if (sequence_reset_confirm) {

        sequence_reset_yes = (direction > 0);
        return;

      }

      if (sequence_menu) {

        sequence_menu_mode = static_cast<SequenceMenuMode>(
          constrain(
            static_cast<int>(sequence_menu_mode) + direction,
            SEQUENCE_LOAD,
            SEQUENCE_RESET
          )
        );

        return;

      }

      if (sequence_slot_mode) {

        selected_sequence = constrain(
          selected_sequence + direction,
          0,
          SAVED_SEQUENCES - 1
        );

        return;

      }

      if (!EditMode()) {
        if (direction < 0 && cursor == 0) {
          page = MAIN_PAGE;
          cursor = FROG_SELECT;
          CancelEdit();
          return;
        }

        MoveCursor(cursor, direction, NOTE_SEQ_CURSOR_LAST);

        // Skip unused step positions.
        if (cursor > sequence_length && cursor < LANE1_DIRECTION) {
          cursor = direction > 0 ? LANE1_DIRECTION : sequence_length;
        }

        return;
      }

      if (cursor == sequence_length) {
        sequence_length = constrain(
          sequence_length + direction,
          1,
          FROGSEQ_STEPS
        );
        cursor = sequence_length;
      }
      else if (cursor < sequence_length) {
        EditSequenceNote(direction);
      }
      else if (cursor >= LANE1_DIRECTION &&
               cursor <= LANE3_FLOW) {

        const int lane = (cursor - LANE1_DIRECTION) / 3;
        const int control = (cursor - LANE1_DIRECTION) % 3;

        if (control == 0) {
          lane_reverse[lane] = (direction < 0);
        }
        else if (control == 1) {
          lane_rate[lane] = static_cast<TrafficRate>(
            constrain(
              static_cast<int>(lane_rate[lane]) + direction,
              TRAFFIC_DIV_2,
              TRAFFIC_X4
            )
          );
        }
        else if (control == 2) {
          lane_flow[lane] = constrain(
            lane_flow[lane] + direction,
            1,
            6
          );
        }
      }
    }
  
}

void FLASHMEM FrogSeq::OnButtonPress() {


    if (page == MAIN_PAGE) {

      if (cursor == RANDOM_SELECT) {
        RandomizeSequence();
        return;
      }

      if (cursor == SEMITONE_SELECT) {
        q_select = !q_select;
        CursorToggle();
        return;
      }

      CursorToggle();
      return;
    }

    if (page == NOTE_SEQ_PAGE) {

      if (sequence_reset_confirm) {
        if (sequence_reset_yes) {
          for (int s = 0; s < FROGSEQ_STEPS; ++s) {
            sequence_notes[s] = 0;
            sequence_mutes[s] = false;
          }
          sequence_length = FROGSEQ_STEPS;
          step = 0;
          reset = true;
          current_note = GetFrogNote(0);
          SaveSequenceMemory(current_sequence);
          SaveRestoreSnapshot();
        }

        sequence_reset_confirm = false;
        sequence_reset_yes = true;
        sequence_menu_mode = SEQUENCE_LOAD;
        selected_sequence = current_sequence;
        CancelEdit();
        cursor = sequence_length;
        return;
      }

      if (sequence_menu) {
        if (sequence_menu_mode == SEQUENCE_RESET) {
          sequence_menu = false;
          sequence_reset_confirm = true;
          sequence_reset_yes = false;
          return;
        }

        sequence_menu = false;
        sequence_slot_mode = true;
        selected_sequence = current_sequence;
        return;
      }

      if (sequence_slot_mode) {
        if (sequence_menu_mode == SEQUENCE_LOAD) {
          if (LoadSequenceMemory(selected_sequence)) {
            current_sequence = selected_sequence;
            SaveRestoreSnapshot();
          }
        }
        else if (sequence_menu_mode == SEQUENCE_MOVE) {
          SaveSequenceMemory(selected_sequence);
          current_sequence = selected_sequence;
          SaveRestoreSnapshot();
        }

        sequence_slot_mode = false;
        sequence_menu_mode = SEQUENCE_LOAD;
        selected_sequence = current_sequence;
        CancelEdit();
        cursor = sequence_length;
        return;
      }

      CursorToggle();

      if (EditMode()) {
        if (cursor == sequence_length && !sequence_menu) {
          SetLabel("Steps");
        }
        else if (cursor >= NOTE_STEP_FIRST &&
                 cursor < sequence_length) {
          int notenum = GetFrogNote(cursor) + 36;
          SetLabel(midi_note_numbers[notenum]);
        }
        else if (cursor >= LANE1_DIRECTION &&
                 cursor <= LANE3_FLOW) {
          const int lane = (cursor - LANE1_DIRECTION) / 3;
          const int control = (cursor - LANE1_DIRECTION) % 3;

          if (control == 0)
            SetLabel(lane_reverse[lane] ? "Left" : "Right");
          else if (control == 1)
            SetLabel("Speed");
          else
            SetLabel("Flow");
        }
      }
      else {
        SetLabel("");
      }

      return;
    }

    CursorToggle();
  
}

void FLASHMEM FrogSeq::AuxButton() {


    if (page == MAIN_PAGE) {

      if (q_select) {
        HS::QuantizerEdit(qselect);
        return;
      }

      if (cursor == FROG_SELECT)
        ToggleFrogAxis();

      return;
    }

    if (page == NOTE_SEQ_PAGE) {

      // AUX on Steps opens the sequence menu.
      if (cursor == sequence_length) {

        if (sequence_menu || sequence_slot_mode || sequence_reset_confirm) {

          sequence_menu = false;
          sequence_slot_mode = false;
          sequence_reset_confirm = false;
          sequence_reset_yes = false;
          sequence_menu_mode = SEQUENCE_LOAD;
          selected_sequence = current_sequence;
          SetLabel("Steps");

        }
        else {

          sequence_menu = true;
          sequence_menu_mode = SEQUENCE_LOAD;
          selected_sequence = current_sequence;
          SetLabel("Load");

        }

        return;
      }

      // AUX cancels sequence selection.
      if (sequence_menu || sequence_slot_mode || sequence_reset_confirm) {

        sequence_menu = false;
        sequence_slot_mode = false;
        sequence_reset_confirm = false;
        sequence_reset_yes = false;
        sequence_menu_mode = SEQUENCE_LOAD;
        selected_sequence = current_sequence;
        SetLabel("Steps");
        return;
      }

      ToggleMute(cursor);
      CancelEdit();
      return;
    }

  
}

uint64_t FLASHMEM FrogSeq::OnDataRequest() {


    uint64_t data = 0;
for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {

      const size_t base = lane * 7;

      Pack(
        data,
        PackLocation{base, 1},
        (uint8_t)lane_reverse[lane]
      );

      Pack(
        data,
        PackLocation{base + 1, 3},
        (uint8_t)lane_rate[lane]
      );

      Pack(
        data,
        PackLocation{base + 4, 3},
        (uint8_t)constrain(lane_flow[lane], 1, 6)
      );
    }

    Pack(
      data,
      PackLocation{21, 3},
      current_sequence
    );


    Pack(
      data,
      PackLocation{24, 4},
      sequence_length - 1
    );

    SaveSequenceMemory(current_sequence);
    SaveRestoreSnapshot();

    return data;
  
}


void FLASHMEM FrogSeq::SaveRestoreSnapshot() {
    SetData(
      FROGSEQ_RESTORE_SLOT_1,
      PackSequenceSteps(0)
    );

    SetData(
      FROGSEQ_RESTORE_SLOT_2,
      PackSequenceSteps(8)
    );
  }

void FLASHMEM FrogSeq::OnDataReceive(uint64_t data) {

    // Frog X is not persisted.
    frog_x = 26;
    current_sequence =
      constrain(
        Unpack(data, PackLocation{21, 3}),
        0,
        SAVED_SEQUENCES - 1
      );


    sequence_length =
      constrain(
        Unpack(data, PackLocation{24, 4}) + 1,
        1,
        FROGSEQ_STEPS
      );

    LoadSequenceMemory(current_sequence);
    SaveRestoreSnapshot();

    collision_ratchet_armed = false;
    collision_ratchets_to_go = 0;
    collision_ratchets_display = 0;
    collision_ratchet_count = 0;
    collision_ratchet_next_tick = 0;
    collision_ratchet_zap = false;
    collision_ratchet_spacing = 0;

    for (int lane = 0; lane < TRAFFIC_LANES; ++lane) {

      const size_t base = lane * 7;

      lane_reverse[lane] =
        Unpack(data, PackLocation{base, 1}) != 0;

      lane_rate[lane] =
        static_cast<TrafficRate>(
          constrain(
            Unpack(data, PackLocation{base + 1, 3}),
            TRAFFIC_DIV_2,
            TRAFFIC_X4
          )
        );

      lane_flow[lane] =
        constrain(
          Unpack(data, PackLocation{base + 4, 3}),
          1,
          6
        );
    }

    frog_y = 14;

    step = 0;

    reset = true;
  
}

FrogSeq::TrafficModifier FLASHMEM FrogSeq::RandomTrafficModifier(int lane) {
    const bool can_ratchet =
      lane_rate[lane] == TRAFFIC_DIV_2 ||
      lane_rate[lane] == TRAFFIC_X2 ||
      lane_rate[lane] == TRAFFIC_X3 ||
      lane_rate[lane] == TRAFFIC_X4;

    const int roll = random(100);

    if (can_ratchet) {
      if (roll < 55)
        return MODIFIER_NOTE;
      if (roll < 85)
        return MODIFIER_GATE;
      return MODIFIER_RATCHET;
    }

    return roll < 55 ? MODIFIER_NOTE : MODIFIER_GATE;
  }

void FLASHMEM FrogSeq::Start() {

    collision_display_until = 0;
    collision_blink_until = 0;
    modifier_display_tick = 0;
    modifier_icon = nullptr;
    collision_ratchet_armed = false;
    collision_ratchets_to_go = 0;
    collision_ratchets_display = 0;
    collision_ratchet_count = 0;
    collision_ratchet_next_tick = 0;
    collision_ratchet_zap = false;
    collision_ratchet_spacing = 0;

    frog_x = 26;
    frog_y = FROG_Y[0];
    ResetTraffic();

    page = MAIN_PAGE;

    cursor = FROG_SELECT;

    frog_horizontal = false;

    for (int s = 0; s < FROGSEQ_STEPS; ++s) {
      sequence_notes[s] = 0;
      sequence_mutes[s] = false;
    }

    current_note = GetFrogNote(0);

    step = 0;
    reset = true;
  
}


uint64_t FLASHMEM FrogSeq::PackSequenceSteps(int first_step) {
    uint64_t data = 0;

    for (int s = first_step; s < first_step + 8; ++s) {
      const uint8_t note =
        constrain(sequence_notes[s] + 24, 0, 59);

      const uint8_t packed =
        note
        | (sequence_mutes[s] ? 0x40 : 0);

      Pack(
        data,
        PackLocation{static_cast<size_t>((s - first_step) * 8), 8},
        packed
      );
    }

    return data;
  }

void FLASHMEM FrogSeq::UnpackSequenceSteps(uint64_t data, int first_step) {
    for (int s = first_step; s < first_step + 8; ++s) {
      const size_t offset = (s - first_step) * 8;
      const uint8_t packed =
        Unpack(data, PackLocation{offset, 8});

      sequence_notes[s] =
        constrain((packed & 0x3f) - 24, -24, 35);
      sequence_mutes[s] =
        (packed & 0x40) != 0;
    }
  }

bool FLASHMEM FrogSeq::RestoreSequence() {
    uint64_t data = 0;

    if (!GetData(FROGSEQ_RESTORE_SLOT_1, data))
      return false;

    UnpackSequenceSteps(data, 0);

    if (!GetData(FROGSEQ_RESTORE_SLOT_2, data))
      return false;

    UnpackSequenceSteps(data, 8);

    collision_display_until = 0;
    collision_blink_until = 0;
    modifier_icon = nullptr;
    modifier_value = 0;
    modifier_gate = false;

    collision_ratchet_armed = false;
    collision_ratchets_to_go = 0;
    collision_ratchets_display = 0;
    collision_ratchet_count = 0;
    collision_ratchet_next_tick = 0;
    collision_ratchet_zap = false;
    collision_ratchet_spacing = 0;

    step = 0;
    reset = true;
    current_note = GetFrogNote(0);

    return true;
  }

void FLASHMEM FrogSeq::SaveSequenceMemory(uint8_t sequence) {
    SetData(
      FROGSEQ_DATA_START + sequence * 2,
      PackSequenceSteps(0)
    );

    SetData(
      FROGSEQ_DATA_START + sequence * 2 + 1,
      PackSequenceSteps(8)
    );
  }

bool FLASHMEM FrogSeq::LoadSequenceMemory(uint8_t sequence) {
    uint64_t data = 0;

    if (!GetData(FROGSEQ_DATA_START + sequence * 2, data))
      return false;

    UnpackSequenceSteps(data, 0);

    if (!GetData(FROGSEQ_DATA_START + sequence * 2 + 1, data))
      return false;

    UnpackSequenceSteps(data, 8);

    return true;
  }



void FLASHMEM FrogSeq::SetHelp(){

    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "Restore";
    help[HELP_CV1] = "Frog X";
    help[HELP_CV2] = "Frog Y";
    help[HELP_OUT1] = "Pitch";
    help[HELP_OUT2] = "Trigger";
    help[HELP_EXTRA1] = "Collisions modify";
    help[HELP_EXTRA2] = "Restore=Last Loaded";
  }

void FrogSeq::Controller() {
    if (frog_lane == 0) {
      frog_x = 29;
      frog_x_reference = 29;
    }
    else {
      frog_x = frog_x_reference;
      Modulate(frog_x, 0, 0, 52);
      frog_x = constrain(frog_x, 0, 52);
    }

    const int frog_y_mod = constrain(SemitoneIn(1) / 12, -3, 3);

    frog_y_position = constrain(
      frog_y_reference + frog_y_mod,
      0,
      3
    );

    frog_y = FROG_Y[frog_y_position];
    frog_lane = frog_y_position;

    if (Clock(1)) {
      RestoreSequence();
    }

    const bool clocked = Clock(0);

    if (clocked) {
      // The previous Ratchet display ends at the new master-clock step.
      // A Ratchet beginning on this same clock will turn ZAP back on.
      collision_ratchet_zap = false;
    }

    MoveTraffic(clocked);

    // Collision Ratchet.
    //
    // The captured note is retained for the entire Ratchet.
    // /2 produces two hits on consecutive master-clock cycles.
    // ×2, ×3 and ×4 use the countdown timer for additional hits
    // between master clocks.
    // The sequence continues advancing on each master clock.
    if (collision_ratchet_armed) {

      const uint32_t now = OC::CORE::ticks;

      const uint32_t ratchet_spacing = collision_ratchet_spacing;

      if (clocked) {

        AdvanceSequence();

        collision_ratchet_count = 1;
        collision_ratchet_zap = true;

        int play_cv = MIDIQuantizer::CV(collision_ratchet_note + 36);
        play_cv = HS::GetQuantEngine(qselect).Process(play_cv, 0, 0);
        Out(0, play_cv);
        ClockOut(1);

        --collision_ratchets_to_go;
        if (collision_ratchets_display > 0)
          --collision_ratchets_display;

        if (collision_ratchets_to_go > 0) {
          collision_ratchet_next_tick = now + ratchet_spacing;
        }
        else {
          collision_ratchet_armed = false;
        }
      }
      else if (collision_ratchet_count > 0 &&
               collision_ratchets_to_go > 0 &&
               static_cast<int32_t>(
                 now - collision_ratchet_next_tick) >= 0) {

          int play_cv = MIDIQuantizer::CV(collision_ratchet_note + 36);
          play_cv = HS::GetQuantEngine(qselect).Process(play_cv, 0, 0);
          Out(0, play_cv);
          ClockOut(1);

          ++collision_ratchet_count;
          --collision_ratchets_to_go;
          if (collision_ratchets_display > 0)
          --collision_ratchets_display;

          if (collision_ratchets_to_go > 0)
            collision_ratchet_next_tick += ratchet_spacing;
          else
            collision_ratchet_armed = false;
        }
      }

    else if (clocked) {

      AdvanceSequence();

      if (muted(step)) {

        GateOut(1, false);

      }
      else {

        int play_cv = MIDIQuantizer::CV(current_note + 36);
        play_cv = HS::GetQuantEngine(qselect).Process(play_cv, 0, 0);
        Out(0, play_cv);

        ClockOut(1);
      }
    }
  }

void FLASHMEM FrogSeq::View() {

    DrawInterface();
  }
