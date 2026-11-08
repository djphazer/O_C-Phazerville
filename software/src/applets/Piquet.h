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


#include "../src/extern/peaks_multistage_envelope.h"
#include "../src/extern/peaks_resources.h"

class Piquet : public HemisphereApplet {
public:
    static constexpr int NUM_CHANNELS = 2;
    static constexpr int NUM_STAGES = 4;
    static constexpr int STAGE_MAX_VALUE = 255;
    static constexpr int LENGTH_DEFAULT = 3;
    static constexpr int LENGTH_MAX = 6;
    static constexpr int DISPLAY_HEIGHT = 32;
    static constexpr int PARAM_X = 9;
    static constexpr int PARAM_Y = 20;
    static constexpr uint8_t MAX_TRIGGER_QUEUE = 4;


    enum EnvType : uint8_t {
      ENV_AD,
      ENV_ADR,
      ENV_AR,
      ENV_ADSR,
      ENV_LOOP_AD,
      ENV_LOOP_ADR,
      ENV_TYPE_COUNT
    };

    enum LoopMode : uint8_t {
      LOOP_CONT,
      LOOP_GATE,
      LOOP_MODE_COUNT
    };

    // Stage indices are also used to index stage_value[].
    enum Stage : uint8_t {
      STAGE_ATTACK,
      STAGE_DECAY,
      STAGE_SUSTAIN,
      STAGE_RELEASE
    };

    enum DelayMode : uint8_t {
      DELAY_OFF,
      DELAY_REPLACE,
      DELAY_QUEUE,
      DELAY_MODE_COUNT
    };

    enum CVMap : uint8_t {
      CVMAP_NONE,
      CVMAP_ATTACK,
      CVMAP_DECAY,
      CVMAP_SUSTAIN,
      CVMAP_RELEASE,
      CVMAP_ADR,
      CVMAP_LEVEL,
      CVMAP_COUNT
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
      SHAPE_COUNT
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
      CURSOR_EOC,
      CURSOR_CV_MAP,
      CURSOR_LEVEL,
      CURSOR_INVERT,
      CURSOR_COUNT
    };

    struct TriggerQueue {
      int32_t ticks[MAX_TRIGGER_QUEUE];
      uint8_t count;

      void Reset() {
        count = 0;
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
      bool eoc_enabled;
      bool eoc_trigger_pending;
      uint8_t length_index;
      peaks::MultistageEnvelope peaks_envelope;
      bool peaks_trigger_pending;
      bool peaks_waiting_for_trigger;
      uint8_t peaks_configured_type;
      uint32_t peaks_config_signature;
      uint8_t peaks_cached_attack;
      uint8_t peaks_cached_decay;
      uint8_t peaks_cached_release;
      uint8_t peaks_cached_sustain;
      uint16_t peaks_cached_attack_time;
      uint16_t peaks_cached_decay_time;
      uint16_t peaks_cached_release_time;
      bool peaks_times_cached;

      bool gate_prev;
      int output;
      TriggerQueue trigger_queue;

      void Init(uint8_t ch) {
        type = (ch == 0) ? ENV_ADSR : ENV_AR;
        stage_value[STAGE_ATTACK] = 28 + ch * 8;
        stage_value[STAGE_DECAY] = 52;
        stage_value[STAGE_SUSTAIN] = 160;
        stage_value[STAGE_RELEASE] = 44 + ch * 8;
        delay_index = 1;
        delay_mode = DELAY_OFF;
        cv_map = CVMAP_NONE;
        level = 255;
        attack_shape = SHAPE_QUARTIC;
        decay_shape = SHAPE_EXPONENTIAL;
        release_shape = SHAPE_EXPONENTIAL;
        loop_mode = LOOP_CONT;
        loop_enabled = false;
        inverted = false;
        eoc_enabled = false;
        eoc_trigger_pending = false;
        length_index = LENGTH_DEFAULT;
        peaks_envelope.Init();
        peaks_trigger_pending = false;
        peaks_waiting_for_trigger = false;
        peaks_configured_type = 0xFF;
        peaks_config_signature = 0xFFFFFFFFu;
        peaks_times_cached = false;
        peaks_cached_sustain = stage_value[STAGE_SUSTAIN];

        gate_prev = false;
        output = 0;
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
        Pack(data, PackLocation {59,2}, length_index & 3);
        Pack(data, PackLocation {61,1}, eoc_enabled ? 1 : 0);
        Pack(data, PackLocation {62,1}, length_index >> 2);
        Pack(data, PackLocation {63,1}, 1); // New Length format
        return data;
      }
      void unpack(uint64_t data) {
        type = constrain(Unpack(data, PackLocation {0,3}), 0, ENV_TYPE_COUNT - 1);
        stage_value[STAGE_ATTACK] = constrain(Unpack(data, PackLocation {3,8}), 1, STAGE_MAX_VALUE);
        stage_value[STAGE_DECAY] = constrain(Unpack(data, PackLocation {11,8}), 1, STAGE_MAX_VALUE);
        stage_value[STAGE_SUSTAIN] = constrain(Unpack(data, PackLocation {19,8}), 0, STAGE_MAX_VALUE);
        stage_value[STAGE_RELEASE] = constrain(Unpack(data, PackLocation {27,8}), 1, STAGE_MAX_VALUE);
        delay_index = constrain(Unpack(data, PackLocation {35,7}), 1, 127);
        delay_mode = constrain(Unpack(data, PackLocation {43,2}), 0, DELAY_MODE_COUNT - 1);
        cv_map = constrain(Unpack(data, PackLocation {45,3}), 0, CVMAP_COUNT - 1);
        level = constrain(Unpack(data, PackLocation {48,8}), 0, 255);
        inverted = Unpack(data, PackLocation {56,1});
        loop_mode = Unpack(data, PackLocation {57,1});
        loop_enabled = loop_mode == LOOP_CONT &&
            Unpack(data, PackLocation {58,1}) != 0;
        const int old_length = Unpack(data, PackLocation {59,2});
        if (Unpack(data, PackLocation {63,1})) {
          const int high_bit = Unpack(data, PackLocation {62,1});
          length_index = constrain(old_length | (high_bit << 2), 0, LENGTH_MAX);
        } else {
          // Old x1/x2/x4/x8 become /4,/2,x1,x2.
          length_index = old_length + 1;
        }
        eoc_enabled = Unpack(data, PackLocation {61,1});
      }

      bool is_looping() const {
        return type == ENV_LOOP_AD || type == ENV_LOOP_ADR;
      }

      bool should_repeat() const {
        return is_looping() && loop_enabled;
      }

      bool has_sustain_hold() const {
        return type == ENV_ADSR;
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
      const EnvType new_type = static_cast<EnvType>(channels[selected_channel].type);

      if (cursor == CURSOR_LOOP_MODE &&
          !cursor_visible_for_type(cursor, new_type)) {
        cursor = CURSOR_ATTACK;
        parameter_navigation = false;
      }

      // Preserve the same parameter when available.
      // Otherwise fall back to the corresponding stage position.
      if (stage_index >= 0 && cursor_stage_index(cursor, new_type) < 0) {
        cursor = cursor_for_stage_index(stage_index, new_type, shape_cursor);
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
    int cursor = CURSOR_CHANNEL;
    bool parameter_navigation = false;
    Channel channels[NUM_CHANNELS];

    struct StageInfo {
      uint8_t cursor;
      uint8_t shape_cursor;
      uint8_t segment_start;
      bool has_shape;
    };

    static const StageInfo* stage_map(EnvType type) {
      static const StageInfo map[3][STAGE_RELEASE + 1] = {
        { // AD / L-AD
          {CURSOR_ATTACK, CURSOR_ATTACK_SHAPE, 0, true},
          {CURSOR_DECAY, CURSOR_DECAY_SHAPE, 1, true},
          {CURSOR_COUNT, CURSOR_COUNT, 0, false},
          {CURSOR_COUNT, CURSOR_COUNT, 0, false}
        },
        { // ADR / ADSR / L-ADR
          {CURSOR_ATTACK, CURSOR_ATTACK_SHAPE, 0, true},
          {CURSOR_DECAY, CURSOR_DECAY_SHAPE, 1, true},
          {CURSOR_SUSTAIN, CURSOR_COUNT, 2, false},
          {CURSOR_RELEASE, CURSOR_RELEASE_SHAPE, 3, true}
        },
        { // AR
          {CURSOR_ATTACK, CURSOR_ATTACK_SHAPE, 0, true},
          {CURSOR_RELEASE, CURSOR_RELEASE_SHAPE, 2, true},
          {CURSOR_COUNT, CURSOR_COUNT, 0, false},
          {CURSOR_COUNT, CURSOR_COUNT, 0, false}
        }
      };

      switch (type) {
        case ENV_AD:
        case ENV_LOOP_AD:
          return map[0];
        case ENV_AR:
          return map[2];
        default:
          return map[1];
      }
    }

    bool cursor_visible_for_type(int item, EnvType type) const {
      if (item == CURSOR_LOOP_MODE)
        return type == ENV_LOOP_AD || type == ENV_LOOP_ADR;

      const StageInfo* stages = stage_map(type);
      for (int i = 0; i <= STAGE_RELEASE; ++i) {
        if (stages[i].cursor == CURSOR_COUNT) break;
        if (item == stages[i].cursor) return true;
        if (stages[i].shape_cursor != CURSOR_COUNT &&
            item == stages[i].shape_cursor)
          return stages[i].has_shape;
      }

      // Unmatched stages and shapes are unavailable.
      return !((item >= CURSOR_ATTACK && item <= CURSOR_RELEASE) ||
               (item >= CURSOR_ATTACK_SHAPE && item <= CURSOR_RELEASE_SHAPE));
    }

    int stage_count(EnvType type) const {
      const StageInfo* stages = stage_map(type);
      int count = 0;
      while (count <= STAGE_RELEASE &&
             stages[count].cursor != CURSOR_COUNT)
        ++count;
      return count;
    }

    static int stage_value_index(int item) {
      switch (item) {
        case CURSOR_ATTACK:  return STAGE_ATTACK;
        case CURSOR_DECAY:   return STAGE_DECAY;
        case CURSOR_SUSTAIN: return STAGE_SUSTAIN;
        case CURSOR_RELEASE: return STAGE_RELEASE;
        default:             return -1;
      }
    }

    int stage_display_value(const Channel &c, int stage) const {
      int value = c.stage_value[stage];

      if (stage == STAGE_SUSTAIN) {
        if (c.peaks_times_cached)
          value = c.peaks_cached_sustain;
        return Proportion(value, STAGE_MAX_VALUE, 100);
      }

      // Display the effective CV-modulated stage duration.
      if (c.peaks_times_cached) {
        if (stage == STAGE_ATTACK)
          value = c.peaks_cached_attack;
        else if (stage == STAGE_DECAY)
          value = c.peaks_cached_decay;
        else if (stage == STAGE_RELEASE)
          value = c.peaks_cached_release;
      }

      const int multiplier = 1 << c.length_index;

      return StageMilliseconds(value, multiplier);
    }

    int cursor_stage_index(int item, EnvType type) const {
      const StageInfo* stages = stage_map(type);
      for (int i = 0; i <= STAGE_RELEASE; ++i) {
        if (stages[i].cursor == CURSOR_COUNT) break;
        if (item == stages[i].cursor ||
            (stages[i].shape_cursor != CURSOR_COUNT &&
             item == stages[i].shape_cursor))
          return i;
      }
      return -1;
    }

    int cursor_for_stage_index(int index, EnvType type, bool shape) const {
      index = constrain(index, 0, stage_count(type) - 1);
      const StageInfo& stage = stage_map(type)[index];
      return shape && stage.has_shape
          ? stage.shape_cursor : stage.cursor;
    }

    int next_visible_cursor(int current, int direction, EnvType type) {
      if (current == CURSOR_CHANNEL) {
        parameter_navigation = false;
        return direction > 0 ? CURSOR_TYPE : CURSOR_CHANNEL;
      }
      if (current == CURSOR_TYPE) {
        parameter_navigation = false;
        if (direction < 0) return CURSOR_CHANNEL;
        if (type == ENV_LOOP_AD || type == ENV_LOOP_ADR)
          return CURSOR_LOOP_MODE;
        return CURSOR_LENGTH;
      }
      if (current == CURSOR_LENGTH) {
        parameter_navigation = false;
        if (direction < 0) {
          if (type == ENV_LOOP_AD || type == ENV_LOOP_ADR)
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
        if (current == CURSOR_DELAY_MODE && direction < 0) {
          parameter_navigation = false;
          return cursor_for_stage_index(stage_count(type) - 1, type, true);
        }

        int next = current;
        while (true) {
          next += direction;
          if (next < CURSOR_DELAY_MODE || next >= CURSOR_COUNT)
            return current;
          if (cursor_visible_for_type(next, type))
            return next;
        }
      }

      const int stage = cursor_stage_index(current, type);
      if (stage < 0) {
        parameter_navigation = true;
        return current;
      }

      const bool is_shape =
          current == CURSOR_ATTACK_SHAPE ||
          current == CURSOR_DECAY_SHAPE ||
          current == CURSOR_RELEASE_SHAPE;

      if (direction > 0) {
        if (!is_shape) {
          const int shape_cursor = cursor_for_stage_index(stage, type, true);
          if (shape_cursor != current &&
              cursor_visible_for_type(shape_cursor, type))
            return shape_cursor;
        }

        if (stage + 1 < stage_count(type))
          return cursor_for_stage_index(stage + 1, type, false);

        parameter_navigation = true;
        return CURSOR_DELAY_MODE;
      }

      if (is_shape)
        return cursor_for_stage_index(stage, type, false);

      if (stage == 0) {
        parameter_navigation = false;
        return CURSOR_LENGTH;
      }

      const int previous_shape =
          cursor_for_stage_index(stage - 1, type, true);
      if (previous_shape != cursor_for_stage_index(stage - 1, type, false) &&
          cursor_visible_for_type(previous_shape, type))
        return previous_shape;

      return cursor_for_stage_index(stage - 1, type, false);
    }
    static int StageTicks(uint8_t value) {
      return HS::kShapeExponential[value] >> 2;
    }

    static int StageMilliseconds(uint8_t value, int multiplier) {
      return (StageTicks(value) * multiplier) / 17;
    }

    static constexpr uint16_t PeaksTime(int ticks) {
      if (ticks < 1) ticks = 1;

      const uint32_t target =
          UINT32_MAX / static_cast<uint32_t>(ticks);

      // Search the descending Peaks timing LUT.
      if (target >= peaks::lut_env_increments[0]) return 0;
      if (target <= peaks::lut_env_increments[255])
        return static_cast<uint16_t>(255 << 8);

      uint16_t low = 0;
      uint16_t high = 255;
      while (high - low > 1) {
        const uint16_t mid = (low + high) / 2;
        if (peaks::lut_env_increments[mid] > target)
          low = mid;
        else
          high = mid;
      }

      const uint32_t upper_difference =
          peaks::lut_env_increments[low] - target;
      const uint32_t lower_difference =
          target - peaks::lut_env_increments[high];

      // On a tie, keep the earlier entry, as before.
      const uint16_t best_index =
          upper_difference <= lower_difference ? low : high;
      return static_cast<uint16_t>(best_index << 8);
    }

    void begin_attack(Channel &c) {
      c.peaks_trigger_pending = true;
    }

    void process_envelope(Channel &c, bool gate, bool gate_fall, int ch) {
      peaks::MultistageEnvelope &env = c.peaks_envelope;

      int attack = c.stage_value[STAGE_ATTACK];
      int decay = c.stage_value[STAGE_DECAY];
      int sustain = c.stage_value[STAGE_SUSTAIN];
      int release = c.stage_value[STAGE_RELEASE];

      switch (c.cv_map) {
        case CVMAP_ATTACK:
          Modulate(attack, ch, 1, STAGE_MAX_VALUE);
          break;
        case CVMAP_DECAY:
          Modulate(decay, ch, 1, STAGE_MAX_VALUE);
          break;
        case CVMAP_SUSTAIN:
          Modulate(sustain, ch, 0, STAGE_MAX_VALUE);
          break;

        case CVMAP_ADR:
          Modulate(attack, ch, 1, STAGE_MAX_VALUE);
          Modulate(decay, ch, 1, STAGE_MAX_VALUE);
          // fall thru
        case CVMAP_RELEASE:
          Modulate(release, ch, 1, STAGE_MAX_VALUE);
          break;
        default: break;
      }

      // Convert Piquet's stage durations to Peaks' time LUT indices.
      // Cache the result until a parameter or its CV-modulated value changes.
      if (!c.peaks_times_cached || c.peaks_cached_attack != attack) {
        c.peaks_cached_attack = attack;
        c.peaks_cached_attack_time = PeaksTime(StageTicks(attack));
      }

      if (!c.peaks_times_cached || c.peaks_cached_decay != decay) {
        c.peaks_cached_decay = decay;
        c.peaks_cached_decay_time = PeaksTime(StageTicks(decay));
      }

      if (!c.peaks_times_cached || c.peaks_cached_release != release) {
        c.peaks_cached_release = release;
        c.peaks_cached_release_time = PeaksTime(StageTicks(release));
      }

      c.peaks_times_cached = true;

      const uint16_t attack_time = c.peaks_cached_attack_time;
      const uint16_t decay_time = c.peaks_cached_decay_time;
      const uint16_t release_time = c.peaks_cached_release_time;
      c.peaks_cached_sustain = static_cast<uint8_t>(sustain);
      const uint16_t sustain_level =
          Proportion(sustain, STAGE_MAX_VALUE, 32767);

      const uint32_t signature =
          static_cast<uint32_t>(c.type) |
          (static_cast<uint32_t>(c.attack_shape) << 4) |
          (static_cast<uint32_t>(c.decay_shape) << 8) |
          (static_cast<uint32_t>(c.release_shape) << 12) |
          (static_cast<uint32_t>(c.length_index) << 17);

      if (c.peaks_configured_type != c.type) {
        env.Init();
        c.peaks_configured_type = c.type;
        c.peaks_config_signature = 0xFFFFFFFFu;
        c.output = 0;
      }

      // Rebuild segment layout only when the mode, shapes, or loop layout changes.
      if (c.peaks_config_signature != signature) {
        env.set_attack_shape(static_cast<peaks::EnvelopeShape>(c.attack_shape));
        env.set_decay_shape(static_cast<peaks::EnvelopeShape>(c.decay_shape));
        env.set_release_shape(static_cast<peaks::EnvelopeShape>(c.release_shape));

        // Initialize Peaks' terminal/inactive slots as well as active segments.
        // ProcessSingleSample() reads the slot after the final active segment.
        // Let Peaks apply Length multipliers when it configures each segment.
        env.set_attack_time_multiplier(c.length_index);
        env.set_decay_time_multiplier(c.length_index);
        env.set_release_time_multiplier(c.length_index);

        switch (c.type) {
          case ENV_AD:
          case ENV_LOOP_AD:
            env.set_ad(attack_time, decay_time, 0,
                c.type == ENV_LOOP_AD ? 2 : 0);
            break;
          case ENV_ADR:
          case ENV_LOOP_ADR:
            env.set_adr(attack_time, decay_time, sustain_level, release_time,
                0, c.type == ENV_LOOP_ADR ? 3 : 0);
            break;
          case ENV_AR:
            env.set_ar(attack_time, release_time);
            break;
          case ENV_ADSR:
          default:
            env.set_adsr(attack_time, decay_time, sustain_level, release_time);
            break;
        }
        // Keep Peaks' current segment valid after changing the layout.
        env.reset();
        c.peaks_config_signature = signature;
      }

      // Update live timing and levels without rebuilding the segment layout.
      env.set_time(0, attack_time);

      switch (c.type) {
        case ENV_AR:
          env.set_time(1, release_time);
          break;

        case ENV_AD:
        case ENV_LOOP_AD:
          env.set_time(1, decay_time);
          break;

        case ENV_ADR:
        case ENV_LOOP_ADR:
        case ENV_ADSR:
        default:
          env.set_level(2, sustain_level);
          env.set_time(2, release_time);
          env.set_time(1, decay_time);
          break;
      }


      // Stay inactive after a type change until a real trigger arrives.
      if (c.peaks_waiting_for_trigger && !c.peaks_trigger_pending) {
        c.output = 0;
        return;
      }

      uint8_t control = 0;
      if (c.peaks_trigger_pending) {
        control |= peaks::CONTROL_GATE_RISING;
        c.peaks_trigger_pending = false;
        c.peaks_waiting_for_trigger = false;
      }
      if (gate_fall && (c.type == ENV_AR || c.has_sustain_hold()))
        control |= peaks::CONTROL_GATE_FALLING;
      if ((c.type == ENV_AR || c.type == ENV_ADSR) && gate)
        control |= peaks::CONTROL_GATE;
      if (c.should_repeat())
        control |= peaks::CONTROL_GATE;

      const uint16_t value = env.ProcessSingleSample(control);
      const int amplitude_cv = Proportion(value, 32767, HEMISPHERE_MAX_CV);
      c.output = constrain(amplitude_cv, 0, HEMISPHERE_MAX_CV);


    }

    static const char* type_name(uint8_t t);
    static const char* dmode_name(uint8_t m);
    static const char* cvmap_name(uint8_t m);
    static const char* shape_name(uint8_t s);

    void DrawIndicators();
    void DrawParamRow(int x_offset = 0);
    const char* cursor_display_label() const;
    static int numeric_text_width(int value);
    static int text_width(const char* value, int max_width);
    int cursor_value_width(const Channel &c) const;
    void DrawEnvelope(int channel, int x_offset, bool absolute_coordinates);
    const char* cursor_label() const;

};

void FLASHMEM Piquet::Controller() {
      // Capture pending EOC triggers before processing either channel.
      bool pending_eoc[NUM_CHANNELS];
      ForEachChannel(ch) {
        pending_eoc[ch] = channels[ch].eoc_trigger_pending;
        channels[ch].eoc_trigger_pending = false;
      }

      ForEachChannel(ch) {
        Channel &c = channels[ch];

        const uint8_t previous_type = c.peaks_configured_type;
        const bool type_changed = previous_type != c.type;
        const bool previous_was_looping =
            previous_type == ENV_LOOP_AD || previous_type == ENV_LOOP_ADR;

        // Discard stale triggers when the envelope type changes.
        // A clock arriving in this same cycle can still trigger normally.
        if (type_changed) {
          c.peaks_trigger_pending = false;
          c.trigger_queue.Reset();
          c.peaks_waiting_for_trigger = true;
        }

        const bool gate = Gate(ch);
        const bool clock_trigger = Clock(ch);
        const bool eoc_trigger = pending_eoc[ch];
        const bool trigger = clock_trigger || eoc_trigger;
        const bool gate_fall = !gate && c.gate_prev;
        c.gate_prev = gate;

        if (c.is_looping() && c.loop_mode != LOOP_CONT) {
          c.loop_enabled = gate;
        }

        // Resume active loops when changing between L-AD and L-ADR,
        // or when configuring a recalled preset.
        // GTD follows the live gate; CON follows its saved state.
        if (type_changed && c.should_repeat() &&
            (previous_was_looping || previous_type == 0xFF)) {
          begin_attack(c);
        }

        int trigger_count = 0;

        if (trigger) {
          const int32_t delay_ms = c.delay_index < 100
              ? static_cast<int32_t>(c.delay_index) * 10
              : 1000 + static_cast<int32_t>(c.delay_index - 100) * 100;
          const int32_t dticks = (delay_ms * 1000L) / 60;

          if (c.delay_mode == DELAY_OFF || dticks <= 0) {
            c.trigger_queue.Reset();
            ++trigger_count;
          } else {
            c.trigger_queue.Add(dticks, c.delay_mode == DELAY_QUEUE);
          }
        }

        if (c.delay_mode == DELAY_OFF) c.trigger_queue.Reset();
        else if (c.trigger_queue.TickReady()) ++trigger_count;

        while (trigger_count-- > 0) {
          if (c.is_looping() && c.loop_mode == LOOP_CONT) {
            if (c.peaks_waiting_for_trigger && c.loop_enabled) {
              begin_attack(c);
            } else {
              c.loop_enabled = !c.loop_enabled;
              if (c.loop_enabled) begin_attack(c);
            }
          } else {
            begin_attack(c);
          }
        }

        process_envelope(c, gate, gate_fall, ch);
        const bool eoc_event =
            !c.peaks_waiting_for_trigger &&
            (c.peaks_envelope.get_state_mask() & peaks::ENV_EOC) != 0;
        if (eoc_event && c.eoc_enabled)
          channels[1 - ch].eoc_trigger_pending = true;

        int out = c.output;
        int level = c.level;
        if (c.cv_map == CVMAP_LEVEL)
          Modulate(level, ch, 0, 255);
        out = Proportion(out, HEMISPHERE_MAX_CV, Proportion(level, 255, HEMISPHERE_MAX_CV));
        if (c.inverted) out = HEMISPHERE_MAX_CV - out;

        Out(ch, out);
      }
}


void FLASHMEM Piquet::View() {
      if (EditMode() || cursor < CURSOR_ATTACK_SHAPE ||
          cursor == CURSOR_ATTACK_SHAPE ||
          cursor == CURSOR_DECAY_SHAPE ||
          cursor == CURSOR_RELEASE_SHAPE) SetLabel(cursor_label());
      else SetLabel("");
      DrawIndicators();
      DrawEnvelope(selected_channel, 0, false);
      if (cursor == CURSOR_TYPE || cursor == CURSOR_LOOP_MODE)
        gfxFrame(0, PARAM_Y + 9, 64, 35);
      DrawParamRow();
      SetAux(true);
}

// Draw both envelopes side by side, with the selected channel's controls
// and parameter row kept on that channel's side of the fullscreen display.
void FLASHMEM Piquet::DrawFullScreen() {
  // Draw fullscreen waveforms in absolute screen coordinates.
  for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
    DrawEnvelope(ch, ch * 64, true);
  }

  const int x_offset = -gfx_offset;

  for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
    const int channel_x = ch * 64 + x_offset;
    const int w = constrain(
      Proportion(ViewOut(ch), HEMISPHERE_MAX_CV, 62), 0, 62);
    const int meter_x = channel_x + (ch == 0 ? 0 : 64 - w);

    // Output meter and channel label.
    gfxRect(meter_x, 14, w, 2);
    gfxPrint(channel_x, PARAM_Y, OutputLabel(ch));

    if (ch == selected_channel &&
        cursor == CURSOR_CHANNEL && EditMode()) {
      gfxFrame(channel_x, PARAM_Y - 1, 8, 11);
    } else {
      gfxInvert(channel_x, PARAM_Y - 1, 7, 9);
    }
  }

  const int channel_x = selected_channel * 64 + x_offset;

  if (cursor == CURSOR_CHANNEL) {
    gfxIcon(channel_x + PARAM_X, PARAM_Y, LEFT_ICON);
  } else if (cursor == CURSOR_LENGTH) {
    gfxIcon(channel_x + PARAM_X + 21, PARAM_Y, RIGHT_ICON);
  }

  if (cursor == CURSOR_TYPE || cursor == CURSOR_LOOP_MODE) {
    gfxFrame(channel_x, PARAM_Y + 9, 64, 35);
  }

  DrawParamRow(channel_x);
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
          c.length_index = constrain(c.length_index + direction, 0, LENGTH_MAX);
          break;
        case CURSOR_TYPE:
          c.type = constrain(c.type + direction, 0, ENV_TYPE_COUNT - 1);
          if (!c.is_looping()) c.loop_enabled = false;
          break;
        case CURSOR_LOOP_MODE:
          c.loop_mode = constrain(
            c.loop_mode + direction, 0, LOOP_MODE_COUNT - 1);
          break;
        case CURSOR_ATTACK:
        case CURSOR_DECAY:
        case CURSOR_SUSTAIN:
        case CURSOR_RELEASE: {
          const int stage = stage_value_index(cursor);
          const int minimum = cursor == CURSOR_SUSTAIN ? 0 : 1;
          c.stage_value[stage] = constrain(
              c.stage_value[stage] + direction,
              minimum, STAGE_MAX_VALUE);
          break;
        }
        case CURSOR_ATTACK_SHAPE:
        case CURSOR_DECAY_SHAPE:
        case CURSOR_RELEASE_SHAPE: {
          uint8_t &shape = cursor == CURSOR_ATTACK_SHAPE ? c.attack_shape
                         : cursor == CURSOR_DECAY_SHAPE ? c.decay_shape
                                                        : c.release_shape;

          shape = constrain(shape + direction, 0, SHAPE_COUNT - 1);
          break;
        }
        case CURSOR_DELAY:
          c.delay_index = constrain(c.delay_index + direction, 1, 127);
          break;
        case CURSOR_DELAY_MODE:
          c.delay_mode = constrain(c.delay_mode + direction, 0, DELAY_MODE_COUNT - 1);
          break;
        case CURSOR_EOC:
          if (direction) c.eoc_enabled = !c.eoc_enabled;
          break;
        case CURSOR_CV_MAP:
          c.cv_map = constrain(c.cv_map + direction, 0, CVMAP_COUNT - 1);
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
        gfxRect(0, 13 + (ch * 3), w, 2);
      }

      gfxPrint(0, PARAM_Y, OutputLabel(selected_channel));

      if (cursor == CURSOR_CHANNEL && EditMode()) {
        gfxFrame(0, PARAM_Y - 1, 8, 11);
      } else {
        gfxInvert(0, PARAM_Y - 1, 7, 9);

        if (cursor == CURSOR_CHANNEL) {
          gfxIcon(PARAM_X, PARAM_Y, LEFT_ICON);
        } else if (cursor == CURSOR_LENGTH) {
          gfxIcon(PARAM_X + 21, PARAM_Y, RIGHT_ICON);
        }
      }
}

// Apply the offset to parameter text, icons, and the edit cursor.
void FLASHMEM Piquet::DrawParamRow(int x_offset) {
      Channel &c = channels[selected_channel];
      const int param_x = x_offset + PARAM_X;

      if (cursor == CURSOR_CHANNEL ||
          cursor == CURSOR_TYPE || cursor == CURSOR_LENGTH ||
          cursor == CURSOR_LOOP_MODE) {
        if (cursor == CURSOR_TYPE) {
          const char* value = type_name(c.type);
          const int value_x = param_x;
          gfxPrint(value_x, PARAM_Y, value);
          if (EditMode()) gfxInvert(value_x, PARAM_Y - 1, strlen(value) * 6, 9);
        } else if (cursor == CURSOR_LOOP_MODE) {
          const char* value = c.loop_mode == LOOP_GATE ? "GTD" : "CON";
          gfxIcon(param_x, PARAM_Y, LOOP_ICON);
          gfxPrint(param_x + 10, PARAM_Y, value);
          if (EditMode()) gfxInvert(param_x + 10, PARAM_Y - 1, strlen(value) * 6, 9);
        }

        static const char* length_names[] = {"/8", "/4", "/2", "x1", "x2", "x4", "x8"};
        gfxIcon(param_x + 32, PARAM_Y, ENV_ICON);
        gfxPrint(param_x + 41, PARAM_Y, length_names[constrain(c.length_index, 0, LENGTH_MAX)]);
        if (cursor == CURSOR_LENGTH && EditMode())
          gfxCursor(param_x + 41, PARAM_Y + 9, 12, 9, cursor_label());
        return;
      }

      if (cursor >= CURSOR_ATTACK_SHAPE) {
        const char *label = "";
        const char *value = nullptr;
        int value_x;
        int value_width;

        switch (cursor) {
          case CURSOR_ATTACK_SHAPE:
            label = "A:";
            value = shape_name(c.attack_shape);
            break;
          case CURSOR_DECAY_SHAPE:
            label = "D:";
            value = shape_name(c.decay_shape);
            break;
          case CURSOR_RELEASE_SHAPE:
            label = "R:";
            value = shape_name(c.release_shape);
            break;
          case CURSOR_DELAY:
            break;
          case CURSOR_DELAY_MODE:
            label = "DlyM:";
            value = dmode_name(c.delay_mode);
            break;
          case CURSOR_EOC:
            label = "EOC";
            value = c.eoc_enabled ? "Yes" : "No";
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

        if (cursor == CURSOR_DELAY) {
          gfxIcon(param_x, PARAM_Y, CLOCK_ICON);
          gfxPrint(param_x + 9, PARAM_Y, ":");
          value_x = param_x + 15;
        } else if (cursor == CURSOR_EOC) {
          gfxPrint(param_x, PARAM_Y, label);
          gfxIcon(param_x + 19, PARAM_Y, RIGHT_ICON);
          gfxPrint(param_x + 26, PARAM_Y, ":");
          value_x = param_x + 32;
        } else {
          gfxPrint(param_x, PARAM_Y, label);
          value_x = param_x + static_cast<int>(strlen(label)) * 6;
        }

        if (cursor == CURSOR_DELAY) {
          if (c.delay_index < 100) {
            gfxPrint(value_x, PARAM_Y, c.delay_index * 10);
            gfxPrint("ms");
          } else {
            const uint8_t tenths = c.delay_index - 90;
            gfxPrint(value_x, PARAM_Y, tenths / 10);
            gfxPrint(".");
            gfxPrint(tenths % 10);
            gfxPrint("s");
          }
        } else if (cursor == CURSOR_LEVEL) {
          int level = c.level;
          if (c.cv_map == CVMAP_LEVEL)
            Modulate(level, selected_channel, 0, 255);
          gfxPrint(value_x, PARAM_Y, Proportion(level, 255, 100));
          gfxPrint("%");
        } else if (value) {
          gfxPrint(value_x, PARAM_Y, value);
        }

        if (cursor == CURSOR_ATTACK_SHAPE ||
            cursor == CURSOR_DECAY_SHAPE ||
            cursor == CURSOR_RELEASE_SHAPE) {
          value_width = text_width(value, 64 - (value_x - x_offset));
        } else if (cursor == CURSOR_DELAY) {
          value_width = cursor_value_width(c);
        } else {
          value_width = min(cursor_value_width(c), 64 - (value_x - x_offset));
        }

        if (EditMode()) {
          gfxCursor(value_x, PARAM_Y + 9, value_width, 9, cursor_display_label());
        }
        return;
      }

      const int stage = stage_value_index(cursor);
      if (stage >= 0) {
        static const char* labels[] = {"A:", "D:", "S:", "R:"};

        gfxPrint(param_x, PARAM_Y, labels[stage]);
        const int duration = stage_display_value(c, stage);
        gfxPrint(param_x + 12, PARAM_Y, duration < 1000 || stage == STAGE_SUSTAIN
            ? duration : duration / 1000);
        if (stage == STAGE_SUSTAIN) {
          gfxPrint("%");
        } else if (duration < 1000) {
          gfxPrint("ms");
        } else {
          gfxPrint(".");
          gfxPrint((duration % 1000) / 100);
          gfxPrint("s");
        }
      }

      if (cursor >= CURSOR_ATTACK && cursor != CURSOR_LOOP_MODE && EditMode()) {
        gfxCursor(param_x + 12, PARAM_Y + 9, cursor_value_width(c), 9, cursor_label());
      }
}

void FLASHMEM Piquet::DrawEnvelope(int channel, int x_offset, bool absolute_coordinates) {
      Channel &c = channels[channel];

      int16_t values[peaks::kPreviewWidth];
      uint16_t segment_starts[16];
      uint16_t loop_points[8];
      uint16_t current_phase = 0;

      uint16_t width = c.peaks_envelope.RenderPreview(
          values, segment_starts, loop_points, current_phase);

      if (width == 0) return;
      if (width > peaks::kPreviewWidth) width = peaks::kPreviewWidth;

      const auto draw_pixel = [&](int x, int y) {
        if (x < x_offset || x >= x_offset + 64 || y < 0 || y >= 64)
          return;
        if (absolute_coordinates) graphics.setPixel(x, y);
        else gfxPixel(x, y);
      };

      const auto clear_pixel = [&](int x, int y) {
        if (x >= x_offset && x < x_offset + 64 &&
            y >= 0 && y < 64) {
          if (absolute_coordinates)
            graphics.clearRect(x, y, 1, 1);
          else
            gfxClear(x, y, 1, 1);
        }
      };

      const auto preview_x = [&](uint16_t point) {
        return x_offset + (width > 1
            ? Proportion(point, width - 1, 63) : 0);
      };


      // Rasterize the envelope contour once. Use the same pixels
      // for both the contour and its infill.
      const auto raster_line =
          [&](int x0, int y0, int x1, int y1, bool fill,
              int *lower_y = nullptr, bool collect_only = false,
              int *upper_y = nullptr) {
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        const bool steep = dy > dx;

        if (steep) {
          std::swap(x0, y0);
          std::swap(x1, y1);
          std::swap(dx, dy);
        }

        if (x0 > x1) {
          std::swap(x0, x1);
          std::swap(y0, y1);
        }

        int error = dx >> 1;
        const int ystep = (y1 > y0) ? 1 : -1;
        int y = y0;
        const int baseline = BottomAlign(0) + 1;

        for (int x = x0; x <= x1; ++x) {
          const int px = steep ? y : x;
          const int py = steep ? x : y;

          if ((lower_y || upper_y) &&
              px >= x_offset && px < x_offset + 64 &&
              py >= 0 && py < 64) {
            const int column = px - x_offset;
            if (lower_y && py > lower_y[column])
              lower_y[column] = py;
            if (upper_y && py < upper_y[column])
              upper_y[column] = py;
          }

          if (!collect_only && fill) {
            const int first_y = std::min(py, baseline);
            const int last_y = std::max(py, baseline);
            if (absolute_coordinates)
              graphics.drawVLine(px, first_y, last_y - first_y + 1);
            else
              gfxRect(px, first_y, 1, last_y - first_y + 1);
          } else if (!collect_only) {
            draw_pixel(px, py);
          }

          error -= dy;
          if (error < 0) {
            y += ystep;
            error += dx;
          }
        }
      };

      int previous_x = -1;
      int previous_y = 0;

      for (uint16_t i = 0; i < width; ++i) {
        const int x = preview_x(i);
        const int level = constrain(
            int(values[i] >> 10), 0, DISPLAY_HEIGHT);
        const int y = BottomAlign(level) + 1;

        if (previous_x >= 0) {
          raster_line(previous_x, previous_y, x, y,
                  i < current_phase);
        } else {
          draw_pixel(x, y);
        }

        previous_x = x;
        previous_y = y;
      }

      // Highlight the selected stage by drawing its contour one pixel
      // alongside the normal contour.
      if (channel == selected_channel) {
        const EnvType type = static_cast<EnvType>(c.type);
        const int stage = cursor_stage_index(cursor, type);
        const int num_stages = stage_count(type);

        uint16_t stage_start = 0;
        uint16_t stage_end = 0;
        if (stage >= 0 && stage < num_stages) {
          const StageInfo& info = stage_map(type)[stage];
          stage_start = segment_starts[info.segment_start];
          stage_end = stage == num_stages - 1
              ? width : segment_starts[info.segment_start + 1];
        }
        // Highlight the Sustain junction at the start of Release.
        if (cursor == CURSOR_SUSTAIN &&
            (type == ENV_ADR || type == ENV_LOOP_ADR) &&
            stage_start == stage_end && stage_start < width) {
          const int node_x = preview_x(stage_start);
          const int node_y = BottomAlign(constrain(
              int(values[stage_start] >> 10), 0, DISPLAY_HEIGHT)) + 1;

          for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
              if (abs(dx) == 2 || abs(dy) == 2)
                clear_pixel(node_x + dx, node_y + dy);
              else
                draw_pixel(node_x + dx, node_y + dy);
            }
          }
        }

        if (stage_start < stage_end &&
            stage_end <= width) {
          // Collect both raster boundaries for the selected stage.
          int stage_lower_y[64];
          int stage_upper_y[64];
          for (int &y : stage_lower_y) y = -1;
          for (int &y : stage_upper_y) y = 64;

          for (uint16_t i = stage_start;
               i < stage_end && i + 1 < width; ++i) {
            const int x1 = preview_x(i);
            const int x2 = preview_x(i + 1);
            const int y1 = BottomAlign(constrain(
                int(values[i] >> 10), 0, DISPLAY_HEIGHT)) + 1;
            const int y2 = BottomAlign(constrain(
                int(values[i + 1] >> 10), 0, DISPLAY_HEIGHT)) + 1;
            raster_line(x1, y1, x2, y2, false,
                        stage_lower_y, true, stage_upper_y);
          }

          const uint16_t final_point =
              (stage_end < width) ? stage_end : width - 1;
          const int first_column =
              constrain(preview_x(stage_start) - x_offset, 0, 63);
          const int last_column =
              constrain(preview_x(final_point) - x_offset, 0, 63);

          const auto trace_stage_boundary = [&](bool highlight) {
            const int *boundary =
                highlight ? stage_upper_y : stage_lower_y;
            const int side = highlight ? -1 : 1;

            const auto put_pixel = [&](int x, int y) {
              if (highlight) draw_pixel(x, y);
              else clear_pixel(x, y);
            };

            int previous_y = -1;
            bool have_previous = false;

            for (int column = first_column;
                 column <= last_column; ++column) {
              if (boundary[column] < 0 || boundary[column] >= 64)
                continue;

              const int x = x_offset + column;
              const int target_y = boundary[column] + side;
              const int connector_x =
                  (highlight == (stage == 0)) ? x - 1 : x;

              if (have_previous) {
                // Connect vertically until corner contact is achieved.
                while (abs(target_y - previous_y) > 1) {
                  previous_y +=
                      (target_y > previous_y) ? 1 : -1;
                  put_pixel(connector_x, previous_y);
                }
              }

              put_pixel(x, target_y);
              previous_y = target_y;
              have_previous = true;
            }
          };

          trace_stage_boundary(false); // Clear-space: lower boundary.
          trace_stage_boundary(true);  // Highlight: upper boundary.

          // Complete clear-space at the selected stage edge.
          const bool is_attack = stage == 0;
          const uint16_t edge_point =
              is_attack ? final_point : stage_start;
          const int edge_x = preview_x(edge_point);
          const int edge_column = edge_x - x_offset;

          if (edge_column >= 0 && edge_column < 64 &&
              stage_lower_y[edge_column] >= 0) {
            const int edge_y = BottomAlign(constrain(
                int(values[edge_point] >> 10),
                0, DISPLAY_HEIGHT)) + 1;
            const int clear_x = edge_x + (is_attack ? 1 : -1);
            const int clear_start_y = stage_lower_y[edge_column] + 1;
            const int step = edge_y >= clear_start_y ? 1 : -1;

            for (int y = clear_start_y; ; y += step) {
              clear_pixel(clear_x, y);
              if (y == edge_y) break;
            }
          }
        }
      }
}

const char* FLASHMEM Piquet::cursor_display_label() const {
      switch (cursor) {
        case CURSOR_ATTACK_SHAPE: return "AttShape";
        case CURSOR_DECAY_SHAPE: return "DcyShape";
        case CURSOR_RELEASE_SHAPE: return "RelShape";
        case CURSOR_DELAY: return "TrgDelay";
        case CURSOR_DELAY_MODE: return "DlyMode";
        case CURSOR_EOC: return "Send EOC";
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

int FLASHMEM Piquet::text_width(const char* value, int max_width) {
      return min(static_cast<int>(strlen(value)) * 6, max_width);
}

int FLASHMEM Piquet::cursor_value_width(const Channel &c) const {
      switch (cursor) {
        case CURSOR_LOOP_MODE:
          return 18;
        case CURSOR_ATTACK:
        case CURSOR_DECAY:
        case CURSOR_SUSTAIN:
        case CURSOR_RELEASE: {
          const int stage = stage_value_index(cursor);
          const int duration = stage_display_value(c, stage);
          if (stage == STAGE_SUSTAIN)
            return numeric_text_width(duration) + 6;
          if (duration < 1000)
            return numeric_text_width(duration) + 12;
          return numeric_text_width(duration / 1000) + 24;
        }
        case CURSOR_ATTACK_SHAPE:
          return text_width(shape_name(c.attack_shape), 36);
        case CURSOR_DECAY_SHAPE:
          return text_width(shape_name(c.decay_shape), 36);
        case CURSOR_RELEASE_SHAPE:
          return text_width(shape_name(c.release_shape), 36);
        case CURSOR_DELAY:
          return c.delay_index < 100
              ? numeric_text_width(c.delay_index * 10) + 12 : 24;
        case CURSOR_DELAY_MODE:
          return text_width(dmode_name(c.delay_mode), 52);
        case CURSOR_EOC:
          return 18;
        case CURSOR_CV_MAP:
          return text_width(cvmap_name(c.cv_map), 52);
        case CURSOR_LEVEL:
          return 24;
        case CURSOR_INVERT:
          return 18;
        default:
          return 6;
      }
}

const char* FLASHMEM Piquet::cursor_label() const {
      static const char* labels[CURSOR_COUNT] = {
        "Channel", "Length", "Envelope", "EnvType", "LoopMode",
        "Attack", "Decay", "Sustain", "Release",
        "AttShape", "DcyShape", "RelShape",
        "Delay Mode", "Delay", "Send EOC", "CV Map", "Level", "Invert"
      };
      return labels[constrain(cursor, 0, CURSOR_COUNT - 1)];
}

const char* FLASHMEM Piquet::type_name(uint8_t t) {
      static const char* names[ENV_TYPE_COUNT] = { "AD", "ADR", "AR", "ADSR", "L-AD", "L-ADR" };
      return names[constrain(t, 0, ENV_TYPE_COUNT - 1)];
}

const char* FLASHMEM Piquet::dmode_name(uint8_t m) {
      static const char* names[DELAY_MODE_COUNT] = { "Off", "Rpl", "Que" };
      return names[constrain(m, 0, DELAY_MODE_COUNT - 1)];
}

const char* FLASHMEM Piquet::cvmap_name(uint8_t m) {
      static const char* names[CVMAP_COUNT] = { "None", "Att", "Dec", "Sus", "Rel", "ADR", "Lvl" };
      return names[constrain(m, 0, CVMAP_COUNT - 1)];
}

const char* FLASHMEM Piquet::shape_name(uint8_t s) {
      static const char* names[SHAPE_COUNT] = {
        "Linear", "Expo", "Quart", "Sine", "Plat", "Cliff",
        "Gate", "BigDip", "MedDip", "LitDip", "SinFld"
      };
      return names[constrain(s, 0, SHAPE_COUNT - 1)];
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
      CONSTRAIN(channels[ch].attack_shape, 0, SHAPE_COUNT - 1);
      CONSTRAIN(channels[ch].decay_shape, 0, SHAPE_COUNT - 1);
      CONSTRAIN(channels[ch].release_shape, 0, SHAPE_COUNT - 1);
    }
  }

  // Keep the cursor valid for the restored envelope type.
  const EnvType type = static_cast<EnvType>(channels[selected_channel].type);
  if (!cursor_visible_for_type(cursor, type)) {
    cursor = CURSOR_ATTACK;
    parameter_navigation = false;
  }
}
