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

class Relabi : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Relabi";
    }

    void Start() {
        freq[0] = 200;
        freq[1] = 300;
        freq[2] = 500;
        freq[3] = 100;
        xmod[0] = 20;
        xmod[1] = 20;
        xmod[2] = 20;
        xmod[3] = 20;
        freqLinkMul = 1;
        freqLinkDiv = 1;
        freqKnobMul = 1;
        freqKnobDiv = 1;
        bipolar = true;

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

        if (clkDiv == clkCalc) {




            if (linked && hemisphere == LEFT_HEMISPHERE) {
                // Linked: read the frequency multiplier from the right hemisphere to the left hemisphere from RelabiManager.
                manager->ReadMul(mulLink);
                manager->ReadDiv(divLink);
            } else {
                mulLink = 1;
                divLink = 1;
            }




            if (linked && hemisphere == RIGHT_HEMISPHERE) {
           
                // Linked: write the frequency multiplier from the right hemisphere to the RelabiManager.
                manager->WriteMul(freqLinkMul);
                manager->WriteDiv(freqLinkDiv);


                // Linked: Receive lfo values from RelabiManager
                manager->ReadValues(sample[0], sample[1], sample[2], sample[3]);
                wave1 = (static_cast<float>(sample[2]));
                wave2 = (static_cast<float>(sample[3]));





                } else {

                    cvIn = (In(0))/51.15;
                    //Proportion(DetentedIn(0), HEMISPHERE_3V_CV, 3000
                    
                    if (oldClock != Clock(0)) { //Clock(0) is TRIG1 port
                        if (oldClock == 1) {
                            for (uint8_t pcount = 0; pcount < 4; pcount++) {
                                if (Clock(0) > 0) {
                                    int setPhase = round(phase[pcount] / 100 * 12);
                                    osc[pcount].SetPhase(setPhase);
                                }
                            }
                        }
                    }
                    oldClock = Clock(0);

                        cvIn = 10000.0 * pow(max(((cvIn / 100.0) + 1.0), 0.01) / 2.0, 1.0); //set range for cvIn to be 0 to 10000
                        for (uint8_t lfo = 0; lfo < 4; lfo++) {

                            osc[lfo].SetScale(HEMISPHERE_3V_CV);
                            // multiply an lfo's set frequency by the first cv input and by the crossmodulation amount multipled with the previous sample value of the preceding oscillator. Scale it and then add the lfo's set frequency times the cv input.
                            //float crossMod = (static_cast<float>(2.0) * xmod[lfo] * (sample[(lfo + 3) % 4]) / 921600.0) - static_cast<float>(1.0);
                            float ratioLink = static_cast<float>(mulLink) / static_cast<float>(divLink);
                            float crossMod = (static_cast<float>(xmod[lfo]) / 100.0) * ((static_cast<float>(sample[(lfo + 3) % 4]) + HEMISPHERE_3V_CV) / (2 * HEMISPHERE_3V_CV));

                            // if (crossMod < 0) {crossMod = -1 * crossMod;} // Uncomment to get cross modulation based on amplitude instead of bipolar FM.
                            
                            float setFreq =  ratioLink * (static_cast<float>(cvIn) / 2500.0 * ((freq[lfo] * crossMod) + freq[lfo])) * 16; 
                            displayFreq[lfo] = setFreq;
                            osc[lfo].SetFrequency(setFreq);
                            sample[lfo] = osc[lfo].Next();
                        }
                    
                    if (manager->IsLinked() && hemisphere == LEFT_HEMISPHERE) {

                    // Linked: Send lfo values to RelabiManager
                        manager->WriteValues(sample[0], sample[1], sample[2], sample[3]);
                    }
                    
                    // CV1 outputs LFO1 // CV2 outputs LFO2

                    
                        wave1 = (static_cast<float>(sample[0]) /*+ HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV*/); 
                        wave2 = (static_cast<float>(sample[1]) /*+ HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV*/);
                    
                }

                
            if (!bipolar) {
                wave1 = wave1 + HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV;
                wave2 = wave2 + HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV;
               }

            Out(0, wave1);
            Out(1, wave2);
            
            }

        
        clkDiv++;
        clkDiv = clkDiv %32;
        

    }

    void View() {

        if (linked && hemisphere == RIGHT_HEMISPHERE) {

            
            gfxPrint(2, 15, "FREQ");
            //Display LINKED GLOBAL FREQ label and value
            gfxPrint(26, 15, "*");
            gfxPrint(32, 15, freqKnobMul);
            gfxPrint(45, 15, "/");
            gfxPrint(50, 15, freqKnobDiv);

            gfxPrint(2, 26, "C1");
            gfxPrint(17, 26, "C2");
            gfxPrint(32, 26, "C3");
            gfxPrint(47, 26, "C4");

            int bar[4];
            for (int i = 0; i < 4; ++i) {
                bar[i] = 14.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;        
                gfxRect(2 + (15 * i), 62 - bar[i], 13, bar[i]);
            };

            switch (selectedParam) {
            case 0:
                gfxCursor(32, 23, 13);
                break;
            case 1:
                gfxCursor(51, 23, 13);
                break;
            }

        }else {

            //gfxPrint(35, 2, sample[0]);
            
            // Display OSC label and value
                gfxPrint(1, 15, "OSC");
                gfxPrint(21, 15, selectedChannel);

            // Display POLARITY
                gfxPrint(31, 15, "POL");
                if (bipolar) {
                    gfxPrint("+-");
                } else {
                    gfxPrint("+");
                        }

            
            // Display FREQ label and value
            gfxPrint(1, 26, "FREQ");
            float fDisplay = freq[selectedChannel];
            gfxPrint(1, 35, ones(fDisplay));
            if (fDisplay < 2000) {
                if (fDisplay < 199) {
                    gfxPrint(".");
                    int h = hundredths(fDisplay);
                    if (h < 10) {gfxPrint("0");}
                    gfxPrint(h);
                }
                else {
                    gfxPrint(".");
                    int t = hundredths(fDisplay);
                    t = (t / 10) % 10;
                    gfxPrint(t);
                }
            }
        
         
        
        // Display MOD label and value
        gfxPrint(31, 26, "XMOD");
        gfxPrint(31, 35, xmod[selectedChannel]);
        
        // Display PHAS label and value
        gfxPrint(1, 46, "PHAS");
        gfxPrint(1, 55, phase[selectedChannel]);




        int bar[4];
        for (int i = 0; i < 4; ++i) {
            bar[i] = 8.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;       
            gfxRect(31 + (6 * i), 62 - bar[i], 5, bar[i]);
        };


            
            switch (selectedParam) {
            case 0:
                gfxCursor(1, 23, 30);
                break;
            case 1:
                gfxCursor(31, 23, 30);
                break;
            case 2:
                gfxCursor(1, 43, 30);
                break;
            case 3:
                gfxCursor(31, 43, 30);
                break;
            case 4:
                gfxCursor(1, 63, 30);
                break;
            }
        }
    }

    void OnButtonPress() {
        if (linked && hemisphere == RIGHT_HEMISPHERE) {
            ++cursor;
            cursor = cursor % 2;
            selectedParam = cursor;
            ResetCursor();
        } else {
            ++cursor;
            cursor = cursor % 5;
            selectedParam = cursor;
            ResetCursor();
        }
    }

    void OnEncoderMove(int direction) {

        

        if (linked && hemisphere == RIGHT_HEMISPHERE) {
            switch (selectedParam) {
                //Linked and right hemisphere: controls only global frequency multiplier or divider.

                case 0: //Global frequency multiplier
                    freqKnobMul += direction;
                    freqKnobMul = freqKnobMul + 65;
                    freqKnobMul = freqKnobMul % 65; 
                    freqLinkMul = freqKnobMul;
                    break;
                case 1: //Global frequency divider
                    freqKnobDiv = (freqKnobDiv + 64 + direction - 1) % 64 + 1;  // Cycle from 1 to 64
                    freqLinkDiv = freqKnobDiv;
                    break;
            }
        } else {
            switch (selectedParam) {
                //Not linked or left hemisphere: controls select LFO, freq, xmod, and phase.
            
                case 0: // Cycle through parameters when selecting OSC
                    selectedChannel = selectedChannel + direction + 5;
                    selectedChannel = selectedChannel % 5;
                    break;
                case 1: // POLARITY(+ or +-)
                    bipolar = (bipolar + direction + 2) % 2;
                    break;
                case 2: // FREQ (0-20.0)
                    freqKnob[selectedChannel] += direction;
                    if (freqKnob[selectedChannel] < 0) {freqKnob[selectedChannel] = 510;}
                    if (freqKnob[selectedChannel] > 510) {freqKnob[selectedChannel] = 0;}
                    if (freqKnob[selectedChannel] < 200) {
                        freq[selectedChannel] = freqKnob[selectedChannel];
                    }
                    else if (freqKnob[selectedChannel] < 380) {
                        freq[selectedChannel] = 200 + ((freqKnob[selectedChannel] - 200) * 10);
                    }
                    else {
                            freq[selectedChannel] = 2000 + ((freqKnob[selectedChannel] - 380) * 100);
                        }
                    break;
                case 3: // XMOD (0-100)
                    xmodKnob[selectedChannel] += (direction);
                    xmodKnob[selectedChannel] = xmodKnob[selectedChannel] + 401;
                    xmodKnob[selectedChannel] = xmodKnob[selectedChannel] % 401;
                    xmod[selectedChannel] = xmodKnob[selectedChannel];
                    break;
                case 4: // PHAS (0-100)
                    phase[selectedChannel] += direction;
                    phase[selectedChannel] = phase[selectedChannel] + 101;
                    phase[selectedChannel] = phase[selectedChannel] % 101;
                    break;
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
            //                               "------------------" <-- Size Guide
            help[HEMISPHERE_HELP_DIGITALS] = "1=NA 2=NA";
            help[HEMISPHERE_HELP_CVS]      = "1=NA 2=NA";
            help[HEMISPHERE_HELP_OUTS]     = "A=LFO3 B=LFO4";
            help[HEMISPHERE_HELP_ENCODER]  = "GlobalFreqMul/Div";
            //            
        } else {
            //                               "------------------" <-- Size Guide
            help[HEMISPHERE_HELP_DIGITALS] = "1=Reset 2=NA";
            help[HEMISPHERE_HELP_CVS]      = "1=AllFreq 2=NA";
            help[HEMISPHERE_HELP_OUTS]     = "A=LFO1 B=LFO2";
            help[HEMISPHERE_HELP_ENCODER]  = "Freq/XMod/Phase";
            //                               "------------------" <-- Size Guide
        }
    }
    
private:
    static constexpr int pow10_lut[] = { 1, 10, 100, 1000 };
    int cursor; // 0=Freq A; 1=Cross Mod A; 2=Phase A; 3=Freq B; 4=Cross Mod B; etc.
    VectorOscillator osc[4];
    constexpr static uint8_t ch = 4;
    constexpr static uint8_t numParams = 5;
    uint8_t selectedOsc;
    float freq[ch]; // in centihertz
    uint16_t xmod[ch];
    //uint8_t selectedXmod;
    uint8_t phase[ch];
    int selectedChannel = 0;
    uint8_t selectedParam = 0;
    int sample[ch];
    float outFreq[ch];
    float freqKnob[4];
    float cvIn;
    uint16_t xmodKnob[4];
    uint8_t countLimit = 0;
    int waveform_number[4];    
    int ones(int n) {return (n / 100);}
    int hundredths(int n) {return (n % 100);}
    int valueToDisplay;
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
    bool linked;
    bool bipolar;
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
