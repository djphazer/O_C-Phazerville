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
        // Algorithm 1 (page 0) state
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
        
        // Algorithm 2 (page 1) state
        equation2 = 0;
        speed2 = 255;
        pitch2 = 1;
        p0_2 = 126;
        p1_2 = 126;
        p2_2 = 127;
        stepmode2 = false;
        loopmode2 = false;
        loopstart2 = 0;
        loopend2 = 255;
        
        current_page = 0;
        cursor = EQUATION;
        frame_counter = 0;
        
        // Initialize CV assignments (all false)
        for (int i = 0; i <= CURSOR_LAST; i++) {
            cv1_assignments[i] = false;
            cv2_assignments[i] = false;
            cv1_assignments2[i] = false;
            cv2_assignments2[i] = false;
        }
        
        bytebeat.Init();
        bytebeat2.Init();
        ConfigureBytebeat();
        ConfigureBytebeat2();
    }

    void Controller() {
        uint8_t gate_state = 0;
        if (Gate(0)) gate_state |= peaks::CONTROL_GATE;
        
        // Detect rising edge
        if (Gate(0) && !prev_gate) gate_state |= peaks::CONTROL_GATE_RISING;
        
        // Detect falling edge
        if (!Gate(0) && prev_gate) gate_state |= peaks::CONTROL_GATE_FALLING;
        
        prev_gate = Gate(0);
        
        // Process Algorithm 1 (always active, outputs to channel 1)
        ProcessAlgorithm1(gate_state);
        
        // Process Algorithm 2 (always active, outputs to channel 2)
        ProcessAlgorithm2(gate_state);
    }

    void View() {
        DrawInterface();
    }
    
    void AuxButton() {
        if (cursor > CURSOR_LAST) return;
        
        // Get current page's CV assignment arrays
        bool* cv1_assign = (current_page == 0) ? cv1_assignments : cv1_assignments2;
        bool* cv2_assign = (current_page == 0) ? cv2_assignments : cv2_assignments2;
        
        // Check current assignments for this parameter
        bool is_cv1 = cv1_assign[cursor];
        bool is_cv2 = cv2_assign[cursor];
        
        // Cycle through assignment states: None -> CV1 -> CV2 -> None
        if (!is_cv1 && !is_cv2) {
            // None -> CV1
            cv1_assign[cursor] = true;
            cv2_assign[cursor] = false;
        } else if (is_cv1 && !is_cv2) {
            // CV1 -> CV2
            cv1_assign[cursor] = false;
            cv2_assign[cursor] = true;
        } else {
            // CV2 (or Both) -> None
            cv1_assign[cursor] = false;
            cv2_assign[cursor] = false;
        }
        
        SetHelp();
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
                    cursor = CURSOR_LAST;
                }
            } else if (new_cursor < 0) {
                // Move to previous page if not already on first page
                if (current_page == 1) {
                    current_page = 0;
                    cursor = CURSOR_LAST;
                } else {
                    // Already on page A, stay at first parameter
                    cursor = 0;
                }
            } else {
                cursor = new_cursor;
            }
            return;
        }

        // Edit parameter based on cursor and current page
        if (current_page == 0) {
            EditPage1Parameter(direction);
        } else {
            EditPage2Parameter(direction);
        }
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
        // Note: CV assignments are not persisted due to space constraints
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
        ConfigureBytebeat();
        ConfigureBytebeat2();
        SetHelp();
    }

protected:
    void SetHelp() {
      help[HELP_DIGITAL1] = "Clock";
      help[HELP_DIGITAL2] = "Reset";
      
      const char* param_names[] = {
          "Eq", "Speed", "Pitch", "Step",
          "P0", "P1", "P2", 
          "Loop", "Start", "End"
      };
      
      // Check CV1 assignments across both pages
      int cv1_count = 0;
      int cv1_single_param = -1;
      for (int i = 0; i <= CURSOR_LAST; i++) {
          if (cv1_assignments[i] || cv1_assignments2[i]) {
              cv1_count++;
              if (cv1_count == 1) cv1_single_param = i;
          }
      }
      
      if (cv1_count == 0) {
          help[HELP_CV1] = "--";
      } else if (cv1_count == 1) {
          help[HELP_CV1] = param_names[cv1_single_param];
      } else {
          help[HELP_CV1] = "Multi";
      }
      
      // Check CV2 assignments across both pages
      int cv2_count = 0;
      int cv2_single_param = -1;
      for (int i = 0; i <= CURSOR_LAST; i++) {
          if (cv2_assignments[i] || cv2_assignments2[i]) {
              cv2_count++;
              if (cv2_count == 1) cv2_single_param = i;
          }
      }
      
      if (cv2_count == 0) {
          help[HELP_CV2] = "--";
      } else if (cv2_count == 1) {
          help[HELP_CV2] = param_names[cv2_single_param];
      } else {
          help[HELP_CV2] = "Multi";
      }
      
      help[HELP_OUT1] = "Beat1";
      help[HELP_OUT2] = "Beat2";
    }
    
private:
    peaks::ByteBeat bytebeat;
    peaks::ByteBeat bytebeat2;
    
    int current_page; // 0 or 1
    int cursor;
    
    // Algorithm 1 state (page 0)
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
    
    // Algorithm 2 state (page 1)
    uint8_t equation2;
    uint8_t speed2;
    uint8_t pitch2;
    uint8_t p0_2;
    uint8_t p1_2;
    uint8_t p2_2;
    bool stepmode2;
    bool loopmode2;
    uint8_t loopstart2;
    uint8_t loopend2;
    
    bool prev_gate;
    uint8_t frame_counter;

    bool cv1_assignments[CURSOR_LAST + 1];
    bool cv2_assignments[CURSOR_LAST + 1];
    bool cv1_assignments2[CURSOR_LAST + 1];
    bool cv2_assignments2[CURSOR_LAST + 1];

    void ProcessAlgorithm1(uint8_t gate_state) {
        // Apply CV modulation to algorithm 1 parameters
        uint8_t mod_equation = equation;
        uint8_t mod_speed = speed;
        uint8_t mod_pitch = pitch;
        uint8_t mod_p0 = p0;
        uint8_t mod_p1 = p1;
        uint8_t mod_p2 = p2;
        uint8_t mod_loopstart = loopstart;
        uint8_t mod_loopend = loopend;
        
        // Apply CV1 modulation
        if (cv1_assignments[EQUATION]) {
            mod_equation = constrain(equation + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
        }
        if (cv1_assignments[SPEED]) {
            mod_speed = constrain(speed + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments[PITCH]) {
            mod_pitch = constrain(pitch + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
        }
        if (cv1_assignments[PARAM0]) {
            mod_p0 = constrain(p0 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments[PARAM1]) {
            mod_p1 = constrain(p1 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments[PARAM2]) {
            mod_p2 = constrain(p2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments[LOOPSTART]) {
            mod_loopstart = constrain(loopstart + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments[LOOPEND]) {
            mod_loopend = constrain(loopend + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        
        // Apply CV2 modulation
        if (cv2_assignments[EQUATION]) {
            mod_equation = constrain(mod_equation + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
        }
        if (cv2_assignments[SPEED]) {
            mod_speed = constrain(mod_speed + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments[PITCH]) {
            mod_pitch = constrain(mod_pitch + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
        }
        if (cv2_assignments[PARAM0]) {
            mod_p0 = constrain(mod_p0 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments[PARAM1]) {
            mod_p1 = constrain(mod_p1 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments[PARAM2]) {
            mod_p2 = constrain(mod_p2 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments[LOOPSTART]) {
            mod_loopstart = constrain(mod_loopstart + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments[LOOPEND]) {
            mod_loopend = constrain(mod_loopend + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        
        // Ensure loop start <= loop end after all CV modulation
        if (mod_loopstart > mod_loopend) {
            mod_loopstart = mod_loopend;
        }
        
        // Configure algorithm 1 with modulated values
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
        
        // Process single sample and output to channel 1
        uint16_t sample = bytebeat.ProcessSingleSample(gate_state);
        Out(0, (sample - 32768) * HEMISPHERE_3V_CV / 32768); // Convert to bipolar output
    }
    
    void ProcessAlgorithm2(uint8_t gate_state) {
        // Apply CV modulation to algorithm 2 parameters
        uint8_t mod_equation = equation2;
        uint8_t mod_speed = speed2;
        uint8_t mod_pitch = pitch2;
        uint8_t mod_p0 = p0_2;
        uint8_t mod_p1 = p1_2;
        uint8_t mod_p2 = p2_2;
        uint8_t mod_loopstart = loopstart2;
        uint8_t mod_loopend = loopend2;
        
        // Apply CV1 modulation
        if (cv1_assignments2[EQUATION]) {
            mod_equation = constrain(equation2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
        }
        if (cv1_assignments2[SPEED]) {
            mod_speed = constrain(speed2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments2[PITCH]) {
            mod_pitch = constrain(pitch2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
        }
        if (cv1_assignments2[PARAM0]) {
            mod_p0 = constrain(p0_2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments2[PARAM1]) {
            mod_p1 = constrain(p1_2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments2[PARAM2]) {
            mod_p2 = constrain(p2_2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments2[LOOPSTART]) {
            mod_loopstart = constrain(loopstart2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv1_assignments2[LOOPEND]) {
            mod_loopend = constrain(loopend2 + Proportion(In(0), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        
        // Apply CV2 modulation
        if (cv2_assignments2[EQUATION]) {
            mod_equation = constrain(mod_equation + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 15), 0, 15);
        }
        if (cv2_assignments2[SPEED]) {
            mod_speed = constrain(mod_speed + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments2[PITCH]) {
            mod_pitch = constrain(mod_pitch + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 1, 255);
        }
        if (cv2_assignments2[PARAM0]) {
            mod_p0 = constrain(mod_p0 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments2[PARAM1]) {
            mod_p1 = constrain(mod_p1 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments2[PARAM2]) {
            mod_p2 = constrain(mod_p2 + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments2[LOOPSTART]) {
            mod_loopstart = constrain(mod_loopstart + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        if (cv2_assignments2[LOOPEND]) {
            mod_loopend = constrain(mod_loopend + Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, 255), 0, 255);
        }
        
        // Ensure loop start <= loop end after all CV modulation
        if (mod_loopstart > mod_loopend) {
            mod_loopstart = mod_loopend;
        }
        
        // Configure algorithm 2 with modulated values
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
        
        bytebeat2.Configure(parameters, stepmode2, loopmode2);
        
        // Process single sample and output to channel 2
        uint16_t sample = bytebeat2.ProcessSingleSample(gate_state);
        Out(1, (sample - 32768) * HEMISPHERE_3V_CV / 32768); // Convert to bipolar output
    }

    void EditPage1Parameter(int direction) {
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
                loopstart = constrain(loopstart + direction, 0, loopend);
                break;
            case LOOPEND:
                loopend = constrain(loopend + direction, loopstart, 255);
                break;
            default:
                break;
        }
        ConfigureBytebeat();
    }
    
    void EditPage2Parameter(int direction) {
        switch (cursor) {
            case EQUATION:
                equation2 = constrain(equation2 + direction, 0, 15);
                break;
            case SPEED:
                speed2 = constrain(speed2 + direction, 0, 255);
                break;
            case PITCH:
                pitch2 = constrain(pitch2 + direction, 1, 255);
                break;
            case PARAM0:
                p0_2 = constrain(p0_2 + direction, 0, 255);
                break;
            case PARAM1:
                p1_2 = constrain(p1_2 + direction, 0, 255);
                break;
            case PARAM2:
                p2_2 = constrain(p2_2 + direction, 0, 255);
                break;
            case STEPMODE:
                stepmode2 = !stepmode2;
                break;
            case LOOPMODE:
                loopmode2 = !loopmode2;
                break;
            case LOOPSTART:
                loopstart2 = constrain(loopstart2 + direction, 0, loopend2);
                break;
            case LOOPEND:
                loopend2 = constrain(loopend2 + direction, loopstart2, 255);
                break;
            default:
                break;
        }
        ConfigureBytebeat2();
    }

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
    
    void ConfigureBytebeat2() {
        int32_t parameters[12] = {0};
        parameters[0] = static_cast<int32_t>(equation2) << 12;
        parameters[1] = static_cast<int32_t>(speed2) << 8;
        parameters[2] = static_cast<int32_t>(p0_2) << 8;
        parameters[3] = static_cast<int32_t>(p1_2) << 8;
        parameters[4] = static_cast<int32_t>(p2_2) << 8;
        // Parameters 5-10 are loop parameters
        parameters[5] = 0;
        parameters[6] = 0; // Loop start medium (not used)
        parameters[7] = static_cast<int32_t>(loopstart2) << 8;
        parameters[8] = 0;
        parameters[9] = 0; // Loop end medium (not used)
        parameters[10] = static_cast<int32_t>(loopend2) << 8;
        parameters[11] = static_cast<int32_t>(pitch2) << 8;
        
        bytebeat2.Configure(parameters, stepmode2, loopmode2);
    }

    void DrawInterface() {
        frame_counter++;
        
        // Draw page indicator
        gfxPrint(48, 2, current_page == 0 ? "A" : "B");
        
        // Get current page parameters
        uint8_t current_equation = (current_page == 0) ? equation : equation2;
        uint8_t current_speed = (current_page == 0) ? speed : speed2;
        uint8_t current_pitch = (current_page == 0) ? pitch : pitch2;
        uint8_t current_p0 = (current_page == 0) ? p0 : p0_2;
        uint8_t current_p1 = (current_page == 0) ? p1 : p1_2;
        uint8_t current_p2 = (current_page == 0) ? p2 : p2_2;
        bool current_stepmode = (current_page == 0) ? stepmode : stepmode2;
        bool current_loopmode = (current_page == 0) ? loopmode : loopmode2;
        uint8_t current_loopstart = (current_page == 0) ? loopstart : loopstart2;
        uint8_t current_loopend = (current_page == 0) ? loopend : loopend2;
        
        // Icons for each parameter
        const uint8_t* icons[] = {
            PhzIcons::trending,     // Equation (cursor = 0)
            CLOCK_ICON,             // Speed (cursor = 1)
            NOTE_ICON,              // Pitch (cursor = 2)
            STAIRS_ICON,            // Step mode (cursor = 3)
        };
        
        // Parameter values
        int values[] = {
            current_equation,
            current_speed,
            current_pitch,
        };
        
        // Draw main column parameters (equation, speed, pitch, stepmode)
        for (int i = 0; i < 4; i++) {
            int y = 12 + (i * 9);
            
            gfxIcon(1, y, icons[i]);
            
            if (i == 3) {
                gfxPrint(14, y, current_stepmode ? "Yes" : "No");
            } else {
                gfxPrint(14, y, values[i]);
            }
            
            if (cursor == i) gfxCursor(14, y + 8, 19);
            
            // Show CV assignment indicators
            bool* cv1_assign = (current_page == 0) ? cv1_assignments : cv1_assignments2;
            bool* cv2_assign = (current_page == 0) ? cv2_assignments : cv2_assignments2;
            
            if (cv1_assign[i]) {
                gfxBitmap(10, y, 3, SUP_ONE);
            }
            if (cv2_assign[i]) {
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
                int param_values[] = {current_p0, current_p1, current_p2};
                
                // Display parameter icon
                gfxIcon(34, y, PARAM_MAP_ICONS + 8 * (i + 1));
                
                // Display parameter value
                gfxPrint(44, y, param_values[i]);
                
                // Draw cursor if this parameter is selected
                if (cursor == param_idx) gfxCursor(46, y + 8, 19);
                
                // Show CV assignment indicators
                bool* cv1_assign = (current_page == 0) ? cv1_assignments : cv1_assignments2;
                bool* cv2_assign = (current_page == 0) ? cv2_assignments : cv2_assignments2;
                
                if (cv1_assign[param_idx]) {
                    gfxBitmap(43, y, 3, SUP_ONE);
                }
                if (cv2_assign[param_idx]) {
                    gfxBitmap(43, y, 3, SUB_TWO);
                }
            } else {
                // Draw loop mode
                gfxIcon(34, y, LOOP_ICON);
                gfxPrint(45, y, current_loopmode ? "Yes" : "No");
                
                // Draw cursor if loop mode is selected
                if (cursor == LOOPMODE) gfxCursor(46, y + 8, 19);
                
                // Show CV assignment indicators
                bool* cv1_assign = (current_page == 0) ? cv1_assignments : cv1_assignments2;
                bool* cv2_assign = (current_page == 0) ? cv2_assignments : cv2_assignments2;
                
                if (cv1_assign[LOOPMODE]) {
                    gfxBitmap(43, y, 3, SUP_ONE);
                }
                if (cv2_assign[LOOPMODE]) {
                    gfxBitmap(43, y, 3, SUB_TWO);
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
        int start_x = BAR_X + (current_loopstart * (BAR_WIDTH - 2) / 255) + 1;
        int end_x = BAR_X + (current_loopend * (BAR_WIDTH - 2) / 255) + 1;
        
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
        gfxPrint(13, BAR_Y - 10, current_loopstart);
        if (cursor == LOOPSTART) gfxCursor(13, BAR_Y - 10 + 8, 19);
        
        gfxIcon(34, BAR_Y - 10, RIGHT_ICON);
        gfxPrint(44, BAR_Y - 10, current_loopend);
        if (cursor == LOOPEND) gfxCursor(45, BAR_Y - 10 + 8, 19);
        
        // CV assignment indicators for loop start/end
        bool* cv1_assign = (current_page == 0) ? cv1_assignments : cv1_assignments2;
        bool* cv2_assign = (current_page == 0) ? cv2_assignments : cv2_assignments2;
        
        if (cv1_assign[LOOPSTART]) {
            gfxBitmap(10, BAR_Y - 10, 3, SUP_ONE);
        }
        if (cv2_assign[LOOPSTART]) {
            gfxBitmap(10, BAR_Y - 10, 3, SUB_TWO);
        }
        if (cv1_assign[LOOPEND]) {
            gfxBitmap(42, BAR_Y - 10, 3, SUP_ONE);
        }
        if (cv2_assign[LOOPEND]) {
            gfxBitmap(42, BAR_Y - 10, 3, SUB_TWO);
        }
    }   
};
