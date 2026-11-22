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

// Two pulses separated in time: Spacing icon
alignas(8) const uint8_t BURST_SPACING_ICON[8] =
    {0x40, 0x7e, 0x40, 0x40, 0x40, 0x7e, 0x40, 0x00};

class Burst : public HemisphereApplet {
public:
    enum BurstCursor {
      CLKPASSTHRU, PROB, NUMBER, RETRIG,
      GATE_PASSTHRU, DIVISION, SPACING, ACCEL, JITTER,

      MAX_CURSOR = JITTER
    };

    const char* applet_name() {
        return "Burst";
    }
    const uint8_t* applet_icon() { return PhzIcons::burst; }

    void Start() {
        cursor = 0;
        retrig_mode = 0; // Restart by default
        number = 4;
        number_mod = 4;
        div = 1;
        effective_div = 1;
        spacing = 50;
        display_spacing = 50;
        accel = 0;
        jitter = 0;
        bursts_to_go = 0;
        burst_countdown = 0;
        burst_count = 0;
        burst_pending = false;
        out_a_wait_ticks = 0;
        out_b_high = false;
        out_b_edge_pending = false;
        gate_thru_popup_tick = 0;
        out_b_hold = false;
        out_b_gap_ticks = 0;
        out_b_pulse_ticks = 0;
        out_b_thru = false;
        clocked = 0;
        skip_tick = 0;
        zap_active = false;
    }

    void Controller() {
        if (cursor == GATE_PASSTHRU &&
            EditMode() &&
            HS::get_tick() - gate_thru_popup_tick >=
                HEMISPHERE_CURSOR_TICKS * 4) {
            enc_edit[hemisphere].isEditing = false;
        }
        if (out_b_gap_ticks > 0) {
            --out_b_gap_ticks;
        } else if (out_b_pulse_ticks > 0) {
            --out_b_pulse_ticks;
        }

        bool burst_started = false;
        bool burst_finished = false;

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
        // Block passthrough while a burst or its final interval is active.
        const bool block_passthru = retrig_mode == 2 &&
            (bursts_to_go > 0 || burst_countdown > 0);

        const bool clock_edge = Clock(0);
        bool burst_pulse = false;
        bool passthru_pulse = false;
        if (clock_edge) {
            if (clocked) {
                spacing = max(1u, ClockCycleTicks(0) / number_mod / HEMISPHERE_CLOCK_TICKS);
            } else clocked = 1;

            if ((passthru & 1) && !block_passthru)
                passthru_pulse = true;
        }

        int effective_spacing = get_effective_spacing();
        if (!clocked) {
            Modulate(effective_spacing, 1, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
            display_spacing = effective_spacing;
        }
        // Handle a burst set in progress
        if (bursts_to_go > 0) {
            if (--burst_countdown <= 0) {
                int modded_spacing = effective_spacing;
                int accel_span = burst_count + bursts_to_go - 1;
                if (accel > 0) {
                    int accel_min = effective_spacing < HEM_BURST_SPACING_MIN
                        ? 1
                        : HEM_BURST_SPACING_MIN;
                    int amount_from_min = modded_spacing - accel_min;
                    int spacing_accel = amount_from_min * burst_count / accel_span * accel / HEM_BURST_ACCEL_MAX;
                    modded_spacing -= spacing_accel;
                }
                if (accel < 0) {
                    int accel_max = effective_spacing > HEM_BURST_SPACING_MAX
                        ? HEM_BURST_SPACING_MAX * HEM_BURST_CLOCKDIV_MAX
                        : HEM_BURST_SPACING_MAX;
                    int amount_from_max = accel_max - modded_spacing;
                    int spacing_accel = amount_from_max * burst_count / accel_span * abs(accel) / HEM_BURST_ACCEL_MAX;
                    modded_spacing += spacing_accel;
                }
                if (jitter > 0) {
                    int rand = random(10 * -jitter, 1 + (10 * jitter));
                    int jitter_offset = Proportion(rand, (HEM_BURST_JITTER_MAX * 10), modded_spacing);
                    modded_spacing += jitter_offset;
                }
                modded_spacing = max(1, modded_spacing);
                burst_pulse = true;
                zap_active = true;
                burst_count++;
                if (--bursts_to_go <= 0) burst_finished = true;
                burst_countdown = modded_spacing * HEMISPHERE_CLOCK_TICKS;
            }
        } else if (burst_countdown > 0) {
            --burst_countdown;
        }

        // Handle the triggering of a new burst set.
        //
        bool trigger = Clock(1);
        bool btrig = trigger && (random(100) >= prob);
        if (trigger && !btrig) skip_tick = HS::get_tick();
        bool burst_ready = !burst_pending &&
            bursts_to_go == 0 && burst_countdown <= 0;
        bool start_burst = btrig && (retrig_mode == 0 || burst_ready);

        if ((passthru & 0x2) && trigger &&
            !block_passthru && !start_burst)
            passthru_pulse = true;

        if (start_burst && clocked) {
            burst_pending = true;
        }

        if ((start_burst && !clocked) ||
            (burst_pending && clock_edge)) {
            burst_pending = false;
            zap_active = true;
            burst_pulse = true;
            burst_started = true;
            bursts_to_go = number_mod - 1;
            burst_countdown = effective_spacing * HEMISPHERE_CLOCK_TICKS;
            burst_count = 1;
        }

        // Output A: positive wait = pulse + low gap; negative = a queued
        // burst edge waiting for its low gap. This keeps a single timer.
        const int gap = HEMISPHERE_CLOCK_TICKS;
        const int ch = io_offset;
        const bool pending_due = out_a_wait_ticks == -1;
        if (out_a_wait_ticks > 0) --out_a_wait_ticks;
        else if (out_a_wait_ticks < 0) ++out_a_wait_ticks;

        if (burst_pulse) {
            if (HS::frame.clock_countdown[ch] > 0) {
                // Restart during a high pulse: cut it now, then retrigger.
                HS::frame.clock_countdown[ch] = 0;
                HS::frame.Out((DAC_CHANNEL)ch, 0, true);
                out_a_wait_ticks = -gap;
            } else if (out_a_wait_ticks > 0) {
                // The pulse ended; finish its remaining low gap.
                out_a_wait_ticks = -out_a_wait_ticks;
            }
        }

        const bool fire_burst = (burst_pulse || pending_due) && out_a_wait_ticks == 0;
        const bool fire_thru = passthru_pulse && out_a_wait_ticks == 0 &&
            !burst_pulse && !pending_due && HS::frame.clock_countdown[ch] == 0 &&
            (burst_countdown <= 0 || burst_countdown > gap);
        if (fire_burst || fire_thru) {
            int width = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
            if (burst_countdown > 0)
                width = min(width, max(1, burst_countdown - gap));
            ClockOut(0, width);
            if (HS::frame.clock_countdown[ch] > 0)
                out_a_wait_ticks = width + gap;
        }

        // Output B: sustained burst state with quantizer-safe edges.
        if (burst_started) {
            if (out_b_high) {
                GateOut(1, 0);
                out_b_high = false;
                out_b_gap_ticks = HEMISPHERE_CLOCK_TICKS;
            }

            out_b_hold = bursts_to_go > 0;
            out_b_pulse_ticks = 0;
            out_b_edge_pending = true;

        } else if (burst_finished) {
            // Release at the final hit.
            out_b_hold = false;
            out_b_pulse_ticks = 0;
        }

        // Allowed passthrough pulses B only when its gate is low.
        // Blk-mode suppression is handled upstream.
        if (out_b_thru && passthru_pulse &&
            !out_b_high && !out_b_hold &&
            !out_b_edge_pending &&
            out_b_gap_ticks == 0 &&
            !burst_started && !burst_finished) {
            out_b_pulse_ticks = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
        }

        // Deliver the burst's rising edge after any restart gap.
        // If the burst already finished, use a minimum 1 ms pulse.
        if (out_b_edge_pending && out_b_gap_ticks == 0) {
            out_b_edge_pending = false;
            if (!out_b_hold)
                out_b_pulse_ticks = HEMISPHERE_CLOCK_TICKS * HS::trig_length;
        }

        const bool b_should_be_high = out_b_gap_ticks == 0 &&
            (out_b_hold || out_b_pulse_ticks > 0);

        if (b_should_be_high != out_b_high) {
            GateOut(1, b_should_be_high ? 1 : 0);
            out_b_high = b_should_be_high;
        }
    }

    void View() {

        DrawSelector();
        DrawIndicator();

}

    void OnButtonPress() {
        if (cursor == GATE_PASSTHRU) {
            out_b_thru = !out_b_thru;
            gate_thru_popup_tick = HS::get_tick();
            enc_edit[hemisphere].isEditing = true;
            ResetCursor();
        } else {
            CursorToggle();
        }
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, MAX_CURSOR);
            if (!clocked && cursor == DIVISION)
                MoveCursor(cursor, direction, MAX_CURSOR);
            return;
        }

        switch (cursor) {
          case CLKPASSTHRU:
            passthru = constrain(int(passthru) + direction, 0, 2);
            break;
          case RETRIG:
            retrig_mode = constrain(int(retrig_mode) + direction, 0, 2);
            break;
          case PROB:
            prob = constrain(prob + direction, 0, 100);
            break;
          case NUMBER:
            number = constrain(number + direction, 1, HEM_BURST_NUMBER_MAX);
            number_mod = constrain(number + SemitoneIn(0) / 5, 1, HEM_BURST_NUMBER_MAX);
            break;
          case SPACING:
            spacing = constrain(spacing + direction, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
            clocked = 0;
            display_spacing = spacing;
            Modulate(display_spacing, 1, HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
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
        // Nine bits for 8..500 ms; retain the legacy positions of other fields.
        const uint16_t saved_spacing = constrain((int)spacing,
                                                 HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
        Pack(data, PackLocation {8,8}, saved_spacing & 0xff);
        Pack(data, PackLocation {52,1}, (saved_spacing >> 8) & 0x01);
        Pack(data, PackLocation {16,8}, div + 8);
        Pack(data, PackLocation {24,8}, jitter);
        Pack(data, PackLocation {32,8}, (uint8_t)accel);

        Pack(data, PackLocation {40,7}, prob);
        Pack(data, PackLocation {47,2}, passthru);
        Pack(data, PackLocation {49,2}, retrig_mode);
        Pack(data, PackLocation {51,1}, out_b_thru ? 1 : 0);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        number = constrain(Unpack(data, PackLocation {0,8}), 1, HEM_BURST_NUMBER_MAX);
        spacing = constrain(Unpack(data, PackLocation {8,8}) |
                            (Unpack(data, PackLocation {52,1}) << 8),
                            HEM_BURST_SPACING_MIN, HEM_BURST_SPACING_MAX);
        div = Unpack(data, PackLocation {16,8}) - 8; div_constrain(div); // special constrain for div
        jitter = constrain(Unpack(data, PackLocation {24,8}), 0, HEM_BURST_JITTER_MAX);
        accel = Unpack(data, PackLocation {32,8});
        CONSTRAIN(accel, -HEM_BURST_ACCEL_MAX, HEM_BURST_ACCEL_MAX);

        prob = Unpack(data, PackLocation {40,7});
        passthru = Unpack(data, PackLocation {47,2});
        retrig_mode = constrain(int(Unpack(data, PackLocation {49,2})), 0, 2);
        out_b_thru = Unpack(data, PackLocation {51,1}) != 0;
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
    bool burst_pending; // Waiting for the next clock edge
    bool out_b_high;
    bool out_b_edge_pending;
    int out_a_wait_ticks; // Positive: cooldown; negative: pending burst edge
    bool out_b_hold;
    bool out_b_thru;
    uint32_t gate_thru_popup_tick;
    int out_b_gap_ticks;
    int out_b_pulse_ticks;
    bool clocked; // True after the first clock; later clocks set spacing
    bool zap_active; // ZAP display state

    int div; // Clock divide/multiply
    int effective_div; // CV2-modulated divide/multiply
    uint8_t number; // Bursts per trigger
    int number_mod; // CV1-modulated bursts per trigger
    uint16_t spacing; // Time between bursts in ms
    int display_spacing; // CV2-modulated spacing shown on display
    uint32_t skip_tick; // Skipped clock indicator timeout
    int8_t accel; // Acceleration or deceleration
    uint8_t jitter; // Randomness
    uint8_t passthru; // Clock/burst passthrough
    uint8_t retrig_mode; // 0=Restart, 1=Ignore, 2=Block
    uint8_t prob; // Skip probability

    void DrawSelector() {
        int y = 13;

        const uint8_t* icons[] = {CHECK_OFF_ICON, CHECK_ON_ICON, BURST_ICON, ZAP_ICON};
        uint8_t display_passthru = constrain(passthru, 0, 2);

        // Clock passthrough
        gfxIcon(1, y, CLOCK_ICON);
        gfxIcon(11, y, icons[display_passthru]);
        if (cursor == CLKPASSTHRU && EditMode())
            gfxInvert(10, y - 1, 10, 10);

        bool show_skip = skip_tick && HS::get_tick() - skip_tick < HEMISPHERE_CURSOR_TICKS;
        gfxPrint(32, y, "%");
        gfxPrint(40, y, prob);
        const int prob_w = gfxGetPrintPosX() - 40;
        if (show_skip) gfxInvert(31, y, 8, 8);

        y += 9;

        // Steps
        gfxBitmap(1, y - 1, 8, BURST_ICON);
        gfxPrint(10 + pad(10, number_mod), y, number_mod);
        const int number_w = 12;
        if (number_mod != number) { gfxIcon(23, y, CV_ICON); gfxBitmap(31, y, 3, SUP_ONE); }
        gfxIcon(35, y, LOOP_ICON);
        gfxPrint(44, y, retrig_mode == 0 ? "Rst" : retrig_mode == 1 ? "Ign" : "Blc");

        y += 9;

        // Gate+Thru (left) and Clock Division (right)
        gfxIcon(1, y, GATE_ICON);
        gfxIcon(10, y, out_b_thru ? CHECK_ON_ICON : CHECK_OFF_ICON);

        int div_w = 0;
        if (clocked) {
            bool multiplying = effective_div < 0;
            gfxIcon(28, y, CLOCK_ICON);
            gfxPrint(37, y, multiplying ? "x" : "/");
            gfxPrint(multiplying ? -effective_div : effective_div);
            div_w = gfxGetPrintPosX() - 37;
            if (effective_div != div) {
                gfxIcon(39 + div_w, y, CV_ICON);
                gfxBitmap(47 + div_w, y, 3, SUB_TWO);
            }
        }

        y += 9;

        // Spacing
        gfxIcon(1, y, BURST_SPACING_ICON);
        gfxPrint(10, y, clocked ? get_effective_spacing() : display_spacing);
        gfxPrint("ms");
        const int spacing_w = gfxGetPrintPosX() - 10;
        if (!clocked && display_spacing != spacing) {
            gfxIcon(43, y, CV_ICON);
            gfxBitmap(51, y, 3, SUB_TWO);
        }

        y += 9;

        // Acceleration
        gfxIcon(1, y-1, GAUGE_ICON);
        gfxPrint(10, y, accel);
        const int accel_w = gfxGetPrintPosX() - 10;

        // Jitter
        gfxIcon(32, y - 1, RANDOM_ICON);
        gfxPrint(40, y, jitter);
        const int jitter_w = gfxGetPrintPosX() - 40;

        // Cursor
        switch (cursor) {
          case CLKPASSTHRU:
            gfxIcon(21, 13, LEFT_ICON);
            SetLabel(passthru == 1 ? "ClockThru" : passthru == 2 ? "BurstThru" : "BurstOnly");
            break;
          case RETRIG:
            gfxCursor(44, 30, 18, 9, "Retrig");
            break;
          case PROB:
            gfxCursor(40, 21, prob_w, 9, "Skip %");
            break;
          case NUMBER:
            gfxCursor(10, 30, number_w, 9, "Count");
            break;
          case GATE_PASSTHRU:
            gfxIcon(19, 31, LEFT_ICON);
            SetLabel("Gate+Thru");
            SetAux(false);
            break;
          case DIVISION:
            gfxCursor(37, 39, div_w, 9, "ClkDiv");
            break;
          case SPACING:
            gfxCursor(10, 48, spacing_w, 9, "Spacing");
            break;
          case ACCEL:
            gfxCursor(10, 57, accel_w, 9, "Accel");
            break;
          case JITTER:
            gfxCursor(40, 57, jitter_w, 9, "Jitter");
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
            effective_spacing = max(1, effective_spacing);
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
