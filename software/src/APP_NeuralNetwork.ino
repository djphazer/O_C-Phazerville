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

#ifdef ENABLE_APP_NEURAL_NETWORK

#include "HSApplication.h"
#include "HSMIDI.h"
#include "neuralnet/LogicGate.h"

// 9 sets of 24 bytes allocated for storage
#define NN_SETTING_LAST 216

class NeuralNetwork : public HSApplication, public SystemExclusiveHandler,
    public settings::SettingsBase<NeuralNetwork, NN_SETTING_LAST> {
public:
    void Start() {
        for (int ch = 0; ch < 16; ch++) output_neuron[ch] = ch % 4;
    }
    
    void Resume() {
        LoadFromEEPROMStage();
    }

    void Controller() {
        ListenForSysEx();
        uint16_t tmp_source_state = source_state; // Set temporary state for this cycle

        // Check inputs
        for (byte i = 0; i < 8; i++)
        {
            bool set = (i < 4) ? Gate(i) : (In(i - 4) > HSAPPLICATION_3V);
            tmp_source_state = SetSource(tmp_source_state, i, set);
            input_state[i] = set; // For display
        }
        
        // Process neurons
        for (byte n = 0; n < 6; n++)
        {
            byte ix = (setup * 6) + n;
            bool set = neuron[ix].Calculate(tmp_source_state);
            tmp_source_state = SetSource(tmp_source_state, 8 + n, set);
        }
        
        // Set outputs based on assigned neuron's last state
        for (byte o = 0; o < 4; o++)
        {
            byte ix = (setup * 4) + o;
            byte n = output_neuron[ix] + (setup * 6);
            bool set = neuron[n].state;
            Out(o, set * HSAPPLICATION_5V);
        }

        source_state = tmp_source_state;
    }

    void View() {
        if (copy_mode) DrawCopyScreen();
        else DrawInterface();
    }

    // Public access to save method
    void OnSaveSettings() {SaveToEEPROMStage();}

    /* Send the current setup via SysEx */
    void OnSendSysEx() {
        byte V[39];
        int ix = 0;

        ix = 0;
        for (byte n = 0; n < 6; n++)
        {
            byte ni = (setup * 6) + n;

            // Encode a neuron
            V[ix++] = (neuron[ni].type << 4) | neuron[ni].source1;
            V[ix++] = (neuron[ni].source2 << 4) | neuron[ni].source3;
            V[ix++] = neuron[ni].weight1 + 128;
            V[ix++] = neuron[ni].weight2 + 128;
            V[ix++] = neuron[ni].weight3 + 128;
            V[ix++] = neuron[ni].threshold + 128;
        }

        // Encode the output assignments
        byte o = (setup * 4);
        V[ix++] = (output_neuron[o] << 4) | output_neuron[o + 1];
        V[ix++] = (output_neuron[o + 2] << 4) | output_neuron[o + 3];

        UnpackedData unpacked;
        unpacked.set_data(ix, V);
        PackedData packed = unpacked.pack();
        SendSysEx(packed, 'N');
    }

    void OnReceiveSysEx() {
        // Since only one Setup is coming, use the currently-selected setup to determine
        // where to stash it.
        byte V[39];
        if (ExtractSysExData(V, 'N')) {
            int ix = 0;
            byte b = 0;

            // Decode neurons
            for (byte n = 0; n < 6; n++)
            {
                byte ni = (setup * 6) + n;
                b = V[ix++]; // Type and source 1
                neuron[ni].type = (b >> 4) & 0x0f;
                neuron[ni].source1 = b & 0x0f;
                b = V[ix++]; // Source 2 and source 3
                neuron[ni].source2 = (b >> 4) & 0x0f;
                neuron[ni].source3 = b & 0x0f;
                neuron[ni].weight1 = static_cast<int>(V[ix++] - 128);
                neuron[ni].weight2 = static_cast<int>(V[ix++] - 128);
                neuron[ni].weight3 = static_cast<int>(V[ix++] - 128);
                neuron[ni].threshold = static_cast<int>(V[ix++] - 128);
                neuron[ni].state = 0;
                neuron[ni].source_state = 0;
            }

            // Decode output assignments
            byte o = (setup * 4);
            b = V[ix++]; // Output 1 and 2
            output_neuron[o] = (b >> 4) & 0x0f;
            output_neuron[o + 1] = b & 0x0f;
            b = V[ix++]; // Output 3 and 4
            output_neuron[o + 2] = (b >> 4) & 0x0f;
            output_neuron[o + 3] = b & 0x0f;
        }

    }

    /* Perform a copy or sysex dump */
    void CopySetup(int target, int source) {
        if (source == target) {
            OnSendSysEx();
        } else {
            for (byte n = 0; n < 6; n++)
                memcpy(&neuron[(target * 6) + n], &neuron[(source * 6) + n], sizeof(neuron[(source * 6) + n]));

            for (byte o = 0; o < 4; o++)
                output_neuron[(target * 4) + 0] = output_neuron[(source * 4) + o];

            setup = target;
        }
        copy_mode = 0;
    }

    /////////////////////////////////////////////////////////////////
    // Control handlers
    /////////////////////////////////////////////////////////////////
    void OnLeftButtonPress() {
        if (copy_mode) copy_mode = 0;
        else screen = 1 - screen;
    }

    void OnLeftButtonLongPress() {
        all_connections = 1 - all_connections;
    }

    void OnRightButtonPress() {
        if (copy_mode) {
            CopySetup(copy_setup_target, setup);
            copy_mode = 0;
        }
        cursor++;
        if (selected < 6) {
            byte ix = (setup * 6) + selected;
            if (cursor >= neuron[ix].NumParam()) cursor = 0;
        } else {
            if (cursor >= 4) cursor = 0;
        }
        ResetCursor();
    }

    void OnUpButtonPress() {
        if (copy_mode) {
            copy_setup_target = constrain(copy_setup_target + 1, 0, 3);
        } else {
            setup = constrain(setup + 1, 0, 3);
            LoadSetup(setup);
        }
    }

    void OnDownButtonPress() {
        if (copy_mode) {
            copy_setup_target = constrain(copy_setup_target - 1, 0, 3);
        }
        setup = constrain(setup - 1, 0, 3);
    }

    void OnDownButtonLongPress() {
        copy_mode = 1 - copy_mode;
        copy_setup_target = setup + 1;
        if (copy_setup_target > 3) copy_setup_target = 0;
    }

    void OnLeftEncoderMove(int direction) {
        selected = constrain(selected + direction, 0, 6);
        cursor = 0;
        ResetCursor();
    }

    void OnRightEncoderMove(int direction) {
        if (selected < 6) {
            byte ix = (setup * 6) + selected;
            neuron[ix].UpdateValue(cursor, direction);
        } else {
            byte ix = (setup * 4) + cursor;
            output_neuron[ix] = constrain(output_neuron[ix] + direction, 0, 5);
        }
    }

private:
    // Screen and edit states
    byte cursor = 0; // Cursor on the neuron select screen
    int selected = 0; // 0-5, which neuron is currently selected
    int setup = 0; // 0-3, which set is currently selectd
    bool screen = 0; // 0 = selector screen, 1 = edit screen
    bool copy_mode = 0; // Copy mode on/off
    bool all_connections = 0; // Connections for all neurons shown instead of just selected
    int copy_setup_target; // Which setup is being copied TO?
    
    LogicGate neuron[24]; // Four sets of six neurons
    int output_neuron[16]; // Four sets of four output assignments
    bool input_state[8];
    
    // Source state is passed to each neuron for use in its logic gate calculation.
    // The value is a bitfield, with each bit indicating the value of a source's most-
    // recent result:
    //     Bits 0-3: Digital inputs
    //     Bits 4-7: CV inputs
    //     Bits 8-13: Neuron outputs
    uint16_t source_state = 0;

    void DrawInterface() {
        gfxHeader("Neural Net");
        gfxPrint(128 - 42, 1, "Setup ");
        gfxPrint(setup + 1);
        if (screen) DrawEditScreen();
        else DrawSelectorScreen();
    }
    
    void DrawSelectorScreen() {
        // Draw the neurons
        byte offset_ix = setup * 6;
        for (byte n = 0; n < 6; n++)
        {
            byte ix = offset_ix + n;

            // Neurons are arranged in a 3x2 grid, top to bottom, left to right, like this:
            //     135
            //     246
            byte cell_x = n / 2;
            byte cell_y = n % 2;
            byte x = (cell_x * 32) + 24;
            byte y = (cell_y * 24) + 16;
            neuron[ix].DrawSmallAt(x + 1, y, (selected == n && CursorBlink()));
            
            if (selected == n || all_connections) {
                neuron[ix].DrawInputs(n);
            }
        }

        // Draw the inputs
        gfxPrint(0, 16, "DV");
        for (byte i = 0; i < 8; i++)
        {
            // Inputs are arranged in a 2x4 grid, top to bottom, left to right, like this:
            //     15
            //     26
            //     37
            //     48
            byte cell_x = i / 4;
            byte cell_y = i % 4;
            byte x = (cell_x * 6);
            byte y = (cell_y * 10) + 24;
            gfxPrint(x, y, cell_y + 1);
            
            if (input_state[i]) gfxInvert(x, y, 6, 8);
        }
        
        // Draw outputs
        byte x = 120;
        for (byte o = 0; o < 4; o++)
        {
            // Outputs are arranged top to bottom
            byte y = (o * 12) + 16;
            char out_name[2] = {static_cast<char>(o + 'A'), '\0'};
            gfxPrint(x, y, out_name);
            
            if (ViewOut(o)) gfxInvert(x, y, 6, 8);

            // Draw line to the output if selected
            byte ix = (setup * 4) + o;
            if (output_neuron[ix] == selected || all_connections) {
                if (neuron[(setup * 6) + output_neuron[ix]].type > LogicGateType::NONE) {
                    byte fx = ((output_neuron[ix] / 2) * 32) + 45;
                    byte fy = ((output_neuron[ix] % 2) * 24) + 29;
                    byte ty = (o * 12) + 20;
                    gfxDottedLine(fx, fy, 120, ty, 4);
                }
            }
        }
        if (selected == 6 && CursorBlink()) gfxLine(118, 16, 118, 60);
    }
    
    void DrawEditScreen() {
        if (selected < 6) {
            // Draw the neuron header
            gfxPrint(0, 15, "#");
            gfxPrint(selected + 1);
            for (byte n = 0; n < 6; n++)
            {
                byte cell_x = n / 2;
                byte cell_y = n % 2;
                byte x = (cell_x * 8) + 40;
                byte y = (cell_y * 5) + 15;
                if (n == selected) gfxRect(x, y, 6, 4);
                else gfxFrame(x, y, 6, 4);
            }

            // Draw the editor for a neuron
            byte ix = (setup * 6) + selected;
            byte p = neuron[ix].NumParam(); // Number of parameters for this neuron
            for (byte c = 0; c < p; c++)
            {
                if (c < 4) { // This is the right-hand pane, so only show first four parameters
                    byte y = (c * 10) + 25;
                    neuron[ix].PrintParamNameAt(0, y, c);
                    neuron[ix].PrintValueAt(38, y, c);
                    if (c == cursor) gfxCursor(39, y + 8, 23);
                }
            }
            DrawLarge(ix);
        } else {
            // Draw the Output Assign editor
            gfxPrint(0, 15, "Assign Outputs");
            for (byte o = 0; o < 4; o++)
            {
                byte y = (o * 10) + 25;
                gfxPrint(0, y, "Output ");
                char out_name[2] = {static_cast<char>(o + 'A'), '\0'};
                gfxPrint(out_name);

                byte ix = (setup * 4) + o;
                gfxPrint(64, y, "Neuron ");
                gfxPrint(output_neuron[ix] + 1);

                if (cursor == o) gfxCursor(65, y + 8, 47);
            }
        }
    }

    void DrawCopyScreen() {
        gfxHeader("Copy");

        graphics.setPrintPos(8, 28);
        graphics.print("Setup ");
        graphics.print(setup + 1);
        graphics.print(" -");
        graphics.setPrintPos(58, 28);
        graphics.print("> ");
        if (setup == copy_setup_target) graphics.print("SysEx");
        else {
            graphics.print("Setup ");
            graphics.print(copy_setup_target + 1);
        }

        graphics.setPrintPos(0, 55);
        graphics.print("[CANCEL]");

        graphics.setPrintPos(90, 55);
        graphics.print(setup == copy_setup_target ? "[DUMP]" : "[COPY]");
    }

    void LoadSetup(byte setup_) {
            cursor = 0;
            if (selected < 6) selected = 0;
            setup = setup_;
    }
    
    uint16_t SetSource(uint16_t source_state, byte b, bool v)
    {
        if (v) source_state |= (0x01 << b);
        else source_state &= ~(0x01 << b);
        return source_state;
    }
    
    /* The system settings are just bytes. Move them into the instance variables here */
    void LoadFromEEPROMStage() {
        byte ix = 0;
        for (byte n = 0; n < 24; n++)
        {
            neuron[n].type = values_[ix++];
            neuron[n].source1 = values_[ix++];
            neuron[n].source2 = values_[ix++];
            neuron[n].source3 = values_[ix++];
            neuron[n].weight1 = values_[ix++] - 128;
            neuron[n].weight2 = values_[ix++] - 128;
            neuron[n].weight3 = values_[ix++] - 128;
            neuron[n].threshold = values_[ix++] - 128;

            // Adjust to 0 when no data is saved yet
            if (neuron[n].weight1 == -128) neuron[n].weight1 = 0;
            if (neuron[n].weight2 == -128) neuron[n].weight2 = 0;
            if (neuron[n].weight3 == -128) neuron[n].weight3 = 0;
            if (neuron[n].threshold == -128) neuron[n].threshold = 0;
        }
        for (byte o = 0; o < 16; o++) output_neuron[o] = values_[ix++];
            
    }
    
    void SaveToEEPROMStage() {
        byte ix = 0;
        for (byte n = 0; n < 24; n++)
        {
            values_[ix++] = neuron[n].type;
            values_[ix++] = neuron[n].source1;
            values_[ix++] = neuron[n].source2;
            values_[ix++] = neuron[n].source3;
            values_[ix++] = neuron[n].weight1 + 128;
            values_[ix++] = neuron[n].weight2 + 128;
            values_[ix++] = neuron[n].weight3 + 128;
            values_[ix++] = neuron[n].threshold + 128;
        }
        for (byte o = 0; o < 16; o++) values_[ix++] = output_neuron[o];
    }

    void DrawLarge(byte ix) {
        if (neuron[ix].type == LogicGateType::TL_NEURON) DrawTLNeuron(ix);
        else DrawLogicGate(ix);
    }
    
    //////// LOGIC GATE EDIT SCREEN REPRESENTATIONS
    void DrawTLNeuron(byte ix) {
        // Draw Dendrites
        int dendrite_weight[3] = {neuron[ix].weight1, neuron[ix].weight2, neuron[ix].weight3};
        for (int d = 0; d < 3; d++)
        {
            byte indent = d == 1 ? 4 : 0;
            int weight = dendrite_weight[d];
            gfxCircle(73 + indent, 22 + (16 * d), 8); // Dendrite
            gfxPrint((weight < 0 ? 66 : 72) + indent , 19 + (16 * d), weight);
            if (cursor == (d + 4) && CursorBlink()) gfxCircle(73 + indent, 22 + (16 * d), 7);
            gfxDottedLine(81 + indent, 22 + (16 * d), 100, 38, neuron[ix].SourceValue(d) ? 1 : 3); // Synapse
        }

        // Draw Axon
        int threshold = neuron[ix].threshold;
        int x = 105; // Starting x position for number
        if (threshold < 10 && threshold > -10) x += 5; // Shove over a bit if a one-digit number
        if (threshold < 0) x -= 5; // Pull back if a sign is necessary
        gfxPrint(x, 34, threshold);
        if (cursor == 7 && CursorBlink()) gfxCircle(112, 38, 11);
        
        int r = 0;
        if (neuron[ix].state) {
        		if (random(1, 100) > 60) r = random(0, 3) - 1;
        }
        gfxCircle(112, 38, 12 + (r * 2));
    }
    
    void DrawLogicGate(byte ix) {
        if (neuron[ix].type != LogicGateType::NONE) {
            DrawInputs(ix);
            DrawBody(ix);
            DrawNegation(ix);
            DrawOutput(ix);
        }
    }

    void DrawInputs(byte ix) {
        if (neuron[ix].type == LogicGateType::NOT) {
            gfxDottedLine(64, 36, 76, 36, neuron[ix].SourceValue(0) ? 1 : 3);
        } else {
            gfxDottedLine(64, 28, 76, 28, neuron[ix].SourceValue(0) ? 1 : 3);
            gfxDottedLine(64, 44, 76, 44, neuron[ix].SourceValue(1) ? 1 : 3);
        }
    }

    void DrawBody(byte ix) {
        if (neuron[ix].type == LogicGateType::NOT) {
            gfxLine(76, 20, 76, 52);
            gfxLine(76, 20, 108, 36);
            gfxLine(76, 52, 108, 36);
        }
        if (neuron[ix].type == LogicGateType::AND
         || neuron[ix].type == LogicGateType::NAND
        ) {
            gfxLine(76, 20, 76, 52);
            gfxLine(76, 20, 96, 20);
            gfxLine(96, 20, 103, 26);
            gfxLine(103, 26, 108, 32);
            gfxLine(108, 32, 108, 40);
            gfxLine(76, 52, 96, 52);
            gfxLine(96, 52, 103, 46);
            gfxLine(103, 46, 108, 40);
        }
        if (neuron[ix].type == LogicGateType::OR
         || neuron[ix].type == LogicGateType::NOR
         || neuron[ix].type == LogicGateType::XOR
         || neuron[ix].type == LogicGateType::XNOR
        ) {
            gfxLine(76, 20, 96, 20);
            gfxLine(96, 20, 108, 36);
            gfxLine(76, 52, 96, 52);
            gfxLine(96, 52, 108, 36);
            gfxLine(76, 20, 80, 36);
            gfxLine(76, 52, 80, 36);

            if (neuron[ix].type == LogicGateType::XOR
             || neuron[ix].type == LogicGateType::XNOR
            ) {
                gfxLine(70, 20, 74, 36);
                gfxLine(70, 52, 74, 36);
            }
        }
        if (neuron[ix].type == LogicGateType::D_FLIPFLOP || neuron[ix].type == LogicGateType::T_FLIPFLOP) {
            gfxLine(76, 20, 76, 52);
            gfxLine(76, 20, 108, 20);
            gfxLine(76, 52, 108, 52);
            gfxLine(108, 20, 108, 52);
            gfxLine(76, 40, 84, 44);
            gfxLine(76, 48, 84, 44);
            gfxPrint(78, 25, neuron[ix].type == LogicGateType::D_FLIPFLOP ? "D" : "T");
        }
        if (neuron[ix].type == LogicGateType::LATCH) {
            gfxLine(76, 28, 84, 28); // Lines to NOR gates
            gfxLine(76, 44, 84, 44);
            gfxBitmap(84, 27, 16, NN_LOGIC_ICON[6]);
            gfxBitmap(84, 39, 16, NN_LOGIC_ICON[6]);
            gfxLine(100, 30, 108, 30);
            gfxLine(108, 30, 108, 36);
            gfxLine(102, 30, 84, 40); // Line from Reset to Set in
            gfxLine(100, 42, 84, 32); // Line from Set to Reset in

        }
    }

    void DrawNegation(byte ix) {
        if (neuron[ix].type == LogicGateType::NOT
         || neuron[ix].type == LogicGateType::NAND
         || neuron[ix].type == LogicGateType::NOR
         || neuron[ix].type == LogicGateType::XNOR
        ) {
            gfxCircle(112, 36, 4);
        } else {
            gfxDottedLine(108, 36, 117, 36, neuron[ix].state ? 1 : 3);
        }
    }

    void DrawOutput(byte ix) {
		int y = 0;
    		if (neuron[ix].state) {
    			// Shimmer
    			if (random(1, 100) > 60) y = random(0, 3) - 1;
    		}
        gfxDottedLine(116, 36 + (y * 2), 127, 36 + (y * 2), neuron[ix].state ? 1 : 3);
    }
};

// TOTAL EEPROM SIZE: 216 bytes
#define NN_EEPROM_DATA {0,0,255,"St",NULL,settings::STORAGE_TYPE_U8},
#define NN_DO_TWENTYFOUR_TIMES(A) A A A A A A A A A A A A A A A A A A A A A A A A
SETTINGS_DECLARE(NeuralNetwork, NN_SETTING_LAST) {
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
    NN_DO_TWENTYFOUR_TIMES(NN_EEPROM_DATA)
};

NeuralNetwork NeuralNetwork_instance;

// App stubs
void NeuralNetwork_init() {
    NeuralNetwork_instance.BaseStart();
}

constexpr size_t NeuralNetwork_storageSize() {return NeuralNetwork::storageSize();}
size_t NeuralNetwork_save(void *storage) {return NeuralNetwork_instance.Save(storage);}
size_t NeuralNetwork_restore(const void *storage) {return NeuralNetwork_instance.Restore(storage);}

void NeuralNetwork_isr() {
    return NeuralNetwork_instance.BaseController();
}

void NeuralNetwork_handleAppEvent(OC::AppEvent event) {
    if (event ==  OC::APP_EVENT_RESUME) {
        NeuralNetwork_instance.Resume();
    }
    if (event == OC::APP_EVENT_SUSPEND) {
        NeuralNetwork_instance.OnSaveSettings();
        NeuralNetwork_instance.OnSendSysEx();
    }
}

void NeuralNetwork_loop() {} // Deprecated

void NeuralNetwork_menu() {
    NeuralNetwork_instance.BaseView();
}

void NeuralNetwork_screensaver() {} // Deprecated

void NeuralNetwork_handleButtonEvent(const UI::Event &event) {
    // For left encoder, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_L) {
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) NeuralNetwork_instance.OnLeftButtonLongPress();
        if (event.type == UI::EVENT_BUTTON_PRESS) NeuralNetwork_instance.OnLeftButtonPress();
    }

    // For right encoder, only handle press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_R && event.type == UI::EVENT_BUTTON_PRESS) NeuralNetwork_instance.OnRightButtonPress();

    // For up button, handle only press (long press is reserved)
    if (event.control == OC::CONTROL_BUTTON_UP && event.type == UI::EVENT_BUTTON_PRESS) NeuralNetwork_instance.OnUpButtonPress();

    // For down button, handle press and long press
    if (event.control == OC::CONTROL_BUTTON_DOWN) {
        if (event.type == UI::EVENT_BUTTON_PRESS) NeuralNetwork_instance.OnDownButtonPress();
        if (event.type == UI::EVENT_BUTTON_LONG_PRESS) NeuralNetwork_instance.OnDownButtonLongPress();
    }
}

void NeuralNetwork_handleEncoderEvent(const UI::Event &event) {
    // Left encoder turned
    if (event.control == OC::CONTROL_ENCODER_L) NeuralNetwork_instance.OnLeftEncoderMove(event.value);

    // Right encoder turned
    if (event.control == OC::CONTROL_ENCODER_R) NeuralNetwork_instance.OnRightEncoderMove(event.value);
}

#endif
