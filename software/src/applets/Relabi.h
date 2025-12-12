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

#include "../HSRelabiManager.h"
#include "../vector_osc/HSVectorOscillator.h"
#include "../vector_osc/WaveformManager.h"

class Relabi : public HemisphereApplet {

public:
  static constexpr int PROCESS_TICKS = 4;

  const char* applet_name() {
    return "Relabi";
  }

  void Start() {
    freqKnob[0] = 30; // 3 Hz
    freqKnob[1] = 34; // 5 Hz
    freqKnob[2] = 38; // 7 Hz
    freqKnobMul = 1; //
    freqKnobDiv = 0; //

    for (int i = 0; i < 3; i++) {
      xmodKnob[i] = 1; // 20%
      phaseKnob[i] = 0; // 0%
      threshKnob[i] = 3; // 0%

      // Set oscillator to sine wave
      osc[i] = WaveformManager::VectorOscillatorFromWaveform(35);
      osc[i].SetFrequency( DecodeFreq(freqKnob[i]) * 100 * PROCESS_TICKS );
      osc[i].SetScale(HEMISPHERE_3V_CV);
    }

    outputAssign[0] = 0; // Default outputs to LFOs 1..4
    outputAssign[1] = 1;
    outputAssign[2] = 2;
    outputAssign[3] = 6; // Defaults final output to stepped CV derived from gates 0-2.

    // This could simplify registration, but requires AllowRestart()
    // ...which means params would be reset to defaults every time you switch away and come back.
    //manager.Register(hemisphere);
  }
  void Unload() {
    manager.Unload(hemisphere);
  }

  void Controller() {
    manager.Register(hemisphere);

    linked = manager.IsLinked();
    const bool link_follow = (linked && (hemisphere & 1));

    if (Clock(0)) { // Rising edge detected on TRIG1 input
      for (uint8_t pcount = 0; pcount < 3; pcount++) {
        // Get the total number of segments in the waveform
        uint8_t totalSegments = osc[pcount].GetSegment(0).Segments(); // Use first segment's TOC
        // Calculate phase position based on phase percentage (0–100)
        int setPhase = round((phase(pcount) / 100.0) * totalSegments);
        // Reset the phase of the oscillator
        osc[pcount].Reset(setPhase);
      }
    }

    // stagger calculations between ticks, to avoid excessive ISR processing
    uint8_t clkCalc = (hemisphere & 1)? 3 : 1;
    ++clkDiv %= PROCESS_TICKS;
    if (clkDiv == clkCalc) {

      if (link_follow) {
        // Linked as Follower: Receive lfo values from RelabiManager to display on right
        manager.ReadValues(sample[0], sample[1], sample[2]);
        manager.ReadGates(gateState);
      } else {
        float normalizedCV = InF(0, HEMISPHERE_3V_CV); // -1..1 incoming fm amount
        float normalizedCV2 = InF(1, HEMISPHERE_3V_CV); // -1..1 incoming xmod amount
        normalizedCV = powf(10.0f, normalizedCV * 2); // 0.01..100
        float fModCV = constrain(normalizedCV, 0.01f, 100.0f);
        float xModCV = constrain(normalizedCV2, 0.0f, 1.0f);

        // frequency modulation:
        for (uint8_t lfo = 0; lfo < 3; lfo++) {
          // Calculate gate outputs based on thresholds
          if (sample[lfo] >= (thresh(lfo) * HEMISPHERE_MAX_CV) / 200) {
            // Gate is high
            gateState[lfo] = true;
          } else if (sample[lfo] < (thresh(lfo) * HEMISPHERE_MAX_CV) / 200) {
            // Signal is below or equal to the threshold, clear the gate
            gateState[lfo] = false;
          }

          // Incorporate CV2 with cross-modulation
          float xmodCombo = xmod(lfo) + xModCV * 100; // 0..140 + -100..100 = -100..240
          // Previously
          // float xmodCombo = xmod(lfo) +  (cvIn2 / 100) - 50; //???!

          // Calculate cross-frequency modulation factor
          float crossFreqMod = (xmodCombo / 100.0)
            * (static_cast<float>(sample[(lfo + 2) % 3]) / HEMISPHERE_3V_CV);

          // Combine base frequency, cross-modulation, and CV input
          float freq = DecodeFreq(freqKnob[lfo]);
          freq = fModCV * (freq + (freq * crossFreqMod));

          // Scale frequency by global freqMul and freqDiv amounts.
          freq = freq * freqMulMap[freqKnobMul] / freqDivMap[freqKnobDiv];

          // Ensure the frequency stays within valid bounds
          // freq = constrain(freq, 0.01, 15000.0);

          // Set the modulated frequency to the oscillator
          osc[lfo].SetFrequency(
            freq * 100 * PROCESS_TICKS
          ); // VectorOscillator uses centiHertz.

          // Update sample
          sample[lfo] = osc[lfo].Next();
        }

        if (linked) {
          // we're inside the "else" clause of (link_follow) so this must be the leader
          // Leader is Linked: Send lfo values and gates to RelabiManager
          manager.WriteValues(sample[0], sample[1], sample[2]);
          manager.WriteGates(gateState);
        }
      }
    }

    // A momentary trigger pulse when any gate changes state
    bool trigout = GateStateChanged();

    // Set outputs based on assignments
    ForEachChannel(ch) {
      int assign = outputAssign[ch + link_follow*2];
      if (assign == 7) { // trigger out
        if (trigout) ClockOut(ch);
      } else
        Out(ch, GetOutputValue(assign));
    }
  }

  void View() {
    if (linked && (hemisphere & 1)) {
      gfxPrint(1, 55, "C:");
      DrawOutputOption(13, 55, outputAssign[2]); // OUT3

      gfxPrint(31, 55, "D:");
      DrawOutputOption(43, 55, outputAssign[3]); // OUT4

      DrawVUMetersRight();

      // Highlight selected parameter
      switch (cursor) {
        case 0:
          gfxCursor(2, 63, 30);
          break; // OUT3
        case 1:
          gfxCursor(32, 63, 30);
          break; // OUT4
      }
      return;
    }

    int currentPage = (cursor > 4);
    if (currentPage == 0) {
      // Page 1: Main parameters for non-linked or left hemisphere
      gfxPrint(1, 15, "LFO");
      gfxPrint(21, 15, selectedChannel + 1);

      gfxPrint(1, 26, "FREQ");
      float fDisplay = DecodeFreq(freqKnob[selectedChannel]);
      PrintScaledFloat(2, 35, fDisplay);

      gfxPrint(31, 26, "XFM");
      gfxPrint(32, 35, xmod(selectedChannel));

      gfxPrint(1, 46, "PHAS");
      gfxPrint(2, 55, phase(selectedChannel));

      gfxPrint(31, 46, "THRS");
      gfxPrint(32, 55, thresh(selectedChannel));

      // Highlight selected parameter
      switch (cursor) {
        case 0:
          gfxCursor(1, 23, 30);
          break; // OSC
        case 1:
          gfxCursor(2, 43, 30);
          break; // FREQ
        case 2:
          gfxCursor(32, 43, 30);
          break; // XMOD
        case 3:
          gfxCursor(2, 63, 30);
          break; // PHAS
        case 4:
          gfxCursor(32, 63, 30);
          break; // THRS
      }
      DrawVUMetersLeft();

    } else if (currentPage == 1) {
      // Page 2: Clock mult, Polarity, and Output page

      gfxPrint(1, 15, "ALL LFOs");
      gfxPrint(2, 25, "FREQx ");
      gfxPrint(1, 35, freqMulMap[freqKnobMul]);

      gfxPrint(16, 35, "/");
      gfxPrint(22, 35, freqDivMap[freqKnobDiv]);

      gfxPrint(1, 55, "A:");
      DrawOutputOption(13, 55, outputAssign[0]); // OUT1

      gfxPrint(31, 55, "B:");
      DrawOutputOption(43, 55, outputAssign[1]); // OUT2

      // Highlight selected parameter (use your original locations)
      switch (cursor) {
        case 5: // MULT
          gfxCursor(1, 44, 14);
          break;
        case 6: // DIV
          gfxCursor(22, 44, 14);
          break;
        case 7: // OUT1
          gfxCursor(2, 63, 30);
          break;
        case 8: // OUT2
          gfxCursor(32, 63, 30);
          break;
      }
    }
  }

  void DrawOutputOption(int x, int y, uint8_t assign) {
    if (assign < 3) {
      // LFO output
      gfxBitmap(x, y, 8, WAVEFORM_ICON);
      gfxPrint(x + 9, y, assign + 1);
    } else if (assign < 6) {
      // Gate output
      gfxBitmap(x, y, 8, GATE_ICON);
      gfxPrint(x + 9, y, assign - 2);
    } else if (assign == 6) {
      // Gate Combo output
      gfxBitmap(x, y, 8, STAIRS_ICON);
    } else if (assign == 7) {
      // All Gates to Triggers output
      gfxBitmap(x, y, 8, CLOCK_ICON);
    }
  }

  void DrawVUMetersRight() {
    int bar[3];
    for (int i = 0; i < 3; ++i) {
      // Calculate bar height based on sample value (assuming bipolar -3V to +3V
      // range)
      bar[i] = 14.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;

      // Draw vertical bars (adjust x-position and width as needed)
      gfxRect(2 + (20 * i), 42 - bar[i], 18, bar[i]);
    }
  }

  void DrawVUMetersLeft() {
    int bar[4];
    for (int i = 0; i < 3; ++i) {
      // Calculate bar height based on sample value (assuming bipolar -3V to +3V
      // range) Smaller bars for left hemisphere
      bar[i] = 4.0 * (sample[i] + HEMISPHERE_3V_CV) / HEMISPHERE_3V_CV;
      gfxRect(
        31 + (10 * i), 22 - bar[i], 9, bar[i]
      ); // Adjusted for smaller size
    }
  }

  void PrintScaledFloat(int x, int y, float value) {
    // Clamp value to 0..150
    CONSTRAIN(value, 0.0f, 150.0f);

    char buf[12]; // enough for "150\0" or "99.9\0"

    gfxPos(x, y);
    if (value >= 100.0f) {
      // 3-digit number, no fraction
      int whole = static_cast<int>(value + 0.5f);
      graphics.printf("%3d", whole); // "100", "150"
    } else {
      // Show up to 2-digit whole + 1 decimal
      int scaled = static_cast<int>(value * 10 + 0.5f); // round to 1 decimal
      int whole = scaled / 10;
      int frac = scaled % 10;
      graphics.printf("%u.%u", whole, frac); // always "X.Y" or "XX.Y"
    }
  }

  //void OnButtonPress() { }

  void OnEncoderMove(int direction) {
    // Determine how many parameters we have based on linkage and hemisphere
    // linked && RIGHT_HEMISPHERE: 2 parameters (0 and 1)
    // otherwise: adjust for both pages (parameters 0–7)
    const bool link_follow = (linked && (hemisphere & 1));
    int max_param = link_follow ? 1 : 8;

    if (!EditMode()) {
      // Not editing: move the cursor through the available parameters
      MoveCursor(cursor, direction, max_param);
      return;
    }

    // Editing: adjust parameters based on the current page and cursor

    // Determine the current page based on cursor position
    // Page 0: Main parameters
    // Page 1: THRES, OUT1, OUT2
    int currentPage = (cursor > 4);

    if (currentPage == 0) {
      // Page 0: Main parameters
      if (link_follow) {
        // Linked and RIGHT side: Only global frequency multiplier or divider
        switch (cursor) {
          case 0: // OUT3 assignment
            outputAssign[2] = constrain(outputAssign[2] + direction, 0, 7);
            break;
          case 1: // OUT4 assignment
            outputAssign[3] = constrain(outputAssign[3] + direction, 0, 7);
            break;
        }
      } else {
        // Not linked or LEFT Hemisphere: Full parameter set (0–4)
        switch (cursor) {
          case 0: // Cycle through OSC selection
            selectedChannel = constrain(selectedChannel + direction, 0, 2);
            break;

          case 1: // FREQ
            freqKnob[selectedChannel] = constrain(freqKnob[selectedChannel] + direction, 0, 63);
            break;

          case 2: // XMOD (0–100) scaling
            xmodKnob[selectedChannel] = constrain(xmodKnob[selectedChannel] + direction, 0, 7);
            break;

          case 3: // PHAS (0–100)
            phaseKnob[selectedChannel] = constrain(phaseKnob[selectedChannel] + direction, 0, 7);
            break;

          case 4: // THRS adjustment
            threshKnob[selectedChannel] = constrain(threshKnob[selectedChannel] + direction, 0, 6);
            break;
        }
      }
    } else if (currentPage == 1) {
      // Page 1: THRES, OUT1, OUT2
      switch (cursor) {

        case 5: // Global frequency multiplier
          freqKnobMul = constrain(freqKnobMul + direction, 0, 7);
          break;
        case 6: // Global frequency divider
          freqKnobDiv = constrain(freqKnobDiv + direction, 0, 7);
          break;
        case 7: // OUT1 assignment
          outputAssign[0] = constrain(outputAssign[0] + direction, 0, 7);
          break;
        case 8: // OUT2 assignment
          outputAssign[1] = constrain(outputAssign[1] + direction, 0, 7);
          break;
      }
    }
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    for (size_t i = 0; i < 3; i++) {
      // 1) freqKnob[3], 6 bits each → 18 bits total
      Pack(data, PackLocation{0 + i*6, 6}, freqKnob[i]);
      // 2) xmodKnob[3], 3 bits each → 9 bits total
      Pack(data, PackLocation{18 + i*3, 3}, xmodKnob[i]);
      // 3) phaseKnob[3], 3 bits each → 9 bits total
      Pack(data, PackLocation{27 + i*3, 3}, phaseKnob[i]);
      // 4) threshKnob[3], 3 bits each → 9 bits total
      Pack(data, PackLocation{36 + i*3, 3}, threshKnob[i]);
    }
    // 5) freqKnobMul → 3 bits
    Pack(data, PackLocation{45, 3}, freqKnobMul);
    // 6) freqKnobDiv → 3 bits
    Pack(data, PackLocation{48, 3}, freqKnobDiv);
    // 7) outputAssign → 3 bits each → 12 bits
    for (size_t i = 0; i < 4; i++) {
      Pack(data, PackLocation{51 + i*3, 3}, outputAssign[i]);
    }
    return data;
  }

  void OnDataReceive(uint64_t data) {
    for (size_t i = 0; i < 3; i++) {
      freqKnob[i] = Unpack(data, PackLocation{0 + i*6, 6});
      xmodKnob[i] = Unpack(data, PackLocation{18 + i*3, 3});
      phaseKnob[i] = Unpack(data, PackLocation{27 + i*3, 3});
      threshKnob[i] = Unpack(data, PackLocation{36 + i*3, 3});
    }

    freqKnobMul = Unpack(data, PackLocation{45, 3});
    freqKnobDiv = Unpack(data, PackLocation{48, 3});

    for (size_t i = 0; i < 4; i++) {
      outputAssign[i] = Unpack(data, PackLocation{51 + i*3, 3});
    }
  }

protected:
  void SetHelp() {
    help[HELP_DIGITAL1] = "Reset";
    help[HELP_DIGITAL2] = "";
    if (linked && (hemisphere & 1)) {
      help[HELP_CV1] = "";
      help[HELP_CV2] = "";
      help[HELP_OUT1] = GetOutputLabel(outputAssign[2]);
      help[HELP_OUT2] = GetOutputLabel(outputAssign[3]);
      help[HELP_EXTRA2] = "Set: OutC/OutD";
    } else {
      help[HELP_CV1] = "AllFreq";
      help[HELP_CV2] = "AllXmod";
      help[HELP_OUT1] = GetOutputLabel(outputAssign[0]);
      help[HELP_OUT2] = GetOutputLabel(outputAssign[1]);
      help[HELP_EXTRA1] = "Set: Frq/XFM/Phs/Thrs";
      help[HELP_EXTRA2] = "P2: Mul/Div/OutA/OutB";
    }
  }

private:
  constexpr static int CHAN_COUNT = 3;
  constexpr static int numParams = 5;
  const int freqMulMap[8] = {0, 1, 2, 3, 4, 6, 8, 12};
  const int freqDivMap[8] = {1, 2, 3, 4, 8, 12, 16, 32};

  RelabiManager& manager = RelabiManager::get();
  VectorOscillator osc[3];

  // parameters to be saved and loaded
  uint8_t freqKnob[CHAN_COUNT]; // 18 bits (6 each) // Each 0..19.5
  uint8_t xmodKnob[CHAN_COUNT]; // 9 bits (3 each) // Each 0..140%
  int8_t phaseKnob[CHAN_COUNT]; // 9 bits (3 each) // Each 0..87%
  int8_t threshKnob[CHAN_COUNT]; // 9 bits (3 each) // Thresholds for each LFO (-84%..84%)
  uint8_t freqKnobMul; // 3 bits // All freqs x 0..14
  uint8_t freqKnobDiv; // 3 bits // All freqs / 1, 2, 3, 4, 8, 16, 32, 64
  //    uint8_t xmodoffset; // 3 bits // All xmod + 0..140%
  uint8_t outputAssign[4]; // 12 bits (3 each) // Output assignments for A and B
                           // (0-7 for LFO1-LFO4, GATE1-GATE4)

  int selectedChannel = 0;
  int cursor = 0;
  int sample[CHAN_COUNT];

  uint8_t clkDiv = 0; // clkDiv allows us to calculate every other tick to save cycles

  bool linked;
  //    bool bipolar;
  bool gateState[3] = {false, false, false};
  bool previousGateState[3] = {false, false, false}; // Stores the previous state of the gates

  const uint8_t xmod(int idx) const { return xmodKnob[idx] * 20; }
  const uint8_t phase(int idx) const { return phaseKnob[idx] * 12.5; }
  const int8_t thresh(int idx) const { return (threshKnob[idx] * 28) - 84; }

  int GetOutputValue(uint8_t assign) {
    if (assign < 3) {
      // LFO outputs
      return sample[assign] + HEMISPHERE_3V_CV;
    } else if (assign < 6) {
      // Gate outputs
      return gateState[assign - 3] ? HEMISPHERE_MAX_CV : 0;
    } else if (assign == 6) {
      // Stepped CV from gate states
      uint8_t gateCombo = (gateState[0] ? 1 : 0) // Bit 0
        | (gateState[1] ? 1 : 0) << 1 // Bit 1
        | (gateState[2] ? 1 : 0) << 2; // Bit 2
      const int scaleFactor = HEMISPHERE_MAX_CV / 7;
      return gateCombo * scaleFactor;
    }
    return 0; // Default output
  }

  const bool GateStateChanged() {
    bool gateChanged = false;
    for (int i = 0; i < 3; i++) {
      if (gateState[i] != previousGateState[i]) {
        gateChanged = true;
        previousGateState[i] = gateState[i]; // Update the previous state
      }
    }
    return gateChanged;
  }

  constexpr float DecodeFreq(uint8_t index) {
    // 0 => 0.0 Hz, 29 => 2.9Hz, 63 => 18.5 Hz
    if (index < 30) {
      return index * 0.1f;
    } else {
      return 3 + ((index - 30) * 0.5f);
    }
  }

  constexpr uint8_t EncodeFreq(float f) {
    // clamp to 0..25.5
    if (f < 0.0f) f = 0.0f;
    if (f > 25.5f) f = 25.5f;
    // each step = 0.1 Hz
    return (uint8_t)roundf(f * 10.0f);
  }

  const char* GetOutputLabel(uint8_t assign) {
    switch (assign) {
      case 0:
        return "LFO1";
      case 1:
        return "LFO2";
      case 2:
        return "LFO3";
      case 3:
        return "GAT1";
      case 4:
        return "GAT2";
      case 5:
        return "GAT3";
      case 6:
        return "STEP";
      default:
        return "TRIG";
    }
  }
};
