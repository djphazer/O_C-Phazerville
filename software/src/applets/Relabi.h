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

    for (int i = 0; i < 3; i++) {
      phaseKnob[i] = 0; // 0%

      // Set oscillator to sine wave
      osc[i] = WaveformManager::VectorOscillatorFromWaveform(35);
      osc[i].SetFrequency( DecodeFreq(freqKnob[i]) * 100 * PROCESS_TICKS );
      osc[i].SetScale(HEMISPHERE_3V_CV);
    }
    for (int i = 0; i < 4; i++) {
      threshKnob[i] = 15; // centered threshold
    }

    outputAssign[0] = 0; // Gate 1
    outputAssign[1] = 1; // Gate 2
    outputAssign[2] = 2; // Gate 3
    outputAssign[3] = 4; // Relabi wave

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
        int setPhase = round((phaseKnob[pcount] / 16.0) * totalSegments);
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
        UpdateRelabiWave();
      } else {
        float normalizedCV = InF(0, HEMISPHERE_3V_CV);
        float normalizedCV2 = InF(1, HEMISPHERE_3V_CV);
        const float fModCV = constrain(
          powf(10.0f, normalizedCV * 2.0f), 0.01f, 100.0f
        );
        const float xModCV = constrain(normalizedCV2, 0.0f, 1.0f);

        // frequency modulation:
        for (uint8_t lfo = 0; lfo < 3; lfo++) {
          // Incorporate CV2 with cross-modulation
          float xmodCombo = 30.0f + xModCV * 100; // Fixed 30% plus 0..100% from CV2

          // Calculate cross-frequency modulation factor
          float crossFreqMod = (xmodCombo / 100.0)
            * (static_cast<float>(sample[(lfo + 2) % 3]) / HEMISPHERE_3V_CV);

          // Combine base frequency, cross-modulation, and CV input
          float freq = DecodeFreq(freqKnob[lfo]);
          freq = fModCV * (freq + (freq * crossFreqMod));

          // Ensure the frequency stays within valid bounds
          // freq = constrain(freq, 0.01, 15000.0);

          // Set the modulated frequency to the oscillator
          osc[lfo].SetFrequency(
            freq * 100 * PROCESS_TICKS
          ); // VectorOscillator uses centiHertz.

          // Update sample
          sample[lfo] = osc[lfo].Next();
        }

        UpdateRelabiWave();
        for (uint8_t gate = 0; gate < 4; gate++) {
          const int thresholdCV = (thresh(gate) * HEMISPHERE_3V_CV) / 100;
          if (relabiWave >= thresholdCV) {
            // Gate is high
            gateState[gate] = true;
          } else {
            gateState[gate] = false;
          }
        }

        if (linked) {
          // we're inside the "else" clause of (link_follow) so this must be the leader
          // Leader is Linked: Send lfo values and gates to RelabiManager
          manager.WriteValues(sample[0], sample[1], sample[2]);
          manager.WriteGates(gateState);
        }
      }
      if (++scopeSampleDivider >= SCOPE_SAMPLE_DECIMATION) {
        scopeSampleDivider = 0;
        RecordRelabiScopeSample();
      }
    }

    // Set outputs based on assignments
    ForEachChannel(ch) {
      uint8_t assign = outputAssign[ch + link_follow*2];
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
      DrawGateIndicators();

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

    if (cursor < 8) {
      gfxPrint(1, 15, "FREQ");
      gfxPrint(27, 15, "PHAS");
      for (int lfo = 0; lfo < CHAN_COUNT; ++lfo) {
        const int rowY = 25 + lfo * 10;
        const int freqCursor = lfo * 2;
        const int phaseCursor = freqCursor + 1;

        PrintScaledFloat(2, rowY, DecodeFreq(freqKnob[lfo]));
        gfxPrint(28, rowY, phase(lfo));
        if (cursor == freqCursor) gfxCursor(1, rowY + 8, 25);
        if (cursor == phaseCursor) gfxCursor(26, rowY + 8, 18);
      }

      DrawVUMetersLeft();

      gfxPrint(1, 55, "A:");
      DrawOutputOption(13, 55, outputAssign[0]);
      gfxPrint(31, 55, "B:");
      DrawOutputOption(43, 55, outputAssign[1]);
      if (cursor == 6) gfxCursor(1, 63, 30);
      if (cursor == 7) gfxCursor(31, 63, 30);
    } else {
      const int threshold = cursor - 8;
      const int thresholdValue = thresh(threshold);
      gfxPos(1, 15);
      graphics.printf("THR%d  >", threshold + 1);
      gfxPrint(thresholdValue);
      if (gateState[threshold]) {
        gfxInvert(37, 15, thresholdValue < 0 ? 24 : 18, 8);
      }
      DrawRelabiScope(1, 23, 62, 40, thresh(threshold), true);
      if (EditMode()) {
        gfxInvert(1, 23, 62, 40);
      }
    }
  }

  void DrawOutputOption(int x, int y, uint8_t assign) {
    if (assign < 4) {
      // Gate output
      gfxBitmap(x, y, 8, GATE_ICON);
      gfxPrint(x + 9, y, assign + 1);
    } else {
      // Relabi wave followed by "R", then the three individual LFOs.
      gfxBitmap(x, y, 8, WAVEFORM_ICON);
      if (assign == 4) gfxPrint(x + 9, y, "R");
      else gfxPrint(x + 9, y, assign - 4);
    }
  }

  void DrawRelabiScope(
    int x, int y, int width, int height,
    int thresholdPercent = 0, bool showThreshold = false
  ) {
    gfxFrame(x, y, width, height);

    const int centerY = y + height / 2;
    const int maxPixelAmplitude = (height - 4) / 2;
    const int maxWaveAmplitude = HEMISPHERE_3V_CV;
    const int thresholdCV = (thresholdPercent * maxWaveAmplitude) / 100;
    int previousX = x + 1;
    int previousY = centerY;
    int previousWave = 0;

    for (int i = 0; i < SCOPE_WIDTH; ++i) {
      const int sampleIndex = (scopeWriteIndex + i) % SCOPE_WIDTH;
      const int wave = constrain(
        scopeHistory[sampleIndex],
        -maxWaveAmplitude,
        maxWaveAmplitude
      );
      const int currentX = x + 1 + i;
      const int currentY = centerY - (wave * maxPixelAmplitude) / maxWaveAmplitude;
      if (i > 0) {
        gfxLine(previousX, previousY, currentX, currentY);
        if (
          showThreshold
          && (previousWave >= thresholdCV || wave >= thresholdCV)
        ) {
          gfxLine(previousX, previousY - 1, currentX, currentY - 1);
        }
      }
      previousX = currentX;
      previousY = currentY;
      previousWave = wave;
    }
    if (showThreshold) {
      const int thresholdY = centerY
        - (thresholdPercent * maxPixelAmplitude) / 100;
      gfxLine(x + 1, thresholdY, x + width - 2, thresholdY);
    }
  }

  void RecordRelabiScopeSample() {
    scopeHistory[scopeWriteIndex] = relabiWave;
    scopeWriteIndex = (scopeWriteIndex + 1) % SCOPE_WIDTH;
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

  void DrawGateIndicators() {
    constexpr int indicatorY = 49;
    constexpr int indicatorX[4] = {8, 24, 40, 56};
    for (int gate = 0; gate < 4; ++gate) {
      const int x = indicatorX[gate];
      gfxCircle(x, indicatorY, 3);
      if (gateState[gate]) gfxRect(x - 1, indicatorY - 1, 3, 3);
    }
  }

  void DrawVUMetersLeft() {
    constexpr int meterX = 45;
    constexpr int meterWidth = 18;
    for (int lfo = 0; lfo < CHAN_COUNT; ++lfo) {
      const int y = 25 + lfo * 10;
      const int centerX = meterX + meterWidth / 2;
      const int halfWidth = (meterWidth - 4) / 2;
      const int level = constrain(
        (sample[lfo] * halfWidth) / HEMISPHERE_3V_CV,
        -halfWidth,
        halfWidth
      );
      gfxFrame(meterX, y, meterWidth, 8);
      gfxLine(centerX, y + 1, centerX, y + 6);
      if (level > 0) gfxRect(centerX + 1, y + 2, level, 4);
      else if (level < 0) gfxRect(centerX + level, y + 2, -level, 4);
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
    // The linked right hemisphere exposes only output C and D.
    const bool link_follow = (linked && (hemisphere & 1));
    const int max_param = link_follow ? 1 : 11;

    if (!EditMode()) {
      // Not editing: move the cursor through the available parameters
      MoveCursor(cursor, direction, max_param);
      return;
    }

    if (link_follow) {
      outputAssign[cursor + 2] = constrain(
        outputAssign[cursor + 2] + direction, 0, 7
      );
      return;
    }

    if (cursor < 6) {
      const int lfo = cursor / 2;
      switch (cursor % 2) {
        case 0: // FREQ
          freqKnob[lfo] = constrain(freqKnob[lfo] + direction, 0, 63);
          break;
        case 1: // PHAS
          phaseKnob[lfo] = constrain(phaseKnob[lfo] + direction, 0, 15);
          break;
      }
    } else if (cursor < 8) {
      const int output = cursor - 6;
      outputAssign[output] = constrain(
        outputAssign[output] + direction, 0, 7
      );
    } else {
      const int threshold = cursor - 8;
      threshKnob[threshold] = constrain(
        threshKnob[threshold] + direction, 0, 31
      );
    }
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    for (size_t i = 0; i < 3; i++) {
      // 1) freqKnob[3], 6 bits each → 18 bits total
      Pack(data, PackLocation{0 + i*6, 6}, freqKnob[i]);
      // 2) phaseKnob[3], 4 bits each → 12 bits total
      Pack(data, PackLocation{18 + i*4, 4}, phaseKnob[i]);
    }
    // threshKnob[4], 5 bits each → 20 bits total
    for (size_t i = 0; i < 4; i++) {
      Pack(data, PackLocation{30 + i*5, 5}, threshKnob[i]);
    }
    // outputAssign → 3 bits each → 12 bits
    for (size_t i = 0; i < 4; i++) {
      Pack(data, PackLocation{50 + i*3, 3}, outputAssign[i]);
    }
    Pack(data, PackLocation{62, 2}, 1); // Mark the high-resolution phase layout.
    return data;
  }

  void OnDataReceive(uint64_t data) {
    const bool hasHighResolutionPhase = Unpack(data, PackLocation{62, 2}) == 1;
    for (size_t i = 0; i < 3; i++) {
      freqKnob[i] = Unpack(data, PackLocation{0 + i*6, 6});
      if (hasHighResolutionPhase) {
        phaseKnob[i] = Unpack(data, PackLocation{18 + i*4, 4});
      } else {
        phaseKnob[i] = Unpack(data, PackLocation{21 + i*3, 3}) * 2;
      }
    }

    for (size_t i = 0; i < 4; i++) {
      threshKnob[i] = Unpack(data, PackLocation{30 + i*5, 5});
    }

    for (size_t i = 0; i < 4; i++) {
      outputAssign[i] = Unpack(data, PackLocation{50 + i*3, 3});
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
      help[HELP_EXTRA1] = "P1: Frq/Phs/OutA/OutB";
      help[HELP_EXTRA2] = "P2-5: Thresholds 1-4";
    }
  }

private:
  constexpr static int CHAN_COUNT = 3;
  constexpr static int SCOPE_WIDTH = 60;
  constexpr static uint8_t SCOPE_SAMPLE_DECIMATION = 16;
  constexpr static int numParams = 5;
  int relabiWave = 0;

  RelabiManager& manager = RelabiManager::get();
  VectorOscillator osc[3];

  // parameters to be saved and loaded
  uint8_t freqKnob[CHAN_COUNT]; // 18 bits (6 each) // Each 0..19.5
  uint8_t phaseKnob[CHAN_COUNT]; // 12 bits total (4 each) // 0..93.75%
  uint8_t threshKnob[4]; // 20 bits (5 each) // Thresholds for four gates (-95..95)
  uint8_t outputAssign[4]; // 12 bits (3 each): gates 1-4, Relabi wave, LFOs 1-3

  int cursor = 0;
  int sample[CHAN_COUNT] = {0};

  uint8_t clkDiv = 0; // clkDiv allows us to calculate every other tick to save cycles
  int scopeHistory[SCOPE_WIDTH] = {};
  uint8_t scopeWriteIndex = 0;
  uint8_t scopeSampleDivider = 0;

  bool linked;
  //    bool bipolar;
  bool gateState[4] = {false, false, false, false};

  const uint8_t phase(int idx) const {
    return static_cast<uint8_t>((phaseKnob[idx] * 100 + 8) / 16);
  }
  const int thresh(int idx) const {
    const int value = constrain(static_cast<int>(threshKnob[idx]), 0, 31);
    if (value <= 15) return (value * 95) / 15 - 95;
    return ((value - 15) * 95) / 16;
  }

  void UpdateRelabiWave() {
    relabiWave = constrain(
      (sample[0] + sample[1] + sample[2]) / 3,
      -HEMISPHERE_3V_CV,
      HEMISPHERE_3V_CV
    );
  }

  int GetOutputValue(uint8_t assign) {
    if (assign < 4) {
      // Gate outputs
      return gateState[assign] ? HEMISPHERE_MAX_CV : 0;
    } else if (assign == 4) {
      // Offset the bipolar +/-3V wave for the DAC range.
      return relabiWave + HEMISPHERE_3V_CV;
    } else if (assign < 8) {
      // LFO outputs
      return sample[assign - 5] + HEMISPHERE_3V_CV;
    }
    return 0; // Default output
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
        return "GAT1";
      case 1:
        return "GAT2";
      case 2:
        return "GAT3";
      case 3:
        return "GAT4";
      case 4:
        return "RELW";
      case 5:
        return "LFO1";
      case 6:
        return "LFO2";
      case 7:
        return "LFO3";
      default:
        return "GAT1";
    }
  }
};