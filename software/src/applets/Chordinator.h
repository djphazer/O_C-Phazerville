// Copyright (c) 2021, Bryan Head
//
// Based on Braids Quantizer, Copyright 2015 Émilie Gillet.
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

class Chordinator : public HemisphereApplet {
public:
  const char *applet_name() { return "Chordnate"; }

  void Start() {
    scale = 5;
    continuous[0] = 1;
    continuous[1] = 1;
    set_scale(scale);
    update_chord_quantizer();
  }

  void Controller() {
    ForEachChannel(ch) {
      if (Clock(ch)) {
        continuous[ch] = 0;
        StartADCLag(ch);
      }
    }

    if (continuous[0] || EndOfADCLag(0)) {
      chord_root_raw = In(0);
      int32_t new_root_pitch =
          Quantize(0, chord_root_raw, root << 7, 0);
      if (new_root_pitch != chord_root_pitch) {
        update_chord_quantizer();
        chord_root_pitch = new_root_pitch;
      }
      Out(0, chord_root_pitch);
    }

    if (continuous[1] || EndOfADCLag(1)) {
      harm_pitch =
          Quantize(1, In(1) + chord_root_pitch, root << 7, 0);
      Out(1, harm_pitch);
    }
  }

  void View() {
    gfxPrint(0, 15, OC::scale_names_short[scale]);
    if (cursor == 0) gfxCursor(0, 23, 30);

    gfxPrint(36, 15, OC::Strings::note_names_unpadded[root]);
    if (cursor == 1) gfxCursor(36, 23, 12);

    uint16_t mask = chord_mask;
    for (int i = 0; i < int(active_scale.num_notes); i++) {
      int y = 7*(i / 12); // longer scales spill over
      int x = 5*(i % 12);
      if (mask & 1) {
        gfxRect(x, 25 + y, 4, 4);
      } else {
        gfxFrame(x, 25 + y, 4, 4);
      }
      if (cursor - 2 == i) {
        gfxCursor(x, 30 + y, 4);
      }

      mask >>= 1;
    }

    size_t root_ix = note_ix(chord_root_pitch);
    gfxBitmap(5 * root_ix, 40, 8, NOTE4_ICON);
    gfxBitmap(5 * note_ix(harm_pitch), 50, 8, NOTE4_ICON);
  }

  void OnButtonPress() {
    if (cursor < 2) {
      isEditing = !isEditing;
    } else {
      chord_mask ^= 1 << (cursor - 2);
      update_chord_quantizer();
    }
  }

  void OnEncoderMove(int direction) {
    if (!isEditing) {
      MoveCursor(cursor, direction, 1 + int(active_scale.num_notes));
      return;
    }

    switch (cursor) {
    case 0:
      set_scale(scale + direction);
      break;
    case 1:
      root = constrain(root + direction, 0, 11);
      break;
    default:
      // shouldn't happen...
      break;
    }
    update_chord_quantizer();
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation{0, 8}, scale);
    Pack(data, PackLocation{8, 4}, root);
    Pack(data, PackLocation{12, 16}, chord_mask);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    set_scale(Unpack(data, PackLocation{0, 8}));
    root = Unpack(data, PackLocation{8, 4});
    chord_mask = Unpack(data, PackLocation{12, 16});
    update_chord_quantizer();
  }

protected:
  void SetHelp() {
    help[HEMISPHERE_HELP_DIGITALS] = "S&H Sig, S&H Wgt";
    help[HEMISPHERE_HELP_CVS] = "Signal, Weight";
    help[HEMISPHERE_HELP_OUTS] = "Sig Thru, Scl Deg";
    help[HEMISPHERE_HELP_ENCODER] = "Scale, Root, Mask";
  }

private:
  int scale; // SEMI
  int16_t root;
  bool continuous[2];
  braids::Scale active_scale;

  int cursor = 0;

  // Leftmost is root, second to left is 2, etc. Defaulting here to basic triad.
  uint16_t chord_mask = 0b10101;

  int16_t chord_root_raw = 0;
  int16_t chord_root_pitch = 0;

  int16_t harm_pitch = 0;

  void update_chord_quantizer() {
    size_t num_notes = active_scale.num_notes;
    chord_root_pitch = Quantize(0, chord_root_raw, root, 0);
    size_t chord_root = note_ix(chord_root_pitch);
    uint16_t mask = rotl32(chord_mask, num_notes, chord_root);
    QuantizerConfigure(1, scale, mask);
  }

  size_t note_ix(int pitch) {
    int rel_pitch = pitch % active_scale.span;
    int d = active_scale.span;
    size_t p = 0;
    for (int i = 0; i < (int) active_scale.num_notes; i++) {
      int e = abs(rel_pitch - active_scale.notes[i]);
      if (e < d) {
        p = i;
        d = e;
      }
    }
    return p;
  }

  void set_scale(int value) {
    if (value < 0) scale = OC::Scales::NUM_SCALES - 1;
    else if (value >= OC::Scales::NUM_SCALES) scale = 0;
    else scale = value;
    active_scale = OC::Scales::GetScale(scale);
    QuantizerConfigure(0, scale);
  }
};
