// Copyright (c) 2024, _________
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

    const char* applet_name() {
        return "BitBeat";
    }

    void Start() {
        equation = 0;
        speed = 255;
        pitch = 1;
        p0 = 126;
        p1 = 126;
        p2 = 127;
        stepmode = false;
        loopmode = false;
        loopstart = 0;
        loopend = 255;
        cursor = EQUATION;
        frame_counter = 0;
        cv_dest[0] = 0xFF; // No assignment
        cv_dest[1] = 0xFF; // No assignment
        bytebeat.Init();
        ConfigureBytebeat();
    }

    void Controller() {
        uint8_t gate_state = 0;
        if (Gate(0)) gate_state |= peaks::CONTROL_GATE;
        
        // Detect rising edge
        if (Gate(0) && !prev_gate) gate_state |= peaks::CONTROL_GATE_RISING;
        
        // Detect falling edge
        if (!Gate(0) && prev_gate) gate_state |= peaks::CONTROL_GATE_FALLING;
        
        prev_gate = Gate(0);
        
        // Apply CV modulation to selected parameters
        uint8_t mod_equation = equation;
        uint8_t mod_speed = speed;
        uint8_t mod_pitch = pitch;
        uint8_t mod_p0 = p0;
        uint8_t mod_p1 = p1;
        uint8_t mod_p2 = p2;
        uint8_t mod_loopstart = loopstart;
        uint8_t mod_loopend = loopend;
        
        for (int cv_ch = 0; cv_ch < 2; cv_ch++) {
            if (cv_dest[cv_ch] > CURSOR_LAST) continue; // Skip if no assignment
            
            int cv_value = In(cv_ch);
            
            switch (cv_dest[cv_ch]) {
                case EQUATION:
                    mod_equation = constrain(equation + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
                    break;
                case SPEED:
                    mod_speed = constrain(speed + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
                    break;
                case PITCH:
                    mod_pitch = constrain(pitch + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
                    break;
                case PARAM0:
                    mod_p0 = constrain(p0 + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
                    break;
                case PARAM1:
                    mod_p1 = constrain(p1 + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
                    break;
                case PARAM2:
                    mod_p2 = constrain(p2 + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
                    break;
                case LOOPSTART:
                    mod_loopstart = constrain(loopstart + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
                    if (mod_loopstart > mod_loopend) mod_loopstart = mod_loopend;
                    break;
                case LOOPEND:
                    mod_loopend = constrain(loopend + Proportion(cv_value, HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
                    if (mod_loopend < mod_loopstart) mod_loopend = mod_loopstart;
                    break;
                default:
                    break;
            }
        }
        
        // Configure with modulated values
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
        
        bytebeat.Configure(parameters, stepmode, loopmode);
        
        // Process single sample and output with bipolar offset
        uint16_t sample = bytebeat.ProcessSingleSample(gate_state);
        Out(0, (sample - 32768) * HEMISPHERE_3V_CV / 32768); // Convert to bipolar output
        Out(1, In(1)); // Passthrough on second channel
    }

    void View() {
        DrawInterface();
    }
    
    void AuxButton() {
        if (cursor > CURSOR_LAST) return;
        
        // Check current assignments
        bool is_cv1 = (cv_dest[0] == cursor);
        bool is_cv2 = (cv_dest[1] == cursor);
        
        if (!is_cv1 && !is_cv2) {
            // Parameter not assigned yet
            if (cv_dest[0] == 0xFF) {
                // CV1 free → assign directly
                cv_dest[0] = cursor;
            } else if (cv_dest[1] == 0xFF) {
                // CV1 taken, CV2 free → move existing CV1 param to CV2, assign new param to CV1
                cv_dest[1] = cv_dest[0];
                cv_dest[0] = cursor;
            } else {
                // Both CV1 and CV2 occupied → rotate: CV1 param moves to CV2, CV2 param becomes unassigned
                uint8_t old_cv1_param = cv_dest[0];
                cv_dest[1] = old_cv1_param;      // move old CV1 assignment to CV2
                cv_dest[0] = cursor;             // assign new parameter to CV1
            }
        } else if (is_cv1 && !is_cv2) {
            // Currently CV1 - try to move to CV2
            if (cv_dest[1] == 0xFF) {
                // CV2 is free - move assignment from CV1 to CV2
                cv_dest[0] = 0xFF;
                cv_dest[1] = cursor;
            } else {
                // CV2 is taken - swap assignments
                uint8_t old_cv2_param = cv_dest[1];
                cv_dest[0] = old_cv2_param;
                cv_dest[1] = cursor;
            }
        } else if (!is_cv1 && is_cv2) {
            // Currently CV2 - clear it
            cv_dest[1] = 0xFF;
        } else {
            // Both CV1 and CV2 (shouldn't happen) - clear CV1
            cv_dest[0] = 0xFF;
        }
        
        SetHelp();
    }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            cursor = constrain(cursor + direction, 0, CURSOR_LAST);
            return;
        }

        // Edit parameter based on cursor
        switch (cursor) {
            case EQUATION:
                equation = constrain(equation + direction, 0, 15);
                break;
            case SPEED:
                speed = constrain(speed + direction, 0, 255);
                break;
            case PITCH:
                pitch = constrain(pitch + direction, 1, 255);
                break;
            case PARAM0:
                p0 = constrain(p0 + direction, 0, 255);
                break;
            case PARAM1:
                p1 = constrain(p1 + direction, 0, 255);
                break;
            case PARAM2:
                p2 = constrain(p2 + direction, 0, 255);
                break;
            case STEPMODE:
                stepmode = !stepmode;
                break;
            case LOOPMODE:
                loopmode = !loopmode;
                break;
            case LOOPSTART:
                loopstart = constrain(loopstart + direction, 0, 255);
                break;
            case LOOPEND:
                loopend = constrain(loopend + direction, 0, 255);
                break;
            default:
                break;
        }
        ConfigureBytebeat();
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,8}, equation);
        Pack(data, PackLocation {8,8}, speed);
        Pack(data, PackLocation {16,8}, pitch);
        Pack(data, PackLocation {24,8}, p0);
        Pack(data, PackLocation {32,8}, p1);
        Pack(data, PackLocation {40,8}, p2);
        Pack(data, PackLocation {48,1}, stepmode ? 1 : 0);
        Pack(data, PackLocation {49,1}, loopmode ? 1 : 0);
        Pack(data, PackLocation {50,8}, loopstart);
        Pack(data, PackLocation {58,8}, loopend);
        Pack(data, PackLocation {66,8}, cv_dest[0]);
        Pack(data, PackLocation {74,8}, cv_dest[1]);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        equation = Unpack(data, PackLocation {0,8});
        speed = Unpack(data, PackLocation {8,8});
        pitch = Unpack(data, PackLocation {16,8});
        p0 = Unpack(data, PackLocation {24,8});
        p1 = Unpack(data, PackLocation {32,8});
        p2 = Unpack(data, PackLocation {40,8});
        stepmode = Unpack(data, PackLocation {48,1}) > 0;
        loopmode = Unpack(data, PackLocation {49,1}) > 0;
        loopstart = Unpack(data, PackLocation {50,8});
        loopend = Unpack(data, PackLocation {58,8});
        cv_dest[0] = Unpack(data, PackLocation {66,8});
        cv_dest[1] = Unpack(data, PackLocation {74,8});
        ConfigureBytebeat();
        SetHelp();
    }

protected:
    void SetHelp() {
      help[HELP_DIGITAL1] = "Clock";
      help[HELP_DIGITAL2] = "Reset";
      
      const char* param_names[] = {
          "Equation", "Speed", "Pitch", "StepMode",
          "Param0", "Param1", "Param2", 
          "LoopMode", "LoopStart", "LoopEnd"
      };
      
      help[HELP_CV1] = (cv_dest[0] <= CURSOR_LAST) ? param_names[cv_dest[0]] : "--";
      help[HELP_CV2] = (cv_dest[1] <= CURSOR_LAST) ? param_names[cv_dest[1]] : "--";
      help[HELP_OUT1] = "ByteBeat";
      help[HELP_OUT2] = "Thru";
    }
    
private:
    peaks::ByteBeat bytebeat;
    int cursor;
    uint8_t equation;
    uint8_t speed;
    uint8_t pitch;
    uint8_t p0;
    uint8_t p1;
    uint8_t p2;
    bool stepmode;
    bool loopmode;
    uint8_t loopstart;
    uint8_t loopend;
    bool prev_gate;
    uint8_t frame_counter;
    uint8_t cv_dest[2];

    void ConfigureBytebeat() {
        int32_t parameters[12] = {0};
        parameters[0] = static_cast<int32_t>(equation) << 12;
        parameters[1] = static_cast<int32_t>(speed) << 8;
        parameters[2] = static_cast<int32_t>(p0) << 8;
        parameters[3] = static_cast<int32_t>(p1) << 8;
        parameters[4] = static_cast<int32_t>(p2) << 8;
        // Parameters 5-10 are loop parameters
        parameters[5] = 0;
        parameters[6] = 0; // Loop start medium (not used)
        parameters[7] = static_cast<int32_t>(loopstart) << 8;
        parameters[8] = 0;
        parameters[9] = 0; // Loop end medium (not used)
        parameters[10] = static_cast<int32_t>(loopend) << 8;
        parameters[11] = static_cast<int32_t>(pitch) << 8;
        
        bytebeat.Configure(parameters, stepmode, loopmode);
    }

    void DrawInterface() {
        frame_counter++;
        
        // Icons for each parameter
        const uint8_t* icons[] = {
            PhzIcons::trending,     // Equation (cursor = 0)
            CLOCK_ICON,             // Speed (cursor = 1)
            NOTE_ICON,              // Pitch (cursor = 2)
            STAIRS_ICON,            // Step mode (cursor = 3)
        };
        
        // Parameter values
        int values[] = {
            equation,
            speed,
            pitch,
        };
        
        // Draw main column parameters (equation, speed, pitch, stepmode)
        for (int i = 0; i < 4; i++) {
            int y = 12 + (i * 9);
            
            gfxIcon(1, y, icons[i]);
            
            if (i == 3) {
                gfxPrint(14, y, stepmode ? "Yes" : "No");
            } else {
                gfxPrint(14, y, values[i]);
            }
            
            if (cursor == i) gfxCursor(14, y + 8, 19);
            
            // Show CV assignment indicators
            for (int cv_idx = 0; cv_idx < 2; cv_idx++) {
                if (cv_dest[cv_idx] == i) {
                    gfxBitmap(10, y, 3, cv_idx ? SUB_TWO : SUP_ONE);
                }
            }
        }
        
        // Draw p0, p1, p2 and loop mode in right column
        int total_params = 4; // p0, p1, p2, LP
        
        for (int i = 0; i < total_params; i++) {
            int y = 12 + (i * 9);
            
            if (i < 3) {
                // Draw p0, p1, p2
                int param_idx = PARAM0 + i;
                int param_values[] = {p0, p1, p2};
                
                // Display parameter icon
                gfxIcon(34, y, PARAM_MAP_ICONS + 8 * (i + 1));
                
                // Display parameter value
                gfxPrint(44, y, param_values[i]);
                
                // Draw cursor if this parameter is selected
                if (cursor == param_idx) gfxCursor(46, y + 8, 19);
                
                // Show CV assignment indicators
                for (int cv_idx = 0; cv_idx < 2; cv_idx++) {
                    if (cv_dest[cv_idx] == param_idx) {
                        gfxBitmap(43, y, 3, cv_idx ? SUB_TWO : SUP_ONE);
                    }
                }
            } else {
                // Draw loop mode
                gfxIcon(34, y, LOOP_ICON);
                gfxPrint(45, y, loopmode ? "Yes" : "No");
                
                // Draw cursor if loop mode is selected
                if (cursor == LOOPMODE) gfxCursor(46, y + 8, 19);
                
                // Show CV assignment indicators
                for (int cv_idx = 0; cv_idx < 2; cv_idx++) {
                    if (cv_dest[cv_idx] == LOOPMODE) {
                        gfxBitmap(43, y, 3, cv_idx ? SUB_TWO : SUP_ONE);
                    }
                }
            }
        }
        
        // Draw loop range bar visualization at bottom
        const int BAR_Y = 57;
        const int BAR_HEIGHT = 4;
        const int BAR_X = 1;
        const int BAR_WIDTH = 62;
        
        // Draw background bar
        gfxFrame(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT);
        
        // Calculate start and end positions on the bar
        int start_x = BAR_X + (loopstart * (BAR_WIDTH - 2) / 255) + 1;
        int end_x = BAR_X + (loopend * (BAR_WIDTH - 2) / 255) + 1;
        
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
        gfxPrint(13, BAR_Y - 10, loopstart);
        if (cursor == LOOPSTART) gfxCursor(13, BAR_Y - 10 + 8, 19);
        
        gfxIcon(34, BAR_Y - 10, RIGHT_ICON);
        gfxPrint(44, BAR_Y - 10, loopend);
        if (cursor == LOOPEND) gfxCursor(44, BAR_Y - 10 + 8, 19);
        
        // CV assignment indicators for loop start/end
        for (int cv_idx = 0; cv_idx < 2; cv_idx++) {
            if (cv_dest[cv_idx] == LOOPSTART) {
                gfxBitmap(10, BAR_Y - 10, 3, cv_idx ? SUB_TWO : SUP_ONE);
            }
            if (cv_dest[cv_idx] == LOOPEND) {
                gfxBitmap(43, BAR_Y - 10, 3, cv_idx ? SUB_TWO : SUP_ONE);
            }
        }
        
        // Show aux button hint if a parameter is selected
        if (cursor <= CURSOR_LAST) {
            gfxBitmap(55, 1, 8, CV_ICON);
        }
    }
};
