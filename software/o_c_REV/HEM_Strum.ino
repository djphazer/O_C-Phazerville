// Copyright (c) 2021, Bryan Head
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

#include "bjorklund.h"
#include "braids_quantizer.h"

class Strum : public HemisphereApplet {
public:
  const char *applet_name() { return "Strum"; }

  void Start() { set_scale(5); }

  void Controller() {

    // TODO:
    // - Decide on repeated trigger behavior; options:
    //   - Reset countdown, advance index (resetting if at end)
    //   - Reset countdown, don't advance index unless at end
    //   - Just advance index (allowing repeated notes)
    //   - Start another, overlapping strum
    if (Clock(0))
      inc = 1;
    if (Clock(1))
      inc = -1;
    if ((Clock(0) || Clock(1)) && !dig_high) {
      countdown = 0;
      if (index <= 0 && inc < 0)
        index = length - 1;
      if (index >= length - 1 && inc > 0)
        index = 0;
    }

    if (countdown <= 0 && index >= 0 && index < length) {
      int raw_pitch = In(0);
      Quantize(0, raw_pitch, root << 7, 0);
      int note_num = GetQuantizer(0)->GetLatestNoteNumber();
      int pitch = QuantizerLookup(0, note_num + intervals[index]);
      disp = note_num;
      Out(0, pitch);
      ClockOut(1);

      int spacing_mod =
          Proportion(In(1), HEMISPHERE_MAX_INPUT_CV, HEM_BURST_SPACING_MAX);
      last_note_dur =
          17 * constrain((spacing + spacing_mod), HEM_BURST_SPACING_MIN,
                         HEM_BURST_SPACING_MAX);
      countdown += last_note_dur;
      index += inc;
    } else if (countdown > 0) {
      countdown -= 1;
    }
    dig_high = Clock(0) || Clock(1);
  }

  void View() {
    gfxPrint(0, 15, OC::scale_names_short[scale]);
    if (cursor == SCALE)
      gfxCursor(0, 23, 30);

    gfxPrint(36, 15, OC::Strings::note_names_unpadded[root]);
    if (cursor == ROOT)
      gfxCursor(36, 23, 12);

    gfxPrint(0, 25, spacing);
    gfxPrint(28, 25, "ms");
    if (cursor == SPACING)
      gfxCursor(0, 33, 28);

    int num_notes = OC::Scales::GetScale(scale).num_notes;
    for (int i = 0; i < length; i++) {
      int row = i / 3;
      int col = i % 3;
      int offset = intervals[i] % num_notes;
      int octave = intervals[i] / num_notes;
      if (offset < 0) {
        offset += num_notes;
        octave -= 1;
      }
      const uint8_t *octave_mark = octave < 0 ? DOWN_ARROWS + (-octave - 1) * 6
                                              : UP_ARROWS + (octave - 1) * 6;
      int disp_offset = offset + 1;
      gfxPrint(col * 18 + pad(10, offset), 35 + row * 10, disp_offset);
      if (octave != 0)
        gfxBitmap(col * 18 + 12, 35 + row * 10, 6, octave_mark);
      if (cursor == INTERVAL_START + i) {
        gfxCursor(col * 18, 43 + row * 10, 18);
      }
      if (index - inc == i) {
        int c = inc < 0 ? countdown : last_note_dur - countdown;
        int x = col * 18 + Proportion(c, last_note_dur, 18);
        int y = 35 + row * 10;
        gfxLine(x, y, x, y + 10);
        if (ViewOut(1))
          gfxRect(col * 18, 35 + row * 10, 18, 10);
      }
    }
    if (cursor == LENGTH) {
      gfxCursor(0, 43 + 10 * ((length - 1) / 3), 3 * 18);
    }
    gfxPrint(0, 55, disp);
  }

  void OnButtonPress() { CursorAction(cursor, 3); }

  void OnEncoderMove(int direction) {
    if (!isEditing) {
      MoveCursor(cursor, direction, INTERVAL_START + length);
      return;
    }
    switch (cursor) {
    case SCALE:
      // scale = constrain(scale + direction, 0, OC::Scales::NUM_SCALES - 1);
      set_scale(scale + direction);
      break;
    case ROOT:
      root = constrain(root + direction, 0, 11);
      break;
    case SPACING:
      spacing = constrain(spacing + direction, HEM_BURST_SPACING_MIN,
                          HEM_BURST_SPACING_MAX);
      break;
    case LENGTH:
      length = constrain(length + direction, 1, MAX_CHORD_LENGTH);
      break;
    default:
      int ix = cursor - INTERVAL_START;
      intervals[ix] =
          constrain(intervals[ix] + direction, MIN_INTERVAL, MAX_INTERVAL);
    }
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation{0, 8}, scale);
    Pack(data, PackLocation{8, 4}, root);
    Pack(data, PackLocation{12, 9}, spacing);
    Pack(data, PackLocation{21, 4}, length);
    for (size_t i = 0; i < MAX_CHORD_LENGTH; i++) {
      Pack(data, PackLocation{25 + i * 6, 6}, intervals[i] - MIN_INTERVAL);
    }
    return data;
  }

  void OnDataReceive(uint64_t data) {
    set_scale(Unpack(data, PackLocation{0, 8}));
    root = Unpack(data, PackLocation{8, 4});
    spacing = Unpack(data, PackLocation{12, 9});
    length = Unpack(data, PackLocation{21, 4});
    for (size_t i = 0; i < MAX_CHORD_LENGTH; i++) {
      intervals[i] = Unpack(data, PackLocation{25 + i * 6, 6}) + MIN_INTERVAL;
    }
  }

protected:
  void SetHelp() {
    help[HEMISPHERE_HELP_DIGITALS] = "Strum Up, Down";
    help[HEMISPHERE_HELP_CVS] = "Root, Spacing";
    help[HEMISPHERE_HELP_OUTS] = "Pitch, Trig";
  }

private:
  static const int MAX_CHORD_LENGTH = 6;
  static const int MIN_INTERVAL = -12;
  static const int MAX_INTERVAL = 48;
  enum Params { SCALE, ROOT, SPACING, LENGTH, INTERVAL_START };

  int8_t intervals[MAX_CHORD_LENGTH] = {0, 4, 7, 9, 11, 14};
  int length = 6;
  int spacing;
  int last_note_dur;

  bool dig_high = false;
  int index = 0;
  int inc = 0;
  int countdown = 0;

  int scale;
  int16_t root;

  int cursor = 0;

  int disp = 0;

  void set_scale(int value) {
    if (value < 0)
      scale = OC::Scales::NUM_SCALES - 1;
    else if (value >= OC::Scales::NUM_SCALES)
      scale = 0;
    else
      scale = value;
    QuantizerConfigure(0, scale);
  }
};

Strum Strum_instance[2];

void Strum_Start(bool hemisphere) {
  Strum_instance[hemisphere].BaseStart(hemisphere);
}

void Strum_Controller(bool hemisphere, bool forwarding) {
  Strum_instance[hemisphere].BaseController(forwarding);
}

void Strum_View(bool hemisphere) { Strum_instance[hemisphere].BaseView(); }

void Strum_OnButtonPress(bool hemisphere) {
  Strum_instance[hemisphere].OnButtonPress();
}

void Strum_OnEncoderMove(bool hemisphere, int direction) {
  Strum_instance[hemisphere].OnEncoderMove(direction);
}

void Strum_ToggleHelpScreen(bool hemisphere) {
  Strum_instance[hemisphere].HelpScreen();
}

uint64_t Strum_OnDataRequest(bool hemisphere) {
  return Strum_instance[hemisphere].OnDataRequest();
}

void Strum_OnDataReceive(bool hemisphere, uint64_t data) {
  Strum_instance[hemisphere].OnDataReceive(data);
}