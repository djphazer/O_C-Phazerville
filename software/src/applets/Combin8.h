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
    CH1_AUX1,
    CH1_AUX2,
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
      int signal = In(ch) + sources[ch][0].In() + sources[ch][1].In();
      // probably not necessary to constrain here, but doesn't hurt
      CONSTRAIN(signal, HEMISPHERE_MIN_CV, HEMISPHERE_MAX_CV);

      // TODO: gate inputs for mutes? S&H?
      // Mute:
      //if (Gate(ch)) Out(ch, 0); else Out(ch, signal);
      // S&H:
      //if (Clock(ch)) StartADCLag(ch);
      //if (EndOfADCLag(ch)) Out(ch, signal);

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

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
      MoveCursor(cursor, direction, MAX_CURSOR);
      return;
    }
    if (EditSelectedInputMap(direction)) return;

    sources[cursor/2][cursor%2].ChangeSource(direction);
  }

  uint64_t OnDataRequest() {
    return PackPackables(
      sources[0][0], sources[0][1], sources[1][0], sources[1][1]
    );
  }

  void OnDataReceive(uint64_t data) {
    UnpackPackables(
      data, sources[0][0], sources[0][1], sources[1][0], sources[1][1]
    );
  }

protected:
  void SetHelp() {
    //                    "-------" <-- Label size guide
    help[HELP_DIGITAL1] = "";
    help[HELP_DIGITAL2] = "";
    help[HELP_CV1]      = "CV Ch1";
    help[HELP_CV2]      = "CV Ch2";
    help[HELP_OUT1]     = "Out1";
    help[HELP_OUT2]     = "Out2";
    help[HELP_EXTRA1] = "3 inputs per chan";
    help[HELP_EXTRA2] = "";
    //                  "---------------------" <-- Extra text size guide
  }

private:
  int cursor;

  // two extra sources per channel
  CVInputMap sources[2][2];
  // XXX: offset?

  void DrawInterface() {
    ForEachChannel(ch) {
      int ypos = 13 + 26*ch;
      gfxPrint(2, ypos, OutputLabel(ch));
      gfxPrint("=");

      gfxPrint(cvmap[ch + io_offset]);

      gfxPrint(" +");
      gfxStartCursor();
      gfxPrint(sources[ch][0]);
      gfxEndCursor(cursor == ch*2 + 0, false, sources[ch][0].InputName());

      gfxPrint(" +");
      gfxStartCursor();
      gfxPrint(sources[ch][1]);
      gfxEndCursor(cursor == ch*2 + 1, false, sources[ch][1].InputName());

      // meters
      DrawMeter(In(ch), ypos + 11, 1);
      DrawMeter(sources[ch][0].In(), ypos + 13, 1);
      DrawMeter(sources[ch][1].In(), ypos + 15, 1);

      DrawMeter(ViewOut(ch), ypos + 17, 3);
    }
  }

  void DrawMeter(int cv, int ypos, int height = 1) {
      // positve values extend bars from left side of screen to the right
      // negative values go from right side to left
      int max_length = 60;//px
      int length = ProportionCV(abs(cv), max_length);
      if (cv < 0)
          gfxRect(max_length - length, ypos, length, height);
      else
          gfxRect(1, ypos, length, height);
  }
};
