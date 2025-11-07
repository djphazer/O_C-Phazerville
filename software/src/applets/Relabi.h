// Copyright (c) 2024, Samuel Burt
// Relabi code by Samuel Burt based on a concept by John Berndt.
//
// Copyright (c) 2018, Jason Justian
// Jason Justian created the original Ornament & Crime Hemisphere applets.
// This code is built on his initial work and from an app template. 
// 
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

#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"
#include "../HSRelabiManager.h"
#include "../PhzIcons.h"

class Relabi : public HemisphereApplet {

public:

   const char* applet_name() {
       return "Relabi";
   }

   void Start() {
       // freq[0] = 120;
       // freq[1] = 360;
       // freq[2] = 600;
       // freq[3] = 840;
       freqKnob[0] = 30; // 3 Hz
       freqKnob[1] = 50; // 5 Hz
       freqKnob[2] = 70; // 7 Hz
       //freqKnob[3] = 42; // 4.2 Hz
    //    xmodoffset = 0; // 0%
       freqKnobMul = 1; // 
       freqKnobDiv = 0; //
       freqMul = freqMulMap[freqKnobMul];
       freqDiv = freqDivMap[freqKnobDiv];
    //    bipolar = true;
    

      for (int i = 0; i < 3; i++) {
          
          float freqFloat = DecodeFreq(freqKnob[i]); 
          xmodKnob[i] = 0; // 0%
          phaseKnob[i] = 0; // 0%
          threshKnob[i] = 3; // 0%
          xmod[i] = xmodKnob[i] * 20;
          phase[i] = phaseKnob[i] * 12.5;
          thresh[i] = (threshKnob[i] * 28) - 84;

          // Optionally set oscillator or do whatever else you want:
          osc[i] = WaveformManager::VectorOscillatorFromWaveform(35);
          osc[i].SetFrequency(freqFloat);

          
      }

        outputAssign[0] = 0; // Default outputs to LFOs 1..4
        outputAssign[1] = 1;
        outputAssign[2] = 2;
        outputAssign[3] = 6; // Defaults final output to stepped CV derived from gates 0-2.
      









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
              for (uint8_t pcount = 0; pcount < 3; pcount++) {
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

              // Linked: Receive lfo values from RelabiManager to display on right
              manager->ReadValues(sample[0], sample[1], sample[2]);
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
              for (uint8_t lfo = 0; lfo < 3; lfo++) {
                  osc[lfo].SetScale(HEMISPHERE_3V_CV);

              // Use freqKnobMul and freqKnobDiv to adjust frequency
              float globalFreqFactor = static_cast<float>(freqMul) / static_cast<float>(freqDiv);


              // Calculate gate outputs based on thresholds
              if (sample[lfo] >= (thresh[lfo] * HEMISPHERE_MAX_CV) / 200) {
                  // Gate is high
                  gateState[lfo] = true;
                  } else if (sample[lfo] < (thresh[lfo] * HEMISPHERE_MAX_CV) / 200) {
                  // Signal is below or equal to the threshold, clear the gate
                  gateState[lfo] = false;
              }

              // Normalize the CV input to a range between 0 and 1
              float normalizedCV = static_cast<float>(cvIn) / 2500.0; // Adjust divisor if needed based on CV range

              // Incorporate CV2 with cross-modulation
              float xmodCombo = xmod[lfo] +  (cvIn2 / 100) - 50;
            //   float xmodCombo = xmodplus +  xmod[lfo] +  (cvIn2 / 100) - 50;

              // Calculate cross-frequency modulation factor
              float crossFreqMod = (static_cast<float>(xmodCombo) / 100.0) * (static_cast<float>(sample[(lfo + 2) % 3]) / HEMISPHERE_3V_CV);


              // Combine base frequency, cross-modulation, and CV input
              float modulatedFreq = normalizedCV * globalFreqFactor * (DecodeFreq(freqKnob[lfo]) + ((DecodeFreq(freqKnob[lfo])) * crossFreqMod));

              // float modulatedFreq = freq[lfo] + (freq[lfo] * crossFreqMod * normalizedCV);

              // Ensure the frequency stays within valid bounds
              modulatedFreq = constrain(modulatedFreq, 0.01, 15000.0); // Example: limit to 1 Hz to 20 kHz

              // Set the modulated frequency to the oscillator
              osc[lfo].SetFrequency(modulatedFreq * 1500);

              // Update sample
              sample[lfo] = osc[lfo].Next();

              // Store the display frequency for visualization
              displayFreq[lfo] = modulatedFreq;
          }


              if (manager->IsLinked() && hemisphere == LEFT_HEMISPHERE) {

                  // Linked: Send lfo values to RelabiManager
                  manager->WriteValues(sample[0], sample[1], sample[2]);
                  // Linked: Send gate values to RelabiManager
                  manager->WriteGates(gateState);
              }

              if (manager->IsLinked() && hemisphere == RIGHT_HEMISPHERE) {
                  manager->ReadValues(sample[0], sample[1], sample[2]);
                  manager->ReadGates(gateState);
              }
              
              // CV1 outputs LFO1 // CV2 outputs LFO2

                  wave1 = (static_cast<float>(sample[0]) + HEMISPHERE_3V_CV /*+ HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV*/); 
                  wave2 = (static_cast<float>(sample[1]) + HEMISPHERE_3V_CV/*+ HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV*/);
                  
              }
      }
          

        //   if (!bipolar) {
        //       wave1 = wave1 + HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV;
        //       wave2 = wave2 + HEMISPHERE_CENTER_CV + HEMISPHERE_3V_CV;
        //      } else {
        //       wave1 = wave1;
        //       wave2 = wave2;
        //      }
        int outputA;
        int outputB;

          // Set outputs based on assignments
          if (linked && hemisphere == RIGHT_HEMISPHERE) {
            outputA = GetOutputValue(outputAssign[2]);
            outputB = GetOutputValue(outputAssign[3]);
          } else {
            outputA = GetOutputValue(outputAssign[0]);
            outputB = GetOutputValue(outputAssign[1]);
          }
          Out(0, outputA);
          Out(1, outputB);

          // Out(0, wave1);
          // Out(1, wave2);
          
          clkDiv++;
          clkDiv = clkDiv %32;
   }



void View() {
    if (linked && hemisphere == RIGHT_HEMISPHERE) {
        gfxPrint(1, 55, "C:");
        DrawOutputOption(13, 55, outputAssign[2]); // OUT3

        gfxPrint(31, 55, "D:");
        DrawOutputOption(43, 55, outputAssign[3]); // OUT4


        DrawVUMetersRight();


          // Highlight selected parameter
          switch (selectedParam) {
              // case 0: HighlightParameter(2, 35, 14); break; // MULT
              // case 1: HighlightParameter(22, 35, 14); break; // DIV
              case 0: HighlightParameter(2, 63, 30); break; // OUT3
              case 1: HighlightParameter(32, 63, 30); break; // OUT4
          }
   }
   else if (currentPage == 0) {
      // Page 1: Main parameters for non-linked or left hemisphere
      gfxPrint(1, 15, "LFO");
      gfxPrint(21, 15, selectedChannel + 1);

      gfxPrint(1, 26, "FREQ");
      float fDisplay = DecodeFreq(freqKnob[selectedChannel]);
      PrintScaledFloat(2, 35, fDisplay);

      gfxPrint(31, 26, "XFM");
      gfxPrint(32, 35, xmod[selectedChannel]);

      gfxPrint(1, 46, "PHAS");
      gfxPrint(2, 55, phase[selectedChannel]);

      gfxPrint(31, 46, "THRS");
      gfxPrint(32, 55, thresh[selectedChannel]);

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
   else if (currentPage == 1) {
       // Page 2: Clock mult, Polarity, and Output page

      gfxPrint(1, 15, "ALL LFOs");
      gfxPrint(2, 25, "FREQx ");
      gfxPrint(1, 35, freqMul);

      gfxPrint(16, 35, "/");
      gfxPrint(22, 35, freqDiv);

    //   gfxPrint(36, 26,  "XFM");
    //   gfxPrint(37, 35, xmodplus); // ALL CROSS_MODULATION

    //   gfxPrint(1, 46, "POL");
    //   gfxPrint(20, 46, bipolar ? "+-" : "+ ");

      gfxPrint(1, 55, "A:");
      DrawOutputOption(13, 55, outputAssign[0]); // OUT1

      gfxPrint(31, 55, "B:");
      DrawOutputOption(43, 55, outputAssign[1]); // OUT2


      // Highlight selected parameter (use your original locations)
      switch (selectedParam) {
          case 5: HighlightParameter(1, 44, 14); break; // MULT
          case 6: HighlightParameter(22, 44, 14); break; // DIV
        //   case 7: HighlightParameter(36, 44, 20); break; // ALLMOD
        //   case 8: HighlightParameter(1, 54, 35); break; // POL
          case 7: HighlightParameter(2, 63, 30); break; // OUT1
          case 8: HighlightParameter(32, 63, 30); break; // OUT2
      }
   }

}


void DrawOutputOption(int x, int y, uint8_t assign) {
    if (assign < 3) {
        // LFO output
        gfxBitmap(x, y, 8, WAVEFORM_ICON);
        const uint8_t* subscript = (assign == 0) ? SUP_ONE : (assign == 1) ? SUB_TWO : SUP_THREE;
        gfxBitmap(x + 9, y, 3, subscript);
    } else if (assign < 6) {
        // Gate output
        gfxBitmap(x, y, 8, GATE_ICON);
        const uint8_t* subscript = (assign == 3) ? SUP_ONE : (assign == 4) ? SUB_TWO :  SUP_THREE;
        gfxBitmap(x + 9, y, 3, subscript);
    } else if (assign == 6) {
        // Gate Combo output
        gfxBitmap(x, y, 8, STAIRS_ICON);
    } else if (assign == 7) {
        // All Gates to Triggers output
        gfxBitmap(x, y, 8, CLOCK_ICON);
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
    int bar[3];
    for (int i = 0; i < 3; ++i) {
        // Calculate bar height based on sample value (assuming bipolar -3V to +3V range)
        bar[i] = 14.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;

      // Draw vertical bars (adjust x-position and width as needed)
      gfxRect(2 + (20 * i), 42 - bar[i], 18, bar[i]);
   }
}

void DrawVUMetersLeft() {
    int bar[4];
    for (int i = 0; i < 3; ++i) {
        // Calculate bar height based on sample value (assuming bipolar -3V to +3V range)
        // Smaller bars for left hemisphere
        bar[i] = 4.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;
        gfxRect(31 + (10 * i), 22 - bar[i], 9, bar[i]); // Adjusted for smaller size

   }
}


void PrintScaledFloat(int x, int y, float value) {
    // Clamp the value to a safe range
    if (value < 0.0f) value = 0.0f;
    if (value > 150.0f) value = 150.0f;

   // Break the value into whole and fractional parts
   int whole = (int)value;                  // Integer part
   int fract = (int)((value - whole) * 100 + 0.5f); // Fractional part, rounded

   // Remove trailing zeros from the fractional part
   if (fract == 0) {
       // Display just the whole number if fract is zero
       char buf[12];
       snprintf(buf, sizeof(buf), "%d", whole);
       gfxPrint(x, y, buf);
   } else if (fract % 10 == 0) {
       // Display with one decimal place if fract ends with zero
       char buf[12];
       snprintf(buf, sizeof(buf), "%d.%d", whole, fract / 10);
       gfxPrint(x, y, buf);
   } else {
       // Display with two decimal places
       char buf[12];
       snprintf(buf, sizeof(buf), "%d.%02d", whole, fract);
       gfxPrint(x, y, buf);
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
    int max_param = (linked && hemisphere == RIGHT_HEMISPHERE) ? 1 : 8;

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
                      outputAssign[2] = (outputAssign[2] + direction + 8) % 8;
                      break;
                  case 1: // OUT4 assignment
                      outputAssign[3] = (outputAssign[3] + direction + 8) % 8;
                      break;
              }
          } else {
              // Not linked or LEFT Hemisphere: Full parameter set (0–4)
              switch (selectedParam) {
                case 0: // Cycle through OSC selection
                      selectedChannel = (selectedChannel + direction + 3) % 3;
                      break;

                case 1: // FREQ
                      {
                          // Step A: move the index by +1 or -1
                          // Because freqKnob[] is uint8_t, we can do modulo arithmetic:
                          int16_t temp = freqKnob[selectedChannel] + direction;
                          if (temp < 0) temp += 64;  // wrap downward
                          else if (temp > 63) temp -= 64; // wrap upward

                          freqKnob[selectedChannel] = (uint8_t)temp;

                          // Step B: decode to float frequency
                          float freqVal = DecodeFreq(freqKnob[selectedChannel]);

                          // Store to freqFixed so the Controller() loop picks up the new freq
                          // freqFixed[selectedChannel] = (uint16_t)(freqVal+ 0.5f);

                          // Step C: set oscillator frequency
                          osc[selectedChannel].SetFrequency(freqVal);

                          break;
                      }



                case 2: // XMOD (0–100) scaling
                      xmodKnob[selectedChannel] += direction;
                      xmodKnob[selectedChannel] = (xmodKnob[selectedChannel] + 8) % 8;
                      xmod[selectedChannel] = xmodKnob[selectedChannel] * 20;
                      break;

                case 3: // PHAS (0–100)
                      phaseKnob[selectedChannel] += direction;
                      phaseKnob[selectedChannel] = (phaseKnob[selectedChannel] + 8) % 8;
                      phase[selectedChannel] = phaseKnob[selectedChannel] * 12.5;
                      break;

                case 4: // THRS adjustment
                    // Correctly wrap around using (current + direction + total) % total
                    threshKnob[selectedChannel] = (threshKnob[selectedChannel] + direction + 7) % 7;
                    thresh[selectedChannel] = (threshKnob[selectedChannel] * 28) - 84;
                    break;

              }
          }
      } else if (currentPage == 1) {
          // Page 1: THRES, OUT1, OUT2
          switch (selectedParam) {

              case 5: // Global frequency multiplier
                  freqKnobMul += direction;
                  freqKnobMul = freqKnobMul + 8;
                  freqKnobMul = freqKnobMul % 8; 
                  freqMul = freqMulMap[freqKnobMul];
                  break;
              case 6: // Global frequency divider
                  freqKnobDiv += direction;
                  freqKnobDiv = freqKnobDiv + 8;
                  freqKnobDiv = freqKnobDiv % 8;
                  freqDiv = freqDivMap[freqKnobDiv];
                  break;
            //   case 7: // Global cross modulation offset
            //       xmodoffset += direction;
            //       xmodoffset = (xmodoffset + 8) % 8;
            //       xmodplus = xmodoffset * 20;
            //       break;
            //   case 8: // POLARITY (+ or +-)
            //       bipolar = (bipolar + direction + 2) % 2;
            //       break;
              case 7: // OUT1 assignment
                  outputAssign[0] = (outputAssign[0] + direction + 8) % 8;
                  break;
              case 8: // OUT2 assignment
                  outputAssign[1] = (outputAssign[1] + direction + 8) % 8;
                  break;
          }
      }
   }
}

// uint64_t OnDataRequest() {}
// void OnDataReceive(uint64_t data) {}

uint64_t OnDataRequest() {
    uint64_t data = 0;
    int pos = 0; // bit offset

    // 1) freqKnob[3], 6 bits each → 18 bits total
    for (int i = 0; i < 3; i++) {
        Pack(data, PackLocation{pos, 6}, freqKnob[i]);
        pos += 6;
    }

    // 2) xmodKnob[3], 3 bits each → 9 bits total
    for (int i = 0; i < 3; i++) {
        Pack(data, PackLocation{pos, 3}, xmodKnob[i]);
        pos += 3;
    }

    // 3) phaseKnob[3], 3 bits each → 9 bits total
    for (int i = 0; i < 3; i++) {
        Pack(data, PackLocation{pos, 3}, phaseKnob[i]);
        pos += 3;
    }

    // 4) threshKnob[3], 3 bits each → 9 bits total
    for (int i = 0; i < 3; i++) {
        Pack(data, PackLocation{pos, 3}, threshKnob[i]);
        pos += 3;
    }

    // 5) freqKnobMul → 3 bits
    Pack(data, PackLocation{pos, 3}, freqKnobMul);
        pos += 3;

    // 6) freqKnobDiv → 4 bits
    Pack(data, PackLocation{pos, 3}, freqKnobDiv);
        pos += 3;

    // // 7) xmodoffset → 3 bits
    // Pack(data, PackLocation{pos, 3}, xmodoffset);
    //     pos += 3;

    // 7) outputAssign → 3 bits each → 12 bits
    for (int i =0; i < 4; i++) {
        Pack(data, PackLocation{pos, 3}, outputAssign[i]);
        pos += 3;
    }

   

    return data;
}

void OnDataReceive(uint64_t data) {
    int pos = 0; // bit offset

    // 1) freqKnob[3]
    for (int i = 0; i < 3; i++) {
        freqKnob[i] = Unpack(data, PackLocation{pos, 6});
        pos += 6;
    }

    // 2) xmodKnob[4]
    for (int i = 0; i < 3; i++) {
        xmodKnob[i] = Unpack(data, PackLocation{pos, 3});
        pos += 3;
    }

    // 3) phaseKnob[4]
    for (int i = 0; i < 3; i++) {
        phaseKnob[i] = Unpack(data, PackLocation{pos, 3});
        pos += 3;
    }

    // 4) threshKnob[4]
    for (int i = 0; i < 3; i++) {
        threshKnob[i] = Unpack(data, PackLocation{pos, 3});
        pos += 3;
    }

    // 5) freqKnobMul
    freqKnobMul = Unpack(data, PackLocation{pos, 3});
    pos += 3;

    // 6) freqKnobDiv
    freqKnobDiv = Unpack(data, PackLocation{pos, 3});
    pos += 3;

    // // 7) xmodoffset
    // xmodoffset = Unpack(data, PackLocation{pos, 3});
    // pos += 3;

    // 7) outputAssign
    for (int i =0; i < 4; i++) {
        outputAssign[i] = Unpack (data, PackLocation{pos, 3});
        pos += 3;
    }

    freqMul = freqMulMap[freqKnobMul];
    freqDiv = freqDivMap[freqKnobDiv];
    //    bipolar = true;
    

    for (int i = 0; i < 3; i++) {
          
        // float freqFloat = DecodeFreq(freqKnob[i]); 
        xmod[i] = xmodKnob[i] * 20;
        phase[i] = phaseKnob[i] * 12.5;
        thresh[i] = (threshKnob[i] * 28) - 84;
    }

}





protected:
    void SetHelp() {
        if (linked && hemisphere == RIGHT_HEMISPHERE) {
            help[HELP_DIGITAL1] = "";
            help[HELP_DIGITAL2] = "";
            help[HELP_CV1]      = "";
            help[HELP_CV2]      = "";
            help[HELP_OUT1]     = "C";
            help[HELP_OUT2]     = "D";
            help[HELP_EXTRA1]   = "";
            help[HELP_EXTRA2]   = "";
        } else {
            help[HELP_DIGITAL1] = "Reset";
            help[HELP_DIGITAL2] = "Phase";
            help[HELP_CV1]      = "AllFreq";
            help[HELP_CV2]      = "AllXmod";
            help[HELP_OUT1]     = "LFO 1";
            help[HELP_OUT2]     = "LFO 2";
            help[HELP_EXTRA1]   = "Set: Frq/XFM/Phs/Thrs";
            help[HELP_EXTRA2]   = "";
        }
    }


private:
    static constexpr int pow10_lut[] = { 1, 10, 100, 1000 };
    int cursor; // 0=Freq A; 1=Cross Mod A; 2=Phase A; 3=Freq B; 4=Cross Mod B; etc.
    bool isEditing = false;  // Indicates if the user is editing a parameter
    uint8_t modal_edit_mode = 2;
    VectorOscillator osc[3];
    constexpr static uint8_t ch = 3;
    constexpr static uint8_t numParams = 5;
    uint8_t selectedOsc;

   // parameters to be saved and loaded
   //float freq[ch]; // in centihertz
   uint8_t freqKnob[ch]; // 18 bits (6 each) // Each 0..19.5
   uint8_t xmodKnob[ch]; // 9 bits (3 each) // Each 0..140%
   int8_t phaseKnob[ch]; // 9 bits (3 each) // Each 0..87%
   int8_t threshKnob[ch]; // 9 bits (3 each) // Thresholds for each LFO (-84%..84%)
   uint8_t freqKnobMul; // 3 bits // All freqs x 0..14
   uint8_t freqKnobDiv; // 3 bits // All freqs / 1, 2, 3, 4, 8, 16, 32, 64
//    uint8_t xmodoffset; // 3 bits // All xmod + 0..140%
   uint8_t outputAssign[4]; // 12 bits (3 each) // Output assignments for A and B (0-7 for LFO1-LFO4, GATE1-GATE4) 

//    uint8_t xmodplus;
   //uint8_t selectedXmod;

   int selectedChannel = 0;
   int selectedParam = 0;
   int sample[ch];
   float outFreq[ch];
   // uint8_t freqKnob[4];
   float cvIn;
   float cvIn2;  
   uint8_t xmod[ch];
   uint8_t phase[ch];
   int8_t thresh[ch];
   uint8_t countLimit = 0;
   uint8_t freqMul;
   uint32_t freqDiv;
   // int waveform_number[4];    
   int ones(int n) {return (n / 100);}
   int hundredths(int n) {return (n % 100);}
   int valueToDisplay;
    // Pulse state and duration management
    bool triggerActive = false;         // Tracks whether the pulse is currently active
    uint32_t triggerStartTime = 0;      // Tracks the start time of the pulse
    bool previousGateState[3] = {false, false, false}; // Stores the previous state of the gates

    int GetOutputValue(uint8_t assign) {
        if (assign < 3) {
            // LFO outputs
            return sample[assign] + HEMISPHERE_3V_CV;
        } else if (assign < 6) {
            // Gate outputs
            return gateState[assign - 3] ? HEMISPHERE_MAX_CV : 0;
        } else if (assign == 6) {
            // Stepped CV from gate states
            uint8_t gateCombo = (gateState[0] ? 1 : 0)       // Bit 0
                            | (gateState[1] ? 1 : 0) << 1  // Bit 1
                            | (gateState[2] ? 1 : 0) << 2; // Bit 2
            const int scaleFactor = HEMISPHERE_MAX_CV / 7;
            return gateCombo * scaleFactor;
        } else if (assign == 7) {
            // Generate a momentary pulse when any gate changes state
            uint32_t currentTime = OC::CORE::ticks; // Get the current tick count

            // Detect if any gate state has changed
            bool gateChanged = false;
            for (int i = 0; i < 3; i++) {
                if (gateState[i] != previousGateState[i]) {
                    gateChanged = true;
                    previousGateState[i] = gateState[i]; // Update the previous state
                }
            }

            if (gateChanged) {
                // Start the pulse if a gate change is detected
                triggerActive = true;
                triggerStartTime = currentTime;
                return HEMISPHERE_MAX_CV; // Output high voltage
            }

            // Check if the pulse duration has elapsed (e.g., 50ms)
            if (triggerActive && (currentTime - triggerStartTime < 50)) {
                return HEMISPHERE_MAX_CV; // Keep high voltage during the pulse
            } else {
                // End the pulse
                triggerActive = false;
                return 0; // Return to 0V
            }
        } else {
            return 0; // Default output
        }
    }

   uint8_t clkDiv = 0; // clkDiv allows us to calculate every other tick to save cycles
   uint8_t clkDivDisplay = 0; // clkDivDisplay allows us to update the display fewer times per second
   uint8_t oldClock = 0;
   float displayFreq[ch];
   // uint8_t freqLinkMul;
   // uint8_t freqLinkDiv;
   uint8_t mulLink;
   uint8_t divLink;
   const int freqMulMap[8] = {0, 1, 2, 3, 4, 6, 8, 12};
   const int freqDivMap[8] = {1, 2, 3, 4, 8, 12, 16, 32};
   uint8_t currentPage = 0; // 0 = main page, 1 = threshold/output page
   bool linked;
    //    bool bipolar;
   bool gateState[4] = {false, false, false, false};
   bool EditMode() { return isEditing; };

   float DecodeFreq(uint8_t index) {
    // 0 => 0.0 Hz, 29 => 2.9Hz, 63 => 18.5 Hz
       if (index < 30) {
          return index * 0.1f;
       } else {
          return 3 + ((index - 30) * 0.5f);
       }
   }

   uint8_t EncodeFreq(float f) {
       // clamp to 0..25.5
       if (f < 0.0f)  f = 0.0f;
       if (f > 25.5f) f = 25.5f;
       // each step = 0.1 Hz
       return (uint8_t)roundf(f * 10.0f);
   }


   const char* GetOutputLabel(uint8_t assign) {
       switch (assign) {
           case 0: return "LFO1";
           case 1: return "LFO2";
           case 2: return "LFO3";
           case 3: return "GAT1";
           case 4: return "GAT2";
           case 5: return "GAT3";
           default: return "???";
       }
   }

   const uint8_t SUP_THREE[3] = {0x09, 0x0D, 0x06}; // Superscript "3"
   const uint8_t SUB_FOUR[3] = {0x30, 0x20, 0xF0}; // Subscript "4"

};

