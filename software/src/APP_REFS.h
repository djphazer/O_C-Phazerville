// Copyright (c) 2016 Patrick Dowling, 2017 Max Stadler & Tim Churches
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
// Enhancements: Max Stadler and Tim Churches
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
// Very simple "reference" voltage app (not so simple any more...)

#include <numeric>
#include "OC_options.h"
#include "OC_apps.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "util/util_settings.h"
#include "OC_autotuner.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

static constexpr int32_t kMaxOffsetError = (65536 / 5);
static constexpr double kAaboveMidCtoC0 = 0.03716272234383494188492;

#ifdef FLIP_180
static constexpr DAC_CHANNEL DAC_CHANNEL_FTM = DAC_CHANNEL_A;
#else
static constexpr DAC_CHANNEL DAC_CHANNEL_FTM = DAC_CHANNEL_D;
#endif

enum ReferenceSetting {
  REF_SETTING_OCTAVE,
  REF_SETTING_SEMI,
  REF_SETTING_RANGE,
  REF_SETTING_RATE,
  REF_SETTING_NOTES_OR_BPM,
  REF_SETTING_A_ABOVE_MID_C_INTEGER,
  REF_SETTING_A_ABOVE_MID_C_MANTISSA,
  REF_SETTING_PPQN,
  REF_SETTING_AUTOTUNE,
  REF_SETTING_DUMMY,
  REF_SETTING_LAST
};

enum ChannelPpqn {
  CHANNEL_PPQN_1,
  CHANNEL_PPQN_2,
  CHANNEL_PPQN_4,
  CHANNEL_PPQN_8,
  CHANNEL_PPQN_16,
  CHANNEL_PPQN_24,
  CHANNEL_PPQN_32,
  CHANNEL_PPQN_48,
  CHANNEL_PPQN_64,
  CHANNEL_PPQN_96,
  CHANNEL_PPQN_LAST
};

const char* const notes_or_bpm[2] = {
 "notes",  "bpm", 
};

const char* const ppqn_labels[10] = {
 " 1",  " 2", " 4", " 8", "16", "24", "32", "48", "64", "96",  
};

class ReferenceChannel : public settings::SettingsBase<ReferenceChannel, REF_SETTING_LAST> {
public:

  void Init(DAC_CHANNEL dac_channel) {
    InitDefaults();

    dac_channel_ = dac_channel;
    calibration_data_ = OC::calibration_data.dac.calibrated_octaves[dac_channel_];
    autotune_calibration_data_ = &OC::global_settings.autotune_calibration_data.channels[dac_channel_];

    rate_phase_ = 0;
    mod_offset_ = 0;
    last_pitch_ = 0;

    autotuner_reset();

    update_enabled_settings();
  }

  int get_octave() const {
    return values_[REF_SETTING_OCTAVE];
  }

  DAC_CHANNEL get_channel() const {
    return dac_channel_;
  }

  void ExitAutotune() { }

  int32_t get_semitone() const {
    return values_[REF_SETTING_SEMI];
  }

  int get_range() const {
    return values_[REF_SETTING_RANGE];
  }

  uint32_t get_rate() const {
    return values_[REF_SETTING_RATE];
  }

  uint8_t get_notes_or_bpm() const {
    return values_[REF_SETTING_NOTES_OR_BPM];
  }

  double get_a_above_mid_c() const {
    double mantissa_divisor = 100.0;
    return static_cast<double>(values_[REF_SETTING_A_ABOVE_MID_C_INTEGER]) + (static_cast<double>(values_[REF_SETTING_A_ABOVE_MID_C_MANTISSA])/mantissa_divisor) ;
  }

  uint8_t get_a_above_mid_c_mantissa() const {
    return values_[REF_SETTING_A_ABOVE_MID_C_MANTISSA];
  }

  ChannelPpqn get_channel_ppqn() const {
    return static_cast<ChannelPpqn>(values_[REF_SETTING_PPQN]);
  }

// BEGIN AUTOTUNER

  bool autotuner_active() const {
    return autotuner_;
  }

  bool autotuner_completed() const {
    return autotune_completed_;
  }

  void autotuner_reset_completed() {
    autotune_completed_ = false;
  }

  const char *autotuner_error() const {
    return auto_error_;
  }

  uint8_t get_octave_cnt() const {
    return octaves_cnt_ + 0x1;
  }

  OC::AUTO_CALIBRATION_STEP autotuner_step() const {
    return static_cast<OC::AUTO_CALIBRATION_STEP>(autotuner_step_);
  }

  void autotuner_arm() {
    autotuner_reset();
    autotuner_ = true;
  }
  
  void autotuner_run() {     
    if (OC::DAC_VOLT_0_ARM == autotuner_step_) {
      autotuner_step_ = OC::DAC_VOLT_0_BASELINE;
    // we start, so reset data to defaults:
      autotuner_copy_defaults();
    }
  }

  void autotuner_next_step() {
    auto_num_passes_ = 0x0;
    auto_DAC_offset_error_ = 0x0;
    correction_direction_ = false;
    correction_cnt_positive_ = correction_cnt_negative_ = 0x0;
    F_correction_factor_ = 0xFF;
    auto_ready_ = false;

    ++autotuner_step_;
    AUTOTUNE_PRINTLN("channel %d : autotune step %d (error=%s)",
                     (int)dac_channel_, (int)autotuner_step_, auto_error_);
  }

  void autotuner_reset() {
    AUTOTUNE_PRINTLN("channel %d : autotuner_reset", (int)dac_channel_);
    autotuner_ = false;
    autotuner_step_ = OC::DAC_VOLT_0_ARM;
    auto_DAC_offset_error_ = 0;
    auto_frequency_ = 0.f;
    auto_last_frequency_ = 0.f;
    auto_error_ = nullptr;
    auto_ready_ = false;
    autotune_completed_ = false;
    auto_freq_sum_ = 0;
    auto_freq_count_ = 0;
    ticks_since_last_freq_ = 0;
    auto_num_passes_ = 0;
    F_correction_factor_ = 0xFF;
    correction_direction_ = false;
    correction_cnt_positive_ = 0x0;
    correction_cnt_negative_ = 0x0;
    octaves_cnt_ = 0;

    for (int i = 0; i <= OCTAVES; i++) {
      auto_calibration_data_[i] = 0;
      auto_target_frequencies_[i] = 0.0f;
    }
  }

  float get_auto_frequency() const {
    return auto_frequency_;
  }

  bool autotuner_ready() const {
     return auto_ready_;
  }

  void autotuner_copy_defaults() {
    AUTOTUNE_PRINTLN("channel %d : reset to core/default calibration data ...", (int)dac_channel_);
    autotune_calibration_data_->CopyFrom(calibration_data_);
  }

  void use_auto_calibration() {
    // TODO[PLD] Enable autocalibration for channel here
    //OC::DAC::set_auto_channel_calibration_data(dac_channel_);
  }
  
  bool auto_frequency() {
    bool _f_result = false;

    if (ticks_since_last_freq_ > ERROR_TIMEOUT) {
      AUTOTUNE_PRINTLN("channel %d : octave %d %lu > %lu",
                       (int)dac_channel_, octaves_cnt_, ticks_since_last_freq_, ERROR_TIMEOUT);
      auto_error_ = "ETIM";
    }

    if (FreqMeasure.available()) {
      auto_freq_sum_ = auto_freq_sum_ + FreqMeasure.read();
      auto_freq_count_ = auto_freq_count_ + 1;

      // take more time as we're converging toward the target frequency
      uint32_t _wait = (F_correction_factor_ == 0x1) ? (FREQ_MEASURE_TIMEOUT << 2) :  (FREQ_MEASURE_TIMEOUT >> 2);
  
      if (ticks_since_last_freq_ > _wait) {

        // store frequency, reset, and poke ui to preempt screensaver:
        auto_frequency_ = FreqMeasure.countToFrequency(auto_freq_sum_ / auto_freq_count_);
        history_.Push(auto_frequency_);
        auto_freq_sum_ = 0;
        auto_ready_ = true;
        auto_freq_count_ = 0;
        _f_result = true;
        ticks_since_last_freq_ = 0x0;
        OC::ui.Poke();
        //AUTOTUNE_PRINTLN("%.3f", auto_frequency_);
      }
    }
    return _f_result;
  }

  void measure_frequency_and_calc_error(OC::IOFrame *ioframe) {

    if (auto_error_ || autotune_completed_) {
      //AUTOTUNE_PRINTLN("auto_error_=%s autotune_completed_=%d", auto_error_, autotune_completed_);
      return;
    }

    switch(autotuner_step_) {
      case OC::DAC_VOLT_0_ARM:
      // do nothing
      break;
      case OC::DAC_VOLT_0_BASELINE:
      // 0V baseline / calibration point: in this case, we don't correct.
      {
        bool _update = auto_frequency();
        if (_update && auto_num_passes_ > kHistoryDepth) { 
          
          auto_last_frequency_ = auto_frequency_;
          // average
          float history[kHistoryDepth]; 
          history_.Read(history);
          float average = std::accumulate(std::begin(history), std::end(history), 0.f);
          // ... and derive target frequency at 0V
          auto_frequency_ = (uint32_t) (0.5f + ((auto_frequency_ + average) / (float)(kHistoryDepth + 1))); // 0V 
          // reset step, and proceed:
          autotuner_next_step();
        }
        else if (_update) 
          auto_num_passes_++;
      }
      break;
      case OC::DAC_VOLT_TARGET_FREQUENCIES: 
      // NOTE This step is "sticky" and repeats for all required octaves
      {
          /* TODO: autotuner is just broken for now...
          switch(OC::DAC::get_voltage_scaling(dac_channel_)) {

            case VOLTAGE_SCALING_1_2V_PER_OCT: // 1.2V/octave
            auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers_1V2[octaves_cnt_];
            break;
            case VOLTAGE_SCALING_2V_PER_OCT: // 2V/octave
            auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers_2V0[octaves_cnt_];
            break;
            default: // 1V/octave
            auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers[octaves_cnt_]; 
            break;
          }
          */
        auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers[octaves_cnt_];
        AUTOTUNE_PRINTLN("channel %d : auto_target_frequencies[%d] = %.3f", (int)dac_channel_, octaves_cnt_, auto_target_frequencies_[octaves_cnt_]);
        octaves_cnt_++;
        // go to next step, if done:
        if (octaves_cnt_ >= OCTAVES) {
          octaves_cnt_ = 0x0;
          autotuner_step_++;
        }
      }
      break;
      case OC::DAC_VOLT_3m:
      case OC::DAC_VOLT_2m:
      case OC::DAC_VOLT_1m: 
      case OC::DAC_VOLT_0:
      case OC::DAC_VOLT_1:
      case OC::DAC_VOLT_2:
      case OC::DAC_VOLT_3:
      case OC::DAC_VOLT_4:
      case OC::DAC_VOLT_5:
      case OC::DAC_VOLT_6:
      { 
        bool _update = auto_frequency();
        
        if (_update && (auto_num_passes_ > MAX_NUM_PASSES)) {  
          /* target frequency reached */
          
          if ((autotuner_step_ > OC::DAC_VOLT_2m) && (auto_last_frequency_ * 1.25f > auto_frequency_)) {
            AUTOTUNE_PRINTLN("channel %d : step %d %.3f * 1.25f > %.3f",
                             (int)dac_channel_, (int)autotuner_step_, auto_last_frequency_, auto_frequency_);
            auto_error_ = "EFREQ"; // throw error, if things don't seem to double ...
          }
          if (auto_DAC_offset_error_ > kMaxOffsetError || auto_DAC_offset_error_ < -kMaxOffsetError) {
            AUTOTUNE_PRINTLN("channel %d : step %d offset error %ld > %ld",
                             (int)dac_channel_, (int)autotuner_step_, auto_DAC_offset_error_, kMaxOffsetError);
            auto_error_ = "EOFFS";
          }

          // average:
          float history[kHistoryDepth]; 
          history_.Read(history);
          float average = std::accumulate(std::begin(history), std::end(history), 0.f);
          // store last frequency:
          auto_last_frequency_  = ((auto_frequency_ + average) / (float)(kHistoryDepth + 1));
          // and DAC correction value:
          AUTOTUNE_PRINTLN("channel %d : step %d offset=%d",
                           (int)dac_channel_, (int)autotuner_step_, (int)auto_DAC_offset_error_);
          auto_calibration_data_[autotuner_step_ - OC::DAC_VOLT_3m] = auto_DAC_offset_error_;
          // and reset step:
          if (!auto_error_)
            autotuner_next_step();
        }
        // 
        else if (_update) {

          // count passes
          auto_num_passes_++;
          // and correct frequency
          if (auto_target_frequencies_[autotuner_step_ - OC::DAC_VOLT_3m] > auto_frequency_) {
            // update correction factor?
            if (!correction_direction_)
              F_correction_factor_ = (F_correction_factor_ >> 1) | 1u;
            correction_direction_ = true;
            
            auto_DAC_offset_error_ += F_correction_factor_;
            // we're converging -- count passes, so we can stop after x attempts:
            if (F_correction_factor_ == 0x1)
              correction_cnt_positive_++;
          }
          else if (auto_target_frequencies_[autotuner_step_ - OC::DAC_VOLT_3m] < auto_frequency_) {
            // update correction factor?
            if (correction_direction_)
              F_correction_factor_ = (F_correction_factor_ >> 1) | 1u;
            correction_direction_ = false;
            
            auto_DAC_offset_error_ -= F_correction_factor_;
            // we're converging -- count passes, so we can stop after x attempts:
            if (F_correction_factor_ == 0x1)
              correction_cnt_negative_++;
          }

          // approaching target? if so, go to next step.
          if (correction_cnt_positive_ > CONVERGE_PASSES && correction_cnt_negative_ > CONVERGE_PASSES)
            auto_num_passes_ = MAX_NUM_PASSES << 1;
        }
      }
      break;
      case OC::AUTO_CALIBRATION_STEP_LAST:
      // NOTE This step is "sticky" and repeats for all required octaves
      // step through the octaves:
      if (ticks_since_last_freq_ > 2000) {
        int32_t new_auto_calibration_point = calibration_data_[octaves_cnt_] + auto_calibration_data_[octaves_cnt_];
        // write to DAC and update data
        if (new_auto_calibration_point >= 65536 || new_auto_calibration_point < 0) {
          AUTOTUNE_PRINTLN("channel %d : octave %d data out of range: %d", (int)dac_channel_, octaves_cnt_, (int)new_auto_calibration_point);
          auto_error_ = "ERNG";
          autotuner_step_++;
        } else {
          ioframe->outputs.set_raw_value(dac_channel_, new_auto_calibration_point);
          //OC::DAC::set(dac_channel_, new_auto_calibration_point);
          //OC::DAC::update_auto_channel_calibration_data(dac_channel_, octaves_cnt_, new_auto_calibration_point);

          AUTOTUNE_PRINTLN("channel %d : set_calibration_point(%d, %d)", (int)dac_channel_, octaves_cnt_, (int)new_auto_calibration_point);
          autotune_calibration_data_->set_calibration_point(octaves_cnt_, new_auto_calibration_point);
          if (octaves_cnt_ == OCTAVES) {
            AUTOTUNE_PRINTLN("channel %d : autotune calibration data valid=true", (int)dac_channel_);
            autotune_calibration_data_->valid = true;
          }

          ticks_since_last_freq_ = 0x0;
          octaves_cnt_++;
        }
      }
      // then stop ... 
      if (octaves_cnt_ > OCTAVES) { 
        autotune_completed_ = true;
        // and point to auto data ...
        use_auto_calibration();
        autotuner_step_++;
      }
      break;
      default:
      //autotuner_step_ = OC::DAC_VOLT_0_ARM;
      //autotuner_ = 0x0;
      break;
    }
  }
  
  void autotune_updateDAC(OC::IOFrame *ioframe) {
    switch(autotuner_step_) {

      case OC::DAC_VOLT_0_ARM: 
      {
        F_correction_factor_ = 0x1; // don't go so fast
        auto_frequency();
        ioframe->outputs.set_pitch_value(dac_channel_, 0); // doesn't really matter by what means
        //OC::DAC::set(dac_channel_, OC::calibration_data.dac.calibrated_octaves[dac_channel_][OC::DAC::kOctaveZero]);
      }
      break;
      case OC::DAC_VOLT_0_BASELINE:
      // set DAC to 0.000V, default calibration:
      ioframe->outputs.set_pitch_value(dac_channel_, 0); // doesn't really matter by what means
      //OC::DAC::set(dac_channel_, OC::calibration_data.dac.calibrated_octaves[dac_channel_][OC::DAC::kOctaveZero]);
      break;
      case OC::DAC_VOLT_TARGET_FREQUENCIES:
      case OC::AUTO_CALIBRATION_STEP_LAST:
      // do nothing
      break;
      default: 
      // set DAC to calibration point + error
      {
        int32_t default_calibration_point = calibration_data_[autotuner_step_ - OC::DAC_VOLT_3m];
        ioframe->outputs.set_raw_value(dac_channel_, default_calibration_point + auto_DAC_offset_error_);
        //OC::DAC::set(dac_channel_, default_calibration_point + auto_DAC_offset_error_);
      }
      break;
    }
  }
 
 // END AUTOTUNER

  void Update(OC::IOFrame *ioframe) {

    int octave = get_octave();
    int range = get_range();
    if (range) {
      rate_phase_ += OC_CORE_TIMER_RATE;
      if (rate_phase_ >= get_rate() * 1000000UL) {
        rate_phase_ = 0;
        mod_offset_ = 1 - mod_offset_;
      }
      octave += mod_offset_ * range;
    } else {
      rate_phase_ = 0;
      mod_offset_ = 0;
    }

    int32_t pitch = OC::PitchUtils::PitchFromSemitone(get_semitone(), octave);
    last_pitch_ = pitch;
    ioframe->outputs.set_pitch_value(dac_channel_, pitch);
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  ReferenceSetting enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  void update_enabled_settings() {
    ReferenceSetting *settings = enabled_settings_;
    *settings++ = REF_SETTING_OCTAVE;
    *settings++ = REF_SETTING_SEMI;
    *settings++ = REF_SETTING_RANGE;
    *settings++ = REF_SETTING_RATE;
    *settings++ = REF_SETTING_AUTOTUNE;
    //*settings++ = REF_SETTING_AUTOTUNE_ERROR;

    if (DAC_CHANNEL_FTM == dac_channel_) {
      *settings++ = REF_SETTING_NOTES_OR_BPM;
      *settings++ = REF_SETTING_A_ABOVE_MID_C_INTEGER;
      *settings++ = REF_SETTING_A_ABOVE_MID_C_MANTISSA;
      *settings++ = REF_SETTING_PPQN;
    }
    else {
      *settings++ = REF_SETTING_DUMMY;
      *settings++ = REF_SETTING_DUMMY;
    }
    
     num_enabled_settings_ = settings - enabled_settings_;
  }

  void RenderScreensaver(weegfx::coord_t start_x, uint8_t chan, const OC::IOSettings &io_settings) const;

private:
  static constexpr size_t kHistoryDepth = 10;

  DAC_CHANNEL dac_channel_;
  const uint16_t *calibration_data_;
  OC::DAC::AutotuneChannelCalibrationData *autotune_calibration_data_;

  uint32_t rate_phase_;
  int mod_offset_;
  int32_t last_pitch_;

// ------ AUTOTUNER -----
  bool autotuner_;
  uint8_t autotuner_step_;
  int32_t auto_DAC_offset_error_;
  float auto_frequency_;
  float auto_last_frequency_;
  const char *auto_error_;
  bool auto_ready_;
  bool autotune_completed_;

  uint32_t auto_freq_sum_;
  uint32_t auto_freq_count_;

  uint32_t ticks_since_last_freq_;
  uint32_t auto_num_passes_;
  uint16_t F_correction_factor_;
  bool correction_direction_;
  int16_t correction_cnt_positive_;
  int16_t correction_cnt_negative_;
  int16_t octaves_cnt_;
  float auto_target_frequencies_[OCTAVES + 1];
  int16_t auto_calibration_data_[OCTAVES + 1];

  util::History<float, kHistoryDepth> history_;
// ------ AUTOTUNER -----

  int num_enabled_settings_;
  ReferenceSetting enabled_settings_[REF_SETTING_LAST];

  // EEPROM size: 11 bytes * 4 channels == 44 bytes
  SETTINGS_ARRAY_DECLARE() {{
    #ifdef NORTHERNLIGHT
    { 0, 0, 9, "Octave", nullptr, settings::STORAGE_TYPE_I8 },
    #elif defined(VOR) 
    { 0, -5, 10, "Octave", nullptr, settings::STORAGE_TYPE_I8 },
    #else
    { 0, -3, 6, "Octave", nullptr, settings::STORAGE_TYPE_I8 },
    #endif
    { 0, 0, 11, "Semitone", OC::Strings::note_names_unpadded, settings::STORAGE_TYPE_U8 },
    { 0, -3, 3, "Mod range oct", nullptr, settings::STORAGE_TYPE_U8 },
    { 0, 0, 30, "Mod rate (s)", nullptr, settings::STORAGE_TYPE_U8 },
    { 0, 0, 1, "Notes/BPM :", notes_or_bpm, settings::STORAGE_TYPE_U8 },
    { 440, 400, 480, "A above mid C", nullptr, settings::STORAGE_TYPE_U16 },
    { 0, 0, 99, " > mantissa", nullptr, settings::STORAGE_TYPE_U8 },
    { CHANNEL_PPQN_4, CHANNEL_PPQN_1, CHANNEL_PPQN_LAST - 1, "> ppqn", ppqn_labels, settings::STORAGE_TYPE_U8 },
    { 0, 0, 0, "--> autotune", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 0, "-", NULL, settings::STORAGE_TYPE_U8 } // dummy
  }};
};
SETTINGS_ARRAY_DEFINE(ReferenceChannel);

namespace OC {

OC_APP_TRAITS(AppReferences, TWOCCS("RF"), "References", "Voltages");
class OC_APP_CLASS(AppReferences) {
public:
  OC_APP_INTERFACE_DECLARE(AppReferences);
  OC_APP_STORAGE_SIZE(DAC_CHANNEL_LAST * ReferenceChannel::storageSize());

private:
  OC::Autotuner<ReferenceChannel> autotuner;

  struct {
    int selected_channel;
    menu::ScreenCursor<menu::kScreenLines> cursor;
  } ui;

  ReferenceChannel channels_[DAC_CHANNEL_LAST];

  ReferenceChannel &selected_channel() { return channels_[ui.selected_channel]; }
  const ReferenceChannel &selected_channel() const { return channels_[ui.selected_channel]; }

  float get_frequency() const {
    return(frequency_) ;
  }

  float get_ppqn() const {
    float ppqn_ = 4.0f;
    switch(channels_[DAC_CHANNEL_FTM].get_channel_ppqn()){
      case CHANNEL_PPQN_1:
        ppqn_ = 1.0f;
        break;
      case CHANNEL_PPQN_2:
        ppqn_ = 2.0f;
        break;
      case CHANNEL_PPQN_4:
        ppqn_ = 4.0f;
        break;
      case CHANNEL_PPQN_8:
        ppqn_ = 8.0f;
        break;
      case CHANNEL_PPQN_16:
        ppqn_ = 16.0f;
        break;
      case CHANNEL_PPQN_24:
        ppqn_ = 24.0f;
        break;
      case CHANNEL_PPQN_32:
        ppqn_ = 32.0f;
        break;
      case CHANNEL_PPQN_48:
        ppqn_ = 48.0f;
        break;
      case CHANNEL_PPQN_64:
        ppqn_ = 64.0f;
        break;
      case CHANNEL_PPQN_96:
        ppqn_ = 96.0f;
        break;
      default:
        ppqn_ = 8.0f;
        break;
    }
    return(ppqn_);
  }

  float get_bpm() const {
    return((60.f * frequency_)/get_ppqn()) ;
  }

  bool get_notes_or_bpm() const {
    return(static_cast<bool>(channels_[DAC_CHANNEL_FTM].get_notes_or_bpm())) ;
  }

  float get_C0_freq() const {
    return(static_cast<float>(channels_[DAC_CHANNEL_FTM].get_a_above_mid_c() * kAaboveMidCtoC0));
  }

private:
  double freq_sum_;
  uint32_t freq_count_;
  float frequency_ ;
  elapsedMillis milliseconds_since_last_freq_;
};

// App stubs

void AppReferences::Init() {
  int dac_channel = DAC_CHANNEL_A;
  for (auto &channel : channels_)
    channel.Init(static_cast<DAC_CHANNEL>(dac_channel++));

  ui.selected_channel = DAC_CHANNEL_FTM;
  ui.cursor.Init(0, channels_[DAC_CHANNEL_FTM].num_enabled_settings() - 1);

  freq_sum_ = 0;
  freq_count_ = 0;
  frequency_ = 0;
  autotuner.Init();
}

void AppReferences::Process(OC::IOFrame *ioframe) {
    
  bool autotuner_active = false;
  for (auto &channel : channels_) {
    channel.Update(ioframe);
    if (channel.autotuner_active()) {
      channel.measure_frequency_and_calc_error(ioframe);
      autotuner.Tick();
      autotuner_active = true;
    }
  }

  if (!autotuner_active && FreqMeasure.available()) {
    // average several readings together
    freq_sum_ = freq_sum_ + FreqMeasure.read();
    freq_count_ = freq_count_ + 1;
    
    if (milliseconds_since_last_freq_ > 750) {
      frequency_ = FreqMeasure.countToFrequency(freq_sum_ / freq_count_);
      freq_sum_ = 0;
      freq_count_ = 0;
      milliseconds_since_last_freq_ = 0;
     }
   } else if (milliseconds_since_last_freq_ > 100000) {
    frequency_ = 0.0f;
   }
}

size_t AppReferences::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  for (auto &channel : channels_)
    channel.Save(stream_buffer);
  return stream_buffer.written();
}

size_t AppReferences::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  for (auto &channel : channels_) {
    channel.Restore(stream_buffer);
    channel.update_enabled_settings();
  }
  ui.cursor.AdjustEnd(channels_[0].num_enabled_settings() - 1);
  return stream_buffer.read();
}

void AppReferences::GetIOConfig(OC::IOConfig &ioconfig) const
{
  ioconfig.digital_inputs[DAC_CHANNEL_FTM].set("Frequency counter");

  ioconfig.outputs[DAC_CHANNEL_A].set("CH1", OC::OUTPUT_MODE_PITCH);
  ioconfig.outputs[DAC_CHANNEL_B].set("CH2", OC::OUTPUT_MODE_PITCH);
  ioconfig.outputs[DAC_CHANNEL_C].set("CH3", OC::OUTPUT_MODE_PITCH);
  ioconfig.outputs[DAC_CHANNEL_D].set("CH4", OC::OUTPUT_MODE_PITCH);
}

void AppReferences::HandleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      ui.cursor.set_editing(false);
      FreqMeasure.begin();
      autotuner.Close();
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      //references_app.autotuner.Reset();
      for (size_t i = 0; i < DAC_CHANNEL_LAST; ++i)
        channels_[i].autotuner_reset();
      break;
  }
}

void AppReferences::Loop() {
  autotuner.Update();
}

void AppReferences::DrawMenu() const {
  // autotuner ...
  if (autotuner.active()) {
    autotuner.Draw();
    return;
  }

  menu::QuadTitleBar::Draw();
  for (uint_fast8_t i = 0; i < DAC_CHANNEL_LAST; ++i) {
    menu::QuadTitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
  }
  menu::QuadTitleBar::Selected(ui.selected_channel);

  const auto &channel = selected_channel();
  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(ui.cursor);
  menu::SettingsListItem list_item;

  while (settings_list.available()) {
    const int setting = 
      channel.enabled_setting_at(settings_list.Next(list_item));
    const int value = channel.get_value(setting);
    const auto &attr = ReferenceChannel::value_attributes(setting);

    switch (setting) {
      case REF_SETTING_AUTOTUNE:
      case REF_SETTING_DUMMY:
         list_item.DrawNoValue<false>(value, attr);
      break;
      default:
        list_item.DrawDefault(value, attr);
      break;
    }
  }
}

void AppReferences::DrawScreensaver() const {
  channels_[0].RenderScreensaver( 0, 0, io_settings_);
  channels_[1].RenderScreensaver(32, 1, io_settings_);
  channels_[2].RenderScreensaver(64, 2, io_settings_);
  channels_[3].RenderScreensaver(96, 3, io_settings_);
  graphics.setPrintPos(2, 44);

  float frequency_ = get_frequency() ;
  float c0_freq_ = get_C0_freq() ;
  float bpm_ = (60.0 * frequency_)/get_ppqn() ;

  int32_t freq_decicents_deviation_ = round(12000.0f * log2f(frequency_ / c0_freq_)) + 500;
  int8_t freq_octave_ = -2 + ((freq_decicents_deviation_)/ 12000) ;
  int8_t freq_note_ = (freq_decicents_deviation_ - ((freq_octave_ + 2) * 12000)) / 1000;
  int32_t freq_decicents_residual_ = ((freq_decicents_deviation_ - ((freq_octave_ - 1) * 12000)) % 1000) - 500;

  if (frequency_ > 0.0) {
    #ifdef FLIP_180
    graphics.printf("TR1 %7.3f Hz", frequency_);
    #else
    graphics.printf("TR4 %7.3f Hz", frequency_);
    #endif
    graphics.setPrintPos(2, 56);
    if (get_notes_or_bpm()) {
      graphics.printf("%7.2f bpm %2.0fppqn", bpm_, get_ppqn());
    } else if(frequency_ >= c0_freq_) {
      graphics.printf("%+i %s %+7.1fc", freq_octave_, OC::Strings::note_names[freq_note_], freq_decicents_residual_ / 10.0) ;
    }
  } else {
    graphics.print("TR4 no input") ;
  }
}

void AppReferences::HandleButtonEvent(const UI::Event &event) {

  auto &channel = selected_channel();
  if (autotuner.active()) {
    autotuner.HandleButtonEvent(event);
    return;
  }

  if (CONTROL_BUTTON_UP == event.control) {
    channel.change_value(REF_SETTING_OCTAVE, 1);
  } else if (CONTROL_BUTTON_DOWN == event.control) {
    channel.change_value(REF_SETTING_OCTAVE, -1);
  } else if (CONTROL_BUTTON_R == event.control) {
    switch (channel.enabled_setting_at(ui.cursor.cursor_pos())) {
      case REF_SETTING_AUTOTUNE:
      autotuner.Open(&channel);
      break;
      case REF_SETTING_DUMMY:
      break;
      default:
      ui.cursor.toggle_editing();
      break;
    }
  }
}

void AppReferences::HandleEncoderEvent(const UI::Event &event) {

  if (autotuner.active()) {
    autotuner.HandleEncoderEvent(event);
    return;
  }
  
  if (CONTROL_ENCODER_L == event.control) {
    
    int previous = selected_channel().num_enabled_settings();
    int selected = ui.selected_channel + event.value;
    CONSTRAIN(selected, 0, DAC_CHANNEL_LAST - 0x1);
    ui.selected_channel = selected;

    // hack -- deal w/ menu items / channels
    if ((ui.cursor.cursor_pos() > 4) && (previous > selected_channel().num_enabled_settings())) {
      ui.cursor.Init(0, 0);
      ui.cursor.AdjustEnd(selected_channel().num_enabled_settings() - 1);
    }
    else
      ui.cursor.AdjustEnd(selected_channel().num_enabled_settings() - 1);
  } else if (CONTROL_ENCODER_R == event.control) {
    if (ui.cursor.editing()) {
        auto &channel = selected_channel();
        ReferenceSetting setting = channel.enabled_setting_at(ui.cursor.cursor_pos());
        if (setting == REF_SETTING_DUMMY) 
          ui.cursor.set_editing(false);
        channel.change_value(setting, event.value);
        channel.update_enabled_settings();
    } else {
      ui.cursor.Scroll(event.value);
    }
  }
}

void AppReferences::DrawDebugInfo() const {
}

} // namespace OC


static void print_voltage(int octave, int fraction) {
  graphics.printf("%01d", octave);
  graphics.movePrintPos(-1, 0); graphics.print('.');
  graphics.movePrintPos(-2, 0); graphics.printf("%03d", fraction);
}

void ReferenceChannel::RenderScreensaver(weegfx::coord_t start_x, uint8_t chan, const OC::IOSettings &io_settings) const {
  using namespace OC;
  // Mostly borrowed from QQ

  weegfx::coord_t x = start_x + 26;
  weegfx::coord_t y = 34 ; // was 60
  // for (int i = 0; i < 5 ; ++i, y -= 4) // was i < 12
    graphics.setPixel(x, y);

  int32_t pitch = last_pitch_;
  int32_t scaled_pitch = IO::pitch_scale(pitch, io_settings.get_output_scaling(chan));

  // pitch gets displayed as closest note value + octave indicator
  pitch = IO::pitch_rel_to_abs(pitch);
  int32_t octave = pitch / (12 << 7);
  pitch -= (octave * 12 << 7);
  int semitone = pitch >> 7;

  y = 34 - semitone * 2; // was 60, multiplier was 4
  if (semitone < 6)
    graphics.setPrintPos(start_x + menu::kIndentDx, y - 7);
  else
    graphics.setPrintPos(start_x + menu::kIndentDx, y);
  graphics.print(OC::Strings::note_names_unpadded[semitone]);

  graphics.drawHLine(start_x + 16, y, 8);
  graphics.drawBitmap8(start_x + 28, 34 - octave * 2 - 1, OC::kBitmapLoopMarkerW, OC::bitmap_loop_markers_8 + OC::kBitmapLoopMarkerW); // was 60

  // scaled pitch to voltage
  scaled_pitch = IO::pitch_rel_to_abs(scaled_pitch);
  CONSTRAIN(scaled_pitch, 0, 120 << 7);
  int32_t millivolts = IO::pitch_to_millivolts(scaled_pitch);
  int32_t volts = millivolts / 1000;
  millivolts -= volts * 1000;
  volts -= DAC::kOctaveZero; // Back to relative

  // We want [sign]d.ddd = 6 chars in 32px space; with the current font width
  // of 6px that's too tight, so squeeze in the mini minus...
  y = menu::kTextDy;
  graphics.setPrintPos(start_x + menu::kIndentDx, y);
  if (volts >= 0) {
    print_voltage(volts, millivolts);
  } else {
    graphics.drawHLine(start_x, y + 3, 2);
    if (millivolts)
      print_voltage(-volts - 1, 1000 - millivolts);
    else
      print_voltage(-volts, 0);
  }
}
