// Copyright (c) 2018, Jason Justian
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#define HEM_BURST_NUMBER_MAX 12
#define HEM_BURST_SPACING_MAX 500
#define HEM_BURST_SPACING_MIN 8
#define HEM_BURST_CLOCKDIV_MAX 8
#define HEM_BURST_ACCEL_MAX 50
#define HEM_BURST_JITTER_MAX 50

class Burst : public HemisphereApplet {
public:
    enum BurstCursor {
      CLKPASSTHRU, PROB,
      NUMBER, SPACING, ACCEL, JITTER, DIVISION,

      MAX_CURSOR = JITTER
    };

    const char* applet_name() {
        return "Burst";
    }
    const uint8_t* applet_icon() { return PhzIcons::burst; }

    void Start() {
        cursor = 0;
        number = 4;
        number_mod = 4;
        div = 1;
        effective_div = 1;
        spacing = 50;
        display_spacing = 50;
        accel = 0;
        jitter = 0;
        bursts_to_go = 0;
        clocked = 0;
        passthru_popup_tick = 0;
        skip_tick = 0;
        zap_active = false;
    }

    void Controller() {
        if (CLKPASSTHRU == cursor && passthru && OC::CORE::ticks - passthru_popup_tick >= HEMISPHERE_CURSOR_TICKS * 4) enc_edit[hemisphere].isEditing = false;
        number_mod = constrain(number + SemitoneIn(0) / 5, 1, HEM_BURST_NUMBER_MAX);
        if (clocked) {
            static const int div_states[] = {-8, -7, -6, -5, -4, -3, -2, 1, 2, 3, 4, 5, 6, 7, 8};
            int base_pos = 0;
            for (int i = 0; i < 15; ++i) {
                if (div_states[i] == div) base_pos = i;
            }
            int cv_steps = Proportion(DetentedIn(1), HEMISPHERE_MAX_INPUT_CV, 7);
            int effective_pos = constrain(base_pos + cv_steps, 0, 14);
            effective_div = div_states[effective_pos];
        }
        if (Clock(0)) {
            if (clocked) {
                spacing = ClockCycleTicks(0) / number_mod / HEMISPHERE_CLOCK_TICKS;
            } else clocked = 1;

            if (passthru & 1)
              ClockOut(0);
        }

        int effective_spacing = get_effective_spacing();
        if (!clocked) {
            Modulate(effective_spacing, 1, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
            display_spacing = effective_spacing;
        }
        // Handle a burst set in progress
        if (bursts_to_go > 0) {
            if (--burst_countdown <= 0) {
                int modded_spacing = constrain(effective_spacing, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
                int accel_span = burst_count + bursts_to_go - 1;
                if (accel > 0) {
                    int amount_from_min = modded_spacing - HEM_BURST_SPACING_MIN;
                    int spacing_accel = amount_from_min * burst_count / accel_span * accel / HEM_BURST_ACCEL_MAX;
                    modded_spacing -= spacing_accel;
                }
                if (accel < 0) {
                    int amount_from_max = HEM_BURST_SPACING_MAX - modded_spacing;
                    int spacing_accel = amount_from_max * burst_count / accel_span * abs(accel) / HEM_BURST_ACCEL_MAX;
                    modded_spacing += spacing_accel;
                }
                if (jitter > 0) {
                    int rand = random(10 * -jitter, 1 + (10 * jitter));
                    int jitter_offset = Proportion(rand, (HEM_BURST_JITTER_MAX * 10), modded_spacing);
                    modded_spacing += jitter_offset;
                }
                modded_spacing = constrain(modded_spacing, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
                ClockOut(0);
                zap_active = true;
                burst_count++;
                if (--bursts_to_go <= 0) GateOut(1, 0);
                burst_countdown = modded_spacing * HEMISPHERE_CLOCK_TICKS;
            }
        } else if (burst_countdown > 0) {
            --burst_countdown;
        }

        // Handle the triggering of a new burst set.
        //
        bool trigger = Clock(1);
        bool btrig = trigger && (random(100) >= prob);
        if (trigger) { zap_active = btrig; if (!btrig) skip_tick = OC::CORE::ticks; }
        if ((passthru & 0x2) && trigger) ClockOut(0);

        if (btrig) {
            ClockOut(0);
            GateOut(1, 1);
            bursts_to_go = number_mod - 1;
            if (bursts_to_go == 0) GateOut(1, 0);
            burst_countdown = effective_spacing * HEMISPHERE_CLOCK_TICKS;
            burst_count = 1;
        }
    }

    void View() {

        DrawSelector();
        DrawIndicator();

    }

    void OnButtonPress() {
      if (CLKPASSTHRU == cursor) {
        ++passthru %= 3;
        passthru_popup_tick = OC::CORE::ticks;
        enc_edit[hemisphere].isEditing = passthru != 0;
        ResetCursor();
      } else
        CursorToggle();
    }

    void OnEncoderMove(int direction) {
        if (passthru && cursor == CLKPASSTHRU) passthru_popup_tick = 0;
        if (!EditMode()) {
            MoveCursor(cursor, direction, MAX_CURSOR + clocked);
            return;
        }

        switch (cursor) {
          case CLKPASSTHRU:
            SetLabel(passthru == 1 ? "ClockPass" : passthru == 2 ? "BurstPass" : "Burst");
            break;
          case PROB:
            prob = constrain(prob + direction, 0, 100);
            break;
          case NUMBER:
            number = constrain(number + direction, 1, HEM_BURST_NUMBER_MAX);
            break;
          case SPACING:
            spacing = constrain(spacing + direction, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
            clocked = 0;
            break;
          case ACCEL:
            accel = constrain(accel + direction, -HEM_BURST_ACCEL_MAX, HEM_BURST_ACCEL_MAX);
            break;
          case JITTER:
            jitter = constrain(jitter + direction, 0, HEM_BURST_JITTER_MAX);
            break;

          case DIVISION:
            div += direction;
            div_constrain(div, direction);
            break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, number);
        Pack(data, PackLocation {8,8}, spacing);
        Pack(data, PackLocation {16,8}, div + 8);
        Pack(data, PackLocation {24,8}, jitter);
        Pack(data, PackLocation {32,8}, (uint8_t)accel);

        Pack(data, PackLocation {40,7}, prob);
        Pack(data, PackLocation {47,2}, passthru);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        number = constrain(Unpack(data, PackLocation {0,8}), 1, HEM_BURST_NUMBER_MAX);
        spacing = constrain(Unpack(data, PackLocation {8,8}), HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
        div = Unpack(data, PackLocation {16,8}) - 8; div_constrain(div); // special constrain for div
        jitter = constrain(Unpack(data, PackLocation {24,8}), 0, HEM_BURST_JITTER_MAX);
        accel = Unpack(data, PackLocation {32,8});
        CONSTRAIN(accel, -HEM_BURST_ACCEL_MAX, HEM_BURST_ACCEL_MAX);

        prob = Unpack(data, PackLocation {40,7});
        passthru = Unpack(data, PackLocation {47,2});
    }

protected:
  void SetHelp() {
    help[HELP_DIGITAL1] = "Clock";
    help[HELP_DIGITAL2] = "Burst";
    help[HELP_CV1]      = "Steps";
    help[HELP_CV2]      = "Spacing";
    help[HELP_OUT1]     = "Burst";
    help[HELP_OUT2]     = "Gate";
    help[HELP_EXTRA1] = "";
    help[HELP_EXTRA2] = "";
  }

private:
    int cursor; // Current parameter
    int burst_countdown; // Ticks until the next burst
    int bursts_to_go; // Remaining bursts in the current set
    int burst_count; // Bursts fired in the current set
    bool clocked; // True after the first clock; later clocks set spacing
    bool zap_active; // ZAP display state

    int div; // Clock divide/multiply
    int effective_div; // CV2-modulated divide/multiply
    uint8_t number; // Bursts per trigger
    int number_mod; // CV1-modulated bursts per trigger
    uint16_t spacing; // Time between bursts in ms
    int display_spacing; // CV2-modulated spacing shown on display
    uint32_t passthru_popup_tick; // Passthrough popup timeout
    uint32_t skip_tick; // Skipped clock indicator timeout
    int8_t accel; // Acceleration or deceleration
    uint8_t jitter; // Randomness
    uint8_t passthru; // Clock/burst passthrough
    uint8_t prob; // Skip probability

    void DrawSelector() {
        int y = 13;

        const uint8_t* icons[] = {CHECK_OFF_ICON, CHECK_ON_ICON, BURST_ICON, ZAP_ICON};
        uint8_t display_passthru = constrain(passthru, 0, 2);

        // Clock passthrough
        gfxIcon(1, y, CLOCK_ICON);
        gfxIcon(10, y, icons[display_passthru]);

        if (skip_tick && OC::CORE::ticks - skip_tick < HEMISPHERE_CURSOR_TICKS) gfxPrint(33, y, "*");
        gfxPrint(40, y, prob);
        gfxPrint("%");

        y += 9;

        // Steps
        gfxPrint(1, y, number_mod);
        gfxPrint(18, y, "Steps");
        if (number_mod != number) { gfxIcon(50, y, CV_ICON); gfxBitmap(59, y, 3, SUP_ONE); }

        y += 9;

        // Spacing
        gfxPrint(1, y, clocked ? get_effective_spacing() : display_spacing);
        gfxPrint(28, y, "ms");
        if (!clocked && display_spacing != spacing) { gfxIcon(43, y, CV_ICON); gfxBitmap(51, y, 3, SUB_TWO); }

        y += 9;

        // Acceleration
        gfxIcon(1, y-1, GAUGE_ICON);
        gfxPrint(10, y, accel);

        // Jitter
        gfxIcon(32, y - 1, RANDOM_ICON);
        gfxPrint(40, y, jitter);

        y += 9;

        // Div
        if (clocked) {
            bool multiplying = effective_div < 0;
            gfxBitmap(1, y, 8, CLOCK_ICON);
            gfxPrint(6, y, multiplying ? "x" : "/");
            gfxPrint(multiplying ? -effective_div : effective_div);
            gfxPrint(multiplying ? " Mult" : " Div");
            if (effective_div != div) { gfxIcon(49, y, CV_ICON); gfxBitmap(57, y, 3, SUB_TWO); }
        }

        // Cursor
        switch (cursor) {
          case CLKPASSTHRU:
            gfxIcon(19, 13, LEFT_ICON);
            SetLabel(passthru == 1 ? "ClockPass" : passthru == 2 ? "BurstPass" : "Burst");
            break;
          case PROB:
            gfxCursor(40, 21, 18, 9, "Skip %");
            break;
          case NUMBER:
            gfxCursor( 1, 30, 13, 9, "Count");
            break;
          case SPACING:
            gfxCursor( 1, 39, 19, 9, "Spacing");
            break;
          case ACCEL:
            gfxCursor(10, 48, 13, 9, "Accel");
            break;
          case JITTER:
            gfxCursor(40, 48, 13, 9, "Jitter");
            break;
          case DIVISION:
            gfxCursor(1, 57, 47, 9, "ClkDiv");
            break;
        }
    }

    void DrawIndicator() {
        if (zap_active && burst_countdown > 0)
            gfxIcon(max(0, 1 + ((number_mod - 1) * 5) - 2), 56, ZAP_ICON);
        // Countdown markers: 3x3 with 1 px between markers.
        int display_bursts = constrain(bursts_to_go, 0, HEM_BURST_NUMBER_MAX);
        for (int i = 0; i < display_bursts; i++)
            gfxFrame(1 + (i * 5), 59, 3, 3);
    }

    int get_effective_spacing() {
        int effective_spacing = spacing;
        if (clocked) {
            if (effective_div > 1) effective_spacing *= effective_div;
            if (effective_div < 0) effective_spacing /= -effective_div;
        }
        return effective_spacing;
    }

    void div_constrain(int &value, int dir = 1) {
        if (value > HEM_BURST_CLOCKDIV_MAX) value = HEM_BURST_CLOCKDIV_MAX;
        if (value < -HEM_BURST_CLOCKDIV_MAX) value = -HEM_BURST_CLOCKDIV_MAX;
        if (value == 0) value = dir > 0 ? 1 : -2; // No such thing as 1/1 Multiple
        if (value == -1) value = 1; // Must be moving up to hit -1
    }
};
