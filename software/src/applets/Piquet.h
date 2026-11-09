// Copyright (c) 2026, Hans-Henning Klos, Benjamin Mulkey, Nicholas J. Michalek
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

/* This was supposed to reuse the existing Peaks code, but alas... a new algorithm */

class Piquet : public HemisphereApplet {
public:
    static constexpr int NUM_CHANNELS = 2;
    static constexpr int NUM_STAGES = 4;
    static constexpr int STAGE_MAX_VALUE = 255;
    static constexpr int DISPLAY_HEIGHT = 30;
    static constexpr uint8_t MAX_TRIGGER_QUEUE = 4;
    static constexpr int SHAPE_LUT_SIZE = 256;
    static constexpr uint16_t SHAPE_LUT_MAX = 65535;

    inline static uint16_t lookup_shape_u16(uint16_t phase_0_1024, const uint16_t* lut) {
      // phase_0_1024 is 0-1024, map to LUT index 0-255, return full uint16_t range
      int idx = (phase_0_1024 * SHAPE_LUT_SIZE) >> 10;
      if (idx >= SHAPE_LUT_SIZE) idx = SHAPE_LUT_SIZE - 1;
      return lut[idx];
    }

    inline static uint16_t lookup_shape_inverted_u16(uint16_t phase_0_1024, const uint16_t* lut) {
      int idx = (phase_0_1024 * SHAPE_LUT_SIZE) >> 10;
      if (idx >= SHAPE_LUT_SIZE) idx = SHAPE_LUT_SIZE - 1;
      return SHAPE_LUT_MAX - lut[idx];
    }

    inline static uint16_t lookup_shape_scaled_u16(uint16_t phase_0_1024, const uint16_t* lut, int depth) {
      // Scale dipper depth around the linear ramp so derived dippers stay anchored at 0 and 1.
      int idx = (phase_0_1024 * SHAPE_LUT_SIZE) >> 10;
      if (idx >= SHAPE_LUT_SIZE) idx = SHAPE_LUT_SIZE - 1;
      const int32_t linear = (static_cast<uint32_t>(phase_0_1024) * SHAPE_LUT_MAX + 512) >> 10;
      const int32_t offset = static_cast<int32_t>(lut[idx]) - linear;
      const int32_t scaled = linear + ((offset * depth + 512) >> 10);
      return static_cast<uint16_t>(constrain(scaled, 0, static_cast<int32_t>(SHAPE_LUT_MAX)));
    }

    enum EnvType : uint8_t {
      ENV_AD,
      ENV_AHR,
      ENV_ASR,
      ENV_ADSR,
      ENV_LOOP_AD,
      ENV_LOOP_AHR,
      ENV_TYPE_LAST
    };

    enum LoopMode : uint8_t {
      LOOP_CONT,
      LOOP_GATE,
      LOOP_MODE_LAST
    };

    enum Stage : int8_t {
      STAGE_ATTACK,
      STAGE_DECAY,
      STAGE_SUSTAIN,
      STAGE_RELEASE,
      STAGE_IDLE = -1
    };

    enum DelayMode : uint8_t {
      DELAY_OFF,
      DELAY_REPLACE,
      DELAY_QUEUE,
      DELAY_MODE_LAST
    };

    enum CVMap : uint8_t {
      CVMAP_NONE,
      CVMAP_ATTACK,
      CVMAP_DECAY,
      CVMAP_SUSTAIN,
      CVMAP_RELEASE,
      CVMAP_ADR,
      CVMAP_LEVEL,
      CVMAP_LAST
    };

    enum ShapeType : uint8_t {
      SHAPE_LINEAR,
      SHAPE_EXPONENTIAL,
      SHAPE_QUARTIC,
      SHAPE_SINE,
      SHAPE_PLATEAU,
      SHAPE_CLIFF,
      SHAPE_GATE,
      SHAPE_BIG_DIPPER,
      SHAPE_MEDIUM_DIPPER,
      SHAPE_LITTLE_DIPPER,
      SHAPE_SINEFOLD,
      SHAPE_LAST
    };

    enum Cursor : uint8_t {
      CURSOR_CHANNEL,
      CURSOR_LENGTH,
      CURSOR_ENVELOPE,
      CURSOR_TYPE,
      CURSOR_LOOP_MODE,
      CURSOR_ATTACK,
      CURSOR_DECAY,
      CURSOR_SUSTAIN,
      CURSOR_RELEASE,
      CURSOR_ATTACK_SHAPE,
      CURSOR_DECAY_SHAPE,
      CURSOR_RELEASE_SHAPE,
      CURSOR_DELAY_MODE,
      CURSOR_DELAY,
      CURSOR_CV_MAP,
      CURSOR_LEVEL,
      CURSOR_INVERT,
      CURSOR_LAST
    };

    struct TriggerQueue {
      int32_t ticks[MAX_TRIGGER_QUEUE];
      uint8_t count;

      void Reset() {
        count = 0;
        for (uint8_t i = 0; i < MAX_TRIGGER_QUEUE; ++i) ticks[i] = 0;
      }

      void Add(int32_t delay_ticks, bool queue_mode) {
        if (delay_ticks <= 0) return;
        if (!queue_mode) {
          count = 1;
          ticks[0] = delay_ticks;
          return;
        }
        if (count < MAX_TRIGGER_QUEUE) {
          ticks[count++] = delay_ticks;
          return;
        }
        // Queue is full: preserve pending triggers and ignore the new one.
        return;
      }

      bool TickReady() {
        if (!count) return false;
        for (uint8_t i = 0; i < count; ++i) --ticks[i];
        if (ticks[0] <= 0) {
          for (uint8_t i = 1; i < count; ++i) ticks[i - 1] = ticks[i];
          --count;
          return true;
        }
        return false;
      }
    };

    struct Channel {
      uint8_t type;
      uint8_t stage_value[NUM_STAGES];
      uint8_t delay_index;
      uint8_t delay_mode;
      uint8_t cv_map;
      uint8_t level;
      uint8_t attack_shape;
      uint8_t decay_shape;
      uint8_t release_shape;
      uint8_t loop_mode;
      bool loop_enabled;
      bool inverted;
      uint8_t length_index;

      Stage stage;
      int32_t stage_ticks;
      bool gate_prev;
      simfloat amplitude;
      int segment_start_cv;
      TriggerQueue trigger_queue;

      void Init(uint8_t ch) {
        type = (ch == 0) ? ENV_ADSR : ENV_ASR;
        stage_value[STAGE_ATTACK] = 28 + ch * 8;
        stage_value[STAGE_DECAY] = 52;
        stage_value[STAGE_SUSTAIN] = 160;
        stage_value[STAGE_RELEASE] = 44 + ch * 8;
        delay_index = 0;
        delay_mode = DELAY_OFF;
        cv_map = CVMAP_NONE;
        level = 255;
        attack_shape = SHAPE_QUARTIC;
        decay_shape = SHAPE_EXPONENTIAL;
        release_shape = SHAPE_EXPONENTIAL;
        loop_mode = LOOP_CONT;
        loop_enabled = true;
        inverted = false;
        length_index = 0;

        stage = STAGE_IDLE;
        stage_ticks = 0;
        gate_prev = false;
        amplitude = 0;
        segment_start_cv = 0;
        trigger_queue.Reset();
      }

      uint64_t pack() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,3}, type);
        Pack(data, PackLocation {3,8}, stage_value[STAGE_ATTACK]);
        Pack(data, PackLocation {11,8}, stage_value[STAGE_DECAY]);
        Pack(data, PackLocation {19,8}, stage_value[STAGE_SUSTAIN]);
        Pack(data, PackLocation {27,8}, stage_value[STAGE_RELEASE]);
        Pack(data, PackLocation {35,7}, delay_index);
        Pack(data, PackLocation {43,2}, delay_mode);
        Pack(data, PackLocation {45,3}, cv_map);
        Pack(data, PackLocation {48,8}, level);
        Pack(data, PackLocation {56,1}, inverted ? 1 : 0);
        Pack(data, PackLocation {57,1}, loop_mode);
        Pack(data, PackLocation {58,1}, loop_enabled ? 1 : 0);
        Pack(data, PackLocation {59,2}, length_index);
        // 3 empty bits
        return data;
      }
      void unpack(uint64_t data) {
        type = constrain(Unpack(data, PackLocation {0,3}), 0, ENV_TYPE_LAST - 1);
        stage_value[STAGE_ATTACK] = constrain(Unpack(data, PackLocation {3,8}), 1, STAGE_MAX_VALUE);
        stage_value[STAGE_DECAY] = constrain(Unpack(data, PackLocation {11,8}), 1, STAGE_MAX_VALUE);
        stage_value[STAGE_SUSTAIN] = constrain(Unpack(data, PackLocation {19,8}), 0, STAGE_MAX_VALUE);
        stage_value[STAGE_RELEASE] = constrain(Unpack(data, PackLocation {27,8}), 1, STAGE_MAX_VALUE);
        delay_index = constrain(Unpack(data, PackLocation {35,7}), 0, 126);
        delay_mode = constrain(Unpack(data, PackLocation {43,2}), 0, DELAY_MODE_LAST - 1);
        cv_map = constrain(Unpack(data, PackLocation {45,3}), 0, CVMAP_LAST - 1);
        level = constrain(Unpack(data, PackLocation {48,8}), 0, 255);
        inverted = Unpack(data, PackLocation {56,1});
        loop_mode = Unpack(data, PackLocation {57,1});
        loop_enabled = Unpack(data, PackLocation {58,1});
        length_index = constrain(Unpack(data, PackLocation {59,2}), 0, 3);
        // 3 empty bits
      }

      bool is_looping() const {
        return type == ENV_LOOP_AD || type == ENV_LOOP_AHR;
      }

      bool should_repeat() const {
        return is_looping() && loop_enabled;
      }

      bool has_decay() const {
        return type == ENV_AD || type == ENV_AHR ||
               type == ENV_ADSR || type == ENV_LOOP_AD ||
               type == ENV_LOOP_AHR;
      }

      bool has_sustain_hold() const {
        return type == ENV_ASR || type == ENV_ADSR;
      }

    };

    const char* applet_name() {
      return "Piquet";
    }

    const uint8_t* applet_icon() {
      return PhzIcons::ADSR_EG;
    }

    void Start() {
      selected_channel = 0;
      cursor = CURSOR_TYPE;
      ForEachChannel(ch) channels[ch].Init(ch);
    }

    void Controller();

    void View();
    void DrawFullScreen() override;

    void OnEncoderMove(int direction);

    void OnButtonPress() {
      CursorToggle();
    }

    void AuxButton() {
      const EnvType old_type = static_cast<EnvType>(channels[selected_channel].type);
      const int stage_index = cursor_stage_index(cursor, old_type);
      const bool shape_cursor =
        cursor == CURSOR_ATTACK_SHAPE ||
        cursor == CURSOR_DECAY_SHAPE ||
        cursor == CURSOR_RELEASE_SHAPE;

      selected_channel = 1 - selected_channel;

      if (cursor == CURSOR_LOOP_MODE &&
          !cursor_visible_for_type(cursor, static_cast<EnvType>(channels[selected_channel].type))) {
        cursor = CURSOR_ATTACK;
        parameter_navigation = false;
      }

      if (stage_index >= 0) {
        cursor = cursor_for_stage_index(
          stage_index, static_cast<EnvType>(channels[selected_channel].type), shape_cursor);
      }
    }

    uint64_t OnDataRequest();

    void OnDataReceive(uint64_t data);

protected:
    void SetHelp() {
      help[HELP_DIGITAL1] = "Trig 1";
      help[HELP_DIGITAL2] = "Trig 2";
      help[HELP_CV1] = "CV Mod1";
      help[HELP_CV2] = "CV Mod2";
      help[HELP_OUT1] = "Env 1";
      help[HELP_OUT2] = "Env 2";
      help[HELP_EXTRA1] = "Btn: Edit";
      help[HELP_EXTRA2] = "Aux: Chan";
    }

private:
    int selected_channel = 0;
    int envelope_drawing_channel = 0;
    int drawing_x_offset = 0;
    int cursor = CURSOR_CHANNEL;
    bool parameter_navigation = false;
    Channel channels[NUM_CHANNELS];

    bool cursor_visible_for_type(int item, EnvType type) const {
      switch (item) {
        case CURSOR_LOOP_MODE:
          return type == ENV_LOOP_AD || type == ENV_LOOP_AHR;
        case CURSOR_DECAY:
        case CURSOR_DECAY_SHAPE:
          if (item == CURSOR_DECAY_SHAPE &&
              (type == ENV_AHR || type == ENV_LOOP_AHR)) {
            return false;
          }
          return type == ENV_AD || type == ENV_AHR ||
                 type == ENV_ADSR || type == ENV_LOOP_AD ||
                 type == ENV_LOOP_AHR;
        case CURSOR_SUSTAIN:
          return type == ENV_ASR || type == ENV_ADSR;
        case CURSOR_RELEASE:
        case CURSOR_RELEASE_SHAPE:
          return type == ENV_AHR || type == ENV_ASR || type == ENV_ADSR ||
                 type == ENV_LOOP_AHR;
        default:
          return true;
      }
    }

    int stage_count(EnvType type) const {
      switch (type) {
        case ENV_AD:
        case ENV_LOOP_AD:
          return 2;
        case ENV_AHR:
        case ENV_ASR:
        case ENV_LOOP_AHR:
          return 3;
        case ENV_ADSR:
          return 4;
        default:
          return 2;
      }
    }

    int cursor_stage_index(int item, EnvType type) const {
      switch (item) {
        case CURSOR_ATTACK:
        case CURSOR_ATTACK_SHAPE:
          return 0;
        case CURSOR_DECAY:
        case CURSOR_DECAY_SHAPE:
          return (type == ENV_AD || type == ENV_AHR ||
                  type == ENV_ADSR || type == ENV_LOOP_AD ||
                  type == ENV_LOOP_AHR)
            ? 1 : -1;
        case CURSOR_SUSTAIN:
          if (type == ENV_ASR) return 1;
          if (type == ENV_ADSR) return 2;
          return -1;
        case CURSOR_RELEASE:
        case CURSOR_RELEASE_SHAPE:
          if (type == ENV_AHR || type == ENV_LOOP_AHR) return 2;
          if (type == ENV_ASR) return 2;
          if (type == ENV_ADSR) return 3;
          return -1;
        default:
          return -1;
      }
    }

    int cursor_for_stage_index(int index, EnvType type, bool shape) const {
      index = constrain(index, 0, stage_count(type) - 1);

      if (index == 0) return shape ? CURSOR_ATTACK_SHAPE : CURSOR_ATTACK;

      if (type == ENV_AD || type == ENV_LOOP_AD) {
        return shape ? CURSOR_DECAY_SHAPE : CURSOR_DECAY;
      }

      if (type == ENV_AHR || type == ENV_LOOP_AHR) {
        if (index == 1) return CURSOR_DECAY;
        return shape ? CURSOR_RELEASE_SHAPE : CURSOR_RELEASE;
      }

      if (type == ENV_ASR) {
        if (index == 1) return CURSOR_SUSTAIN;
        return shape ? CURSOR_RELEASE_SHAPE : CURSOR_RELEASE;
      }

      if (index == 1) return shape ? CURSOR_DECAY_SHAPE : CURSOR_DECAY;
      if (index == 2) return CURSOR_SUSTAIN;
      return shape ? CURSOR_RELEASE_SHAPE : CURSOR_RELEASE;
    }

    int next_visible_cursor(int current, int direction, EnvType type) {
      if (current == CURSOR_CHANNEL) {
        parameter_navigation = false;
        return direction > 0 ? CURSOR_TYPE : CURSOR_CHANNEL;
      }

      if (current == CURSOR_TYPE) {
        parameter_navigation = false;
        if (direction < 0) return CURSOR_CHANNEL;
        if (type == ENV_LOOP_AD || type == ENV_LOOP_AHR)
          return CURSOR_LOOP_MODE;
        return CURSOR_LENGTH;
      }

      if (current == CURSOR_LENGTH) {
        parameter_navigation = false;
        if (direction < 0) {
          if (type == ENV_LOOP_AD || type == ENV_LOOP_AHR)
            return CURSOR_LOOP_MODE;
          return CURSOR_TYPE;
        }
        return cursor_for_stage_index(0, type, false);
      }

      if (current == CURSOR_LOOP_MODE) {
        parameter_navigation = false;
        return direction < 0 ? CURSOR_TYPE : CURSOR_LENGTH;
      }

      if (current == CURSOR_ENVELOPE) {
        parameter_navigation = false;
        return direction < 0 ? CURSOR_CHANNEL : CURSOR_TYPE;
      }

      if (parameter_navigation) {
        if (current == CURSOR_ATTACK_SHAPE && direction < 0) {
          parameter_navigation = false;
          return cursor_for_stage_index(stage_count(type) - 1, type, false);
        }

        int next = current;
        while (true) {
          next += direction;

          // Stop at either end; never wrap around.
          if (next < CURSOR_ATTACK_SHAPE || next >= CURSOR_LAST) {
            return current;
          }

          // Skip the main sequence's stage values.
          if (next == CURSOR_ENVELOPE ||
              next == CURSOR_ATTACK ||
              next == CURSOR_DECAY ||
              next == CURSOR_SUSTAIN ||
              next == CURSOR_RELEASE) {
            continue;
          }

          if (cursor_visible_for_type(next, type)) return next;
        }
      }

      const int stage = cursor_stage_index(current, type);
      if (stage >= 0 &&
          current != CURSOR_ATTACK_SHAPE &&
          current != CURSOR_DECAY_SHAPE &&
          current != CURSOR_RELEASE_SHAPE) {
        if (direction > 0) {
          if (stage + 1 >= stage_count(type)) {
            parameter_navigation = true;
            return CURSOR_ATTACK_SHAPE;
          }
          return cursor_for_stage_index(stage + 1, type, false);
        }

        if (stage == 0) {
          return CURSOR_LENGTH;
        }
        return cursor_for_stage_index(stage - 1, type, false);
      }

      parameter_navigation = true;
      return current;
    }

    static int StageTicks(int value) {
      return HS::kShapeExponential[constrain(value, 0, STAGE_MAX_VALUE)] >> 1;
    }

    bool read_gate_source(uint8_t source) const {
      return Gate(constrain(source, 0, 1));
    }

    void begin_attack(Channel &c) {
      set_stage(c, STAGE_ATTACK);
    }

    void set_stage(Channel &c, Stage stage) {
      c.stage = stage;
      c.stage_ticks = 0;
      c.segment_start_cv = simfloat2int(c.amplitude);
    }


    void process_envelope(Channel &c, bool gate, bool gate_fall, int cv_in, int ch) {
      const bool hold_at_peak = c.type == ENV_AHR || c.type == ENV_LOOP_AHR;
      const bool ad_envelope = c.type == ENV_AD || c.type == ENV_LOOP_AD;
      const bool has_sustain_hold = c.has_sustain_hold();

      if (c.stage == STAGE_IDLE) {
        if (!c.is_looping()) c.amplitude = 0;
        return;
      }

      if (gate_fall && has_sustain_hold) {
        set_stage(c, STAGE_RELEASE);
      }

      ++c.stage_ticks;

      if (c.stage == STAGE_ATTACK) {
        int attack = c.stage_value[STAGE_ATTACK];
        if (c.cv_map == CVMAP_ATTACK || c.cv_map == CVMAP_ADR) {
          Modulate(attack, ch, 1, STAGE_MAX_VALUE);
        }
        if (run_segment(c, HEMISPHERE_MAX_CV, StageTicks(attack) * (1 << c.length_index))) {
          if (c.has_decay()) {
            set_stage(c, STAGE_DECAY);
          } else if (has_sustain_hold) {
            set_stage(c, STAGE_SUSTAIN);
          } else if (c.type == ENV_LOOP_AHR) {
            set_stage(c, gate ? STAGE_SUSTAIN : STAGE_RELEASE);
          } else {
            set_stage(c, STAGE_RELEASE);
          }
        }
        return;
      }

      if (c.stage == STAGE_DECAY) {
        int decay = c.stage_value[STAGE_DECAY];
        if (c.cv_map == CVMAP_DECAY || c.cv_map == CVMAP_ADR) {
          Modulate(decay, ch, 1, STAGE_MAX_VALUE);
        }

        int target = 0;

        if (hold_at_peak) {
          target = HEMISPHERE_MAX_CV;
        } else if (!ad_envelope) {
          int sustain_cv = c.stage_value[STAGE_SUSTAIN];
          if (c.cv_map == CVMAP_SUSTAIN) {
            Modulate(sustain_cv, ch, 0, STAGE_MAX_VALUE);
          }
          target = Proportion(sustain_cv, STAGE_MAX_VALUE, HEMISPHERE_MAX_CV);
        }

        if (run_segment(c, target, StageTicks(decay) * (1 << c.length_index))) {
          if (hold_at_peak) {
            set_stage(c, STAGE_RELEASE);
          } else if (ad_envelope) {
            set_stage(c, c.should_repeat() ? STAGE_ATTACK : STAGE_IDLE);
          } else {
            set_stage(c, STAGE_SUSTAIN);
          }
        }
        return;
      }

      if (c.stage == STAGE_SUSTAIN) {
        if (has_sustain_hold) {
          int sustain_cv = c.stage_value[STAGE_SUSTAIN];
          if (c.cv_map == CVMAP_SUSTAIN) {
            Modulate(sustain_cv, ch, 0, STAGE_MAX_VALUE);
          }
          c.amplitude = int2simfloat(Proportion(sustain_cv, STAGE_MAX_VALUE, HEMISPHERE_MAX_CV));
          if (!gate) {
            set_stage(c, STAGE_RELEASE);
          }
        }
        return;
      }

      if (c.stage == STAGE_RELEASE) {
        int release = c.stage_value[STAGE_RELEASE];
        if (c.cv_map == CVMAP_RELEASE || c.cv_map == CVMAP_ADR) {
          Modulate(release, ch, 1, STAGE_MAX_VALUE);
        }
        if (run_segment(c, 0, StageTicks(release) * 2 * (1 << c.length_index))) {
          set_stage(c, c.should_repeat() ? STAGE_ATTACK : STAGE_IDLE);
        }
      }
    }

    // Apply shape transformation to 10-bit phase (0..1024) and return u16 phase (0..65535)
    uint16_t apply_shape_u16(uint16_t phase_0_1024, uint8_t shape) {
      const uint16_t p = constrain(phase_0_1024, 0, 1024);

      switch (shape) {
        case SHAPE_LINEAR:
          // Convert 10-bit phase to 16-bit phase with rounding.
          return (static_cast<uint32_t>(p) * SHAPE_LUT_MAX + 512) >> 10;
        case SHAPE_EXPONENTIAL:
          return lookup_shape_u16(p, HS::kShapeExponential);
        case SHAPE_QUARTIC:
          return lookup_shape_u16(p, HS::kShapeQuartic);
        case SHAPE_SINE:
          return lookup_shape_u16(p, HS::kShapeSine);
        case SHAPE_PLATEAU:
          return lookup_shape_u16(p, HS::kShapePlateau);
        case SHAPE_CLIFF:
          // Computed by inverting PLATEAU (fast start → slow end)
          return lookup_shape_inverted_u16(p, HS::kShapePlateau);
        case SHAPE_GATE:
          // Hold low, then jump to the top.
          return (p < 768) ? 3276 : SHAPE_LUT_MAX;
        case SHAPE_BIG_DIPPER:
          return lookup_shape_u16(p, HS::kShapeBigDipper);
        case SHAPE_MEDIUM_DIPPER:
          // Derived from BIG_DIPPER with 50% depth scaling
          return lookup_shape_scaled_u16(p, HS::kShapeBigDipper, 512);
        case SHAPE_LITTLE_DIPPER:
          // Derived from BIG_DIPPER with 25% depth scaling
          return lookup_shape_scaled_u16(p, HS::kShapeBigDipper, 256);
        case SHAPE_SINEFOLD:
          return lookup_shape_u16(p, HS::kShapeSinefold);
        default:
          return (static_cast<uint32_t>(p) * SHAPE_LUT_MAX + 512) >> 10;
      }
    }

    bool run_segment(Channel &c, int target_cv, int total_ticks) {
      if (total_ticks < 1) total_ticks = 1;
      const int ticks_remaining = total_ticks - c.stage_ticks;
      if (ticks_remaining <= 0) {
        c.amplitude = int2simfloat(constrain(target_cv, 0, HEMISPHERE_MAX_CV));
        return true;
      }

      // Normalized segment phase in 10-bit fixed-point (0..1024).
      const uint16_t phase_0_1024 = constrain((static_cast<uint32_t>(c.stage_ticks) * 1024u) / total_ticks, 0u, 1024u);

      uint8_t shape = c.attack_shape;
      if (c.stage == STAGE_DECAY) {
        shape = c.decay_shape;
      } else if (c.stage == STAGE_RELEASE) {
        shape = c.release_shape;
      }
      const uint16_t shaped_phase_u16 =
        apply_shape_u16(phase_0_1024, shape);

      const int32_t start = c.segment_start_cv;
      const int32_t delta = static_cast<int32_t>(target_cv) - start;
      const int32_t abs_delta = (delta >= 0) ? delta : -delta;
      // Use a fast fixed-point interpolation; the endpoint checks below ensure the
      // envelope reaches its target exactly.
      const int32_t interp = (abs_delta * static_cast<int32_t>(shaped_phase_u16) + (1 << 15)) >> 16;
      int32_t amp_cv = (delta >= 0) ? (start + interp) : (start - interp);
      amp_cv = constrain(amp_cv, 0, HEMISPHERE_MAX_CV);
      c.amplitude = int2simfloat(amp_cv);

      const bool timed_flat_hold =
        (c.type == ENV_AHR || c.type == ENV_LOOP_AHR) &&
        c.stage == STAGE_DECAY;
      if (c.stage_ticks >= total_ticks ||
          (shape != SHAPE_CLIFF && !timed_flat_hold &&
           abs(target_cv - amp_cv) <= 1)) {
        c.amplitude = int2simfloat(constrain(target_cv, 0, HEMISPHERE_MAX_CV));
        return true;
      }
      return false;
    }

    int effective_level(const Channel &c, int cv_in) {
      int level = c.level;
      if (c.cv_map == CVMAP_LEVEL) {
        level = constrain(level + Proportion(cv_in, HEMISPHERE_MAX_INPUT_CV, STAGE_MAX_VALUE / 2), 0, 255);
      }
      return level;
    }

    int apply_level_and_polarity(const Channel &c, int cv_in) {
      int out = simfloat2int(c.amplitude);
      const int level = effective_level(c, cv_in);
      out = Proportion(out, HEMISPHERE_MAX_CV, Proportion(level, 255, HEMISPHERE_MAX_CV));
      if (c.inverted) out = HEMISPHERE_MAX_CV - out;
      return constrain(out, 0, HEMISPHERE_MAX_CV);
    }

    static const char* type_name(uint8_t t);
    static const char* dmode_name(uint8_t m);
    static const char* cvmap_name(uint8_t m);
    static const char* shape_name(uint8_t s);

    void DrawIndicators();
    // Draw the parameter row at a horizontal offset. The default of zero
    // preserves the normal Hemisphere layout.
    void DrawParamRow(int x_offset = 0);
    const char* cursor_display_label() const;
    static int numeric_text_width(int value);
    int cursor_value_width(const Channel &c) const;
    const char* cursor_label() const;

    void DrawEnvelope(int channel, int x_offset);
    void draw_curve_segment(int x, int x2, int start_level,
                            int end_level, uint8_t shape, bool draw,
                            int y_offset = 0);
    void draw_stage_highlight(int x, int x2, int start_level,
                              int end_level, uint8_t shape);
    int draw_attack(const Channel &c, int attack, int x, int length);
    int draw_decay(const Channel &c, int decay, int sustain, int x, int length);
    int draw_sustain(const Channel &c, int sustain, int x, int length);
    int draw_release(const Channel &c, int sustain, int release, int x, int length);
};

void Piquet::Controller() {
      ForEachChannel(ch) {
        Channel &c = channels[ch];
        const int cv_in = DetentedIn(ch);
        const bool gate = read_gate_source(ch);
        const bool clock_trigger = Clock(ch);
        const bool gate_fall = !gate && c.gate_prev;
        c.gate_prev = gate;

        if (c.is_looping()) {
          if (c.loop_mode == LOOP_CONT) {
            if (clock_trigger) c.loop_enabled = !c.loop_enabled;
          } else {
            c.loop_enabled = gate;
          }
        }

        if (clock_trigger) {
          const int32_t delay_ms = c.delay_index < 100
              ? static_cast<int32_t>(c.delay_index) * 10
              : 1000 + static_cast<int32_t>(c.delay_index - 100) * 100;
          const int32_t dticks = (delay_ms * 1000L) / 60;
          if (c.delay_mode == DELAY_OFF || dticks <= 0) {
            c.trigger_queue.Reset();
            begin_attack(c);
          } else {
            c.trigger_queue.Add(dticks, c.delay_mode == DELAY_QUEUE);
          }
        }

        if (c.trigger_queue.TickReady()) begin_attack(c);

        process_envelope(c, gate, gate_fall, cv_in, ch);
        Out(ch, apply_level_and_polarity(c, cv_in));
      }
}


void FLASHMEM Piquet::View() {
      if (EditMode() || cursor < CURSOR_ATTACK_SHAPE ||
          cursor == CURSOR_ATTACK_SHAPE ||
          cursor == CURSOR_DECAY_SHAPE ||
          cursor == CURSOR_RELEASE_SHAPE) SetLabel(cursor_label());
      else SetLabel("");
      DrawIndicators();
      DrawEnvelope(selected_channel, 0);
      if (cursor == CURSOR_TYPE || cursor == CURSOR_LOOP_MODE)
        gfxFrame(0, 31, 64, 32);
      DrawParamRow();
      SetAux(true);
}

// Draw both envelopes side by side, with the selected channel's controls
// and parameter row kept on that channel's side of the fullscreen display.
void FLASHMEM Piquet::DrawFullScreen() {
  // Draw channel A on the left and channel B on the right.
  const int x_offset = -gfx_offset;
  DrawEnvelope(0, x_offset);
  DrawEnvelope(1, 64 + x_offset);

  // Draw each output meter at its outside edge, growing toward the center.
  for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
    int w = constrain(
      Proportion(ViewOut(ch), HEMISPHERE_MAX_CV, 62), 0, 62);
    int x = ((ch == 0) ? 0 : 128 - w) + x_offset;
    gfxRect(x, 15, w, 2);
  }

  // Keep both channel labels visible, with a box around the selected A/B.
  for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
    const int channel_x = ch * 64 + x_offset;
    gfxPrint(channel_x, 22, OutputLabel(ch));
    if (ch == selected_channel &&
        cursor == CURSOR_CHANNEL && EditMode()) {
      gfxFrame(channel_x, 20, 8, 11);
    } else {
      gfxInvert(channel_x, 21, 7, 9);
    }
  }

  // Keep the existing navigation and parameter editing layout.
  const int channel_x = selected_channel * 64 + x_offset;
  if (cursor == CURSOR_CHANNEL) {
    gfxIcon(channel_x + 8, 22, LEFT_ICON);
  } else if (cursor == CURSOR_LENGTH) {
    gfxIcon(channel_x + 30, 22, RIGHT_ICON);
  }

  if (cursor == CURSOR_TYPE || cursor == CURSOR_LOOP_MODE) {
    gfxFrame(channel_x, 31, 64, 32);
  }

  DrawParamRow(selected_channel * 64 + x_offset);

  SetAux(true);
}

void FLASHMEM Piquet::OnEncoderMove(int direction) {
      if (!EditMode()) {
        cursor = next_visible_cursor(cursor, direction, static_cast<EnvType>(channels[selected_channel].type));

        SetLabel(cursor_label());
        return;
      }

      if (cursor == CURSOR_CHANNEL) {
        selected_channel = constrain(selected_channel + direction, 0, NUM_CHANNELS - 1);
        return;
      }

      Channel &c = channels[selected_channel];
      switch (cursor) {
        case CURSOR_LENGTH:
          c.length_index = constrain(c.length_index + direction, 0, 3);
          break;
        case CURSOR_TYPE:
          c.type = constrain(c.type + direction, 0, ENV_TYPE_LAST - 1);
          break;
        case CURSOR_LOOP_MODE:
          c.loop_mode = constrain(
            c.loop_mode + direction, 0, LOOP_MODE_LAST - 1);
          break;
        case CURSOR_ATTACK:
          c.stage_value[STAGE_ATTACK] = constrain(c.stage_value[STAGE_ATTACK] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_ATTACK_SHAPE:
          c.attack_shape = constrain(c.attack_shape + direction, 0, SHAPE_LAST - 1);
          break;
        case CURSOR_DECAY:
          c.stage_value[STAGE_DECAY] = constrain(c.stage_value[STAGE_DECAY] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_DECAY_SHAPE:
          c.decay_shape = constrain(c.decay_shape + direction, 0, SHAPE_LAST - 1);
          break;
        case CURSOR_SUSTAIN:
          c.stage_value[STAGE_SUSTAIN] = constrain(c.stage_value[STAGE_SUSTAIN] + direction, 0, STAGE_MAX_VALUE);
          break;
        case CURSOR_RELEASE:
          c.stage_value[STAGE_RELEASE] = constrain(c.stage_value[STAGE_RELEASE] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_RELEASE_SHAPE:
          c.release_shape = constrain(c.release_shape + direction, 0, SHAPE_LAST - 1);
          break;
        case CURSOR_DELAY:
          c.delay_index = constrain(c.delay_index + direction, 0, 126);
          break;
        case CURSOR_DELAY_MODE:
          c.delay_mode = constrain(c.delay_mode + direction, 0, DELAY_MODE_LAST - 1);
          break;
        case CURSOR_CV_MAP:
          c.cv_map = constrain(c.cv_map + direction, 0, CVMAP_LAST - 1);
          break;
        case CURSOR_LEVEL:
          c.level = constrain(c.level + direction, 0, 255);
          break;
        case CURSOR_INVERT:
          if (direction) c.inverted = !c.inverted;
          break;
        default:
          break;
      }
}

void FLASHMEM Piquet::DrawIndicators() {
      ForEachChannel(ch) {
        int w = Proportion(ViewOut(ch), HEMISPHERE_MAX_CV, 62);
        gfxRect(0, 15 + (ch * 3), w, 2);
      }

      gfxPrint(0, 22, OutputLabel(selected_channel));

      if (cursor == CURSOR_CHANNEL && EditMode()) {
        gfxFrame(0, 20, 8, 11);
      } else {
        gfxInvert(0, 21, 7, 9);

        if (cursor == CURSOR_CHANNEL) {
          gfxIcon(8, 22, LEFT_ICON);
        } else if (cursor == CURSOR_LENGTH) {
          gfxIcon(30, 22, RIGHT_ICON);
        }
      }
}

// Apply the offset to parameter text, icons, and the edit cursor.
void FLASHMEM Piquet::DrawParamRow(int x_offset) {
      Channel &c = channels[selected_channel];

      if (cursor == CURSOR_CHANNEL ||
          cursor == CURSOR_TYPE || cursor == CURSOR_LENGTH ||
          cursor == CURSOR_LOOP_MODE) {
        if (cursor == CURSOR_TYPE) {
          const char* value = type_name(c.type);
          const int value_x = 9 + x_offset;
          gfxPrint(value_x, 22, value);
          if (EditMode()) gfxInvert(value_x, 21, strlen(value) * 6, 9);
        } else if (cursor == CURSOR_LOOP_MODE) {
          const char* value = c.loop_mode == LOOP_GATE ? "GTD" : "CON";
          gfxIcon(9 + x_offset, 22, LOOP_ICON);
          gfxPrint(19 + x_offset, 22, value);
          if (EditMode()) gfxInvert(19 + x_offset, 21, strlen(value) * 6, 9);
        }

        static const char* length_names[] = {"x1", "x2", "x4", "x8"};
        gfxIcon(41 + x_offset, 22, ENV_ICON);
        gfxPrint(50 + x_offset, 22, length_names[constrain(c.length_index, 0, 3)]);
        if (cursor == CURSOR_LENGTH && EditMode())
          gfxCursor(50 + x_offset, 31, 12, 9, cursor_label());
        return;
      }

      if (cursor >= CURSOR_ATTACK_SHAPE) {
        const char *label = "";
        const char *value = nullptr;
        int value_x;
        int value_width;

        switch (cursor) {
          case CURSOR_ATTACK_SHAPE:
            label = "AShp:";
            value = shape_name(c.attack_shape);
            break;
          case CURSOR_DECAY_SHAPE:
            label = "DShp:";
            value = shape_name(c.decay_shape);
            break;
          case CURSOR_RELEASE_SHAPE:
            label = "RShp:";
            value = shape_name(c.release_shape);
            break;
          case CURSOR_DELAY:
            label = "Dly:";
            break;
          case CURSOR_DELAY_MODE:
            label = "DlyM:";
            value = dmode_name(c.delay_mode);
            break;
          case CURSOR_CV_MAP:
            label = "Cv:";
            value = cvmap_name(c.cv_map);
            break;
          case CURSOR_LEVEL:
            label = "Lvl:";
            break;
          case CURSOR_INVERT:
            label = "Inv:";
            value = c.inverted ? "Yes" : "No";
            break;
          default:
            break;
        }

        gfxPrint(9 + x_offset, 22, label);
        value_x = x_offset + 9 + static_cast<int>(strlen(label)) * 6;

        if (cursor == CURSOR_DELAY) {
          if (c.delay_index < 100) {
            gfxPrint(value_x, 22, ".");
            if (c.delay_index < 10) gfxPrint("0");
            gfxPrint(c.delay_index);
          } else {
            const uint8_t tenths = c.delay_index - 90;
            gfxPrint(value_x, 22, tenths / 10);
            gfxPrint(".");
            gfxPrint(tenths % 10);
          }
          gfxPrint("s");
        } else if (cursor == CURSOR_LEVEL) {
          gfxPrint(value_x, 22, Proportion(effective_level(c, DetentedIn(selected_channel)), 255, 100));
          gfxPrint("%");
        } else if (value) {
          gfxPrint(value_x, 22, value);
        }

        if (cursor == CURSOR_ATTACK_SHAPE ||
            cursor == CURSOR_DECAY_SHAPE ||
            cursor == CURSOR_RELEASE_SHAPE) {
          value_width = min(static_cast<int>(strlen(value)) * 6, 64 - (value_x - x_offset));
        } else if (cursor == CURSOR_DELAY) {
          value_width = c.delay_index < 100 ? 24 : 24;
        } else {
          value_width = min(cursor_value_width(c), 64 - (value_x - x_offset));
        }

        if (EditMode()) {
          gfxCursor(value_x, 31, value_width, 9, cursor_display_label());
        }
        return;
      }

      switch (cursor) {
        case CURSOR_LOOP_MODE: {
          const char* loop_text = c.loop_mode == LOOP_GATE ? "GTD" : "CON";
          gfxIcon(9 + x_offset, 22, LOOP_ICON);
          gfxPrint(19 + x_offset, 22, loop_text);
          if (EditMode()) gfxCursor(19 + x_offset, 31, 18, 9, cursor_label());
          break;
        }
        case CURSOR_ATTACK:
          gfxPrint(9 + x_offset, 22, StageTicks(c.stage_value[STAGE_ATTACK]) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_DECAY:
          gfxPrint(9 + x_offset, 22, StageTicks(c.stage_value[STAGE_DECAY]) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_SUSTAIN:
          gfxPrint(9 + x_offset, 22,
                   Proportion(c.stage_value[STAGE_SUSTAIN],
                              STAGE_MAX_VALUE, 100));
          gfxPrint("%");
          break;
        case CURSOR_RELEASE:
          gfxPrint(9 + x_offset, 22,
                   (StageTicks(c.stage_value[STAGE_RELEASE]) * 2) / 17);
          gfxPrint("ms");
          break;
        default:
          break;
      }

      if (cursor >= CURSOR_ATTACK && cursor != CURSOR_LOOP_MODE && EditMode()) {
        gfxCursor(9 + x_offset, 31, cursor_value_width(c), 9, cursor_label());
      }
}

void FLASHMEM Piquet::DrawEnvelope(int channel, int x_offset) {
      envelope_drawing_channel = channel;
      drawing_x_offset = x_offset;
      Channel &c = channels[channel];

      int attack = c.stage_value[STAGE_ATTACK];
      int decay = c.stage_value[STAGE_DECAY];
      int sustain = c.stage_value[STAGE_SUSTAIN];
      int release = c.stage_value[STAGE_RELEASE];

      if (c.cv_map == CVMAP_ATTACK || c.cv_map == CVMAP_ADR) {
        Modulate(attack, channel, 1, STAGE_MAX_VALUE);
      }
      if (c.cv_map == CVMAP_DECAY || c.cv_map == CVMAP_ADR) {
        Modulate(decay, channel, 1, STAGE_MAX_VALUE);
      }
      if (c.cv_map == CVMAP_SUSTAIN) {
        Modulate(sustain, channel, 0, STAGE_MAX_VALUE);
      }
      if (c.cv_map == CVMAP_RELEASE || c.cv_map == CVMAP_ADR) {
        Modulate(release, channel, 1, STAGE_MAX_VALUE);
      }

      const bool has_decay = c.has_decay();
      const bool has_sustain = c.has_sustain_hold();
      const bool has_release = c.type == ENV_AHR || c.type == ENV_ASR ||
        c.type == ENV_ADSR || c.type == ENV_LOOP_AHR;

      int length = attack;
      if (has_decay) length += decay;
      if (has_sustain) length += 36;
      if (has_release) length += release;

      int x = draw_attack(c, attack, 0, length);

      if (has_decay) {
        const int decay_target = (c.type == ENV_AHR || c.type == ENV_LOOP_AHR)
          ? STAGE_MAX_VALUE
          : ((c.type == ENV_AD || c.type == ENV_LOOP_AD) ? 0 : sustain);
        x = draw_decay(c, decay, decay_target, x, length);
      }

      if (has_sustain) {
        if (!has_decay) {
          const int ys = Proportion(sustain, STAGE_MAX_VALUE, DISPLAY_HEIGHT);
          gfxLine(x + drawing_x_offset, BottomAlign(DISPLAY_HEIGHT),
                  x + drawing_x_offset, BottomAlign(ys), true);
        }
        x = draw_sustain(c, sustain, x, length);
      }

      if (has_release) {
        const int release_start = has_sustain ? sustain : STAGE_MAX_VALUE;
        draw_release(c, release_start, release, x, length);
      }
      drawing_x_offset = 0;
}

void FLASHMEM Piquet::draw_curve_segment(int x, int x2, int start_level, int end_level, uint8_t shape, bool draw, int y_offset) {
      x += drawing_x_offset;
      x2 += drawing_x_offset;
      auto y_for_level = [y_offset](int level) {
        return BottomAlign(
          Proportion(level, STAGE_MAX_VALUE, DISPLAY_HEIGHT)) + y_offset;
      };

      // Keep flat, unselected stages visibly dotted.
      if (draw && start_level == end_level && x2 > x) {
        const int y = y_for_level(end_level);
        for (int px = x + 1; px <= x2; px += 2) {
          gfxPixel(px, y);
        }
        return;
      }

      if (x2 <= x) {
        const int y1 = y_for_level(start_level);
        const int y2 = y_for_level(end_level);
        gfxLine(x, y1, x, y2, draw);
        return;
      }

      int previous_x = x;
      int previous_y = y_for_level(start_level);
      const int32_t delta = end_level - start_level;

      for (int px = x + 1; px <= x2; ++px) {
        const uint16_t phase = static_cast<uint16_t>(
          (static_cast<uint32_t>(px - x) * 1024u) / (x2 - x));

        const uint16_t shaped = apply_shape_u16(phase, shape);
        const int level = start_level +
          static_cast<int32_t>(delta * static_cast<int32_t>(shaped)) /
          SHAPE_LUT_MAX;

        const int y = y_for_level(level);
        gfxLine(previous_x, previous_y, px, y, draw);

        previous_x = px;
        previous_y = y;
      }

      // Ensure the segment lands exactly on its endpoint.
      const int end_y = y_for_level(end_level);
      gfxLine(previous_x, previous_y, x2, end_y, draw);
}

void FLASHMEM Piquet::draw_stage_highlight(int x, int x2, int start_level, int end_level, uint8_t shape) {
      draw_curve_segment(x, x2, start_level, end_level, shape, false, -1);
      draw_curve_segment(x, x2, start_level, end_level, shape, false, 0);
      draw_curve_segment(x, x2, start_level, end_level, shape, false, 1);
}

int FLASHMEM Piquet::draw_attack(const Channel &c, int attack, int x, int length) {
      const int x2 = x + Proportion(attack, length, 62);
      const bool selected = envelope_drawing_channel == selected_channel && cursor == CURSOR_ATTACK;
      draw_curve_segment(
        x, x2, 0, STAGE_MAX_VALUE, c.attack_shape, !selected);
      if (selected) {
        draw_stage_highlight(x, x2, 0, STAGE_MAX_VALUE, c.attack_shape);
      }
      return x2;
}

int FLASHMEM Piquet::draw_decay(const Channel &c, int decay, int sustain, int x, int length) {
      const int x2 = x + Proportion(decay, length, 62);
      const bool selected = envelope_drawing_channel == selected_channel && cursor == CURSOR_DECAY;
      draw_curve_segment(
        x, x2, STAGE_MAX_VALUE, sustain, c.decay_shape, !selected);
      if (selected) {
        draw_stage_highlight(x, x2, STAGE_MAX_VALUE, sustain, c.decay_shape);
      }
      return x2;
}

int FLASHMEM Piquet::draw_sustain(const Channel &c, int sustain, int x, int length) {
      const int x2 = x + Proportion(36, length, 62);
      const int ys = Proportion(sustain, STAGE_MAX_VALUE, DISPLAY_HEIGHT);
      const int y = BottomAlign(ys);
      const bool selected = envelope_drawing_channel == selected_channel && cursor == CURSOR_SUSTAIN;
      if (!selected) {
        gfxLine(x + drawing_x_offset, y, x2 + drawing_x_offset, y, true);
      } else {
        draw_stage_highlight(x, x2, sustain, sustain, 0);
      }
      return x2;
}

int FLASHMEM Piquet::draw_release(const Channel &c, int sustain, int release, int x, int length) {
      const int x2 = x + Proportion(release, length, 62);
      const bool selected = envelope_drawing_channel == selected_channel && cursor == CURSOR_RELEASE;
      draw_curve_segment(
        x, x2, sustain, 0, c.release_shape, !selected);
      if (selected) {
        draw_stage_highlight(x, x2, sustain, 0, c.release_shape);
      }
      return x2;
}

const char* FLASHMEM Piquet::cursor_display_label() const {
      switch (cursor) {
        case CURSOR_ATTACK_SHAPE: return "AttShape";
        case CURSOR_DECAY_SHAPE: return "DcyShape";
        case CURSOR_RELEASE_SHAPE: return "RelShape";
        case CURSOR_DELAY: return "TrgDelay";
        case CURSOR_DELAY_MODE: return "DlyMode";
        case CURSOR_CV_MAP: return "CV Map";
        case CURSOR_LEVEL: return "Level";
        case CURSOR_INVERT: return "Invert";
        default: return cursor_label();
      }
}

int FLASHMEM Piquet::numeric_text_width(int value) {
      int width = 6;
      while (value >= 10) {
        value /= 10;
        width += 6;
      }
      return width;
}

int FLASHMEM Piquet::cursor_value_width(const Channel &c) const {
      switch (cursor) {
        case CURSOR_LOOP_MODE:
          return 18;
        case CURSOR_ATTACK:
          return numeric_text_width(StageTicks(c.stage_value[STAGE_ATTACK]) / 17) + 12;
        case CURSOR_DECAY:
          return numeric_text_width(StageTicks(c.stage_value[STAGE_DECAY]) / 17) + 12;
        case CURSOR_SUSTAIN:
          return numeric_text_width(Proportion(c.stage_value[STAGE_SUSTAIN], STAGE_MAX_VALUE, 100)) + 6;
        case CURSOR_RELEASE:
          return numeric_text_width((StageTicks(c.stage_value[STAGE_RELEASE]) * 2) / 17) + 12;
        case CURSOR_ATTACK_SHAPE:
          return min(static_cast<int>(strlen(shape_name(c.attack_shape))) * 6, 30);
        case CURSOR_DECAY_SHAPE:
          return min(static_cast<int>(strlen(shape_name(c.decay_shape))) * 6, 30);
        case CURSOR_RELEASE_SHAPE:
          return min(static_cast<int>(strlen(shape_name(c.release_shape))) * 6, 30);
        case CURSOR_DELAY:
          return c.delay_index < 100 ? 30 : 24;
        case CURSOR_DELAY_MODE:
          return min(static_cast<int>(strlen(dmode_name(c.delay_mode))) * 6, 52);
        case CURSOR_CV_MAP:
          return min(static_cast<int>(strlen(cvmap_name(c.cv_map))) * 6, 52);
        case CURSOR_LEVEL:
          return 24;
        case CURSOR_INVERT:
          return 18;
        default:
          return 6;
      }
}

const char* FLASHMEM Piquet::cursor_label() const {
      static const char* labels[CURSOR_LAST] = {
        "Channel", "Length", "Envelope", "EnvType", "LoopMode",
        "Attack", "Decay", "Sustain", "Release",
        "AttShape", "DcyShape", "RelShape",
        "Delay Mode", "Delay", "CV Map", "Level", "Invert"
      };
      if (channels[selected_channel].type == ENV_AHR ||
          channels[selected_channel].type == ENV_LOOP_AHR) {
        if (cursor == CURSOR_DECAY) return "Hold";
      }
      return labels[constrain(cursor, 0, CURSOR_LAST - 1)];
}

const char* FLASHMEM Piquet::type_name(uint8_t t) {
      static const char* names[ENV_TYPE_LAST] = { "AD", "AHR", "ASR", "ADSR", "L-AD", "L-AHR" };
      return names[constrain(t, 0, ENV_TYPE_LAST - 1)];
}

const char* FLASHMEM Piquet::dmode_name(uint8_t m) {
      static const char* names[DELAY_MODE_LAST] = { "Off", "Rpl", "Que" };
      return names[constrain(m, 0, DELAY_MODE_LAST - 1)];
}

const char* FLASHMEM Piquet::cvmap_name(uint8_t m) {
      static const char* names[CVMAP_LAST] = { "None", "Att", "Dec", "Sus", "Rel", "ADR", "Lvl" };
      return names[constrain(m, 0, CVMAP_LAST - 1)];
}

const char* FLASHMEM Piquet::shape_name(uint8_t s) {
      static const char* names[SHAPE_LAST] = { "Lin", "Exp", "Qua", "Sin", "Pla", "Cli", "Gat", "BD", "MD", "LD", "SF" };
      return names[constrain(s, 0, SHAPE_LAST - 1)];
}

uint64_t FLASHMEM Piquet::OnDataRequest() {
  uint64_t data = channels[1].pack();
  SetData(0, data);

  data = PackPackables(
    channels[0].attack_shape, channels[0].decay_shape, channels[0].release_shape,
    channels[1].attack_shape, channels[1].decay_shape, channels[1].release_shape
  );
  SetData(1, data);

  return channels[0].pack();
}

void FLASHMEM Piquet::OnDataReceive(uint64_t data) {
  if (!data) {
    Start();
    return;
  }

  // reset to sane defaults first
  ForEachChannel(ch) channels[ch].Init(ch);

  channels[0].unpack(data);
  if (GetData(0, data)) { channels[1].unpack(data); }
  if (GetData(1, data)) {
    UnpackPackables(data,
      channels[0].attack_shape, channels[0].decay_shape, channels[0].release_shape,
      channels[1].attack_shape, channels[1].decay_shape, channels[1].release_shape
    );
    ForEachChannel(ch) {
      CONSTRAIN(channels[ch].attack_shape, 0, SHAPE_LAST - 1);
      CONSTRAIN(channels[ch].decay_shape, 0, SHAPE_LAST - 1);
      CONSTRAIN(channels[ch].release_shape, 0, SHAPE_LAST - 1);
    }
  }
}
