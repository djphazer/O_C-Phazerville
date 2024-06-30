#ifndef OC_AUTOTUNER_H
#define OC_AUTOTUNER_H

#include "OC_options.h"
#include "OC_visualfx.h"
#include "OC_DAC.h"
#include "OC_global_settings.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_ui.h"

// autotune constants:
#ifdef VOR
static constexpr int ACTIVE_OCTAVES = OCTAVES;
#else
static constexpr int ACTIVE_OCTAVES = OCTAVES - 1;
#endif

static constexpr size_t kHistoryDepth = 10;

#define ZERO_OFFSET (5 - OC::DAC::kOctaveZero)

#define FREQ_MEASURE_TIMEOUT 512
#define ERROR_TIMEOUT (FREQ_MEASURE_TIMEOUT << 0x4)
#define MAX_NUM_PASSES 1500
#define CONVERGE_PASSES 5

#if defined(NORTHERNLIGHT) && !defined(IO_10V)
const char* const AT_steps[] = {
  " ", " ", " ", " ", " ", // 5 blanks
  "0.0V", "1.2V", "2.4V", "3.6V", "4.8V", "6.0V", "7.2V", "8.4V", "9.6V", "10.8V", " " 
};
#else
const char* const AT_steps[] = {
  "-5V", "-4V", "-3V", "-2V", "-1V", " 0V", "+1V", "+2V", "+3V", "+4V", "+5V", "+6V", "+7V", "+8V", "+9V", "+10V", " " 
};
#endif

constexpr float target_multipliers[OCTAVES + 6] = { 0.03125f, 0.0625f, 0.125f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f, 128.0f, 256.0f, 512.0f, 1024.0f };

constexpr float target_multipliers_1V2[OCTAVES] = {
  0.1767766952966368931843f,
  0.3149802624737182976666f,
  0.5612310241546865086093f,
  1.0f,
  1.7817974362806785482150f,
  3.1748021039363991668836f,
  5.6568542494923805818985f,
  10.0793683991589855253324f,
  17.9593927729499718282113f,
  32.0f
};

constexpr float target_multipliers_2V0[OCTAVES] = {
  0.3535533905932737863687f,
  0.5f,
  0.7071067811865475727373f,
  1.0f,
  1.4142135623730951454746f,
  2.0f,
  2.8284271247461902909492f,
  4.0f,
  5.6568542494923805818985f,
  8.0f
};

namespace OC {

enum AUTO_MENU_ITEMS {
  DATA_SELECT,
  AUTOTUNE,
  AUTORUN,
  AUTO_MENU_ITEMS_LAST
};

enum AT_STATUS {
   AT_OFF,
   AT_READY,
   AT_WAIT,
   AT_RUN,
   AT_ERROR,
   AT_DONE,
   AT_LAST,
};

enum AUTO_CALIBRATION_STEP {
  DAC_VOLT_0_ARM,
  DAC_VOLT_0_BASELINE,
  DAC_VOLT_TARGET_FREQUENCIES,
  DAC_VOLT_WAIT,
  DAC_VOLT_3m, 
  DAC_VOLT_2m, 
  DAC_VOLT_1m, 
  DAC_VOLT_0, 
  DAC_VOLT_1, 
  DAC_VOLT_2, 
  DAC_VOLT_3, 
  DAC_VOLT_4, 
  DAC_VOLT_5, 
  DAC_VOLT_6,
#ifdef VOR
  DAC_VOLT_7,
#endif
  AUTO_CALIBRATION_STEP_LAST
};

extern const char* const AT_steps[];
extern const char *const reset_action_strings[];
extern const char *const status_action_strings[];

enum AUTOTUNER_SETTINGS {
  AT_SETTING_RESET,
  AT_SETTING_START,
  AT_SETTING_END,
  AT_SETTING_ACTION,
  AUTOTUNER_SETTINGS_LAST
};

enum AUTOTUNER_RESET_ACTION {
  AUTOTUNER_RESET, AUTOTUNER_USE, AUTOTUNER_RESET_ACTION_LAST
};

enum AUTOTUNER_STATUS_ACTION {
  AUTOTUNER_ARM, AUTOTUNER_RUN, AUTOTUNER_STOP, AUTOTUNER_OK, AUTOTUNER_STATUS_ACTION_LAST
};

class AutotunerSettings : public settings::SettingsBase<AutotunerSettings, AUTOTUNER_SETTINGS_LAST> {
public:
  int start_offset() const { return get_value(AT_SETTING_START); }
  int end_offset() const { return get_value(AT_SETTING_END); }

  AUTOTUNER_STATUS_ACTION get_status_action() const {
    return static_cast<AUTOTUNER_STATUS_ACTION>(get_value(AT_SETTING_ACTION));
  }

  AUTOTUNER_RESET_ACTION get_reset_action() const {
    return static_cast<AUTOTUNER_RESET_ACTION>(get_value(AT_SETTING_RESET));
  }

  SETTINGS_ARRAY_DECLARE() {{
    { OC::AUTOTUNER_RESET, OC::AUTOTUNER_RESET, OC::AUTOTUNER_RESET_ACTION_LAST - 1, "Autotune", OC::reset_action_strings, settings::STORAGE_TYPE_U4 },
    { 0, 0, 3, "Start voltage", OC::AT_steps, settings::STORAGE_TYPE_U4 },
    { 0, -3, 0, "End voltage", /*trust me, I know what I'm doing*/OC::AT_steps + OCTAVES - 1, settings::STORAGE_TYPE_U4 },
    { OC::AUTOTUNER_ARM, OC::AUTOTUNER_ARM, OC::AUTOTUNER_STATUS_ACTION_LAST - 1, "", OC::status_action_strings, settings::STORAGE_TYPE_U4 }
  }};
};

// This is a slight refactoring of the original implementation.
// It tries to treat the Owner as the model, and implements both the view and
// controller parts, i.e. doesn't actually have many smarts.
//
// Ideally (for some value of ideal) the autotuning state might be extracted
// from the channel, and this would interface with that without the templatery.
//
template <typename Owner>
class Autotuner {
public:

  static constexpr uint32_t kAnimationTicks = OC_CORE_ISR_FREQ / 2; 

  void Init() {
    owner_ = nullptr;
    channel_ = DAC_CHANNEL_D;
    calibration_data_ = nullptr;

    settings_.InitDefaults();
    cursor_.Init(AT_SETTING_RESET, AT_SETTING_ACTION);

    memset(status_action_label_, 0, sizeof(status_action_label_));
    ticks_ = 0;
    animation_counter_ = 0;
  }

  inline bool active() const {
    return nullptr != owner_;
  }

  void Open(Owner *owner);
  void Close();
  void Update();
  void Tick();
  void Draw() const;
  void HandleButtonEvent(const UI::Event &event);
  void HandleEncoderEvent(const UI::Event &event);

private:

  Owner *owner_;

  DAC_CHANNEL channel_;
  const DAC::AutotuneChannelCalibrationData *calibration_data_;

  AutotunerSettings settings_;
  menu::ScreenCursor<menu::kScreenLines> cursor_;

  char status_action_label_[16];
  uint32_t ticks_;
  uint_fast8_t animation_counter_;

  void DoReset(AUTOTUNER_RESET_ACTION reset_action);
  void DoAction(AUTOTUNER_STATUS_ACTION status_action);

  void handleButtonLeft(const UI::Event &event);
  void handleButtonRight(const UI::Event &event);
  void handleButtonUp(const UI::Event &event);
  void handleButtonDown(const UI::Event &event);

#if 0
  // TODO: ported from old REFS, might need rewrites

  void autotuner_arm(uint8_t _status) {
    reset_autotuner();
    armed_ = _status ? true : false;
  }
  
  void autotuner_run() {     
    SERIAL_PRINTLN("Starting autotuner...");
    step_ = armed_ ? OC::DAC_VOLT_0_BASELINE : OC::DAC_VOLT_0_ARM;
    if (step_ == OC::DAC_VOLT_0_BASELINE)
    // we start, so reset data to defaults:
      OC::DAC::set_default_channel_calibration_data(channel_);
  }

  void auto_next_step() {
    SERIAL_PRINTLN("Autotuner reset step...");
    ticks_since_last_freq_ = 0x0;
    auto_num_passes_ = 0x0;
    auto_DAC_offset_error_ = 0x0;
    correction_direction_ = false;
    correction_cnt_positive_ = correction_cnt_negative_ = 0x0;
    F_correction_factor_ = 0xFF;
    ready_ = false;
    step_++;
  }

  void reset_autotuner() {
    ticks_since_last_freq_ = 0x0;
    auto_frequency_ = 0x0;
    auto_last_frequency_ = 0x0;
    error_ = 0x0;
    ready_ = 0x0;
    armed_ = 0x0;
    step_ = 0x0;
    F_correction_factor_ = 0xFF;
    correction_direction_ = false;
    correction_cnt_positive_ = 0x0;
    correction_cnt_negative_ = 0x0;
    octaves_cnt_ = 0x0;
    auto_num_passes_ = 0x0;
    auto_DAC_offset_error_ = 0x0;
    completed_ = 0x0;
    reset_calibration_data();
  }

  void reset_calibration_data() {
    
    for (int i = 0; i < OCTAVES + 1; i++) {
      auto_calibration_data_[i] = 0;
      auto_target_frequencies_[i] = 0.0f;
    }
  }

  uint8_t data_available() {
    return OC::DAC::calibration_data_used(channel_);
  }

  void use_default() {
    OC::DAC::set_default_channel_calibration_data(channel_);
  }

  void use_auto_calibration() {
    OC::DAC::set_auto_channel_calibration_data(channel_);
  }
  
  bool auto_frequency() {

    bool _f_result = false;

    if (ticks_since_last_freq_ > ERROR_TIMEOUT) {
      error_ = true;
    }
    
    if (FreqMeasure.available()) {
      
      auto_freq_sum_ = auto_freq_sum_ + FreqMeasure.read();
      auto_freq_count_ = auto_freq_count_ + 1;

      // take more time as we're converging toward the target frequency
      uint32_t _wait = (F_correction_factor_ == 0x1) ? (FREQ_MEASURE_TIMEOUT << 2) :  (FREQ_MEASURE_TIMEOUT >> 2);
  
      if (ticks_since_last_freq_ > _wait) {

        // store frequency, reset, and poke ui to preempt screensaver:
        auto_frequency_ = uint32_t(FreqMeasure.countToFrequency(auto_freq_sum_ / auto_freq_count_) * 1000);
        history_.Push(auto_frequency_);
        auto_freq_sum_ = 0;
        ready_ = true;
        auto_freq_count_ = 0;
        _f_result = true;
        ticks_since_last_freq_ = 0x0;
        OC::ui._Poke();
        history_.Update();
      }
    }
    return _f_result;
  }

  void measure_frequency_and_calc_error() {

    switch(step_) {

      case OC::DAC_VOLT_0_ARM:
      case OC::DAC_VOLT_WAIT:
      // do nothing
      break;
      case OC::DAC_VOLT_0_BASELINE:
      // 0V baseline / calibration point: in this case, we don't correct.
      {
        bool _update = auto_frequency();
        if (_update && auto_num_passes_ > kHistoryDepth) { 
          
          auto_last_frequency_ = auto_frequency_;
          uint32_t history[kHistoryDepth]; 
          uint32_t average = 0;
          // average
          history_.Read(history);
          for (uint8_t i = 0; i < kHistoryDepth; i++)
            average += history[i];
          // ... and derive target frequency at 0V
          auto_frequency_ = ((auto_frequency_ + average) / (kHistoryDepth + 1)); // 0V 
          SERIAL_PRINTLN("Baseline auto_frequency_ = %4.d", auto_frequency_);
          // reset step, and proceed:
          auto_next_step();
          // wait for user to patch output to oscillator V/Oct
          run_status_ = AT_WAIT;
        }
        else if (_update) 
          auto_num_passes_++;
      }
      break;
      case OC::DAC_VOLT_TARGET_FREQUENCIES: 
      {
        switch(OC::DAC::get_voltage_scaling(channel_))
        {
          case VOLTAGE_SCALING_1_2V_PER_OCT: // 1.2V/octave
          auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers_1V2[octaves_cnt_];
          break;
          case VOLTAGE_SCALING_2V_PER_OCT: // 2V/octave
          auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers_2V0[octaves_cnt_];
          break;
          default: // 1V/octave
          auto_target_frequencies_[octaves_cnt_]  =  auto_frequency_ * target_multipliers[octaves_cnt_ + ZERO_OFFSET]; 
          break;
        }
        octaves_cnt_++;

        // go to next step, if done:
        if (octaves_cnt_ > ACTIVE_OCTAVES) {
          octaves_cnt_ = 0x0;
          step_++;
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
      #ifdef VOR
      case OC::DAC_VOLT_7:
      #endif
      { 
        bool _update = auto_frequency();
        
        if (_update && (auto_num_passes_ > MAX_NUM_PASSES)) {  
          /* target frequency reached */
          SERIAL_PRINTLN("* Target Frequency Reached *");
          
          int step_idx = step_ - OC::DAC_VOLT_3m;

          // if things don't seem to double ... we've hit the ceiling.
          if ((step_ > OC::DAC_VOLT_2m) && (auto_last_frequency_ * 1.25f > auto_frequency_))
              step_ = OC::AUTO_CALIBRATION_STEP_LAST - 1;

          // average:
          uint32_t history[kHistoryDepth]; 
          uint32_t average = 0;
          history_.Read(history);
          for (uint8_t i = 0; i < kHistoryDepth; i++)
            average += history[i];

          // store last frequency:
          auto_last_frequency_  = (auto_frequency_ + average) / (kHistoryDepth + 1);
          // and DAC correction value:
          auto_calibration_data_[step_idx] = auto_DAC_offset_error_;

          // reset and step forward
          auto_next_step();
        }
        else if (_update)
        {
          auto_num_passes_++; // count passes

          SERIAL_PRINTLN("auto_target_frequencies[%3d]_ = %3d", step_ - OC::DAC_VOLT_3m,
                        auto_target_frequencies_[step_ - OC::DAC_VOLT_3m] );
          SERIAL_PRINTLN("auto_frequency_ = %3d", auto_frequency_);

          // and correct frequency
          if (auto_target_frequencies_[step_ - OC::DAC_VOLT_3m] > auto_frequency_)
          {
            // update correction factor?
            if (!correction_direction_)
              F_correction_factor_ = (F_correction_factor_ >> 1) | 1u;

            correction_direction_ = true;

            auto_DAC_offset_error_ += F_correction_factor_;

            // we're converging -- count passes, so we can stop after x attempts:
            if (F_correction_factor_ == 0x1) correction_cnt_positive_++;
          }
          else if (auto_target_frequencies_[step_ - OC::DAC_VOLT_3m] < auto_frequency_)
          {
            // update correction factor?
            if (correction_direction_)
              F_correction_factor_ = (F_correction_factor_ >> 1) | 1u;

            correction_direction_ = false;
            
            auto_DAC_offset_error_ -= F_correction_factor_;

            // we're converging -- count passes, so we can stop after x attempts:
            if (F_correction_factor_ == 0x1) correction_cnt_negative_++;
          }

          SERIAL_PRINTLN("auto_DAC_offset_error_ = %3d", auto_DAC_offset_error_);

          // approaching target? if so, go to next step.
          if (correction_cnt_positive_ > CONVERGE_PASSES && correction_cnt_negative_ > CONVERGE_PASSES)
            auto_num_passes_ = MAX_NUM_PASSES << 1;
        }
      }
      break;
      case OC::AUTO_CALIBRATION_STEP_LAST:
      // step through the octaves:
      if (ticks_since_last_freq_ > 2000) {
        int32_t new_auto_calibration_point = OC::calibration_data.dac.calibrated_octaves[channel_][octaves_cnt_] + auto_calibration_data_[octaves_cnt_];
        // write to DAC and update data
        if (new_auto_calibration_point >= 65536 || new_auto_calibration_point < 0) {
          error_ = true;
          step_++;
        }
        else {
          OC::DAC::set(channel_, new_auto_calibration_point);
          OC::DAC::update_auto_channel_calibration_data(channel_, octaves_cnt_, new_auto_calibration_point);
          ticks_since_last_freq_ = 0x0;
          octaves_cnt_++;
        }
      }
      // then stop ... 
      if (octaves_cnt_ > OCTAVES) { 
        completed_ = true;
        // and point to auto data ...
        OC::DAC::set_auto_channel_calibration_data(channel_);
        step_++;
      }
      break;
      default:
      step_ = OC::DAC_VOLT_0_ARM;
      armed_ = 0x0;
      break;
    }
  }
  
  void updateDAC() {

    switch(step_) {

      case OC::DAC_VOLT_0_ARM: 
      {
        F_correction_factor_ = 0x1; // don't go so fast
        auto_frequency();
        OC::DAC::set(channel_, OC::calibration_data.dac.calibrated_octaves[channel_][OC::DAC::kOctaveZero]);
      }
      break;
      case OC::DAC_VOLT_0_BASELINE:
      // set DAC to 0.000V, default calibration:
      OC::DAC::set(channel_, OC::calibration_data.dac.calibrated_octaves[channel_][OC::DAC::kOctaveZero]);
      break;
      case OC::DAC_VOLT_TARGET_FREQUENCIES:
      case OC::DAC_VOLT_WAIT:
      case OC::AUTO_CALIBRATION_STEP_LAST:
      // do nothing
      break;
      default: 
      // set DAC to calibration point + error
      {
        int32_t _default_calibration_point = OC::calibration_data.dac.calibrated_octaves[channel_][step_ - OC::DAC_VOLT_3m];
        OC::DAC::set(channel_, _default_calibration_point + auto_DAC_offset_error_);
      }
      break;
    }
  }
#endif
};
  
  template <typename Owner>
  void Autotuner<Owner>::Open(Owner *owner) {
    owner_ = owner;
    channel_ = owner_->get_channel();
    calibration_data_ = &global_settings.autotune_calibration_data.channels[channel_];
    cursor_.Init(AT_SETTING_RESET, AT_SETTING_ACTION);
    settings_.InitDefaults();
    memset(status_action_label_, 0, sizeof(status_action_label_));

    Update();
  }

  template <typename Owner>
  void Autotuner<Owner>::Close() {
    ui.SetButtonIgnoreMask();
    if (owner_) {
      owner_->autotuner_reset();
      owner_ = nullptr;
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::DoReset(AUTOTUNER_RESET_ACTION reset_action) {
  }

  template <typename Owner>
  void Autotuner<Owner>::DoAction(AUTOTUNER_STATUS_ACTION status_action) {
    switch(status_action) {
    case AUTOTUNER_ARM: owner_->autotuner_arm(); break;
    case AUTOTUNER_RUN: owner_->autotuner_run(); break;
    case AUTOTUNER_STOP:
    case AUTOTUNER_OK: owner_->autotuner_reset(); break;
    default: break;
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::Update() {
    static constexpr const char *ellipses[4] = { "   ", ".  ", ".. ", "..." };
    if (owner_->autotuner_active()) {
      auto error = owner_->autotuner_error();
      if (error) {
        if (animation_counter_ > 4)
          snprintf(status_action_label_, sizeof(status_action_label_), "Autotune fail");
        else
          snprintf(status_action_label_, sizeof(status_action_label_), "%s %d", error, owner_->get_octave_cnt());
        settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_OK);
      } else if (owner_->autotuner_completed()){
        snprintf(status_action_label_, sizeof(status_action_label_), "Autotune done");
        settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_OK);
      } else {
        auto step = owner_->autotuner_step();
        switch(step) {
        case DAC_VOLT_0_ARM:
          snprintf(status_action_label_, sizeof(status_action_label_), "Arming%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_RUN);
          break;
        case DAC_VOLT_0_BASELINE:
          snprintf(status_action_label_, sizeof(status_action_label_), "0V baseline%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_STOP);
          break;
        case DAC_VOLT_TARGET_FREQUENCIES: 
        case AUTO_CALIBRATION_STEP_LAST: {
          auto octaves = owner_->get_octave_cnt();
          auto ptr = status_action_label_;
          while (octaves--) *ptr++ = '.';
          *ptr = '\0';
          //snprintf(status_action_label_, sizeof(status_action_label_), "Setup%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_STOP);
        }
        break;
        default:
          snprintf(status_action_label_, sizeof(status_action_label_), "Running%s", ellipses[animation_counter_>>1]);
          settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_STOP);
        break;
        }
      }
    } else {
      snprintf(status_action_label_, sizeof(status_action_label_), "Ready");
      settings_.apply_value(AT_SETTING_ACTION, AUTOTUNER_ARM);
    }

    settings_.apply_value(AT_SETTING_RESET, calibration_data_->is_valid() ? AUTOTUNER_USE : AUTOTUNER_RESET);
  }

  template <typename Owner>
  void Autotuner<Owner>::Tick() {
    ++ticks_;
    if (ticks_ > kAnimationTicks) {
      animation_counter_ = (animation_counter_ + 1) & 0x7;
      ticks_ = 0;
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::Draw() const {

    menu::DefaultTitleBar::Draw();
    graphics.printf("%s ", Strings::channel_id[channel_]);

    auto step = owner_->autotuner_step();
    if (step >= DAC_VOLT_3m && step <= DAC_VOLT_6) {
      graphics.print(AT_steps[step - DAC_VOLT_3m]);
      graphics.print(' ');
    }

    if (owner_->autotuner_ready()) {
      char freqstr[12];
      snprintf(freqstr, sizeof(freqstr), "%7.3f", owner_->get_auto_frequency());
      graphics.setPrintPos(128, 2);
      graphics.print_right(freqstr);
    }

    menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(cursor_);
    menu::SettingsListItem list_item;
    while (settings_list.available()) {
      const int setting = settings_list.Next(list_item);
      const int value = settings_.get_value(setting);
      const auto &attr = AutotunerSettings::value_attributes(setting);

      if (AT_SETTING_ACTION == setting) {
        list_item.DrawCustomName(status_action_label_, value, attr);
      } else if (AT_SETTING_RESET == setting) {
        if (calibration_data_->is_valid()) {
          list_item.DrawCharName("Slot empty");
          list_item.DrawCustom();
        } else {
          list_item.DrawDefault(value, attr);
        }
      } else {
        list_item.DrawDefault(value, attr);
      }
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::HandleButtonEvent(const UI::Event &event) {
    
     if (UI::EVENT_BUTTON_PRESS == event.type) {
      switch (event.control) {
        case CONTROL_BUTTON_UP: handleButtonUp(event); break;
        case CONTROL_BUTTON_DOWN: handleButtonDown(event); break;
        case CONTROL_BUTTON_L: handleButtonLeft(event); break;    
        case CONTROL_BUTTON_R: handleButtonRight(event); break;
          break;
        default:
          break;
      }
    }
    else if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
       switch (event.control) {
        case CONTROL_BUTTON_UP:
          // screensaver 
        break;
        case CONTROL_BUTTON_DOWN:
          global_settings.autotune_calibration_data.Reset();
        break;
        case CONTROL_BUTTON_L: 
        break;
        case CONTROL_BUTTON_R:
         // app menu
        break;  
        default:
        break;
      }
    }
  }
  
  template <typename Owner>
  void Autotuner<Owner>::HandleEncoderEvent(const UI::Event &event) {
   
    if (CONTROL_ENCODER_R == event.control) {
      if (cursor_.editing())
        settings_.change_value(cursor_.cursor_pos(), event.value);
      else if (settings_.get_status_action() == AUTOTUNER_ARM)
        cursor_.Scroll(event.value);
      else
        DoAction(AUTOTUNER_STOP);
    } else if (CONTROL_ENCODER_L == event.control) {
      DoAction(AUTOTUNER_STOP);
    }
  }

  template <typename Owner>
  void Autotuner<Owner>::handleButtonUp(const UI::Event &event) {
    if (settings_.get_status_action() == AUTOTUNER_ARM)
      DoAction(AUTOTUNER_STOP);
    else
      Close();
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonDown(const UI::Event &event) {
    if (settings_.get_status_action() == AUTOTUNER_ARM)
      DoAction(AUTOTUNER_STOP);
    else
      Close();
  }
  
  template <typename Owner>
  void Autotuner<Owner>::handleButtonLeft(const UI::Event &) {
    Close();
  }

  template <typename Owner>
  void Autotuner<Owner>::handleButtonRight(const UI::Event &event) {
    switch(cursor_.cursor_pos()) {
    case AT_SETTING_RESET: DoReset(settings_.get_reset_action()); break;
    case AT_SETTING_ACTION: DoAction(settings_.get_status_action()); break;
    default:
      cursor_.toggle_editing();
    break;
    }
  }

} // namespace OC

#endif // OC_AUTOTUNER_H
