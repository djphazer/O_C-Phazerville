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

#include "vector_osc/HSVectorOscillator.h"
#include "vector_osc/WaveformManager.h"
#include "HSRelabiManager.h"
#include "HSicons.h"

class Relabi : public HemisphereApplet {

public:

    const char* applet_name() {
        return "Relabi";
    }

    void Start() {
        freq[0] = 120;
        freq[1] = 360;
        freq[2] = 600;
        freq[3] = 840;
        xmod[0] = 0;
        xmod[1] = 0;
        xmod[2] = 0;
        xmod[3] = 0;
        xmodplus = 0;
        freqLinkMul = 1;
        freqLinkDiv = 1;
        freqKnobMul = 1;
        freqKnobDiv = 1;
        bipolar = true;
        for (uint8_t i = 0; i < 4; i++) {
            lfoThreshold[i] = 0; // Default threshold is 0%
        }
        outputAssign[0] = 0; // Default to LFO1
        outputAssign[1] = 1; // Default to LFO2

  

        for (uint8_t count = 0; count < 4; count++) {
            if (freq[count] > 2000) {freqKnob[count] = (freq[count] - 2000) / 100 + 380;}
            if (freq[count] < 2000) {freqKnob[count] = (freq[count] - 200) / 10 + 200;}
            if (freq[count] < 200) {freqKnob[count] = freq[count];}
            xmodKnob[count] = xmod[count];
            osc[count] = WaveformManager::VectorOscillatorFromWaveform(35);
            osc[count].SetFrequency(freq[count]);

        }
    }

    void Controller() {
        
        RelabiManager * manager = RelabiManager::get();
        manager->RegisterRelabi(hemisphere);
        linked = manager->IsLinked();
        int wave1;
        int wave2;
        uint8_t clkCalc;


        if (LEFT_HEMISPHERE) {clkCalc = 1;}
        if (RIGHT_HEMISPHERE) {clkCalc = 3;}

            if (Clock(0) && oldClock == 0) { // Rising edge detected on TRIG1 input
                for (uint8_t pcount = 0; pcount < 4; pcount++) {
                    // Get the total number of segments in the waveform
                    // byte totalSegments = osc[pcount].GetSegment(0).Segments(); // Use first segment's TOC

                    // Calculate phase position based on phase percentage (0–100)
                    //int setPhase = round((phase[pcount] / 100.0) * totalSegments);

                    // Reset the phase of the oscillator
                    //osc[pcount].SetPhase(setPhase);
                    osc[pcount].Reset();
                }
            }

                    //}
                oldClock = Clock(0);

        if (clkDiv == clkCalc) {








            if (linked && hemisphere == RIGHT_HEMISPHERE) {
           



                // Linked: Receive lfo values from RelabiManager
                manager->ReadValues(sample[0], sample[1], sample[2], sample[3]);
                wave1 = (static_cast<float>(sample[2]));
                wave2 = (static_cast<float>(sample[3]));
                bool receivedGateStates[4];
                manager->ReadGates(receivedGateStates);
                for (int i = 0; i < 4; i++) {
                    gateState[i] = receivedGateStates[i];
                
                }




            } else {

                cvIn = (In(0))/51.15;
                cvIn2 = (In(1))/51.15;

                float normalizedCV = ((cvIn / 100.0) + 1.0) / 2.0;
                cvIn = 10000.0 * normalizedCV; //set range for cvIn to be 0 to 10000
                
                float normalizedCV2 = ((cvIn2 / 100.0) + 1.0) / 2.0;
                cvIn2 = 10000.0 * normalizedCV2; //set range for cvIn to be 0 to 10000
                
                

                // frequency modulation:
                for (uint8_t lfo = 0; lfo < 4; lfo++) {
                    osc[lfo].SetScale(HEMISPHERE_3V_CV);

                // Use freqKnobMul and freqKnobDiv to adjust frequency
                float globalFreqFactor = static_cast<float>(freqKnobMul) / static_cast<float>(freqKnobDiv);


                // Calculate gate outputs based on thresholds
                if (sample[lfo] >= (lfoThreshold[lfo] * HEMISPHERE_MAX_CV) / 200) {
                    // Gate is high
                    gateState[lfo] = true;
                    } else if (sample[lfo] < (lfoThreshold[lfo] * HEMISPHERE_MAX_CV) / 200) {
                    // Signal is below or equal to the threshold, clear the gate
                    gateState[lfo] = false;
                }

                // Normalize the CV input to a range between 0 and 1
                float normalizedCV = static_cast<float>(cvIn) / 2500.0; // Adjust divisor if needed based on CV range

                // Incorporate CV2 with cross-modulation
                float xmodCombo = xmodplus +  xmod[lfo] +  (cvIn2 / 100) - 50;

                // Calculate cross-frequency modulation factor
                float crossFreqMod = (static_cast<float>(xmodCombo) / 100.0) * (static_cast<float>(sample[(lfo + 3) % 4]) / HEMISPHERE_3V_CV);

                // Combine base frequency, cross-modulation, and CV input
                float modulatedFreq = normalizedCV * globalFreqFactor * (freq[lfo] + (freq[lfo] * crossFreqMod));

                // float modulatedFreq = freq[lfo] + (freq[lfo] * crossFreqMod * normalizedCV);

                // Ensure the frequency stays within valid bounds
                modulatedFreq = constrain(modulatedFreq, 0.01, 15000.0); // Example: limit to 1 Hz to 20 kHz

                // Set the modulated frequency to the oscillator
                osc[lfo].SetFrequency(modulatedFreq * 15);

                // Update sample
                sample[lfo] = osc[lfo].Next();

                // Store the display frequency for visualization
                displayFreq[lfo] = modulatedFreq;
            }

                
                if (manager->IsLinked() && hemisphere == LEFT_HEMISPHERE) {

                    // Linked: Send lfo values to RelabiManager
                    manager->WriteValues(sample[0], sample[1], sample[2], sample[3]);
                    // Linked: Send gate values to RelabiManager
                    manager->WriteGates(gateState);
                }

                if (manager->IsLinked() && hemisphere == RIGHT_HEMISPHERE) {
                    manager->ReadValues(sample[0], sample[1], sample[2], sample[3]);
                    manager->ReadGates(gateState);
                }
                
                // CV1 outputs LFO1 // CV2 outputs LFO2

                
                    wave1 = (static_cast<float>(sample[0]) /*+ HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV*/); 
                    wave2 = (static_cast<float>(sample[1]) /*+ HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV*/);
                    
                }
        }
            

                
            if (!bipolar) {
                wave1 = wave1 + HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV;
                wave2 = wave2 + HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV;
               } else {
                wave1 = wave1;
                wave2 = wave2;
               }

            // Set outputs based on assignments
            int outputA = GetOutputValue(outputAssign[0]);
            int outputB = GetOutputValue(outputAssign[1]);
            Out(0, outputA);
            Out(1, outputB);

            // Out(0, wave1);
            // Out(1, wave2);
            
            clkDiv++;
            clkDiv = clkDiv %32;
    }




void View() {
    if (currentPage == 0) {
        if (linked && hemisphere == RIGHT_HEMISPHERE) {

            gfxPrint(1, 55, "C:");
            DrawOutputOption(13, 55, outputAssign[0]); // OUT3

            gfxPrint(31, 55, "D:");
            DrawOutputOption(43, 55, outputAssign[1]); // OUT4


            DrawVUMetersRight();


            // Highlight selected parameter
            switch (selectedParam) {
                // case 0: HighlightParameter(2, 35, 14); break; // MULT
                // case 1: HighlightParameter(22, 35, 14); break; // DIV
                case 0: HighlightParameter(2, 63, 30); break; // OUT3
                case 1: HighlightParameter(32, 63, 30); break; // OUT4
            }

        } else {
            // Page 1: Main parameters for non-linked or left hemisphere
            gfxPrint(1, 15, "LFO");
            gfxPrint(21, 15, selectedChannel + 1);

            gfxPrint(1, 26, "FREQ");
            float fDisplay = freq[selectedChannel];
            gfxPrint(2, 35, ones(fDisplay));

            if (fDisplay < 2000) {
                if (fDisplay < 199) {
                    gfxPrint("."); // Add decimal point for low frequencies
                    int h = hundredths(fDisplay);
                    if (h < 10) { gfxPrint("0"); } // Add leading zero for single digits
                    gfxPrint(h);
                } else {
                    gfxPrint("."); // Add decimal point for midrange frequencies
                    int t = hundredths(fDisplay);
                    t = (t / 10) % 10; // Display only the first decimal digit
                    gfxPrint(t);
                }
            }


            gfxPrint(31, 26, "XFM");
            gfxPrint(32, 35, xmod[selectedChannel]);

            gfxPrint(1, 46, "PHAS");
            gfxPrint(2, 55, phase[selectedChannel]);

            gfxPrint(31, 46, "THRS");
            gfxPrint(32, 55, lfoThreshold[selectedChannel]);

            // Highlight selected parameter
            switch (selectedParam) {
                case 0: HighlightParameter(1, 23, 30); break; // OSC
                case 1: HighlightParameter(2, 43, 30); break; // FREQ
                case 2: HighlightParameter(32, 43, 30); break; // XMOD
                case 3: HighlightParameter(2, 63, 30); break; // PHAS
                case 4: HighlightParameter(32, 63, 30); break; // THRS
            }
            DrawVUMetersLeft();
        }
    } else if (currentPage == 1) {

        if (linked && hemisphere == RIGHT_HEMISPHERE) {
        } else {
        
            // Page 2: Clock mult, Polarity, and Output page

            gfxPrint(1, 15, "ALL LFOs");
            gfxPrint(2, 25, "FREQx ");
            gfxPrint(1, 35, freqKnobMul);

            gfxPrint(16, 35, "/");
            gfxPrint(22, 35, freqKnobDiv);

            gfxPrint(1, 55, "C:");
            DrawOutputOption(13, 55, outputAssign[0]); // OUT3

            gfxPrint(31, 55, "D:");
            DrawOutputOption(43, 55, outputAssign[1]); // OUT4

            gfxPrint(36, 26,  "XFM");
            gfxPrint(37, 35, xmodoffset); // ALL CROSS_MODULATION

            gfxPrint(1, 46, "POL");
            gfxPrint(20, 46, bipolar ? "+-" : "+ ");

            gfxPrint(1, 55, "A:");
            DrawOutputOption(13, 55, outputAssign[0]); // OUT1

            gfxPrint(31, 55, "B:");
            DrawOutputOption(43, 55, outputAssign[1]); // OUT2


            // Highlight selected parameter (use your original locations)
            switch (selectedParam) {
                case 5: HighlightParameter(1, 44, 14); break; // MULT
                case 6: HighlightParameter(22, 44, 14); break; // DIV
                case 7: HighlightParameter(36, 44, 20); break; // ALLMOD
                case 8: HighlightParameter(1, 54, 35); break; // POL
                case 9: HighlightParameter(2, 63, 30); break; // OUT1
                case 10: HighlightParameter(32, 63, 30); break; // OUT2
            }
        }   
    }

}

void DrawOutputOption(int x, int y, uint8_t assign) {
    if (assign < 4) {
        // LFO output
        gfxBitmap(x, y, 8, WAVEFORM_ICON);
        const uint8_t* subscript = (assign == 0) ? SUP_ONE : (assign == 1) ? SUB_TWO : (assign == 2) ? SUP_THREE : SUB_FOUR;
        gfxBitmap(x + 9, y, 3, subscript);
    } else {
        // Gate output
        gfxBitmap(x, y, 8, GATE_ICON);
        const uint8_t* subscript = (assign == 4) ? SUP_ONE : (assign == 5) ? SUB_TWO : (assign == 6) ? SUP_THREE : SUB_FOUR;
        gfxBitmap(x + 9, y, 3, subscript);
    }
}



    // A helper method to highlight parameters depending on EditMode
    // If editing, invert rectangle; if not editing, draw a blinking cursor line
    void HighlightParameter(int x, int y, int w) {
        // y is the baseline where we normally draw the parameter text
        // We'll highlight the line above it (like in original code) 
        // and depending on EditMode, invert or draw a line

        if (EditMode()) {
            // Editing: invert the rectangular area to show a highlight
            gfxInvert(x, y - 9, w, 9); // 9 is line height
        } else {
            // Not editing: show a blinking cursor line
            // The Hemisphere system already uses CursorBlink() internally,
            // so this will only draw the line half the time for blinking effect.
            if (CursorBlink()) {
                gfxLine(x, y, x + w - 1, y);
                gfxPixel(x, y-1);
                gfxPixel(x + w - 1, y-1);
            }
        }
    }

void DrawVUMetersRight() {
    int bar[4];
    for (int i = 0; i < 4; ++i) {
        // Calculate bar height based on sample value (assuming bipolar -3V to +3V range)
        bar[i] = 14.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;

        // Draw vertical bars (adjust x-position and width as needed)
        gfxRect(2 + (15 * i), 42 - bar[i], 13, bar[i]);
    }
}

void DrawVUMetersLeft() {
    int bar[4];
    for (int i = 0; i < 4; ++i) {
        // Calculate bar height based on sample value (assuming bipolar -3V to +3V range)
        // Smaller bars for left hemisphere
        bar[i] = 4.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;
        gfxRect(31 + (6 * i), 22 - bar[i], 5, bar[i]); // Adjusted for smaller size

    }
}
    



    void OnButtonPress() {
        if (!EditMode()) {
            // Enter EditMode on button press
            isEditing = true;
        } else {
            // Exit EditMode on button press
            isEditing = false;
        }

    }

    



void OnEncoderMove(int direction) {
    // Determine how many parameters we have based on linkage and hemisphere
    // linked && RIGHT_HEMISPHERE: 2 parameters (0 and 1)
    // otherwise: adjust for both pages (parameters 0–7)
    int max_param = (linked && hemisphere == RIGHT_HEMISPHERE) ? 1 : 10;

    if (!EditMode()) {
        // Not editing: move the cursor through the available parameters
        selectedParam = (selectedParam + direction + max_param + 1) % (max_param + 1);

        // Determine the current page based on the selected parameter
        if (selectedParam <= 4) {
            currentPage = 0; // Page 0: Main parameters
        } else {
            currentPage = 1; // Page 1: THRES, OUT1, OUT2
        }
    } else {
        // Editing: adjust parameters based on the current page and selected parameter
        if (currentPage == 0) {
            // Page 0: Main parameters
            if (linked && hemisphere == RIGHT_HEMISPHERE) {
                // Linked and RIGHT Hemisphere: Only global frequency multiplier or divider
                switch (selectedParam) {
                    case 0: // OUT3 assignment
                        outputAssign[0] = (outputAssign[0] + direction + 8) % 8;
                        break;
                    case 1: // OUT4 assignment
                        outputAssign[1] = (outputAssign[1] + direction + 8) % 8;
                        break;
                }
            } else {
                // Not linked or LEFT Hemisphere: Full parameter set (0–4)
                switch (selectedParam) {
                    case 0: // Cycle through OSC selection
                        selectedChannel = (selectedChannel + direction + 4) % 4;
                        break;

                    case 1: // FREQ (0–20.0) scaling
                        freqKnob[selectedChannel] += direction;
                        if (freqKnob[selectedChannel] < 0) { freqKnob[selectedChannel] = 510; }
                        if (freqKnob[selectedChannel] > 510) { freqKnob[selectedChannel] = 0; }
                        if (freqKnob[selectedChannel] < 200) {
                            freq[selectedChannel] = freqKnob[selectedChannel];
                        } else if (freqKnob[selectedChannel] < 380) {
                            freq[selectedChannel] = 200 + ((freqKnob[selectedChannel] - 200) * 10);
                        } else {
                            freq[selectedChannel] = 2000 + ((freqKnob[selectedChannel] - 380) * 100);
                        }
                        break;

                    case 2: // XMOD (0–100) scaling
                        xmodKnob[selectedChannel] += direction;
                        xmodKnob[selectedChannel] = xmodKnob[selectedChannel] + 401;
                        xmodKnob[selectedChannel] = xmodKnob[selectedChannel] % 401;
                        xmod[selectedChannel] = xmodKnob[selectedChannel];
                        break;

                    case 3: // PHAS (0–100)
                        phase[selectedChannel] += direction;
                        phase[selectedChannel] = (phase[selectedChannel] + 101) % 101;
                        break;

                    case 4: // THRS adjustment
                    lfoThreshold[selectedChannel] = constrain(lfoThreshold[selectedChannel] + direction, -100, 100);
                    break;
                }
            }
        } else if (currentPage == 1) {
            // Page 1: THRES, OUT1, OUT2
            switch (selectedParam) {

                case 5: // Global frequency multiplier
                    freqKnobMul += direction;
                    freqKnobMul = freqKnobMul + 65;
                    freqKnobMul = freqKnobMul % 65; 
                    freqLinkMul = freqKnobMul;
                    break;
                case 6: // Global frequency divider
                    freqKnobDiv = (freqKnobDiv + 64 + direction - 1) % 64 + 1;  // Cycle from 1 to 64
                    freqLinkDiv = freqKnobDiv;
                    break;
                case 7: // Global cross modulation offset
                    xmodoffset += direction;
                    xmodoffset = xmodoffset + 101;
                    xmodoffset = xmodoffset % 101;
                    xmodplus = xmodoffset;
                    break;
                case 8: // POLARITY (+ or +-)
                    bipolar = (bipolar + direction + 2) % 2;
                    break;
                case 9: // OUT1 assignment
                    outputAssign[0] = (outputAssign[0] + direction + 8) % 8;
                    break;
                case 10: // OUT2 assignment
                    outputAssign[1] = (outputAssign[1] + direction + 8) % 8;
                    break;
            }
        }
    }
}

        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        return data;
    }
    
    void OnDataReceive(uint64_t data) {

    }

protected:
    void SetHelp() {
        if (linked && hemisphere == RIGHT_HEMISPHERE) {
            help[HEMISPHERE_HELP_DIGITALS] = "1=NA 2=NA";
            help[HEMISPHERE_HELP_CVS] = "1=NA 2=NA";
            help[HEMISPHERE_HELP_OUTS] = "A=LFO3 B=LFO4";
            help[HEMISPHERE_HELP_ENCODER] = "GlobalFreqMul/Div";
        } else if (currentPage == 0) {
            help[HEMISPHERE_HELP_DIGITALS] = "1=ResetPhase 2=NA";
            help[HEMISPHERE_HELP_CVS] = "1=AllFreq 2=AllXFM";
            help[HEMISPHERE_HELP_OUTS] = "A=LFO1 B=LFO2";
            help[HEMISPHERE_HELP_ENCODER] = "Frq/XFM/Phs/Thrs";
        } else if (currentPage == 1) {
            help[HEMISPHERE_HELP_DIGITALS] = "1=NA 2=NA";
            help[HEMISPHERE_HELP_CVS] = "1=Thresh";
            help[HEMISPHERE_HELP_OUTS] = "A=OUT1 B=OUT2";
            help[HEMISPHERE_HELP_ENCODER] = "Thresh/Pol/Out";
        }
    }

    
private:
    static constexpr int pow10_lut[] = { 1, 10, 100, 1000 };
    int cursor; // 0=Freq A; 1=Cross Mod A; 2=Phase A; 3=Freq B; 4=Cross Mod B; etc.
    bool isEditing = false;  // Indicates if the user is editing a parameter
    uint8_t modal_edit_mode = 2;
    VectorOscillator osc[4];
    constexpr static uint8_t ch = 4;
    constexpr static uint8_t numParams = 5;
    uint8_t selectedOsc;
    float freq[ch]; // in centihertz
    uint16_t xmod[ch];
    uint16_t xmodoffset;
    uint16_t xmodplus;
    //uint8_t selectedXmod;
    uint8_t phase[ch];
    int selectedChannel = 0;
    int selectedParam = 0;
    int sample[ch];
    float outFreq[ch];
    float freqKnob[4];
    float cvIn;
    float cvIn2;   
    uint16_t xmodKnob[4];
    uint8_t countLimit = 0;
    int waveform_number[4];    
    int ones(int n) {return (n / 100);}
    int hundredths(int n) {return (n % 100);}
    int valueToDisplay;
    int GetOutputValue(uint8_t assign) {
        if (assign < 4) {
            // LFO outputs
            return sample[assign];
        } else {
            // Gate outputs
            return gateState[assign - 4] ? HEMISPHERE_MAX_CV : 0;
        }
    }
    int8_t lfoThreshold[ch]; // Thresholds for each LFO (0-100%)
    uint8_t outputAssign[2]; // Output assignments for A and B (0-7 for LFO1-LFO4, GATE1-GATE4)    
    uint8_t clkDiv = 0; // clkDiv allows us to calculate every other tick to save cycles
    uint8_t clkDivDisplay = 0; // clkDivDisplay allows us to update the display fewer times per second
    uint8_t oldClock = 0;
    float displayFreq[ch];
    uint8_t freqLinkMul;
    uint8_t freqKnobMul;
    uint8_t freqLinkDiv;
    uint8_t freqKnobDiv;
    uint8_t mulLink;
    uint8_t divLink;
    uint8_t currentPage = 0; // 0 = main page, 1 = threshold/output page
    bool linked;
    bool bipolar;
    bool gateState[4] = {false, false, false, false};
    bool EditMode() { return isEditing; };
    const char* GetOutputLabel(uint8_t assign) {
        switch (assign) {
            case 0: return "LFO1";
            case 1: return "LFO2";
            case 2: return "LFO3";
            case 3: return "LFO4";
            case 4: return "GAT1";
            case 5: return "GAT2";
            case 6: return "GAT3";
            case 7: return "GAT4";
            default: return "???";
        }
    }

    const uint8_t SUP_THREE[3] = {0x09, 0x0D, 0x06}; // Superscript "3"
    const uint8_t SUB_FOUR[3] = {0x30, 0x20, 0xF0}; // Subscript "4"
    
};



////////////////////////////////////////////////////////////////////////////////
//// Hemisphere Applet Functions
///
///  Once you run the find-and-replace to make these refer to Relabi,
///  it's usually not necessary to do anything with these functions. You
///  should prefer to handle things in the HemisphereApplet child class
///  above.
////////////////////////////////////////////////////////////////////////////////
Relabi Relabi_instance[2];

void Relabi_Start(bool hemisphere) {Relabi_instance[hemisphere].BaseStart(hemisphere);}
void Relabi_Controller(bool hemisphere, bool forwarding) {Relabi_instance[hemisphere].BaseController(forwarding);}
void Relabi_View(bool hemisphere) {Relabi_instance[hemisphere].BaseView();}
void Relabi_OnButtonPress(bool hemisphere) {Relabi_instance[hemisphere].OnButtonPress();}
void Relabi_OnEncoderMove(bool hemisphere, int direction) {Relabi_instance[hemisphere].OnEncoderMove(direction);}
void Relabi_ToggleHelpScreen(bool hemisphere) {Relabi_instance[hemisphere].HelpScreen();}
uint64_t Relabi_OnDataRequest(bool hemisphere) {return Relabi_instance[hemisphere].OnDataRequest();}
void Relabi_OnDataReceive(bool hemisphere, uint64_t data) {Relabi_instance[hemisphere].OnDataReceive(data);}
