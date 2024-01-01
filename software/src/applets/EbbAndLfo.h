#include "resources/tideslite.h"

class EbbAndLfo : public HemisphereApplet {
public:
  const char *applet_name() { return "Ebb & LFO"; }

  void Start() { phase = 0; }

  void Controller() {
    if (Clock(1)) phase = 0;
    if (Clock(0)) {
      clocks_received++;
      //uint32_t next_tick = predictor.Predict(ClockCycleTicks(0));
      if (clocks_received > 1) {
        int new_freq = 0xffffffff / ClockCycleTicks(0);
        pitch = ComputePitch(new_freq);
        phase = 0;
      }
    }

    // handle CV inputs
    pitch_mod = pitch;
    slope_mod = slope;
    shape_mod = shape;
    fold_mod = fold;

    ForEachChannel(ch) {
        switch (cv_type(ch)) {
        case FREQ:
            pitch_mod += In(ch);
            break;
        case SLOPE:
            Modulate(slope_mod, ch, 0, 127);
            break;
        case SHAPE:
            shape_mod += Proportion(DetentedIn(ch), HEMISPHERE_MAX_INPUT_CV, 127);
            while (shape_mod < 0) shape_mod += 128;
            while (shape_mod > 127) shape_mod -= 128;
            break;
        case FOLD:
            Modulate(fold_mod, ch, 0, 127);
            break;
        }
    }

    uint32_t phase_increment = ComputePhaseIncrement(pitch_mod);
    phase += phase_increment;

    // COMPUTE
    int s = constrain(slope_mod * 65535 / 127, 0, 65535);
    ProcessSample(s, shape_mod * 65535 / 127, fold_mod * 32767 / 127, phase, sample);

    ForEachChannel(ch) {
      switch (output(ch)) {
      case UNIPOLAR:
        Out(ch, Proportion(sample.unipolar, 65535, HEMISPHERE_MAX_CV));
        break;
      case BIPOLAR:
        #ifdef VOR
        Out(ch, Proportion(sample.bipolar, 32767, 7680)); // hardcoded at 5V for Plum Audio
        #else
        Out(ch, Proportion(sample.bipolar, 32767, HEMISPHERE_MAX_CV / 2));
        #endif
        break;
      case EOA:
        GateOut(ch, sample.flags & FLAG_EOA);
        break;
      case EOR:
        GateOut(ch, sample.flags & FLAG_EOR);
        break;
      }
    }

    if (knob_accel > (1 << 8))
      knob_accel--;
  }

  void View() {
    ForEachChannel(ch) {
      int h = 17;
      int bottom = 32 + (h + 1) * ch;
      int last = bottom;
      for (int i = 0; i < 64; i++) {
        ProcessSample(slope_mod * 65535 / 127, shape_mod * 65535 / 127,
                      fold_mod * 32767 / 127, 0xffffffff / 64 * i, disp_sample);
        int next = 0;
        switch (output(ch)) {
        case UNIPOLAR:
          next = bottom - disp_sample.unipolar * h / 65535;
          break;
        case BIPOLAR:
          next = bottom - (disp_sample.bipolar + 32767) * h / 65535;
          break;
        case EOA:
          next = bottom - ((disp_sample.flags & FLAG_EOA) ? h : 0);
          break;
        case EOR:
          next = bottom - ((disp_sample.flags & FLAG_EOR) ? h : 0);
          break;
        }
        if (i > 0) gfxLine(i - 1, last, i, next);
        last = next;
        // gfxPixel(i, 50 - disp_sample.unipolar * 35 / 65536);
      }
    }
    uint32_t p = phase / (0xffffffff / 64);
    gfxLine(p, 15, p, 50);

    switch (cursor) {
    case 0:
      // gfxPrint(0, 55, "Frq:");
      gfxPos(0, 56);
      gfxPrintFreq(pitch);
      break;
    case 1:
      gfxPrint(0, 56, "Slope: ");
      gfxPrint(slope);
      break;
    case 2:
      gfxPrint(0, 56, "Shape: ");
      gfxPrint(shape);
      break;
    case 3:
      gfxPrint(0, 56, "Fold: ");
      gfxPrint(fold);
      break;
    case 4:
      gfxPrint(0, 56, hemisphere == 0 ? "A:" : "C:");
      gfxPrint(out_labels[output(0)]);
      gfxPrint(hemisphere == 0 ? " B:" : " D:");
      gfxPrint(out_labels[output(1)]);
      break;
    case 5:
      ForEachChannel(ch) {
          gfxIcon(0 + ch*32, 56, CV_ICON);
          gfxBitmap(8 + ch*32, 56, 3, ch ? SUB_TWO : SUP_ONE);
          gfxPrint(13 + ch*32, 56, cv_labels[cv_type(ch)]);
      }
      break;
    }
    if (EditMode()) gfxInvert(0, 55, 64, 9);
  }

  void OnButtonPress() {
    CursorAction(cursor, 5);
  }

  void OnEncoderMove(int direction) {
    if (!EditMode()) {
        MoveCursor(cursor, direction, 5);
        return;
    }

    switch (cursor) {
    case 0: {
      uint32_t old_pi = ComputePhaseIncrement(pitch);
      pitch += (knob_accel >> 8) * direction;
      while (ComputePhaseIncrement(pitch) == old_pi) {
        pitch += direction;
      }
      break;
    }
    case 1: {
      // slope += (knob_accel >> 4) * direction;
      slope = constrain(slope + direction, 0, 127);
      break;
    }
    case 2: {
      shape += direction;
      while (shape < 0) shape += 128;
      while (shape > 127) shape -= 128;
      break;
    }
    case 3: {
      fold = constrain(fold + direction, 0, 127);
      break;
    }
    case 4: {
      out += direction;
      out %= 0b10000;
      break;
    }
    case 5:
      cv += direction;
      cv %= 0b10000;
      break;
    }

    if (knob_accel < (1 << 13))
      knob_accel <<= 1;
  }

  uint64_t OnDataRequest() {
    uint64_t data = 0;
    Pack(data, PackLocation { 0, 16 }, pitch + (1 << 15));
    Pack(data, PackLocation { 16, 7 }, slope);
    Pack(data, PackLocation { 23, 7 }, shape);
    Pack(data, PackLocation { 30, 7 }, fold);
    Pack(data, PackLocation { 37, 4 }, out);
    Pack(data, PackLocation { 41, 4 }, cv);
    return data;
  }

  void OnDataReceive(uint64_t data) {
    clocks_received = 0;
    pitch = Unpack(data, PackLocation { 0, 16 }) - (1 << 15);
    slope = Unpack(data, PackLocation { 16, 7 });
    shape = Unpack(data, PackLocation { 23, 7 });
    fold = Unpack(data, PackLocation { 30, 7 });
    out = Unpack(data, PackLocation { 37, 4 });
    cv = Unpack(data, PackLocation { 41, 4 });
  }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Reset";
        help[HEMISPHERE_HELP_CVS]      = "1,2=Assignable";
        help[HEMISPHERE_HELP_OUTS]     = "A=OutA  B=OutB";
        help[HEMISPHERE_HELP_ENCODER]  = "Select/Edit params";
        //                               "------------------" <-- Size Guide
    }

private:

  enum Output {
    UNIPOLAR,
    BIPOLAR,
    EOA,
    EOR,
  };
  const char* out_labels[4] = {"Un", "Bi", "Hi", "Lo"};

  enum CV {
    FREQ,
    SLOPE,
    SHAPE,
    FOLD,
  };
  const char* cv_labels[4] = {"Hz", "Sl", "Sh", "Fo"};

  int cursor = 0;
  int16_t pitch = -3 * 12 * 128;
  int slope = 64;
  int shape = 48; // triangle
  int fold = 0;

  // actual values after CV mod
  int16_t pitch_mod;
  int slope_mod;
  int shape_mod;
  int fold_mod;
  
  int clocks_received = 0;

  uint8_t out = 0b0001; // Unipolar on A, bipolar on B
  uint8_t cv = 0b0001; // Freq on 1, shape on 2
  TidesLiteSample disp_sample;
  TidesLiteSample sample;

  int knob_accel = 1 << 8;

  uint32_t phase;

  Output output(int ch) {
    return (Output) ((out >> ((1 - ch) * 2)) & 0b11);
  }

  CV cv_type(int ch) {
    return (CV) ((cv >> ((1 - ch) * 2)) & 0b11);
  }

  void gfxPrintFreq(int16_t pitch) {
    uint32_t num = ComputePhaseIncrement(pitch);
    uint32_t denom = 0xffffffff / 16666;
    bool swap = num < denom;
    if (swap) {
      uint32_t t = num;
      num = denom;
      denom = t;
    }
    int int_part = num / denom;
    int digits = 0;
    if (int_part < 10)
      digits = 1;
    else if (int_part < 100)
      digits = 2;
    else if (int_part < 1000)
      digits = 3;
    else
      digits = 4;

    gfxPrint(int_part);
    gfxPrint(".");

    num %= denom;
    while (digits < 4) {
      num *= 10;
      gfxPrint(num / denom);
      num %= denom;
      digits++;
    }
    if (swap) {
      gfxPrint("s");
    } else {
      gfxPrint("Hz");
    }
  }
};
