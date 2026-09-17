// Copyright (c) 2025, Nicholas J. Michalek
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

class Combin8 : public HemisphereApplet {
public:
  enum Combin8Cursor {
    CH1_OUT,
    CH1_AUX1,
    CH1_AUX2,
    CH2_OUT,
    CH2_AUX1,
    CH2_AUX2,

    MAX_CURSOR = CH2_AUX2
  };

  const char* applet_name() {
    return "Combin8";
  }
  const uint8_t* applet_icon() {
    return PhzIcons::dualAttenuverter;
  }

  void Start() { }

  void Controller() {
    ForEachChannel(ch) {
      const int main_cv = In(ch);
      const int aux1 = sources[ch][0].In();
      const int aux2 = sources[ch][1].In();
      int signal;

      // SUM: Sample and hold the sum of the main input and unmuted AUX inputs.
      if (output_mode[ch] == MODE_SUM) {
        signal = MixerInputSum(ch, main_cv, aux1, aux2);
        if (Clock(ch)) StartADCLag(ch);
        if (EndOfADCLag(ch)) held_cv[ch] = signal;
        signal = held_cv[ch];
      // IN: Sample and hold the main input while unmuted AUX inputs remain live.
      } else if (output_mode[ch] == MODE_IN) {
        if (Clock(ch)) StartADCLag(ch);
        if (EndOfADCLag(ch)) held_cv[ch] = main_cv;
        signal = MixerInputSum(ch, held_cv[ch], aux1, aux2);
      } else {
        signal = MixerInputSum(ch, main_cv, aux1, aux2);
      }

      CONSTRAIN(signal, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
      signal = Quantize(ch, signal);
      Out(ch, signal);
    }
  }

  void View() {
    DrawInterface();
    gfxDisplayInputMapEditor();
  }

  void OnButtonPress() override {
    if (CheckEditInputMapPress(
          cursor,
          IndexedInput(CH1_AUX1, sources[0][0]),
          IndexedInput(CH1_AUX2, sources[0][1]),
          IndexedInput(CH2_AUX1, sources[1][0]),
          IndexedInput(CH2_AUX2, sources[1][1])
        ))
      return;

    CursorToggle();
  }

  void AuxButton() {
    const int ch = Channel();

    if (IsOutput()) {
      QEdit(ch);
    }

    if (IsAux1()) {
      aux_muted[ch][0] = !aux_muted[ch][0];
      return;
    }

    if (IsAux2()) {
      aux_muted[ch][1] = !aux_muted[ch][1];
      return;
    }

    CancelEdit();
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    if (EditSelectedInputMap(direction)) return;

    int ch = Channel();
    if (IsOutput()) {
      int mode = static_cast<int>(output_mode[ch]);
      mode = constrain(mode + (direction > 0 ? 1 : direction < 0 ? -1 : 0),
                       static_cast<int>(MODE_NRM),
                       static_cast<int>(MODE_IN));
      output_mode[ch] = static_cast<OutputMode>(mode);
      return;
    }

    sources[ch][cursor % 3 - 1].ChangeSource(direction);
  }

  uint64_t OnDataRequest() {
    // Persist both channel output modes using the applet's existing data storage.
    uint64_t mode_data = 0;
    Pack(mode_data, PackLocation {0, 2}, output_mode[0]);
    Pack(mode_data, PackLocation {2, 2}, output_mode[1]);
    SetData(0, mode_data);

    return PackPackables(
      sources[0][0],
      sources[0][1],
      sources[1][0],
      sources[1][1]
    );
  }

  void OnDataReceive(uint64_t data) {
    UnpackPackables(
      data,
      sources[0][0],
      sources[0][1],
      sources[1][0],
      sources[1][1]
    );

    // Restore saved output modes; default to NRM if no valid data is available.
    uint64_t mode_data = 0;
    if (GetData(0, mode_data)) {
      uint8_t mode0 = Unpack(mode_data, PackLocation {0, 2});
      uint8_t mode1 = Unpack(mode_data, PackLocation {2, 2});

      if (mode0 > MODE_IN) mode0 = MODE_NRM;
      if (mode1 > MODE_IN) mode1 = MODE_NRM;

      output_mode[0] = static_cast<OutputMode>(mode0);
      output_mode[1] = static_cast<OutputMode>(mode1);
    } else {
      output_mode[0] = MODE_NRM;
      output_mode[1] = MODE_NRM;
    }
  }
protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "HoldCV1";
    help[HELP_DIGITAL2] = "HoldCV2";
    help[HELP_CV1]      = "CV Ch1";
    help[HELP_CV2]      = "CV Ch2";
    help[HELP_OUT1]     = "Out1";
    help[HELP_OUT2]     = "Out2";
    //                  "---------------------" <-- Extra text size guide
    help[HELP_EXTRA1] = "3 inputs per chan";
    help[HELP_EXTRA2] = "";
  }

private:
  int cursor;

  int Channel() const {
    return cursor / 3;
  }

  bool IsOutput() const {
    return cursor % 3 == 0;
  }

  bool IsAux1() const {
    return cursor % 3 == 1;
  }

  bool IsAux2() const {
    return cursor % 3 == 2;
  }

  int MixerInputSum(int ch, int main_cv, int aux1, int aux2) const {
    int sum = main_cv;
    if (!aux_muted[ch][0]) sum += aux1;
    if (!aux_muted[ch][1]) sum += aux2;
    return sum;
  }

  // Additional sources.
  CVInputMap sources[2][2];

  // NRM: Pass the main input and unmuted AUX inputs live.
  // SUM: Sample and hold their sum on the clock.
  // IN: Sample and hold the main input; keep unmuted AUX inputs live.
  enum OutputMode {
    MODE_NRM,
    MODE_SUM,
    MODE_IN,

    MODE_COUNT,
  };

  OutputMode output_mode[2] = {MODE_NRM, MODE_NRM};
  int held_cv[2] = {0, 0};
  bool aux_muted[2][2] = {{false, false}, {false, false}};

  // Draw Combin8 source meters with a 10-pixel limit around the anchor.
  void PrintCombin8Source(CVInputMap &map) const {
    const bool is_dac =
        map.source_type() == CVInputMap::TYPE_DAC && map.index() < 8;

    if (is_dac) {
      gfxPrintIcon(PARAM_MAP_ICONS + (9 + map.index()) * 8);
    } else {
      gfxPrintIcon(map.Icon());
    }

    const int xpos = gfxGetPrintPosX() - 1;
    const int icon_y = gfxGetPrintPosY();
    const int ypos = icon_y + (is_dac ? 2 : 4);
    const int height = constrain(map.InRescaled(9), -9, 9);
    const int endpoint = constrain(ypos - height, ypos - 10, ypos + 10);
    gfxLine(xpos, ypos, xpos, endpoint);
  }

  void DrawInterface() {
    ForEachChannel(ch) {
      const int ypos = 16 + 26 * ch + (ch == 0 ? 2 : 1);
      const int base_cursor = ch * 3;

      gfxPos(2, ypos);
      gfxStartCursor();
      if (output_mode[ch] == MODE_SUM) {
        gfxPrintIcon(CLOCK_ICON);
      } else {
        gfxPrint(OutputLabel(ch));
        gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      }
      gfxPrint("=");

      if (output_mode[ch] == MODE_IN) {
        gfxPrintIcon(CLOCK_ICON);
        const int xpos = gfxGetPrintPosX() - 1;
        const int ypos = gfxGetPrintPosY() + 4;
        const int height = constrain(
            Proportion(held_cv[ch], HEMISPHERE_MAX_INPUT_CV, 9), -9, 9);
        const int endpoint = constrain(ypos - height, ypos - 10, ypos + 10);
        gfxLine(xpos, ypos, xpos, endpoint);
      } else {
        PrintCombin8Source(cvmap[ch + io_offset]);
      }

      gfxEndCursor(cursor == base_cursor, false, nullptr);

      gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());

      gfxPrint("+");
      gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      gfxStartCursor();
      if (aux_muted[ch][0]) {
        gfxPrint("X");
        gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      } else {
        PrintCombin8Source(sources[ch][0]);
      }
      gfxEndCursor(cursor == base_cursor + 1, false,
                   EditMode() ? sources[ch][0].InputName() : nullptr);

      gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      gfxPrint("+");
      gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      gfxStartCursor();
      if (aux_muted[ch][1]) {
        gfxPrint("X");
        gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      } else {
        PrintCombin8Source(sources[ch][1]);
      }
      gfxEndCursor(cursor == base_cursor + 2, false,
                   EditMode() ? sources[ch][1].InputName() : nullptr);

      int main_cv = output_mode[ch] == MODE_IN ? held_cv[ch] : In(ch);
      int mixer_sum = MixerInputSum(
          ch, main_cv, sources[ch][0].In(), sources[ch][1].In());
      mixer_sum = constrain(
          mixer_sum, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);

      DrawMeter(mixer_sum, ypos + 12, 1);
      DrawMeter(ViewOut(ch), ypos + 15, 3);
    }

    if (cursor == CH1_OUT) {
      SetLabel(output_mode[0] == MODE_SUM ? "SUM A" :
               output_mode[0] == MODE_IN ? "IN A" : "NRM A");
      SetAux(true);
    } else if (cursor == CH1_AUX1) {
      SetLabel(aux_muted[0][0] ? "MUTED " : "AUX1 A");
      SetAux(true);
    } else if (cursor == CH1_AUX2) {
      SetLabel(aux_muted[0][1] ? "MUTED " : "AUX2 A");
      SetAux(true);
    } else if (cursor == CH2_OUT) {
      SetLabel(output_mode[1] == MODE_SUM ? "SUM B" :
               output_mode[1] == MODE_IN ? "IN B" : "NRM B");
      SetAux(true);
    } else if (cursor == CH2_AUX1) {
      SetLabel(aux_muted[1][0] ? "MUTED " : "AUX1 B");
      SetAux(true);
    } else if (cursor == CH2_AUX2) {
      SetLabel(aux_muted[1][1] ? "MUTED " : "AUX2 B");
      SetAux(true);
    }
  }

  void DrawMeter(int cv, int ypos, int height = 1) {
    // positive values extend bars from left side of screen to the right
    // negative values go from right side to left
    int max_length = 60;
    int length = ProportionCV(abs(cv), max_length);
    if (cv < 0)
      gfxRect(max_length - length, ypos, length, height);
    else
      gfxRect(1, ypos, length, height);
  }
};
