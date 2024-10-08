//
// Initial app implementation: Patrick Dowling (pld@gurkenkiste.com)
// Modifications by: Tim Churches (tim.churches@gmail.com)
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
//
// Quad enevelope generator app, based on the multistage envelope implementation
// from Peaks by Emilie Gillet (see peaks_multistage_envelope.h/cpp)

#include "OC_apps.h"
#include "OC_bitmaps.h"
#include "OC_digital_inputs.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "util/util_math.h"
#include "util/util_settings.h"
#include "src/extern/peaks_multistage_envelope.h"
#include "src/extern/bjorklund.h"
#include "OC_euclidean_mask_draw.h"

// peaks::MultistageEnvelope allow setting of more parameters per stage, but
// that will involve more editing code, so keeping things simple for now
// with one value per stage.
//
// MultistageEnvelope maps times to lut_env_increments directly, so only 256 discrete values (no interpolation)
// Levels are 0-32767 to be positive on Peaks' bipolar output

enum EnvelopeSettings {
  ENV_SETTING_TYPE,
  ENV_SETTING_SEG1_VALUE,
  ENV_SETTING_SEG2_VALUE,
  ENV_SETTING_SEG3_VALUE,
  ENV_SETTING_SEG4_VALUE,
  ENV_SETTING_TRIGGER_INPUT,
  ENV_SETTING_TRIGGER_DELAY_MODE,
  ENV_SETTING_TRIGGER_DELAY_COUNT,
  ENV_SETTING_TRIGGER_DELAY_MILLISECONDS,
  ENV_SETTING_TRIGGER_DELAY_SECONDS,
  ENV_SETTING_EUCLIDEAN_LENGTH,
  ENV_SETTING_EUCLIDEAN_FILL,
  ENV_SETTING_EUCLIDEAN_OFFSET,
  ENV_SETTING_EUCLIDEAN_RESET_INPUT,
  ENV_SETTING_EUCLIDEAN_RESET_CLOCK_DIV,
  ENV_SETTING_CV1,
  ENV_SETTING_CV2,
  ENV_SETTING_CV3,
  ENV_SETTING_CV4,
  ENV_SETTING_ATTACK_RESET_BEHAVIOUR,
  ENV_SETTING_ATTACK_FALLING_GATE_BEHAVIOUR,
  ENV_SETTING_DECAY_RELEASE_RESET_BEHAVIOUR,
  ENV_SETTING_GATE_HIGH,
  ENV_SETTING_ATTACK_SHAPE,
  ENV_SETTING_DECAY_SHAPE,
  ENV_SETTING_RELEASE_SHAPE,
  ENV_SETTING_ATTACK_TIME_MULTIPLIER,
  ENV_SETTING_DECAY_TIME_MULTIPLIER,
  ENV_SETTING_RELEASE_TIME_MULTIPLIER,
  ENV_SETTING_AMPLITUDE,
  ENV_SETTING_SAMPLED_AMPLITUDE,
  ENV_SETTING_MAX_LOOPS,
  ENV_SETTING_INVERTED,
  ENV_SETTING_LAST
};

enum CVMapping {
  CV_MAPPING_NONE,
  CV_MAPPING_SEG1,
  CV_MAPPING_SEG2,
  CV_MAPPING_SEG3,
  CV_MAPPING_SEG4,
  CV_MAPPING_ADR,
  CV_MAPPING_EUCLIDEAN_LENGTH,
  CV_MAPPING_EUCLIDEAN_FILL,
  CV_MAPPING_EUCLIDEAN_OFFSET,
  CV_MAPPING_DELAY_MSEC,
  CV_MAPPING_AMPLITUDE,
  CV_MAPPING_MAX_LOOPS,
  CV_MAPPING_LAST
};

enum EnvelopeType {
  ENV_TYPE_AD,
  ENV_TYPE_ADSR,
  ENV_TYPE_ADR,
  ENV_TYPE_AR,
  ENV_TYPE_ADSAR,
  ENV_TYPE_ADAR,
  ENV_TYPE_ADL2,
  ENV_TYPE_ADRL3,
  ENV_TYPE_ADL2R,
  ENV_TYPE_ADAL2R,
  ENV_TYPE_ADARL4,
  ENV_TYPE_LAST, ENV_TYPE_FIRST = ENV_TYPE_AD
};

enum TriggerDelayMode {
  TRIGGER_DELAY_OFF,
  TRIGGER_DELAY_QUEUE, // Queue up to kMaxDelayedTriggers delays, additional triggers ignored
  TRIGGER_DELAY_RING,  // Queue up to kMaxDelayedTriggers delays, additional triggers overwrite oldest
  TRIGGER_DELAY_LAST
};

// With only one type, the U4 setting size still works. Each of the these maps to 3 values in the
// setting, 1 for each channel other than the current.
// Ordering here ideally maps to peaks::EnvStateBitMask shifts
enum IntTriggerType {
  INT_TRIGGER_EOC,
  INT_TRIGGER_LAST
};

const char* const envelope_types[ENV_TYPE_LAST] = {
  "AD", "ADSR", "ADR", "ASR", "ADSAR", "ADAR", "ADL2", "ADRL3", "ADL2R", "ADAL2R", "ADARL4"
};

const char* const segment_names[] = {
  "Attack", "Decay", "Sustain/Level", "Release"
};

const char* const cv_mapping_names[CV_MAPPING_LAST] = {
  "None", "Att", "Dec", "Sus", "Rel", "ADR", "Eleng", "Efill", "Eoffs", "Delay", "Ampl", "Loops"
};

const char* const trigger_delay_modes[TRIGGER_DELAY_LAST] = {
  "Off", "Queue", "Ring"
};

const char* const euclidean_lengths[] = {
  "Off", "  2", "  3", "  4", "  5", "  6", "  7", "  8", "  9", " 10",
  " 11", " 12", " 13", " 14", " 15", " 16", " 17", " 18", " 19", " 20",
  " 21", " 22", " 23", " 24", " 25", " 26", " 27", " 28", " 29", " 30",
  " 31", " 32",
};

const char* const time_multipliers[] = {
  "1", "  2", "  4", "  8", "  16", "  32", "  64", " 128", " 256", " 512", "1024", "2048", "4096", "8192"
};

const char* const internal_trigger_types[INT_TRIGGER_LAST] = {
  "EOC", // Keep length == 3
};

inline int TriggerSettingToChannel(int setting_value) __attribute__((always_inline));
inline int TriggerSettingToChannel(int setting_value) {
  return (setting_value - OC::DIGITAL_INPUT_LAST) / INT_TRIGGER_LAST;
}

static inline IntTriggerType TriggerSettingToType(int setting_value, int channel) __attribute__((always_inline));
static inline IntTriggerType TriggerSettingToType(int setting_value, int channel) {
  return static_cast<IntTriggerType>((setting_value - OC::DIGITAL_INPUT_LAST) - channel * INT_TRIGGER_LAST);
}

class EnvelopeGenerator : public settings::SettingsBase<EnvelopeGenerator, ENV_SETTING_LAST> {
public:

  static constexpr int kMaxSegments = 4;
  static constexpr int kEuclideanParams = 3;
  static constexpr int kDelayParams = 1;
  static constexpr int kAmplitudeParams = 2; // incremented to 2 to cover the MAX_LOOPS parameter
  static constexpr size_t kMaxDelayedTriggers = 24; 

  struct DelayedTrigger {
    uint32_t delay;
    uint32_t time_left;

    inline void Activate(uint32_t t) {
      delay = time_left = t;
    }

    inline void Reset() {
      delay = time_left = 0;
    }
  };

  void Init(OC::DigitalInput default_trigger);

  EnvelopeType get_type() const {
    return static_cast<EnvelopeType>(values_[ENV_SETTING_TYPE]);
  }

  int get_trigger_input() const {
    return values_[ENV_SETTING_TRIGGER_INPUT];
  }

  int32_t get_trigger_delay_ms() const {
    return 1000U * values_[ENV_SETTING_TRIGGER_DELAY_SECONDS] + values_[ENV_SETTING_TRIGGER_DELAY_MILLISECONDS] ;
  }

  TriggerDelayMode get_trigger_delay_mode() const {
    return static_cast<TriggerDelayMode>(values_[ENV_SETTING_TRIGGER_DELAY_MODE]);
  }

  peaks::EnvResetBehaviour get_attack_reset_behaviour() const {
    return static_cast<peaks::EnvResetBehaviour>(values_[ENV_SETTING_ATTACK_RESET_BEHAVIOUR]);
  }

  peaks::EnvFallingGateBehaviour get_attack_falling_gate_behaviour() const {
    return static_cast<peaks::EnvFallingGateBehaviour>(values_[ENV_SETTING_ATTACK_FALLING_GATE_BEHAVIOUR]);
  }

  peaks::EnvResetBehaviour get_decay_release_reset_behaviour() const {
    return static_cast<peaks::EnvResetBehaviour>(values_[ENV_SETTING_DECAY_RELEASE_RESET_BEHAVIOUR]);
  }

  uint8_t get_euclidean_length() const {
    return values_[ENV_SETTING_EUCLIDEAN_LENGTH];
  }

  uint8_t get_euclidean_fill() const {
    return values_[ENV_SETTING_EUCLIDEAN_FILL];
  }

  uint8_t get_euclidean_offset() const {
    return values_[ENV_SETTING_EUCLIDEAN_OFFSET];
  }

  uint8_t get_euclidean_reset_trigger_input() const {
    return values_[ENV_SETTING_EUCLIDEAN_RESET_INPUT];
  }

  uint8_t get_euclidean_reset_clock_div() const {
    return values_[ENV_SETTING_EUCLIDEAN_RESET_CLOCK_DIV];
  }

  uint32_t get_euclidean_counter() const {
    return euclidean_counter_;
  }

  uint16_t get_amplitude() const {
    return values_[ENV_SETTING_AMPLITUDE] << 9 ;
  }

  bool is_amplitude_sampled() const {
     return static_cast<bool>(values_[ENV_SETTING_SAMPLED_AMPLITUDE]);
  }

  uint16_t get_max_loops() const {
    return values_[ENV_SETTING_MAX_LOOPS] << 9 ;
  }

  bool is_inverted() const {
     return static_cast<bool>(values_[ENV_SETTING_INVERTED]);
  }

  uint8_t get_s_euclidean_length() const {
    return s_euclidean_length_;
  }

  uint8_t get_s_euclidean_fill() const {
    return s_euclidean_fill_;
  }

  uint8_t get_s_euclidean_offset() const {
    return s_euclidean_offset_;
  }

  uint32_t get_trigger_delay_count() const {
    return values_[ENV_SETTING_TRIGGER_DELAY_COUNT];
  }

  CVMapping get_cv1_mapping() const {
    return static_cast<CVMapping>(values_[ENV_SETTING_CV1]);
  }

  CVMapping get_cv2_mapping() const {
    return static_cast<CVMapping>(values_[ENV_SETTING_CV2]);
  }

  CVMapping get_cv3_mapping() const {
    return static_cast<CVMapping>(values_[ENV_SETTING_CV3]);
  }

  CVMapping get_cv4_mapping() const {
    return static_cast<CVMapping>(values_[ENV_SETTING_CV4]);
  }

//  bool get_hard_reset() const {
//    return values_[ENV_SETTING_HARD_RESET];
//  }

  bool get_gate_high() const {
    return values_[ENV_SETTING_GATE_HIGH];
  }

  peaks::EnvelopeShape get_attack_shape() const {
    return static_cast<peaks::EnvelopeShape>(values_[ENV_SETTING_ATTACK_SHAPE]);
  }

  peaks::EnvelopeShape get_decay_shape() const {
    return static_cast<peaks::EnvelopeShape>(values_[ENV_SETTING_DECAY_SHAPE]);
  }

  peaks::EnvelopeShape get_release_shape() const {
    return static_cast<peaks::EnvelopeShape>(values_[ENV_SETTING_RELEASE_SHAPE]);
  }

  uint16_t get_attack_time_multiplier() const {
    return static_cast<uint16_t>(values_[ENV_SETTING_ATTACK_TIME_MULTIPLIER]);
  }

  uint16_t get_decay_time_multiplier() const {
    return static_cast<uint16_t>(values_[ENV_SETTING_DECAY_TIME_MULTIPLIER]);
  }

  uint16_t get_release_time_multiplier() const {
    return static_cast<uint16_t>(values_[ENV_SETTING_RELEASE_TIME_MULTIPLIER]);
  }

  // Utils
  uint16_t get_segment_value(int segment) const {
    return values_[ENV_SETTING_SEG1_VALUE + segment];
  }

  int num_editable_segments() const {
    switch (get_type()) {
      case ENV_TYPE_AD:
      case ENV_TYPE_AR:
      case ENV_TYPE_ADL2:
        return 2;
      case ENV_TYPE_ADR:
      case ENV_TYPE_ADSR:
      case ENV_TYPE_ADSAR:
      case ENV_TYPE_ADAR:
      case ENV_TYPE_ADRL3:
      case ENV_TYPE_ADL2R:
      case ENV_TYPE_ADARL4:
      case ENV_TYPE_ADAL2R:
        return 4;
      default: break;
    }
    return 0;
  }

  inline void apply_cv_mapping(EnvelopeSettings cv_setting, const int32_t cvs[ADC_CHANNEL_LAST], int32_t segments[CV_MAPPING_LAST]) {
    // segments is indexed directly with CVMapping enum values
    int mapping = values_[cv_setting];
    switch (mapping) {
      case CV_MAPPING_SEG1:
      case CV_MAPPING_SEG2:
      case CV_MAPPING_SEG3:
      case CV_MAPPING_SEG4:
        segments[mapping] += (cvs[cv_setting - ENV_SETTING_CV1] * 65536) >> 12;
        break;
      case CV_MAPPING_ADR:
        segments[CV_MAPPING_SEG1] += (cvs[cv_setting - ENV_SETTING_CV1] * 65536) >> 12;
        segments[CV_MAPPING_SEG2] += (cvs[cv_setting - ENV_SETTING_CV1] * 65536) >> 12;
        segments[CV_MAPPING_SEG4] += (cvs[cv_setting - ENV_SETTING_CV1] * 65536) >> 12;
        break;
      case CV_MAPPING_EUCLIDEAN_LENGTH:
      case CV_MAPPING_EUCLIDEAN_FILL:
      case CV_MAPPING_EUCLIDEAN_OFFSET:
        segments[mapping] += cvs[cv_setting - ENV_SETTING_CV1]  >> 6;
        break;
      case CV_MAPPING_DELAY_MSEC:
        segments[mapping] += cvs[cv_setting - ENV_SETTING_CV1]  >> 2;
        break;
      case  CV_MAPPING_AMPLITUDE:
        segments[mapping] += cvs[cv_setting - ENV_SETTING_CV1] << 5 ;
        break;
      case  CV_MAPPING_MAX_LOOPS:
        segments[mapping] += cvs[cv_setting - ENV_SETTING_CV1] << 2 ;
        break;
      default:
        break;
    }
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  EnvelopeSettings enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  void update_enabled_settings() {
    EnvelopeSettings *settings = enabled_settings_;

    *settings++ = ENV_SETTING_TYPE;
    *settings++ = ENV_SETTING_TRIGGER_INPUT;
    *settings++ = ENV_SETTING_TRIGGER_DELAY_MODE;
    if (get_trigger_delay_mode()) {
      *settings++ = ENV_SETTING_TRIGGER_DELAY_COUNT;
      *settings++ = ENV_SETTING_TRIGGER_DELAY_MILLISECONDS;
      *settings++ = ENV_SETTING_TRIGGER_DELAY_SECONDS;
    }
    
    *settings++ = ENV_SETTING_EUCLIDEAN_LENGTH;
    if (get_euclidean_length()) {
      //*settings++ = ENV_SETTING_EUCLIDEAN_FILL;
      *settings++ = ENV_SETTING_EUCLIDEAN_OFFSET;
      *settings++ = ENV_SETTING_EUCLIDEAN_RESET_INPUT;
      *settings++ = ENV_SETTING_EUCLIDEAN_RESET_CLOCK_DIV;
    }

    *settings++ = ENV_SETTING_ATTACK_SHAPE;
    *settings++ = ENV_SETTING_DECAY_SHAPE;
    *settings++ = ENV_SETTING_RELEASE_SHAPE;

    *settings++ = ENV_SETTING_ATTACK_TIME_MULTIPLIER;
    *settings++ = ENV_SETTING_DECAY_TIME_MULTIPLIER;
    *settings++ = ENV_SETTING_RELEASE_TIME_MULTIPLIER;

    *settings++ = ENV_SETTING_CV1;
    *settings++ = ENV_SETTING_CV2;
    *settings++ = ENV_SETTING_CV3;
    *settings++ = ENV_SETTING_CV4;
    *settings++ = ENV_SETTING_ATTACK_RESET_BEHAVIOUR;
    *settings++ = ENV_SETTING_ATTACK_FALLING_GATE_BEHAVIOUR;
    *settings++ = ENV_SETTING_DECAY_RELEASE_RESET_BEHAVIOUR;
    *settings++ = ENV_SETTING_GATE_HIGH;
    *settings++ = ENV_SETTING_AMPLITUDE;
    *settings++ = ENV_SETTING_SAMPLED_AMPLITUDE;
    *settings++ = ENV_SETTING_MAX_LOOPS;
    *settings++ = ENV_SETTING_INVERTED;

    num_enabled_settings_ = settings - enabled_settings_;
  }

  static bool indentSetting(EnvelopeSettings setting) {
    switch (setting) {
      case ENV_SETTING_TRIGGER_DELAY_COUNT:
      case ENV_SETTING_TRIGGER_DELAY_SECONDS:
      case ENV_SETTING_TRIGGER_DELAY_MILLISECONDS:
      case ENV_SETTING_EUCLIDEAN_FILL:
      case ENV_SETTING_EUCLIDEAN_OFFSET:
      case ENV_SETTING_EUCLIDEAN_RESET_INPUT:
      case ENV_SETTING_EUCLIDEAN_RESET_CLOCK_DIV:
        return true;
      default:
      break;
    }
    return false;
  }

  void Update(OC::IOFrame *ioframe, uint32_t triggers, uint32_t internal_trigger_mask, const int32_t cvs[ADC_CHANNEL_LAST], DAC_CHANNEL dac_channel) {
    int32_t s[CV_MAPPING_LAST];
    s[CV_MAPPING_NONE] = 0; // unused, but needs a placeholder to align with enum CVMapping
    s[CV_MAPPING_SEG1] = SCALE8_16(static_cast<int32_t>(get_segment_value(0)));
    s[CV_MAPPING_SEG2] = SCALE8_16(static_cast<int32_t>(get_segment_value(1)));
    s[CV_MAPPING_SEG3] = SCALE8_16(static_cast<int32_t>(get_segment_value(2)));
    s[CV_MAPPING_SEG4] = SCALE8_16(static_cast<int32_t>(get_segment_value(3)));
    s[CV_MAPPING_ADR] = 0; // unused, but needs a placeholder to align with enum CVMapping
    s[CV_MAPPING_EUCLIDEAN_LENGTH] = static_cast<int32_t>(get_euclidean_length());
    s[CV_MAPPING_EUCLIDEAN_FILL] = static_cast<int32_t>(get_euclidean_fill());
    s[CV_MAPPING_EUCLIDEAN_OFFSET] = static_cast<int32_t>(get_euclidean_offset());
    s[CV_MAPPING_DELAY_MSEC] = get_trigger_delay_ms();
    s[CV_MAPPING_AMPLITUDE] = get_amplitude();
    s[CV_MAPPING_MAX_LOOPS] = get_max_loops();

    apply_cv_mapping(ENV_SETTING_CV1, cvs, s);
    apply_cv_mapping(ENV_SETTING_CV2, cvs, s);
    apply_cv_mapping(ENV_SETTING_CV3, cvs, s);
    apply_cv_mapping(ENV_SETTING_CV4, cvs, s);

    s[CV_MAPPING_SEG1] = USAT16(s[CV_MAPPING_SEG1]);
    s[CV_MAPPING_SEG2] = USAT16(s[CV_MAPPING_SEG2]);
    s[CV_MAPPING_SEG3] = USAT16(s[CV_MAPPING_SEG3]);
    s[CV_MAPPING_SEG4] = USAT16(s[CV_MAPPING_SEG4]);
    CONSTRAIN(s[CV_MAPPING_EUCLIDEAN_LENGTH], 0, 31);
    CONSTRAIN(s[CV_MAPPING_EUCLIDEAN_FILL], 0, 32);
    CONSTRAIN(s[CV_MAPPING_EUCLIDEAN_OFFSET], 0, 32);
    CONSTRAIN(s[CV_MAPPING_DELAY_MSEC], 0, 65535);
    CONSTRAIN(s[CV_MAPPING_AMPLITUDE], 0, 65535);
    CONSTRAIN(s[CV_MAPPING_MAX_LOOPS], 0, 65535);

    EnvelopeType type = get_type();
    switch (type) {
      case ENV_TYPE_AD: env_.set_ad(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], 0, 0); break;
      case ENV_TYPE_ADSR: env_.set_adsr(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4]); break;
      case ENV_TYPE_ADR: env_.set_adr(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4], 0, 0 ); break;
      case ENV_TYPE_AR: env_.set_ar(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2]); break;
      case ENV_TYPE_ADSAR: env_.set_adsar(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4]); break;
      case ENV_TYPE_ADAR: env_.set_adar(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4], 0, 0); break;
      case ENV_TYPE_ADL2: env_.set_ad(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], 0, 2); break;
      case ENV_TYPE_ADRL3: env_.set_adr(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4], 0, 3); break;
      case ENV_TYPE_ADL2R: env_.set_adr(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4], 0, 2); break;
      case ENV_TYPE_ADARL4: env_.set_adar(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4], 0, 4); break;
      case ENV_TYPE_ADAL2R: env_.set_adar(s[CV_MAPPING_SEG1], s[CV_MAPPING_SEG2], s[CV_MAPPING_SEG3]>>1, s[CV_MAPPING_SEG4], 1, 3); break; // was 2, 4
      default:
      break;
    }

    // set the amplitude
    env_.set_amplitude(s[CV_MAPPING_AMPLITUDE], is_amplitude_sampled()) ;
    
    if (type != last_type_) {
      last_type_ = type;
      env_.reset();
    }

    // set the specified reset behaviours
    env_.set_attack_reset_behaviour(get_attack_reset_behaviour());
    env_.set_attack_falling_gate_behaviour(get_attack_falling_gate_behaviour());
    env_.set_decay_release_reset_behaviour(get_decay_release_reset_behaviour());

    // set the envelope segment shapes
    env_.set_attack_shape(get_attack_shape());
    env_.set_decay_shape(get_decay_shape());
    env_.set_release_shape(get_release_shape());

    // set the envelope segment time multipliers
    env_.set_attack_time_multiplier(get_attack_time_multiplier());
    env_.set_decay_time_multiplier(get_decay_time_multiplier());
    env_.set_release_time_multiplier(get_release_time_multiplier());

    // set the looping envelope maximum number of loops
    env_.set_max_loops(s[CV_MAPPING_MAX_LOOPS]);

    int trigger_input = get_trigger_input();
    bool triggered = false;
    bool gate_raised = false;
    if (trigger_input < OC::DIGITAL_INPUT_LAST) {
      triggered = triggers & DIGITAL_INPUT_MASK(trigger_input);
      gate_raised = OC::DigitalInputs::read_immediate(static_cast<OC::DigitalInput>(trigger_input));
    } else {
      const int trigger_channel = TriggerSettingToChannel(trigger_input);
      const IntTriggerType trigger_type = TriggerSettingToType(trigger_input, trigger_channel);
  
      triggered = (internal_trigger_mask >> (trigger_setting_to_channel_index(trigger_channel) * 8)) & (0x1 << trigger_type);
      gate_raised = triggered;
    }

    trigger_display_.Update(1, triggered || gate_raised_);

    if (triggered) ++euclidean_counter_;
    uint8_t euclidean_length = static_cast<uint8_t>(s[CV_MAPPING_EUCLIDEAN_LENGTH]);
    uint8_t euclidean_fill = static_cast<uint8_t>(s[CV_MAPPING_EUCLIDEAN_FILL]);
    uint8_t euclidean_offset = static_cast<uint8_t>(s[CV_MAPPING_EUCLIDEAN_OFFSET]);

    // Process Euclidean pattern reset
    uint8_t euclidean_reset_trigger_input = get_euclidean_reset_trigger_input();
    if (euclidean_reset_trigger_input) {
      if (triggers & DIGITAL_INPUT_MASK(static_cast<OC::DigitalInput>(euclidean_reset_trigger_input - 1))) {
        ++euclidean_reset_counter_;
        if (euclidean_reset_counter_ >= get_euclidean_reset_clock_div()) {
          euclidean_counter_ = 0;
          euclidean_reset_counter_= 0;
        }
      }
    }

    if (triggered && get_euclidean_length() && !EuclideanFilter(euclidean_length, euclidean_fill, euclidean_offset, euclidean_counter_)) {
      triggered = false;
    }

    s_euclidean_length_ = euclidean_length;
    s_euclidean_fill_ = euclidean_fill;
    s_euclidean_offset_ = euclidean_offset;
      
    if (triggered) {
      TriggerDelayMode delay_mode = get_trigger_delay_mode();
      // uint32_t delay = get_trigger_delay_ms() * 1000U;
      uint32_t delay = static_cast<uint32_t>(s[CV_MAPPING_DELAY_MSEC] * 1000U);
      if (delay_mode && delay) {
        triggered = false;
        if (TRIGGER_DELAY_QUEUE == delay_mode) {
          if (delayed_triggers_free_ < get_trigger_delay_count())
            delayed_triggers_[delayed_triggers_free_].Activate(delay);
        } else { // TRIGGER_DELAY_RING
          // Assume these are mostly in order, so the "next" is also the oldest
          if (delayed_triggers_free_ < get_trigger_delay_count())
            delayed_triggers_[delayed_triggers_free_].Activate(delay);
          else
            delayed_triggers_[delayed_triggers_next_].Activate(delay);
        }
      }
    }

    if (DelayedTriggers())
      triggered = true;

    uint8_t gate_state = 0;
    if (triggered)
      gate_state |= peaks::CONTROL_GATE_RISING;
 
    if (gate_raised || get_gate_high())
      gate_state |= peaks::CONTROL_GATE;
    else if (gate_raised_)
      gate_state |= peaks::CONTROL_GATE_FALLING;
    gate_raised_ = gate_raised;

    uint32_t value = env_.ProcessSingleSample(gate_state); // 0 to 32767
    if (is_inverted()) value = 32767 - value;
    const int max_val = OC::DAC::MAX_VALUE;

    // Scale range and offset
#ifdef VOR
    // Full range for Plum Audio
    const uint32_t offset = OC::DAC::get_octave_offset(dac_channel, -OC::DAC::kOctaveZero);
#else
    // Regular O_C settles to 0V
    const uint32_t offset = OC::DAC::get_zero_offset(dac_channel);
#endif

    // scale value
    value = offset + (value * (max_val - offset) / 32767);

    ioframe->outputs.set_unipolar_value(dac_channel, value);
  }

  uint16_t RenderPreview(int16_t *values, uint16_t *segment_start_points, uint16_t *loop_points, uint16_t &current_phase) const {
    return env_.RenderPreview(values, segment_start_points, loop_points, current_phase);
  }

  uint16_t RenderFastPreview(int16_t *values) const {
    return env_.RenderFastPreview(values);
  }

  uint8_t getTriggerState() const {
    return trigger_display_.getState();
  }

  inline void get_next_trigger(DelayedTrigger &trigger) const {
    trigger = delayed_triggers_[delayed_triggers_next_];
  }

#ifdef ENVGEN_DEBUG
  inline uint16_t get_amplitude_value() const {
    return(env_.get_amplitude_value()) ;
  }

  inline uint16_t get_sampled_amplitude_value() const {
    return(env_.get_sampled_amplitude_value()) ;
  }

  inline bool get_is_amplitude_sampled() const {
    return(env_.get_is_amplitude_sampled()) ;
  }
#endif

  inline int trigger_setting_to_channel_index(int s) const {
    return s < channel_index_ ? s : s + 1;
  }

  uint32_t internal_trigger_mask() const {
    return env_.get_state_mask();
  }

private:

  int channel_index_;
 
  peaks::MultistageEnvelope env_;
  EnvelopeType last_type_;
  bool gate_raised_;
  uint32_t euclidean_counter_;
  uint32_t euclidean_reset_counter_;

  // debug/live-view only
  uint8_t s_euclidean_length_;
  uint8_t s_euclidean_fill_;
  uint8_t s_euclidean_offset_;  
  
  DelayedTrigger delayed_triggers_[kMaxDelayedTriggers];
  size_t delayed_triggers_free_;
  size_t delayed_triggers_next_;

  int num_enabled_settings_;
  EnvelopeSettings enabled_settings_[ENV_SETTING_LAST];

  OC::DigitalInputDisplay trigger_display_;

  bool DelayedTriggers() {
    bool triggered = false;

    delayed_triggers_free_ = kMaxDelayedTriggers;
    delayed_triggers_next_ = 0;
    uint32_t min_time_left = -1;

    size_t i = kMaxDelayedTriggers;
    while (i--) {
      DelayedTrigger &trigger = delayed_triggers_[i];
      uint32_t time_left = trigger.time_left;
      if (time_left) {
        if (time_left > OC_CORE_TIMER_RATE) {
          time_left -= OC_CORE_TIMER_RATE;
          if (time_left < min_time_left) {
            min_time_left = time_left;
            delayed_triggers_next_ = i;
          }
          trigger.time_left = time_left;
        } else {
          trigger.Reset();
          delayed_triggers_free_ = i;
          triggered = true;
        }
      } else {
        delayed_triggers_free_ = i;
      }
    }

    return triggered;
  }

  // TOTAL EEPROM SIZE: 4 * 30 bytes
  SETTINGS_ARRAY_DECLARE() {{
    { ENV_TYPE_AD, ENV_TYPE_FIRST, ENV_TYPE_LAST-1, "TYPE", envelope_types, settings::STORAGE_TYPE_U8 },
    { 128, 0, 255, "S1", NULL, settings::STORAGE_TYPE_U16 }, // u16 in case resolution proves insufficent
    { 128, 0, 255, "S2", NULL, settings::STORAGE_TYPE_U16 },
    { 128, 0, 255, "S3", NULL, settings::STORAGE_TYPE_U16 },
    { 128, 0, 255, "S4", NULL, settings::STORAGE_TYPE_U16 },
    { OC::DIGITAL_INPUT_1, OC::DIGITAL_INPUT_1, OC::DIGITAL_INPUT_4 + 3 * INT_TRIGGER_LAST, "Trigger input", OC::Strings::trigger_input_names, settings::STORAGE_TYPE_U4 },
    { TRIGGER_DELAY_OFF, TRIGGER_DELAY_OFF, TRIGGER_DELAY_LAST - 1, "Tr delay mode", trigger_delay_modes, settings::STORAGE_TYPE_U4 },
    { 1, 1, EnvelopeGenerator::kMaxDelayedTriggers, "Tr delay count", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 999, "Tr delay msecs", NULL, settings::STORAGE_TYPE_U16 },
    { 0, 0, 64, "Tr delay secs", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 31, "Eucl length", euclidean_lengths, settings::STORAGE_TYPE_U8 },
    { 1, 0, 32, "Fill", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 32, "Offset", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 4, "Eucl reset", OC::Strings::trigger_input_names_none, settings::STORAGE_TYPE_U8 },
    { 1, 1, 255, "Eucl reset div", NULL, settings::STORAGE_TYPE_U8 },
    { CV_MAPPING_NONE, CV_MAPPING_NONE, CV_MAPPING_LAST - 1, "CV1 -> ", cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { CV_MAPPING_NONE, CV_MAPPING_NONE, CV_MAPPING_LAST - 1, "CV2 -> ", cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { CV_MAPPING_NONE, CV_MAPPING_NONE, CV_MAPPING_LAST - 1, "CV3 -> ", cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { CV_MAPPING_NONE, CV_MAPPING_NONE, CV_MAPPING_LAST - 1, "CV4 -> ", cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { peaks::RESET_BEHAVIOUR_NULL, peaks::RESET_BEHAVIOUR_NULL, peaks::RESET_BEHAVIOUR_LAST - 1, "Attack reset", OC::Strings::reset_behaviours, settings::STORAGE_TYPE_U4 },
    { peaks::FALLING_GATE_BEHAVIOUR_IGNORE, peaks::FALLING_GATE_BEHAVIOUR_IGNORE, peaks::FALLING_GATE_BEHAVIOUR_LAST - 1, "Att fall gt", OC::Strings::falling_gate_behaviours, settings::STORAGE_TYPE_U8 },
    { peaks::RESET_BEHAVIOUR_SEGMENT_PHASE, peaks::RESET_BEHAVIOUR_NULL, peaks::RESET_BEHAVIOUR_LAST - 1, "DecRel reset", OC::Strings::reset_behaviours, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "Gate high", OC::Strings::no_yes, settings::STORAGE_TYPE_U4 },
    { peaks::ENV_SHAPE_QUARTIC, peaks::ENV_SHAPE_LINEAR, peaks::ENV_SHAPE_LAST - 1, "Attack shape", OC::Strings::envelope_shapes, settings::STORAGE_TYPE_U4 },
    { peaks::ENV_SHAPE_EXPONENTIAL, peaks::ENV_SHAPE_LINEAR, peaks::ENV_SHAPE_LAST - 1, "Decay shape", OC::Strings::envelope_shapes, settings::STORAGE_TYPE_U4 },
    { peaks::ENV_SHAPE_EXPONENTIAL, peaks::ENV_SHAPE_LINEAR, peaks::ENV_SHAPE_LAST - 1, "Release shape", OC::Strings::envelope_shapes, settings::STORAGE_TYPE_U4 },
    { 0, 0, 13, "Attack mult", time_multipliers, settings::STORAGE_TYPE_U4 },
    { 0, 0, 13, "Decay mult", time_multipliers, settings::STORAGE_TYPE_U4 },
    {0, 0, 13, "Release mult", time_multipliers, settings::STORAGE_TYPE_U4 },
    {127, 0, 127, "Amplitude", NULL, settings::STORAGE_TYPE_U8 },
    {0, 0, 1, "Sampled Ampl", OC::Strings::no_yes, settings::STORAGE_TYPE_U4 },
    {0, 0, 127, "Max loops", NULL, settings::STORAGE_TYPE_U8 },
    {0, 0, 1, "Inverted", OC::Strings::no_yes, settings::STORAGE_TYPE_U8 },
  }};
};
SETTINGS_ARRAY_DEFINE(EnvelopeGenerator);

void EnvelopeGenerator::Init(OC::DigitalInput default_trigger) {
  InitDefaults();
  apply_value(ENV_SETTING_TRIGGER_INPUT, default_trigger);
  env_.Init();
  channel_index_ = default_trigger;
  last_type_ = ENV_TYPE_LAST;
  gate_raised_ = false;
  euclidean_counter_ = 0;
  euclidean_reset_counter_ = 0;
  
  memset(delayed_triggers_, 0, sizeof(delayed_triggers_));
  delayed_triggers_free_ = delayed_triggers_next_ = 0;

  trigger_display_.Init();

  update_enabled_settings();
}

namespace OC {

OC_APP_TRAITS(AppQuadEnvelopeGenerator, TWOCCS("EG"), "Piqued", "4x EG");
class OC_APP_CLASS(AppQuadEnvelopeGenerator) {
public:
  OC_APP_INTERFACE_DECLARE(AppQuadEnvelopeGenerator);
  OC_APP_STORAGE_SIZE(4 * EnvelopeGenerator::storageSize());

private:
  static constexpr int32_t kCvSmoothing = 16;

  bool euclidean_edit_active() const {
    return
        ui.cursor.editing() &&
        ENV_SETTING_EUCLIDEAN_OFFSET == selected().enabled_setting_at(ui.cursor.cursor_pos());
  }

  enum EnvEditMode {
    MODE_EDIT_SEGMENTS,
    MODE_EDIT_SETTINGS
  };

  struct {
    EnvEditMode edit_mode;

    int selected_channel;
    int selected_segment;
    bool segment_editing;

    menu::ScreenCursor<menu::kScreenLines> cursor;
    OC::EuclideanMaskDraw euclidean_mask_draw;
    bool euclidean_edit_length;
  } ui;

  EnvelopeGenerator &selected() {
    return envelopes_[ui.selected_channel];
  }

  const EnvelopeGenerator &selected() const {
    return envelopes_[ui.selected_channel];
  }

  EnvelopeGenerator envelopes_[4];

  SmoothedValue<int32_t, kCvSmoothing> cv1;
  SmoothedValue<int32_t, kCvSmoothing> cv2;
  SmoothedValue<int32_t, kCvSmoothing> cv3;
  SmoothedValue<int32_t, kCvSmoothing> cv4;

  void HandleTopButton();
  void HandleLowerButton();
  void HandleRightButton();
  void HandleLeftButton();

  void DrawMenuPreview() const;
  void DrawMenuSettings() const;
};

void AppQuadEnvelopeGenerator::Init() {
  int input = OC::DIGITAL_INPUT_1;
  for (auto &env : envelopes_) {
    env.Init(static_cast<OC::DigitalInput>(input));
    ++input;
  }

  ui.edit_mode = MODE_EDIT_SEGMENTS;
  ui.selected_channel = 0;
  ui.selected_segment = 0;
  ui.segment_editing = false;
  ui.cursor.Init(0, envelopes_[0].num_enabled_settings() - 1);
  ui.euclidean_edit_length = false;
}

void AppQuadEnvelopeGenerator::Process(OC::IOFrame *ioframe) {
  // TODO[PLD] Do we need the excessive smoothing?
#ifdef ARDUINO_TEENSY41
  cv1.push(ioframe->cv.values[ADC_CHANNEL_5]);
  cv2.push(ioframe->cv.values[ADC_CHANNEL_6]);
  cv3.push(ioframe->cv.values[ADC_CHANNEL_7]);
  cv4.push(ioframe->cv.values[ADC_CHANNEL_8]);
#else
  cv1.push(ioframe->cv.values[ADC_CHANNEL_1]);
  cv2.push(ioframe->cv.values[ADC_CHANNEL_2]);
  cv3.push(ioframe->cv.values[ADC_CHANNEL_3]);
  cv4.push(ioframe->cv.values[ADC_CHANNEL_4]);
#endif

  const int32_t cvs[ADC_CHANNEL_LAST] = { cv1.value(), cv2.value(), cv3.value(), cv4.value() };
  uint32_t triggers = ioframe->digital_inputs.triggered();

  uint32_t internal_trigger_mask =
      envelopes_[0].internal_trigger_mask() |
      envelopes_[1].internal_trigger_mask() << 8 |
      envelopes_[2].internal_trigger_mask() << 16 |
      envelopes_[3].internal_trigger_mask() << 24;

  envelopes_[0].Update(ioframe, triggers, internal_trigger_mask, cvs, DAC_CHANNEL_A);
  envelopes_[1].Update(ioframe, triggers, internal_trigger_mask, cvs, DAC_CHANNEL_B);
  envelopes_[2].Update(ioframe, triggers, internal_trigger_mask, cvs, DAC_CHANNEL_C);
  envelopes_[3].Update(ioframe, triggers, internal_trigger_mask, cvs, DAC_CHANNEL_D);
}

size_t AppQuadEnvelopeGenerator::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  for (auto &env : envelopes_)
    env.Save(stream_buffer);

  return stream_buffer.written();
}

size_t AppQuadEnvelopeGenerator::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  for (auto &env : envelopes_) {
    env.Restore(stream_buffer);
    env.update_enabled_settings();
  }

  ui.cursor.AdjustEnd(envelopes_[0].num_enabled_settings() - 1);
  return stream_buffer.read();
}

void AppQuadEnvelopeGenerator::HandleAppEvent(AppEvent event) {
  switch (event) {
    case APP_EVENT_RESUME:
      break;
    case APP_EVENT_SUSPEND:
    case APP_EVENT_SCREENSAVER_ON:
    case APP_EVENT_SCREENSAVER_OFF:
      break;
  }
}

void AppQuadEnvelopeGenerator::Loop() {
}

static constexpr weegfx::coord_t kPreviewH = 32;
static constexpr weegfx::coord_t kPreviewTopY = 32;
static constexpr weegfx::coord_t kPreviewBottomY = 32 + kPreviewH - 1;

static constexpr weegfx::coord_t kLoopMarkerY = 28;
static constexpr weegfx::coord_t kCurrentSegmentCursorY = 26;

int16_t preview_values[128 + 64];
uint16_t preview_segment_starts[peaks::kMaxNumSegments];
uint16_t preview_loop_points[peaks::kMaxNumSegments];
static constexpr uint16_t kPreviewTerminator = 0xffff;

settings::ValueAttributes segment_editing_attr = { 128, 0, 255, "DOH!", NULL, settings::STORAGE_TYPE_U16 };

void AppQuadEnvelopeGenerator::DrawMenuPreview() const {
  auto const &env = selected();

  menu::SettingsListItem list_item;
  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX>::AbsoluteLine(0, list_item);
  list_item.selected = false;
  list_item.editing = ui.segment_editing;
  const int selected_segment = ui.selected_segment;

  segment_editing_attr.name = segment_names[selected_segment];
  list_item.DrawDefault(env.get_segment_value(selected_segment), segment_editing_attr);

  // Current envelope shape
  uint16_t current_phase = 0;
  weegfx::coord_t x = 0;
  weegfx::coord_t w = env.RenderPreview(preview_values, preview_segment_starts, preview_loop_points, current_phase);
  const int16_t *data = preview_values;
  while (x <= static_cast<weegfx::coord_t>(current_phase)) {
    const int16_t value = *data++ >> 10;
    graphics.drawVLine(x++, kPreviewBottomY - value, value + 1);
  }

  while (x < w) {
    const int16_t value = *data++ >> 10;
    graphics.setPixel(x++, kPreviewBottomY - value);
  }

  if (x < menu::kDisplayWidth)
    graphics.drawHLine(x, kPreviewBottomY, menu::kDisplayWidth - x);

  // Minimal cursor thang (x is end of preview)
  weegfx::coord_t start = preview_segment_starts[selected_segment];
  weegfx::coord_t end = preview_segment_starts[selected_segment + 1];
  w = kPreviewTerminator == end ? x - start + 1 : end - start + 1;
  if (w < 4) w = 4;
  graphics.drawRect(start, kCurrentSegmentCursorY, w, 2);

  // Current types only loop over full envelope, so just pixel dust
  uint16_t *loop_points = preview_loop_points;
  uint_fast8_t i = 0;
  while (*loop_points != kPreviewTerminator) {
    // odd: end marker, even: start marker
    if (i++ & 1)
      graphics.drawBitmap8(*loop_points++ - 1, kLoopMarkerY, OC::kBitmapLoopMarkerW, OC::bitmap_loop_markers_8 + OC::kBitmapLoopMarkerW);
    else
      graphics.drawBitmap8(*loop_points++, kLoopMarkerY, OC::kBitmapLoopMarkerW, OC::bitmap_loop_markers_8);
  }

  // Brute-force way of handling "pathological" cases where A/D has no visible
  // pixels instead of line-drawing between points
  uint16_t *segment_start = preview_segment_starts;
  while (*segment_start != kPreviewTerminator) {
    weegfx::coord_t x = *segment_start++;
    weegfx::coord_t value = preview_values[x] >> 10;
    graphics.drawVLine(x, kPreviewBottomY - value, value);
  }
}

void AppQuadEnvelopeGenerator::DrawMenuSettings() const {
  auto const &env = selected();

  bool draw_euclidean_editor = false;

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(ui.cursor);
  menu::SettingsListItem list_item;

  while (settings_list.available()) {
    const int setting = 
      env.enabled_setting_at(settings_list.Next(list_item));
    const int value = env.get_value(setting);
    const auto &attr = EnvelopeGenerator::value_attributes(setting);

    switch (setting) {
      case ENV_SETTING_TYPE:
        list_item.SetPrintPos();
        if (list_item.editing) {
          menu::DrawEditIcon(6, list_item.y, value, attr);
          graphics.movePrintPos(6, 0);
        }
        graphics.print(attr.value_names[value]);
        list_item.DrawCustom();
      break;
      case ENV_SETTING_TRIGGER_INPUT:
        if (EnvelopeGenerator::indentSetting(static_cast<EnvelopeSettings>(setting)))
          list_item.x += menu::kIndentDx;
        if (value < OC::DIGITAL_INPUT_LAST) {
          list_item.DrawDefault(value, attr);
        } else {
          const int trigger_channel = TriggerSettingToChannel(value);
          const IntTriggerType trigger_type = TriggerSettingToType(value, trigger_channel);

          char s[6] = "_ xxx";
          s[0] = 'A' + env.trigger_setting_to_channel_index(trigger_channel);
          memcpy(s + 2, internal_trigger_types[trigger_type], 3);
          list_item.DrawDefault(s, value, attr);
        }
      break;
      case ENV_SETTING_EUCLIDEAN_OFFSET:
        if (!list_item.editing) {
          // Use the live values
          ui.euclidean_mask_draw.Render(menu::kDisplayWidth, list_item.y,
                                               env.get_s_euclidean_length(), env.get_s_euclidean_fill(), env.get_s_euclidean_offset(),
                                               env.get_euclidean_counter());
          list_item.DrawCustom();
        } else {
          draw_euclidean_editor = true;
        }
      break;

      default:
        if (EnvelopeGenerator::indentSetting(static_cast<EnvelopeSettings>(setting)))
          list_item.x += menu::kIndentDx;
        list_item.DrawDefault(value, attr);
      break;
    }
  }

  // Ugly. With a capital blargh.
  if (draw_euclidean_editor) {
    weegfx::coord_t y = 32 - menu::kMenuLineH / 2 - 1;
    graphics.clearRect(0, y, menu::kDisplayWidth, menu::kMenuLineH * 2 + 2);
    graphics.drawFrame(0, y, menu::kDisplayWidth, menu::kMenuLineH * 2 + 2);

    y += 2;
    ui.euclidean_mask_draw.Render(menu::kDisplayWidth - 2, y,
                                          env.get_euclidean_length(), env.get_euclidean_fill(), env.get_euclidean_offset(),
                                          env.get_euclidean_counter());

    y += menu::kMenuLineH;
    menu::SettingsListItem list_item;
    list_item.selected = false;
    list_item.editing = true;
    list_item.y = y;

    list_item.x = 1;
    list_item.valuex = 38;
    list_item.endx = 60 - 2;
    if (ui.euclidean_edit_length) {
      auto attr = EnvelopeGenerator::value_attributes(ENV_SETTING_EUCLIDEAN_LENGTH);
      attr.min_ = 1;
      attr.name = "Len";
      list_item.DrawDefault(env.get_euclidean_length(), attr);
    } else {
      auto attr = EnvelopeGenerator::value_attributes(ENV_SETTING_EUCLIDEAN_FILL);
      list_item.DrawValueMax(env.get_euclidean_fill(), attr, env.get_euclidean_length() + 0x1);
    }

    list_item.editing = true;
    list_item.x = 60;
    list_item.valuex = 106;
    list_item.endx = menu::kDisplayWidth - 2;
    list_item.DrawValueMax(env.get_euclidean_offset(), EnvelopeGenerator::value_attributes(ENV_SETTING_EUCLIDEAN_OFFSET), env.get_euclidean_length());
  }
}

void AppQuadEnvelopeGenerator::DrawMenu() const {

  menu::QuadTitleBar::Draw();
  for (uint_fast8_t i = 0; i < 4; ++i) {
    menu::QuadTitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
    menu::QuadTitleBar::DrawGateIndicator(i, envelopes_[i].getTriggerState());

    EnvelopeGenerator::DelayedTrigger trigger;
    envelopes_[i].get_next_trigger(trigger);
    if (trigger.delay) {
      weegfx::coord_t x = menu::QuadTitleBar::ColumnStartX(i) + 28;
      weegfx::coord_t h = (trigger.time_left * 8) / trigger.delay;
      graphics.drawRect(x, menu::QuadTitleBar::kTextY + 7 - h, 2, 1 + h);
    }
  }
  // If settings mode, draw level in title bar?
  menu::QuadTitleBar::Selected(ui.selected_channel);

  if (AppQuadEnvelopeGenerator::MODE_EDIT_SEGMENTS == ui.edit_mode)
    DrawMenuPreview();
  else
    DrawMenuSettings();
}

void AppQuadEnvelopeGenerator::HandleTopButton() {
  auto &selected_env = selected();
  selected_env.change_value(ENV_SETTING_SEG1_VALUE + ui.selected_segment, 32);
}

void AppQuadEnvelopeGenerator::HandleLowerButton() {
  auto &selected_env = selected();
  selected_env.change_value(ENV_SETTING_SEG1_VALUE + ui.selected_segment, -32);
}

void AppQuadEnvelopeGenerator::HandleRightButton() {

  if (AppQuadEnvelopeGenerator::MODE_EDIT_SEGMENTS == ui.edit_mode) {
    ui.segment_editing = !ui.segment_editing;
  } else {
    ui.cursor.toggle_editing();
    ui.euclidean_edit_length = false;
  }
}

void AppQuadEnvelopeGenerator::HandleLeftButton() {
  if (AppQuadEnvelopeGenerator::MODE_EDIT_SETTINGS == ui.edit_mode) {
    if (!euclidean_edit_active()) {
      ui.edit_mode = AppQuadEnvelopeGenerator::MODE_EDIT_SEGMENTS;
      ui.cursor.set_editing(false);
    } else {
      ui.euclidean_edit_length = !ui.euclidean_edit_length;
    }
  } else {
    ui.edit_mode = AppQuadEnvelopeGenerator::MODE_EDIT_SETTINGS;
    ui.segment_editing = false;
  }
}

void AppQuadEnvelopeGenerator::HandleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        HandleTopButton();
        break;
      case OC::CONTROL_BUTTON_DOWN:
        HandleLowerButton();
        break;
      case OC::CONTROL_BUTTON_L:
        HandleLeftButton();
        break;
      case OC::CONTROL_BUTTON_R:
        HandleRightButton();
        break;
    }
  }
}

void AppQuadEnvelopeGenerator::HandleEncoderEvent(const UI::Event &event) {

  if (OC::CONTROL_ENCODER_L == event.control) {
    if (euclidean_edit_active()) {
      if (ui.euclidean_edit_length) {
        // Artificially constrain length here
        int length = selected().get_euclidean_length() + event.value;
        if (length > 0) {
          selected().apply_value(ENV_SETTING_EUCLIDEAN_LENGTH, length);
          // constrain k, offset:
          if (length < selected().get_euclidean_fill())
             selected().apply_value(ENV_SETTING_EUCLIDEAN_FILL, length + 0x1);
          if (length < selected().get_euclidean_offset())
            selected().apply_value(ENV_SETTING_EUCLIDEAN_OFFSET, length);
        }
      } else {
        // constrain k:
        if (selected().get_euclidean_fill() <= selected().get_euclidean_length())
          selected().change_value(ENV_SETTING_EUCLIDEAN_FILL, event.value);
        else if (event.value < 0)
          selected().change_value(ENV_SETTING_EUCLIDEAN_FILL, event.value);
      }
    } else {
      int left_value = ui.selected_channel + event.value;
      CONSTRAIN(left_value, 0, 3);
      ui.selected_channel = left_value;
      auto &selected_env = selected();
      CONSTRAIN(ui.selected_segment, 0, selected_env.num_editable_segments() - 1);
      ui.cursor.AdjustEnd(selected_env.num_enabled_settings() - 1);
    }
  } else if (OC::CONTROL_ENCODER_R == event.control) {
    if (AppQuadEnvelopeGenerator::MODE_EDIT_SEGMENTS == ui.edit_mode) {
      auto &selected_env = selected();
      if (ui.segment_editing) {
        selected_env.change_value(ENV_SETTING_SEG1_VALUE + ui.selected_segment, event.value);
      } else {
        int selected_segment = ui.selected_segment + event.value;
        CONSTRAIN(selected_segment, 0, selected_env.num_editable_segments() - 1);
        ui.selected_segment = selected_segment;
      }
    } else {
      if (ui.cursor.editing()) {
        auto &selected_env = selected();
        EnvelopeSettings setting = selected_env.enabled_setting_at(ui.cursor.cursor_pos());

        if (ENV_SETTING_EUCLIDEAN_OFFSET == setting) {
          // constrain offset 
          if (selected_env.get_euclidean_offset() < selected_env.get_euclidean_length())
            selected_env.change_value(ENV_SETTING_EUCLIDEAN_OFFSET, event.value);
          else if (event.value < 0)
            selected_env.change_value(ENV_SETTING_EUCLIDEAN_OFFSET, event.value);
        }
        else {
           selected_env.change_value(setting, event.value);
        }
        
        if (ENV_SETTING_TRIGGER_DELAY_MODE == setting || ENV_SETTING_EUCLIDEAN_LENGTH == setting)
          selected_env.update_enabled_settings();
          ui.cursor.AdjustEnd(selected_env.num_enabled_settings() - 1);
      } else {
        ui.cursor.Scroll(event.value);
      }
    }
  }
}

int16_t fast_preview_values[peaks::kFastPreviewWidth + 32];

template <weegfx::coord_t startx, weegfx::coord_t y>
void RenderFastPreview(const EnvelopeGenerator &envelope) {
  uint16_t w = envelope.RenderFastPreview(fast_preview_values);
  CONSTRAIN(w, 0, peaks::kFastPreviewWidth); // Just-in-case
  weegfx::coord_t x = startx;
  const int16_t *values = fast_preview_values;
  while (w--) {
    const int16_t value = 1 + ((*values++ >> 10) & 0x1f);
    graphics.drawVLine(x++, y + 32 - value, value);
  }
}

void AppQuadEnvelopeGenerator::DrawScreensaver() const {
#ifdef ENVGEN_DEBUG_SCREENSAVER
  debug::CycleMeasurement render_cycles;
#endif

  #ifdef NORTHERNLIGHT
    RenderFastPreview<0, 32>(envelopes_[0]);
    RenderFastPreview<64, 32>(envelopes_[1]);
    RenderFastPreview<0, 0>(envelopes_[2]);
    RenderFastPreview<64, 0>(envelopes_[3]);
  #else
    RenderFastPreview<0, 0>(envelopes_[0]);
    RenderFastPreview<64, 0>(envelopes_[1]);
    RenderFastPreview<0, 32>(envelopes_[2]);
    RenderFastPreview<64, 32>(envelopes_[3]);
  #endif
  OC::scope_render();

#ifdef ENVGEN_DEBUG_SCREENSAVER
  uint32_t us = debug::cycles_to_us(render_cycles.read());
  graphics.setPrintPos(2, 56);
  graphics.printf("%u",  us);
#endif
}

void AppQuadEnvelopeGenerator::DrawDebugInfo() const {
#ifdef ENVGEN_DEBUG
  for (int i = 0; i < 4; ++i) {
    uint8_t ypos = 10*(i + 1) + 2 ;
    graphics.setPrintPos(2, ypos);
    graphics.print(envelopes_[i].get_amplitude_value()) ;
    graphics.setPrintPos(50, ypos);
    graphics.print(envelopes_[i].get_sampled_amplitude_value()) ;
    graphics.setPrintPos(100, ypos);
    graphics.print(envelopes_[i].get_is_amplitude_sampled()) ;
  }
#endif // ENVGEN_DEBUG
}

void AppQuadEnvelopeGenerator::GetIOConfig(OC::IOConfig &ioconfig) const
{
  ioconfig.outputs[DAC_CHANNEL_A].set("CH1", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[DAC_CHANNEL_B].set("CH2", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[DAC_CHANNEL_C].set("CH3", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[DAC_CHANNEL_D].set("CH4", OC::OUTPUT_MODE_UNI);
}

} // namespace OC
