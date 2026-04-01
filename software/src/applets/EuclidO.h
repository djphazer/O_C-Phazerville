// Copyright (c) 2026, Pablo Lins (https://github.com/defnpablo)
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


class EuclidOState {
public:
  static constexpr uint8_t MAX_LENGTH = 16;

  EuclidOState()
    : pattern_(0)
    , length_(16)
    , fills_(1)
    , rotation_(0)
    , playhead_cursor_(0)
    , saved_fills_(1)
    , saved_rotation_(0)
  {
    recalc_pattern();
  }

  // Getters
  uint32_t pattern() const { return pattern_; }
  uint8_t length() const { return length_; }
  uint8_t fills() const { return fills_; }
  uint8_t rotation() const { return rotation_; }
  uint8_t playhead_cursor() const { return playhead_cursor_; }
  
  bool is_playhead_step_on() const { 
    return (pattern_ >> playhead_cursor_) & 0x01;
  }

  void reset_playhead() {
    playhead_cursor_ = 0;
  }

  void advance_playhead() {
    ++playhead_cursor_;
    if (playhead_cursor_ >= length_) {
      playhead_cursor_ = 0;
    }
  }

  void retreat_playhead() {
    if (playhead_cursor_ == 0) {
      playhead_cursor_ = length_ - 1;
    } else {
      --playhead_cursor_;
    }
  }

  void set_length(uint8_t value) {
    uint8_t new_len = constrain(value, 1, MAX_LENGTH);
    if (new_len == length_) return;

    if (new_len > length_) {
      // Expanding: restore saved fills/rotation, clamped to new length
      fills_ = constrain(saved_fills_, 1, new_len);
      rotation_ = constrain(saved_rotation_, 0, new_len - 1);
    } else {
      // Shrinking: save current fills/rotation, then clamp
      saved_fills_ = fills_;
      saved_rotation_ = rotation_;
      if (fills_ > new_len) fills_ = new_len;
      if (rotation_ >= new_len) rotation_ = new_len - 1;
    }

    length_ = new_len;
    if (playhead_cursor_ >= length_) playhead_cursor_ = 0;
    recalc_pattern();
  }

  void set_fills(uint8_t value) {
    fills_ = constrain(value, 1, length_);
    saved_fills_ = fills_;
    recalc_pattern();
  }

  void set_rotation(int value) {
    // Wrap around instead of clamping
    int n = length_;
    rotation_ = ((value % n) + n) % n;
    saved_rotation_ = rotation_;
    recalc_pattern();
  }

private:
  uint32_t pattern_;
  uint8_t length_;
  uint8_t fills_;
  uint8_t rotation_;
  uint8_t playhead_cursor_;
  uint8_t saved_fills_;
  uint8_t saved_rotation_;

  void recalc_pattern() {
    pattern_ = EuclideanPattern(length_, fills_, rotation_);
  }
};

class EuclidO : public HemisphereApplet {
public:

    const char* applet_name() {
        return "EuclidO";
    }

    void Start() {
        param_cursor_ = 0;
    }

    void Reset() {
        state_.reset_playhead();
    }

    void Controller() {
        if (Clock(1)) {
            Reset();
        }

        if (Clock(0)) {
            // Output trigger on current step, then advance
            if (state_.is_playhead_step_on()) {
                ClockOut(0);
            } else {
                ClockOut(1);
            }
            state_.advance_playhead();
        }
    }

    void View() {
        DrawRing();
    }

    void OnButtonPress() {
        enum {
            FILLS_PARAM = 0,
            ROTATION_PARAM = 1,
            LENGTH_PARAM = 2,
            MAX_PARAM = 3
        };
        
        // Cycle through parameters: FILLS -> START POINT -> LENGTH
        param_cursor_ = (param_cursor_ + 1) % MAX_PARAM;
    }

    void OnEncoderMove(int direction) {
        enum {
            FILLS_PARAM = 0,
            ROTATION_PARAM = 1,
            LENGTH_PARAM = 2,
            MAX_PARAM = 3
        };
        
        // Edit mode: change the selected parameter value
        switch (param_cursor_) {
            case FILLS_PARAM:
                state_.set_fills(state_.fills() + direction);
                break;
            case ROTATION_PARAM:
                state_.set_rotation(state_.rotation() + direction);
                break;
            case LENGTH_PARAM:
                state_.set_length(state_.length() + direction);
                break;
        }
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 5}, state_.length());
        Pack(data, PackLocation {5, 5}, state_.fills());
        Pack(data, PackLocation {10, 4}, state_.rotation());
        return data;
    }

    void OnDataReceive(uint64_t data) {
        state_.set_length(Unpack(data, PackLocation {0, 5}));
        state_.set_fills(Unpack(data, PackLocation {5, 5}));
        state_.set_rotation(Unpack(data, PackLocation {10, 4}));
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "";
        help[HELP_CV2]      = "";
        help[HELP_OUT1]     = "StepOn";
        help[HELP_OUT2]     = "StepOff";
    }

private:
    EuclidOState state_;
    uint8_t param_cursor_;

    // --- Ring drawing ---
    // All 16 positions are fixed on screen. Steps beyond length are hidden.

    // Precomputed positions for 16 steps on an elliptical ring
    // cx=31, cy=38, rx=27, ry=21 — Step 0 at 12 o'clock, clockwise
    static constexpr int8_t step_x[16] = {
        31, 41, 50, 56, 58, 56, 50, 41,
        31, 21, 12,  6,  4,  6, 12, 21
    };
    static constexpr int8_t step_y[16] = {
        17, 19, 23, 30, 38, 46, 53, 57,
        59, 57, 53, 46, 38, 30, 23, 19
    };

    static constexpr int step_r = 3;
    static constexpr int cx = 31;
    static constexpr int cy = 38;

    void DrawRing() {
        const uint8_t n = state_.length();
        const uint32_t pat = state_.pattern();
        const uint8_t playhead = state_.playhead_cursor();
        const uint8_t rot = state_.rotation();

        for (uint8_t i = 0; i < n; i++) {
            int sx = step_x[i];
            int sy = step_y[i];
            bool is_on = (pat >> i) & 0x01;

            if (i == rot) {
                DrawStepTriangle(sx, sy, step_r, is_on);
            } else {
                DrawStepCircle(sx, sy, step_r, is_on);
            }

            if (i == playhead) {
                int sz = (step_r + 1) * 2 + 1;
                gfxFrame(sx - step_r - 1, sy - step_r - 1, sz, sz);
            }
        }

        // Mode label in center
        const char* label;
        int label_w; // pixel width of label
        int val;
        switch (param_cursor_) {
            default:
                label = "fills";  label_w = 30;
                val = state_.fills();
                break;
            case 1:
                label = "offset"; label_w = 36;
                val = state_.rotation();
                break;
            case 2:
                label = "length"; label_w = 36;
                val = state_.length();
                break;
        }
        gfxPrint(cx - label_w / 2, cy - 7, label);

        // Value below label
        if (val < 10) {
            gfxPrint(cx - 2, cy + 4, val);
        } else {
            gfxPrint(cx - 5, cy + 4, val);
        }
    }

    void DrawStepCircle(int x, int y, int r, bool filled) {
        gfxCircle(x, y, r);
        if (filled && r > 0) {
            int inner = r - 1;
            gfxRect(x - inner, y - inner, 2 * inner + 1, 2 * inner + 1);
        }
    }

    void DrawStepTriangle(int sx, int sy, int r, bool filled) {
        // Triangle tip points toward ring center using true direction vector
        int dx = cx - sx;
        int dy = cy - sy;

        // Compute length * 16 for fixed-point scaling (avoid float)
        // dist = sqrt(dx*dx + dy*dy); we approximate with integer math
        int dist_sq = dx * dx + dy * dy;
        // Fast integer sqrt approximation (good enough for r=3, dist~21)
        int dist = 1;
        for (int d = dist_sq; d > 0; d >>= 2) dist <<= 1;
        // Newton's method, 2 iterations
        dist = (dist + dist_sq / dist) / 2;
        dist = (dist + dist_sq / dist) / 2;
        if (dist < 1) dist = 1;

        // px,py = direction toward center, scaled to length r
        int px = dx * r / dist;
        int py = dy * r / dist;
        // perpx,perpy = perpendicular direction, scaled to r
        int perpx = -py;
        int perpy = px;

        // Tip points toward center, base is perpendicular
        int x0 = sx + px;
        int y0 = sy + py;
        int x1 = sx - perpx;
        int y1 = sy - perpy;
        int x2 = sx + perpx;
        int y2 = sy + perpy;

        gfxLine(x0, y0, x1, y1);
        gfxLine(x1, y1, x2, y2);
        // Thicken the base by drawing a second line shifted away from center
        gfxLine(x1 - px, y1 - py, x2 - px, y2 - py);
        gfxLine(x2, y2, x0, y0);

        if (filled) {
            FillTriangle(x0, y0, x1, y1, x2, y2);
        }
    }

    void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2) {
        int minX = x0 < x1 ? x0 : x1; minX = minX < x2 ? minX : x2;
        int maxX = x0 > x1 ? x0 : x1; maxX = maxX > x2 ? maxX : x2;
        int minY = y0 < y1 ? y0 : y1; minY = minY < y2 ? minY : y2;
        int maxY = y0 > y1 ? y0 : y1; maxY = maxY > y2 ? maxY : y2;
        for (int y = minY; y <= maxY; y++)
            for (int x = minX; x <= maxX; x++) {
                int d1 = (x - x1) * (y0 - y1) - (x0 - x1) * (y - y1);
                int d2 = (x - x2) * (y1 - y2) - (x1 - x2) * (y - y2);
                int d3 = (x - x0) * (y2 - y0) - (x2 - x0) * (y - y0);
                if (!((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0)))
                    gfxPixel(x, y);
            }
    }
};
