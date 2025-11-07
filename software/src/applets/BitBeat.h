// Copyright (c) 2025, Eric Gao/oksami
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

#include "../peaks_bytebeat.h"
#include "../util/util_history.h"

class BitBeat : public HemisphereApplet {
public:

    enum BitBeatCursor {
        EQUATION,
        SPEED,
        PITCH,
        STEPMODE,
        PARAM0,
        PARAM1,
        PARAM2,
        LOOPMODE,
        LOOPSTART,
        LOOPEND,

        CURSOR_LAST = LOOPEND
    };
    const char* const param_names[CURSOR_LAST + 1] = {
        "Eq", "Speed", "Pitch", "Step",
        "P0", "P1", "P2",
        "Loop", "Start", "End"
    };

    static constexpr size_t kHistoryDepth = 64;

    const char* applet_name() {
        return "BitBeat";
    }

    void Start() {
        current_page = 0;
        cursor = EQUATION;
        frame_counter = 0;

        ForEachChannel(ch) {
          equation[ch] = ch;
          speed[ch] = 255;
          pitch[ch] = 1;
          p0[ch] = 126;
          p1[ch] = 126;
          p2[ch] = 127;
          stepmode[ch] = false;
          loopmode[ch] = false;
          loopstart[ch] = 0;
          loopend[ch] = 255;

          // Initialize CV assignments (all false)
          for (int i = 0; i <= CURSOR_LAST; i++) {
              cv_assignments[0][i][ch] = false;
              cv_assignments[1][i][ch] = false;
          }

          bytebeat[ch].Init();
          //history_[ch].Init(0);
          ConfigureBytebeat(ch);
        }
    }

    void Controller() {
        ForEachChannel(ch) {
            // Process Algorithm (outputs to corresponding channel)
            ProcessAlgorithm(ch, Clock(ch)? peaks::CONTROL_GATE_RISING : 0);
        }
    }

    void View() {
        DrawInterface();
    }

    void AuxButton() {
        if (cursor > CURSOR_LAST) return;

        // Check current assignments for this parameter
        bool is_cv1 = cv_assignments[current_page][cursor][0];
        bool is_cv2 = cv_assignments[current_page][cursor][1];

        // Cycle through assignment states: None -> CV1 -> CV2 -> None
        if (!is_cv1 && !is_cv2) {
            // None -> CV1
            cv_assignments[current_page][cursor][0] = true;
            cv_assignments[current_page][cursor][1] = false;
        } else if (is_cv1 && !is_cv2) {
            // CV1 -> CV2
            cv_assignments[current_page][cursor][0] = false;
            cv_assignments[current_page][cursor][1] = true;
        } else {
            // CV2 (or Both) -> None
            cv_assignments[current_page][cursor][0] = false;
            cv_assignments[current_page][cursor][1] = false;
        }
    }

    void OnButtonPress() override {
      if (current_page > 1) {
        // ZAP MODE!!!
        current_page = 0;
        cursor = 0;
        ForEachChannel(ch) {
          equation[ch] = random(16);
          speed[ch] = random(256);
          pitch[ch] = random(256);
          p0[ch] = random(256);
          p1[ch] = random(256);
          p2[ch] = random(256);
          loopstart[ch] = random(256);
          loopend[ch] = random(256);
          if (loopstart[ch] > loopend[ch]) {
            loopstart[ch] = loopend[ch];
          }
          loopmode[ch] = random(2);
          stepmode[ch] = random(2);
        }
      } else if (cursor == STEPMODE) {
        stepmode[current_page] ^= 1;
      } else if (cursor == LOOPMODE) {
        loopmode[current_page] ^= 1;
      } else
        CursorToggle();
    }
    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            // Handle cursor navigation and page switching
            int new_cursor = cursor + direction;
            if (new_cursor > CURSOR_LAST) {
                // Move to next page if not already on last page
                if (current_page == 0) {
                    current_page = 1;
                    cursor = 0;
                } else {
                    // Already on page B, stay at last parameter
                    current_page++; // ZAP MODE
                }
            } else if (new_cursor < 0) {
                // Move to previous page if not already on first page
                if (current_page == 1) {
                    current_page = 0;
                    cursor = CURSOR_LAST;
                } else {
                    // Already on page A, stay at first parameter
                }
            } else {
                if (current_page > 1) current_page = 1;
                cursor = new_cursor;
            }
            ResetCursor();
            return;
        }

        // Edit parameter based on cursor and current page
        EditParameter(current_page, direction);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,4}, equation[0]);
        Pack(data, PackLocation {4,4}, equation[1]);
        Pack(data, PackLocation {8,8}, speed[0]);
        Pack(data, PackLocation {16,8}, pitch[0]);
        Pack(data, PackLocation {24,8}, p0[0]);
        Pack(data, PackLocation {32,8}, p1[0]);
        Pack(data, PackLocation {40,8}, p2[0]);
        Pack(data, PackLocation {48,1}, stepmode[0]);
        Pack(data, PackLocation {49,1}, loopmode[0]);
        Pack(data, PackLocation {50,7}, loopstart[0] >> 1);
        Pack(data, PackLocation {57,7}, loopend[0] >> 1);
        // Note: CV assignments are not persisted due to space constraints
        return data;
    }

    void OnDataReceive(uint64_t data) {
        equation[0] = Unpack(data, PackLocation {0,4});
        equation[1] = Unpack(data, PackLocation {4,4});
        speed[0] = Unpack(data, PackLocation {8,8});
        pitch[0] = Unpack(data, PackLocation {16,8});
        p0[0] = Unpack(data, PackLocation {24,8});
        p1[0] = Unpack(data, PackLocation {32,8});
        p2[0] = Unpack(data, PackLocation {40,8});
        stepmode[0] = Unpack(data, PackLocation {48,1});
        loopmode[0] = Unpack(data, PackLocation {49,1});
        loopstart[0] = Unpack(data, PackLocation {50,7}) << 1;
        loopend[0] = Unpack(data, PackLocation {57,7}) << 1;
        ConfigureBytebeat(0);
        ConfigureBytebeat(1);
    }

protected:
    void SetHelp() {
      help[HELP_DIGITAL1] = "Reset1";
      help[HELP_DIGITAL2] = "Reset2";

      ForEachChannel(ch) {
        // Check CV assignments across both pages
        int cv_count = 0;
        int cv_single_param = -1;
        for (int i = 0; i <= CURSOR_LAST; i++) {
            if (cv_assignments[0][i][ch] || cv_assignments[1][i][ch]) {
                cv_count++;
                if (cv_count == 1) cv_single_param = i;
            }
        }

        if (cv_count == 0) {
            help[HELP_CV1 + ch] = "--";
        } else if (cv_count == 1) {
            help[HELP_CV1 + ch] = param_names[cv_single_param];
        } else {
            help[HELP_CV1 + ch] = "Multi";
        }
      }

      help[HELP_OUT1] = "Beat1";
      help[HELP_OUT2] = "Beat2";
    }

private:
    peaks::ByteBeat bytebeat[2];

    int current_page; // 0 or 1
    int cursor;

    // Algorithm state for each algorithm
    uint8_t equation[2];
    uint8_t speed[2];
    uint8_t pitch[2];
    uint8_t p0[2];
    uint8_t p1[2];
    uint8_t p2[2];
    uint8_t loopstart[2];
    uint8_t loopend[2];
    bool stepmode[2];
    bool loopmode[2];

    bool prev_gate[2];
    uint8_t frame_counter;
    uint8_t last_sample[2]; // Add history tracking

    // TODO: this could be a bitmask
    bool cv_assignments[2][CURSOR_LAST + 1][2]; // [page][param][cv]

    //util::History<uint8_t, kHistoryDepth> history_[2]; // Add history buffers for both algorithms

    void ProcessAlgorithm(int alg_idx, uint8_t gate_state) {
        // Apply CV modulation to algorithm parameters
        uint8_t mod_equation = equation[alg_idx];
        uint8_t mod_speed = speed[alg_idx];
        uint8_t mod_pitch = pitch[alg_idx];
        uint8_t mod_p0 = p0[alg_idx];
        uint8_t mod_p1 = p1[alg_idx];
        uint8_t mod_p2 = p2[alg_idx];
        uint8_t mod_loopstart = loopstart[alg_idx];
        uint8_t mod_loopend = loopend[alg_idx];

        // Apply CV1 modulation
        if (cv_assignments[alg_idx][EQUATION][0]) {
            mod_equation = constrain(equation[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
        }
        if (cv_assignments[alg_idx][SPEED][0]) {
            mod_speed = constrain(speed[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][PITCH][0]) {
            mod_pitch = constrain(pitch[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
        }
        if (cv_assignments[alg_idx][PARAM0][0]) {
            mod_p0 = constrain(p0[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][PARAM1][0]) {
            mod_p1 = constrain(p1[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][PARAM2][0]) {
            mod_p2 = constrain(p2[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][LOOPSTART][0]) {
            mod_loopstart = constrain(loopstart[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][LOOPEND][0]) {
            mod_loopend = constrain(loopend[alg_idx] + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }

        // Apply CV2 modulation
        if (cv_assignments[alg_idx][EQUATION][1]) {
            mod_equation = constrain(mod_equation + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
        }
        if (cv_assignments[alg_idx][SPEED][1]) {
            mod_speed = constrain(mod_speed + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][PITCH][1]) {
            mod_pitch = constrain(mod_pitch + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
        }
        if (cv_assignments[alg_idx][PARAM0][1]) {
            mod_p0 = constrain(mod_p0 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][PARAM1][1]) {
            mod_p1 = constrain(mod_p1 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][PARAM2][1]) {
            mod_p2 = constrain(mod_p2 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][LOOPSTART][1]) {
            mod_loopstart = constrain(mod_loopstart + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv_assignments[alg_idx][LOOPEND][1]) {
            mod_loopend = constrain(mod_loopend + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }

        // Ensure loop start <= loop end after all CV modulation
        if (mod_loopstart > mod_loopend) {
            mod_loopstart = mod_loopend;
        }

        // Configure algorithm with modulated values
        int32_t parameters[12] = {0};
        parameters[0] = static_cast<int32_t>(mod_equation) << 12;
        parameters[1] = static_cast<int32_t>(mod_speed) << 8;
        parameters[2] = static_cast<int32_t>(mod_p0) << 8;
        parameters[3] = static_cast<int32_t>(mod_p1) << 8;
        parameters[4] = static_cast<int32_t>(mod_p2) << 8;
        parameters[5] = 0;
        parameters[6] = 0;
        parameters[7] = static_cast<int32_t>(mod_loopstart) << 8;
        parameters[8] = 0;
        parameters[9] = 0;
        parameters[10] = static_cast<int32_t>(mod_loopend) << 8;
        parameters[11] = static_cast<int32_t>(mod_pitch) << 8;

        bytebeat[alg_idx].Configure(parameters, stepmode[alg_idx], loopmode[alg_idx]);

        // Process single sample and output to corresponding channel
        uint16_t sample = bytebeat[alg_idx].ProcessSingleSample(gate_state);

        // Convert to bipolar output and apply history tracking
        uint8_t processed_sample = sample >> 8;
        if (processed_sample != last_sample[alg_idx]) {
            last_sample[alg_idx] = processed_sample;
            Out(alg_idx, (sample - 32768) * HEMISPHERE_3V_CV / 32768);
            //history_[alg_idx].Push(processed_sample); // Add to history when sample changes
        }
    }

    void EditParameter(int page, int direction) {
        switch (cursor) {
            case EQUATION:
                equation[page] = constrain(equation[page] + direction, 0, 15);
                break;
            case SPEED:
                speed[page] = constrain(speed[page] + direction, 0, 255);
                break;
            case PITCH:
                pitch[page] = constrain(pitch[page] + direction, 1, 255);
                break;
            case PARAM0:
                p0[page] = constrain(p0[page] + direction, 0, 255);
                break;
            case PARAM1:
                p1[page] = constrain(p1[page] + direction, 0, 255);
                break;
            case PARAM2:
                p2[page] = constrain(p2[page] + direction, 0, 255);
                break;
            case STEPMODE:
                stepmode[page] = !stepmode[page];
                break;
            case LOOPMODE:
                loopmode[page] = !loopmode[page];
                break;
            case LOOPSTART:
                loopstart[page] = constrain(loopstart[page] + direction, 0, loopend[page]);
                break;
            case LOOPEND:
                loopend[page] = constrain(loopend[page] + direction, loopstart[page], 255);
                break;
            default:
                break;
        }
        ConfigureBytebeat(page);
    }

    void ConfigureBytebeat(int alg_idx) {
        int32_t parameters[12] = {0};
        parameters[0] = static_cast<int32_t>(equation[alg_idx]) << 12;
        parameters[1] = static_cast<int32_t>(speed[alg_idx]) << 8;
        parameters[2] = static_cast<int32_t>(p0[alg_idx]) << 8;
        parameters[3] = static_cast<int32_t>(p1[alg_idx]) << 8;
        parameters[4] = static_cast<int32_t>(p2[alg_idx]) << 8;
        // Parameters 5-10 are loop parameters
        parameters[5] = 0;
        parameters[6] = 0; // Loop start medium (not used)
        parameters[7] = static_cast<int32_t>(loopstart[alg_idx]) << 8;
        parameters[8] = 0;
        parameters[9] = 0; // Loop end medium (not used)
        parameters[10] = static_cast<int32_t>(loopend[alg_idx]) << 8;
        parameters[11] = static_cast<int32_t>(pitch[alg_idx]) << 8;

        bytebeat[alg_idx].Configure(parameters, stepmode[alg_idx], loopmode[alg_idx]);
    }

    void DrawInterface() {
        frame_counter++;

        if (current_page > 1) {
          static int posx = random(56);
          static int posy = random(48);
          static bool blinker = 0;
          if (!blinker && CursorBlink()) {
            posx = random(56);
            posy = random(48);
          }
          blinker = CursorBlink();
          gfxIcon(posx, posy+8, ZAP_ICON);
          if (blinker) {
            gfxIcon(posx, posy, DOWN_BTN_ICON);
            gfxIcon(posx, posy+16, UP_BTN_ICON);
          }
          return;
        }
        const int chan = (hemisphere*2 + current_page);
        gfxPrint((hemisphere & 1)?3:52, 2, OC::Strings::capital_letters[chan]);

        // Icons for each parameter
        const uint8_t* icons[] = {
            PhzIcons::trending,     // Equation (cursor = 0)
            CLOCK_ICON,             // Speed (cursor = 1)
            NOTE_ICON,              // Pitch (cursor = 2)
            STAIRS_ICON,            // Step mode (cursor = 3)
        };

        // Parameter values
        const int values[] = {
            equation[current_page],
            speed[current_page],
            pitch[current_page],
        };

        // Draw main column parameters (equation, speed, pitch, stepmode)
        for (int i = 0; i < 4; i++) {
            int y = 12 + (i * 9);

            gfxIcon(1, y, icons[i]);

            if (i == 3) {
                gfxPrint(14, y, OC::Strings::no_yes[stepmode[current_page]]);
            } else {
                gfxPrint(14, y, values[i]);
            }

            if (cursor == i) gfxCursor(14, y + 8, 19);

            // Show CV assignment indicators
            if (cv_assignments[current_page][i][0]) {
                gfxBitmap(10, y, 3, SUP_ONE);
            }
            if (cv_assignments[current_page][i][1]) {
                gfxBitmap(10, y, 3, SUB_TWO);
            }
        }

        // Draw p0, p1, p2 and loop mode in right column
        int total_params = 4; // p0, p1, p2, LP

        for (int i = 0; i < total_params; i++) {
            int y = 12 + (i * 9);

            if (i < 3) {
                // Draw p0, p1, p2
                int param_idx = PARAM0 + i;
                int param_values[] = {p0[current_page], p1[current_page], p2[current_page]};

                // Display parameter icon
                gfxIcon(34, y, PARAM_MAP_ICONS + 8 * (i + 1));

                // Display parameter value
                gfxPrint(44, y, param_values[i]);

                // Draw cursor if this parameter is selected
                if (cursor == param_idx) gfxCursor(46, y + 8, 18);

                // Show CV assignment indicators
                if (cv_assignments[current_page][param_idx][0]) {
                    gfxBitmap(43, y, 3, SUP_ONE);
                }
                if (cv_assignments[current_page][param_idx][1]) {
                    gfxBitmap(43, y, 3, SUB_TWO);
                }
            } else {
                // Draw loop mode
                gfxIcon(34, y, LOOP_ICON);
                gfxPrint(45, y, OC::Strings::no_yes[loopmode[current_page]]);

                // Draw cursor if loop mode is selected
                if (cursor == LOOPMODE) gfxCursor(46, y + 8, 18);

                // Show CV assignment indicators
                if (cv_assignments[current_page][LOOPMODE][0]) {
                    gfxBitmap(43, y, 3, SUP_ONE);
                }
                if (cv_assignments[current_page][LOOPMODE][1]) {
                    gfxBitmap(43, y, 3, SUB_TWO);
                }
            }
        }

        // Draw loop range bar visualization at bottom
        const int BAR_Y = 57;
        const int BAR_HEIGHT = 4;
        const int BAR_X = 1;
        const int BAR_WIDTH = 61;

        // Draw background bar
        gfxFrame(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT);

        // Calculate start and end positions on the bar
        int start_x = BAR_X + (loopstart[current_page] * (BAR_WIDTH - 2) / 255) + 1;
        int end_x = BAR_X + (loopend[current_page] * (BAR_WIDTH - 2) / 255) + 1;

        // Draw filled section between start and end
        for (int x = start_x; x <= end_x; x++) {
            gfxLine(x, BAR_Y + 1, x, BAR_Y + BAR_HEIGHT - 2);
        }

        // Draw start and end markers (thicker for selected one)
        bool blink_on = (frame_counter & 0x40);

        if (cursor == LOOPSTART) {
            // Regular end marker
            gfxLine(end_x, BAR_Y - 1, end_x, BAR_Y + BAR_HEIGHT);

            if (EditMode() || blink_on) {
                // Highlighted start marker
                gfxRect(start_x - 1, BAR_Y - 1, 3, BAR_HEIGHT + 2);
            } else {
                // Regular start marker when blinking off
                gfxLine(start_x, BAR_Y - 1, start_x, BAR_Y + BAR_HEIGHT);
            }
        } else if (cursor == LOOPEND) {
            // Regular start marker
            gfxLine(start_x, BAR_Y - 1, start_x, BAR_Y + BAR_HEIGHT);

            if (EditMode() || blink_on) {
                // Highlighted end marker
                gfxRect(end_x - 1, BAR_Y - 1, 3, BAR_HEIGHT + 2);
            } else {
                // Regular end marker when blinking off
                gfxLine(end_x, BAR_Y - 1, end_x, BAR_Y + BAR_HEIGHT);
            }
        } else {
            // Regular markers for both
            gfxLine(start_x, BAR_Y - 1, start_x, BAR_Y + BAR_HEIGHT);
            gfxLine(end_x, BAR_Y - 1, end_x, BAR_Y + BAR_HEIGHT);
        }

        // Print loop start/end values and draw cursors
        gfxIcon(1, BAR_Y - 10, LEFT_ICON);
        gfxPrint(13, BAR_Y - 10, loopstart[current_page]);
        if (cursor == LOOPSTART) gfxCursor(13, BAR_Y - 10 + 8, 19);

        gfxIcon(34, BAR_Y - 10, RIGHT_ICON);
        gfxPrint(44, BAR_Y - 10, loopend[current_page]);
        if (cursor == LOOPEND) gfxCursor(45, BAR_Y - 10 + 8, 19);

        // CV assignment indicators for loop start/end
        if (cv_assignments[current_page][LOOPSTART][0]) {
            gfxBitmap(10, BAR_Y - 10, 3, SUP_ONE);
        }
        if (cv_assignments[current_page][LOOPSTART][1]) {
            gfxBitmap(10, BAR_Y - 10, 3, SUB_TWO);
        }
        if (cv_assignments[current_page][LOOPEND][0]) {
            gfxBitmap(42, BAR_Y - 10, 3, SUP_ONE);
        }
        if (cv_assignments[current_page][LOOPEND][1]) {
            gfxBitmap(42, BAR_Y - 10, 3, SUB_TWO);
        }
    }

    /*
    inline void ReadHistory(uint8_t *history, int alg_idx) const {
        history_[alg_idx].Read(history);
    }
    */
};
