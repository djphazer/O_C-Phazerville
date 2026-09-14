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
      int main_cv = In(ch);
      int aux1 = sources[ch][0].In();
      int aux2 = sources[ch][1].In();
      int signal;

      // SUM mode: sample and hold the complete summed input on the clock.
      if (output_mode[ch] == MODE_SUM) {
        signal = main_cv;
        if (!aux_muted[ch][0]) signal += aux1;
        if (!aux_muted[ch][1]) signal += aux2;
        if (Clock(ch)) StartADCLag(ch);
        if (EndOfADCLag(ch)) held_cv[ch] = signal;
        signal = held_cv[ch];
      // IN mode: sample and hold the main input while aux inputs remain live.
      } else if (output_mode[ch] == MODE_IN) {
        if (Clock(ch)) StartADCLag(ch);
        if (EndOfADCLag(ch)) held_cv[ch] = main_cv;
        signal = held_cv[ch];
        if (!aux_muted[ch][0]) signal += aux1;
        if (!aux_muted[ch][1]) signal += aux2;
      } else {
        signal = main_cv;
        if (!aux_muted[ch][0]) signal += aux1;
        if (!aux_muted[ch][1]) signal += aux2;
      }

      CONSTRAIN(signal, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
      signal = HS::GetQuantEngine(io_offset + ch).Process(signal, 0, 0);
      CONSTRAIN(signal, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);
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
    int ch = Channel();

    if (IsOutput()) {
      SetAux(false);
      HS::QuantizerEdit(io_offset + ch);
      CancelEdit();
      return;
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
      if (direction > 0) {
        output_mode[ch] = static_cast<OutputMode>(
          (output_mode[ch] + 1) % 3
        );
      } else if (direction < 0) {
        output_mode[ch] = static_cast<OutputMode>(
          (output_mode[ch] + 2) % 3
        );
      }
      return;
    }

    sources[ch][cursor % 3 - 1].ChangeSource(direction);
  }

  uint64_t OnDataRequest() {
    uint64_t data = PackPackables(
      sources[0][0],
      sources[0][1],
      sources[1][0],
      sources[1][1]
    );

    // Persist both channel output modes using the applet's existing data storage.
    uint64_t mode_data = 0;
    Pack(mode_data, PackLocation {0, 2}, output_mode[0]);
    Pack(mode_data, PackLocation {2, 2}, output_mode[1]);
    SetData(0, mode_data);

    return data;
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

  // Additional sources.
  CVInputMap sources[2][2];

  // Output modes: normal sum, clocked sum/hold, or clocked main input with live aux inputs.
  enum OutputMode {
    MODE_NRM,
    MODE_SUM,
    MODE_IN,
  };

  OutputMode output_mode[2] = {MODE_NRM, MODE_NRM};
  int held_cv[2] = {0, 0};
  bool aux_muted[2][2] = {{false, false}, {false, false}};

  void DrawInterface() {
    ForEachChannel(ch) {
      const int ypos = 13 + 26 * ch;
      const int base_cursor = ch * 3;

      // Output = main input.
      gfxPos(2, ypos);
      gfxStartCursor();
      if (output_mode[ch] == MODE_SUM) {
        gfxPrintIcon(CLOCK_ICON);
      } else {
        gfxPrint(OutputLabel(ch));
        gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      }
      gfxPrint("=");

      // Main input.
      if (output_mode[ch] == MODE_IN) {
        gfxPrintIcon(CLOCK_ICON);
      } else {
        gfxPrint(cvmap[ch + io_offset]);
      }

      // Highlight the OUT cursor when selected.
      gfxEndCursor(cursor == base_cursor, false, nullptr);

      // Tighten spacing after the fixed input.
      gfxPos(gfxGetPrintPosX() - 2, gfxGetPrintPosY());

      // AUX input 1.
      gfxPrint(" +");
      gfxStartCursor();
      if (aux_muted[ch][0]) {
        gfxPrint("X");
        gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      } else {
        gfxPrint(sources[ch][0]);
      }
      gfxEndCursor(cursor == base_cursor + 1, false,
                   EditMode() ? sources[ch][0].InputName() : nullptr);

      // AUX input 2.
      gfxPos(gfxGetPrintPosX() - 2, gfxGetPrintPosY());
      gfxPrint(" +");
      gfxStartCursor();
      if (aux_muted[ch][1]) {
        gfxPrint("X");
        gfxPos(gfxGetPrintPosX() + 2, gfxGetPrintPosY());
      } else {
        gfxPrint(sources[ch][1]);
      }
      gfxEndCursor(cursor == base_cursor + 2, false,
                   EditMode() ? sources[ch][1].InputName() : nullptr);

      DrawMeter(In(ch), ypos + 11, 1);
      DrawMeter(sources[ch][0].In(), ypos + 13, 1);
      DrawMeter(sources[ch][1].In(), ypos + 15, 1);
      DrawMeter(ViewOut(ch), ypos + 17, 3);
    }

    if (cursor == CH1_OUT) {
      SetLabel("OUT A");
      SetAux(true);
    } else if (cursor == CH1_AUX1) {
      SetLabel(aux_muted[0][0] ? "MUTED " : "AUX A1");
      SetAux(true);
    } else if (cursor == CH1_AUX2) {
      SetLabel(aux_muted[0][1] ? "MUTED " : "AUX A2");
      SetAux(true);
    } else if (cursor == CH2_OUT) {
      SetLabel("OUT B");
      SetAux(true);
    } else if (cursor == CH2_AUX1) {
      SetLabel(aux_muted[1][0] ? "MUTED " : "AUX B1");
      SetAux(true);
    } else if (cursor == CH2_AUX2) {
      SetLabel(aux_muted[1][1] ? "MUTED " : "AUX B2");
      SetAux(true);
    }
  }

  void DrawMeter(int cv, int ypos, int height = 1) {


  // Positive values extend bars from left side of screen to the right.
  // Negative values go from right side to left.
  const int max_length = 60;
  int length = ProportionCV(abs(cv), max_length);
  if (cv < 0)
    gfxRect(max_length - length, ypos, length, height);
  else
    gfxRect(1, ypos, length, height);
  }
};
