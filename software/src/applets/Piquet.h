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
    static constexpr int DISPLAY_HEIGHT = 30;
    static constexpr uint8_t MAX_TRIGGER_QUEUE = 4;


    enum EnvType : uint8_t {
      ENV_AD,
      ENV_AHR,
      ENV_AR,
      ENV_ADSR,
      ENV_LOOP_AD,
      ENV_LOOP_AHR,
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
      uint16_t peaks_cached_attack_time;
      uint16_t peaks_cached_decay_time;
      uint16_t peaks_cached_release_time;
      bool peaks_times_cached;

      bool gate_prev;
      simfloat output;
      TriggerQueue trigger_queue;

      void Init(uint8_t ch) {
        type = (ch == 0) ? ENV_ADSR : ENV_AR;
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
        loop_enabled = false;
        inverted = false;
        eoc_enabled = false;
        eoc_trigger_pending = false;
        length_index = 0;
        peaks_envelope.Init();
        peaks_trigger_pending = false;
        peaks_waiting_for_trigger = false;
        peaks_configured_type = 0xFF;
        peaks_config_signature = 0xFFFFFFFFu;
        peaks_cached_attack = 0;
        peaks_cached_decay = 0;
        peaks_cached_release = 0;
        peaks_times_cached = false;

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
        Pack(data, PackLocation {59,2}, length_index);
        Pack(data, PackLocation {61,1}, eoc_enabled ? 1 : 0);
        // 2 empty bits
        return data;
      }
      void unpack(uint64_t data) {
        type = constrain(Unpack(data, PackLocation {0,3}), 0, ENV_TYPE_COUNT - 1);
        stage_value[STAGE_ATTACK] = constrain(Unpack(data, PackLocation {3,8}), 1, STAGE_MAX_VALUE);
        stage_value[STAGE_DECAY] = constrain(Unpack(data, PackLocation {11,8}), 1, STAGE_MAX_VALUE);
        stage_value[STAGE_SUSTAIN] = constrain(Unpack(data, PackLocation {19,8}), 0, STAGE_MAX_VALUE);
        stage_value[STAGE_RELEASE] = constrain(Unpack(data, PackLocation {27,8}), 1, STAGE_MAX_VALUE);
        delay_index = constrain(Unpack(data, PackLocation {35,7}), 0, 126);
        delay_mode = constrain(Unpack(data, PackLocation {43,2}), 0, DELAY_MODE_COUNT - 1);
        cv_map = constrain(Unpack(data, PackLocation {45,3}), 0, CVMAP_COUNT - 1);
        level = constrain(Unpack(data, PackLocation {48,8}), 0, 255);
        inverted = Unpack(data, PackLocation {56,1});
        loop_mode = Unpack(data, PackLocation {57,1});
        loop_enabled = false;
        length_index = constrain(Unpack(data, PackLocation {59,2}), 0, 3);
        eoc_enabled = Unpack(data, PackLocation {61,1});
        // 2 empty bits
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
          return type == ENV_ADSR;
        case CURSOR_RELEASE:
        case CURSOR_RELEASE_SHAPE:
          return type == ENV_AHR || type == ENV_AR || type == ENV_ADSR ||
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
        case ENV_LOOP_AHR:
          return 3;
        case ENV_AR:
          return 2;
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
          if (type == ENV_ADSR) return 2;
          return -1;
        case CURSOR_RELEASE:
        case CURSOR_RELEASE_SHAPE:
          if (type == ENV_AHR || type == ENV_LOOP_AHR) return 2;
          if (type == ENV_AR) return 1;
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

      if (type == ENV_AR) {
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
          if (next < CURSOR_ATTACK_SHAPE || next >= CURSOR_COUNT) {
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
          parameter_navigation = false;
          return CURSOR_LENGTH;
        }
        return cursor_for_stage_index(stage - 1, type, false);
      }

      parameter_navigation = true;
      return current;
    }

    static constexpr int StageTicks(uint8_t value) {
      return HS::kShapeExponential[value] >> 1;
    }

    static constexpr uint16_t PeaksTime(int ticks) {
      if (ticks < 1) ticks = 1;

      const uint64_t target =
          (static_cast<uint64_t>(1) << 32) / static_cast<uint32_t>(ticks);

      uint64_t best_difference = UINT64_MAX;
      uint16_t best_index = 0;

      for (uint16_t i = 0; i < 256; ++i) {
        const uint32_t increment = peaks::lut_env_increments[i];
        const uint64_t difference = increment > target
            ? increment - target
            : target - increment;

        if (difference < best_difference) {
          best_difference = difference;
          best_index = i;
        }
      }

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
        // case NONE:
        default: break;
      }

      // Convert Piquet's stage durations to Peaks' time LUT indices.
      // Cache the result until a parameter or its CV-modulated value changes.
      if (!c.peaks_times_cached ||
          c.peaks_cached_attack != attack ||
          c.peaks_cached_decay != decay ||
          c.peaks_cached_release != release) {
        c.peaks_cached_attack = attack;
        c.peaks_cached_decay = decay;
        c.peaks_cached_release = release;

        c.peaks_cached_attack_time =
            PeaksTime(StageTicks(attack));
        c.peaks_cached_decay_time =
            PeaksTime(StageTicks(decay));
        c.peaks_cached_release_time =
            PeaksTime(StageTicks(release));

        c.peaks_times_cached = true;
      }

      const uint16_t attack_time = c.peaks_cached_attack_time;
      const uint16_t decay_time = c.peaks_cached_decay_time;
      const uint16_t release_time = c.peaks_cached_release_time;
      const uint16_t sustain_level =
          Proportion(sustain, STAGE_MAX_VALUE, 32767);

      const uint32_t signature =
          static_cast<uint32_t>(c.type) |
          (static_cast<uint32_t>(c.attack_shape) << 4) |
          (static_cast<uint32_t>(c.decay_shape) << 8) |
          (static_cast<uint32_t>(c.release_shape) << 12) |
          (static_cast<uint32_t>(c.length_index) << 17);

      if (c.peaks_configured_type != c.type) {
        c.peaks_trigger_pending = false;
        env.Init();
        env.set_attack_reset_behaviour(peaks::RESET_BEHAVIOUR_NULL);
        env.set_decay_release_reset_behaviour(peaks::RESET_BEHAVIOUR_SEGMENT_PHASE);
        env.set_attack_falling_gate_behaviour(
            peaks::FALLING_GATE_BEHAVIOUR_IGNORE);
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
        env.set_release_time_multiplier(c.length_index + 1);

        // seems redundant?
        //env.set_adsar(attack_time, decay_time, sustain_level, release_time);

        switch (c.type) {
          case ENV_AD:
          case ENV_LOOP_AD:
            env.set_ad(attack_time, decay_time, 0,
                c.type == ENV_LOOP_AD ? 2 : 0);
            break;
          case ENV_AHR:
          case ENV_LOOP_AHR:
            env.set_adr(attack_time, decay_time, 32767, release_time,
                0, c.type == ENV_LOOP_AHR ? 3 : 0);
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
        default:
          env.set_level(2, sustain_level);
          // and fall thru
        case ENV_AHR:
        case ENV_LOOP_AHR:
          env.set_time(2, release_time);
          // and fall thru
        case ENV_AD:
        case ENV_LOOP_AD:
          env.set_time(1, decay_time);
          break;

        case ENV_AR:
          env.set_time(1, release_time);
          break;
      }

      env.set_amplitude(65535, false);

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
      if (c.is_looping() && c.should_repeat())
        control |= peaks::CONTROL_GATE;

      const uint16_t value = env.ProcessSingleSample(control);
      const int amplitude_cv = Proportion(value, 32767, HEMISPHERE_MAX_CV);
      c.output = int2simfloat(constrain(amplitude_cv, 0, HEMISPHERE_MAX_CV));


    }

    static const char* type_name(uint8_t t);
    static const char* dmode_name(uint8_t m);
    static const char* cvmap_name(uint8_t m);
    static const char* shape_name(uint8_t s);

    void DrawIndicators();
    void DrawParamRow(int x_offset = 0);
    const char* cursor_display_label() const;
    static int numeric_text_width(int value);
    int cursor_value_width(const Channel &c) const;
    void DrawEnvelope(int channel, int x_offset, bool absolute_coordinates);
    const char* cursor_label() const;

};

void FLASHMEM Piquet::Controller() {
      ForEachChannel(ch) {
        Channel &c = channels[ch];

        // Discard stale triggers when the envelope type changes.
        // A clock arriving in this same cycle can still trigger normally.
        if (c.peaks_configured_type != c.type) {
          c.peaks_trigger_pending = false;
          c.trigger_queue.Reset();
          c.peaks_waiting_for_trigger = true;
        }

        const bool gate = Gate(ch);
        const bool clock_trigger = Clock(ch);
        const bool eoc_trigger = c.eoc_trigger_pending;
        c.eoc_trigger_pending = false;
        const bool trigger = clock_trigger || eoc_trigger;
        const bool gate_fall = !gate && c.gate_prev;
        c.gate_prev = gate;

        if (c.is_looping() && c.loop_mode != LOOP_CONT) {
          c.loop_enabled = gate;
        }

        if (trigger) {
          const int32_t delay_ms = c.delay_index < 100
              ? static_cast<int32_t>(c.delay_index) * 10
              : 1000 + static_cast<int32_t>(c.delay_index - 100) * 100;
          const int32_t dticks = (delay_ms * 1000L) / 60;
          if (c.delay_mode == DELAY_OFF || dticks <= 0) {
            c.trigger_queue.Reset();
            if (c.is_looping() && c.loop_mode == LOOP_CONT) {
              c.loop_enabled = !c.loop_enabled;
              if (c.loop_enabled) begin_attack(c);
            } else {
              begin_attack(c);
            }
          } else {
            c.trigger_queue.Add(dticks, c.delay_mode == DELAY_QUEUE);
          }
        }

        if (c.trigger_queue.TickReady()) {
          if (c.is_looping() && c.loop_mode == LOOP_CONT) {
            c.loop_enabled = !c.loop_enabled;
            if (c.loop_enabled) begin_attack(c);
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

        int out = simfloat2int(c.output);
        int level = c.level;
        if (c.cv_map == CVMAP_LEVEL)
          Modulate(level, ch, 0, 255);
        out = Proportion(out, HEMISPHERE_MAX_CV, Proportion(level, 255, HEMISPHERE_MAX_CV));
        if (c.inverted) out = HEMISPHERE_MAX_CV - out;
        //out = constrain(out, 0, HEMISPHERE_MAX_CV);

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
        gfxFrame(0, 30, 64, 34);
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
    gfxPrint(channel_x, 20, OutputLabel(ch));

    if (ch == selected_channel &&
        cursor == CURSOR_CHANNEL && EditMode()) {
      gfxFrame(channel_x, 19, 8, 11);
    } else {
      gfxInvert(channel_x, 19, 7, 9);
    }
  }

  const int channel_x = selected_channel * 64 + x_offset;

  if (cursor == CURSOR_CHANNEL) {
    gfxIcon(channel_x + 8, 20, LEFT_ICON);
  } else if (cursor == CURSOR_LENGTH) {
    gfxIcon(channel_x + 30, 20, RIGHT_ICON);
  }

  if (cursor == CURSOR_TYPE || cursor == CURSOR_LOOP_MODE) {
    gfxFrame(channel_x, 30, 64, 34);
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
          c.length_index = constrain(c.length_index + direction, 0, 3);
          break;
        case CURSOR_TYPE:
          c.type = constrain(c.type + direction, 0, ENV_TYPE_COUNT - 1);
          break;
        case CURSOR_LOOP_MODE:
          c.loop_mode = constrain(
            c.loop_mode + direction, 0, LOOP_MODE_COUNT - 1);
          break;
        case CURSOR_ATTACK:
          c.stage_value[STAGE_ATTACK] = constrain(c.stage_value[STAGE_ATTACK] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_ATTACK_SHAPE:
          c.attack_shape = constrain(c.attack_shape + direction, 0, SHAPE_COUNT - 1);
          break;
        case CURSOR_DECAY:
          c.stage_value[STAGE_DECAY] = constrain(c.stage_value[STAGE_DECAY] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_DECAY_SHAPE:
          c.decay_shape = constrain(c.decay_shape + direction, 0, SHAPE_COUNT - 1);
          break;
        case CURSOR_SUSTAIN:
          c.stage_value[STAGE_SUSTAIN] = constrain(c.stage_value[STAGE_SUSTAIN] + direction, 0, STAGE_MAX_VALUE);
          break;
        case CURSOR_RELEASE:
          c.stage_value[STAGE_RELEASE] = constrain(c.stage_value[STAGE_RELEASE] + direction, 1, STAGE_MAX_VALUE);
          break;
        case CURSOR_RELEASE_SHAPE:
          c.release_shape = constrain(c.release_shape + direction, 0, SHAPE_COUNT - 1);
          break;
        case CURSOR_DELAY:
          c.delay_index = constrain(c.delay_index + direction, 0, 126);
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

      gfxPrint(0, 20, OutputLabel(selected_channel));

      if (cursor == CURSOR_CHANNEL && EditMode()) {
        gfxFrame(0, 19, 8, 11);
      } else {
        gfxInvert(0, 19, 7, 9);

        if (cursor == CURSOR_CHANNEL) {
          gfxIcon(8, 20, LEFT_ICON);
        } else if (cursor == CURSOR_LENGTH) {
          gfxIcon(30, 20, RIGHT_ICON);
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
          gfxPrint(value_x, 20, value);
          if (EditMode()) gfxInvert(value_x, 19, strlen(value) * 6, 9);
        } else if (cursor == CURSOR_LOOP_MODE) {
          const char* value = c.loop_mode == LOOP_GATE ? "GTD" : "CON";
          gfxIcon(9 + x_offset, 20, LOOP_ICON);
          gfxPrint(19 + x_offset, 20, value);
          if (EditMode()) gfxInvert(19 + x_offset, 19, strlen(value) * 6, 9);
        }

        static const char* length_names[] = {"x1", "x2", "x4", "x8"};
        gfxIcon(41 + x_offset, 20, ENV_ICON);
        gfxPrint(50 + x_offset, 20, length_names[constrain(c.length_index, 0, 3)]);
        if (cursor == CURSOR_LENGTH && EditMode())
          gfxCursor(50 + x_offset, 29, 12, 9, cursor_label());
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

        if (cursor == CURSOR_EOC) {
          gfxPrint(9 + x_offset, 20, label);
          gfxIcon(28 + x_offset, 20, RIGHT_ICON);
          gfxPrint(35 + x_offset, 20, ":");
          value_x = 43 + x_offset;
        } else {
          gfxPrint(9 + x_offset, 20, label);
          value_x = x_offset + 9 + static_cast<int>(strlen(label)) * 6;
        }

        if (cursor == CURSOR_DELAY) {
          if (c.delay_index < 100) {
            gfxPrint(value_x, 20, ".");
            if (c.delay_index < 10) gfxPrint("0");
            gfxPrint(c.delay_index);
          } else {
            const uint8_t tenths = c.delay_index - 90;
            gfxPrint(value_x, 20, tenths / 10);
            gfxPrint(".");
            gfxPrint(tenths % 10);
          }
          gfxPrint("s");
        } else if (cursor == CURSOR_LEVEL) {
          gfxPrint(value_x, 20, Proportion(c.level, 255, 100));
          gfxPrint("%");
        } else if (value) {
          gfxPrint(value_x, 20, value);
        }

        if (cursor == CURSOR_ATTACK_SHAPE ||
            cursor == CURSOR_DECAY_SHAPE ||
            cursor == CURSOR_RELEASE_SHAPE) {
          value_width = min(static_cast<int>(strlen(value)) * 6, 64 - (value_x - x_offset));
        } else if (cursor == CURSOR_DELAY) {
          value_width = 24;
        } else {
          value_width = min(cursor_value_width(c), 64 - (value_x - x_offset));
        }

        if (EditMode()) {
          gfxCursor(value_x, 29, value_width, 9, cursor_display_label());
        }
        return;
      }

      switch (cursor) {
        case CURSOR_ATTACK:
          gfxPrint(9 + x_offset, 20, "A:");
          gfxPrint(21 + x_offset, 20,
                   (StageTicks(c.stage_value[STAGE_ATTACK]) *
                    (1 << c.length_index)) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_DECAY:
          gfxPrint(9 + x_offset, 21,
                   (c.type == ENV_AHR || c.type == ENV_LOOP_AHR) ? "H:" : "D:");
          gfxPrint(21 + x_offset, 20,
                   (StageTicks(c.stage_value[STAGE_DECAY]) *
                    (1 << c.length_index)) / 17);
          gfxPrint("ms");
          break;
        case CURSOR_SUSTAIN:
          gfxPrint(9 + x_offset, 20, "S:");
          gfxPrint(21 + x_offset, 20,
                   Proportion(c.stage_value[STAGE_SUSTAIN],
                              STAGE_MAX_VALUE, 100));
          gfxPrint("%");
          break;
        case CURSOR_RELEASE:
          gfxPrint(9 + x_offset, 20, "R:");
          gfxPrint(21 + x_offset, 20,
                   (StageTicks(c.stage_value[STAGE_RELEASE]) *
                    (2 << c.length_index)) / 17);
          gfxPrint("ms");
          break;
        default:
          break;
      }

      if (cursor >= CURSOR_ATTACK && cursor != CURSOR_LOOP_MODE && EditMode()) {
        gfxCursor(21 + x_offset, 29, cursor_value_width(c), 9, cursor_label());
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
        if (absolute_coordinates) graphics.setPixel(x, y);
        else gfxPixel(x, y);
      };

      const auto draw_line = [&](int x, int y, int x2, int y2,
                                 bool dotted = false) {
        if (absolute_coordinates)
          graphics.drawLine(x, y, x2, y2, dotted ? 2 : 1);
        else
          gfxLine(x, y, x2, y2, dotted);
      };

      const auto preview_x = [&](uint16_t point) {
        return x_offset + 1 + (width > 1
            ? Proportion(point, width - 1, 61) : 0);
      };

      // Fill the envelope with interpolated vertical columns.
      for (uint16_t i = 0; i < width && i <= current_phase; ++i) {
        const int x = preview_x(i);
        const int next_x = i + 1 < width ? preview_x(i + 1) : x;
        const int level = constrain(static_cast<int>(values[i] >> 10), 0, DISPLAY_HEIGHT);
        const int next_level = i + 1 < width
            ? constrain(static_cast<int>(values[i + 1] >> 10), 0, DISPLAY_HEIGHT)
            : level;
        const int baseline = BottomAlign(0) + 1;
        for (int fill_x = x; fill_x <= next_x; ++fill_x) {
          const int interpolated_level = next_x > x
              ? level + (next_level - level) * (fill_x - x) / (next_x - x)
              : level;
          const int endpoint = BottomAlign(interpolated_level) + 1;
          const int top = baseline < endpoint ? baseline : endpoint;
          const int height = abs(endpoint - baseline) + 1;
          if (absolute_coordinates) {
            graphics.drawVLine(fill_x, top, height);
          } else {
            for (int y = top; y < top + height; ++y) gfxPixel(fill_x, y);
          }
        }
      }

      int previous_x = -1;
      int previous_y = 0;

      for (uint16_t i = 0; i < width; ++i) {
        const int x = preview_x(i);
        const int level = constrain(
            static_cast<int>(values[i] >> 10), 0, DISPLAY_HEIGHT);
        const int y = BottomAlign(level) + 1;

        if (previous_x >= 0) {
          draw_line(previous_x, previous_y, x, y, true);
        } else {
          draw_pixel(x, y);
        }

        previous_x = x;
        previous_y = y;
      }

      // EOC marker: top-right corner of the envelope graphic.
      draw_pixel(preview_x(width - 1), BottomAlign(DISPLAY_HEIGHT));

      // Mark stage boundaries along the bottom of the preview.
      for (uint8_t i = 0;
           i < 16 && segment_starts[i] != 0xFFFF; ++i) {
        const uint16_t point = segment_starts[i];
        if (point >= width) continue;

        const int x = preview_x(point);
        draw_pixel(x, BottomAlign(DISPLAY_HEIGHT));
      }

      if (channel != selected_channel) return;

      const int stage =
          (cursor == CURSOR_ATTACK_SHAPE ||
           cursor == CURSOR_DECAY_SHAPE ||
           cursor == CURSOR_RELEASE_SHAPE)
              ? -1
              : cursor_stage_index(cursor, static_cast<EnvType>(c.type));

      if (stage < 0) return;

      int start_index = stage;
      if ((c.type == ENV_AR && stage == 1) ||
          ((c.type == ENV_AHR || c.type == ENV_LOOP_AHR) && stage == 2)) {
        ++start_index;
      }

      if (start_index >= 15 || segment_starts[start_index] == 0xFFFF) return;

      const uint16_t next_point = segment_starts[start_index + 1];
      const uint16_t end_point =
          next_point == 0xFFFF ? width - 1 : next_point;
      const int x1 = preview_x(segment_starts[start_index]);
      const int x2 = preview_x(end_point);

      if (CursorBlink()) {
        const int y = BottomAlign(DISPLAY_HEIGHT + 2);
        draw_line(x1, y, x2, y, false);
        draw_line(x1, y, x1, y + 1, false);
        draw_line(x2, y, x2, y + 1, false);
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

int FLASHMEM Piquet::cursor_value_width(const Channel &c) const {
      switch (cursor) {
        case CURSOR_LOOP_MODE:
          return 18;
        case CURSOR_ATTACK:
          return numeric_text_width(
              (StageTicks(c.stage_value[STAGE_ATTACK]) *
               (1 << c.length_index)) / 17) + 12;
        case CURSOR_DECAY:
          return numeric_text_width(
              (StageTicks(c.stage_value[STAGE_DECAY]) *
               (1 << c.length_index)) / 17) + 12;
        case CURSOR_SUSTAIN:
          return numeric_text_width(Proportion(c.stage_value[STAGE_SUSTAIN], STAGE_MAX_VALUE, 100)) + 6;
        case CURSOR_RELEASE:
          return numeric_text_width(
              (StageTicks(c.stage_value[STAGE_RELEASE]) *
               (2 << c.length_index)) / 17) + 12;
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
        case CURSOR_EOC:
          return 18;
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
      static const char* labels[CURSOR_COUNT] = {
        "Channel", "Length", "Envelope", "EnvType", "LoopMode",
        "Attack", "Decay", "Sustain", "Release",
        "AttShape", "DcyShape", "RelShape",
        "Delay Mode", "Delay", "Send EOC", "CV Map", "Level", "Invert"
      };
      if (channels[selected_channel].type == ENV_AHR ||
          channels[selected_channel].type == ENV_LOOP_AHR) {
        if (cursor == CURSOR_DECAY) return "Hold";
      }
      return labels[constrain(cursor, 0, CURSOR_COUNT - 1)];
}

const char* FLASHMEM Piquet::type_name(uint8_t t) {
      static const char* names[ENV_TYPE_COUNT] = { "AD", "AHR", "AR", "ADSR", "L-AD", "L-AHR" };
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
      static const char* names[SHAPE_COUNT] = { "Lin", "Exp", "Qua", "Sin", "Pla", "Cli", "Gat", "BD", "MD", "LD", "SF" };
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
}
