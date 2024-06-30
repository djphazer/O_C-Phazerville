// Copyright (c)  2015, 2016 Patrick Dowling, Max Stadler, Tim Churches
//
// Author of original O+C firmware: Max Stadler (mxmlnstdlr@gmail.com)
// Author of app scaffolding: Patrick Dowling (pld@gurkenkiste.com)
// Modified for bouncing balls: Tim Churches (tim.churches@gmail.com)
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

// Bouncing balls app

#include "OC_apps.h"
#include "OC_bitmaps.h"
#include "OC_digital_inputs.h"
#include "OC_strings.h"
#include "util/util_math.h"
#include "util/util_settings.h"
#include "OC_menus.h"
#include "src/extern/peaks_bouncing_balls.h"

enum BouncingBallSettings {
  BB_SETTING_GRAVITY,
  BB_SETTING_BOUNCE_LOSS,
  BB_SETTING_INITIAL_AMPLITUDE,
  BB_SETTING_INITIAL_VELOCITY,
  BB_SETTING_TRIGGER_INPUT,
  BB_SETTING_RETRIGGER_BOUNCES,
  BB_SETTING_CV1,
  BB_SETTING_CV2,
  BB_SETTING_CV3,
  BB_SETTING_CV4,
  BB_SETTING_HARD_RESET,
  BB_SETTING_LAST
};

enum BallCVMapping {
  BB_CV_MAPPING_NONE,
  BB_CV_MAPPING_GRAVITY,
  BB_CV_MAPPING_BOUNCE_LOSS,
  BB_CV_MAPPING_INITIAL_AMPLITUDE,
  BB_CV_MAPPING_INITIAL_VELOCITY,
  BB_CV_MAPPING_RETRIGGER_BOUNCES,
  BB_CV_MAPPING_LAST
};

const char* const bb_cv_mapping_names[BB_CV_MAPPING_LAST] = {
  "off", "grav", "bnce", "ampl", "vel", "retr"
};

class BouncingBall : public settings::SettingsBase<BouncingBall, BB_SETTING_LAST> {
public:

  static constexpr int kMaxBouncingBallParameters = 5;

  void Init(OC::DigitalInput default_trigger);

  OC::DigitalInput get_trigger_input() const {
    return static_cast<OC::DigitalInput>(values_[BB_SETTING_TRIGGER_INPUT]);
  }

  BallCVMapping get_cv1_mapping() const {
    return static_cast<BallCVMapping>(values_[BB_SETTING_CV1]);
  }

  BallCVMapping get_cv2_mapping() const {
    return static_cast<BallCVMapping>(values_[BB_SETTING_CV2]);
  }

  BallCVMapping get_cv3_mapping() const {
    return static_cast<BallCVMapping>(values_[BB_SETTING_CV3]);
  }

  BallCVMapping get_cv4_mapping() const {
    return static_cast<BallCVMapping>(values_[BB_SETTING_CV4]);
  }

  bool get_hard_reset() const {
    return values_[BB_SETTING_HARD_RESET];
  }

  uint8_t get_initial_amplitude() const {
    return values_[BB_SETTING_INITIAL_AMPLITUDE];
  }

  uint8_t get_initial_velocity() const {
    return values_[BB_SETTING_INITIAL_VELOCITY];
  }

  uint8_t get_gravity() const {
    return values_[BB_SETTING_GRAVITY];
  }

  uint8_t get_bounce_loss() const {
    return values_[BB_SETTING_BOUNCE_LOSS];
  }

  int32_t get_retrigger_bounces() const {
    return static_cast<int32_t>(values_[BB_SETTING_RETRIGGER_BOUNCES]);
  }

#ifdef BBGEN_DEBUG
  uint16_t get_channel_parameter_value(uint8_t param) {
    return s[param];
  }

  int16_t get_channel_retrigger_bounces() const {
    return(bb_.get_retrigger_bounces()) ;
  }
#endif // BBGEN_DEBUG

  inline void apply_cv_mapping(BouncingBallSettings cv_setting, const int32_t cvs[ADC_CHANNEL_LAST], int32_t segments[kMaxBouncingBallParameters]) {
    int mapping = values_[cv_setting];
    uint8_t bb_cv_rshift = 13 ;
    switch (mapping) {
      case BB_CV_MAPPING_GRAVITY:
      case BB_CV_MAPPING_BOUNCE_LOSS:
      case BB_CV_MAPPING_INITIAL_VELOCITY:
        bb_cv_rshift = 13 ;
        break ;
      case BB_CV_MAPPING_INITIAL_AMPLITUDE:
        bb_cv_rshift = 12 ;
        break;
      case BB_CV_MAPPING_RETRIGGER_BOUNCES:
        bb_cv_rshift = 14 ;
        break;
      default:
        bb_cv_rshift = 13 ;
        break;
    }
    if (mapping)
      segments[mapping - BB_CV_MAPPING_GRAVITY] += (cvs[cv_setting - BB_SETTING_CV1]) << (16 - bb_cv_rshift) ;
  }

  void Update(OC::IOFrame *ioframe, uint32_t triggers, const int32_t cvs[ADC_CHANNEL_LAST], DAC_CHANNEL dac_channel) {

    s[0] = SCALE8_16(static_cast<int32_t>(get_gravity()));
    s[1] = SCALE8_16(static_cast<int32_t>(get_bounce_loss()));
    s[2] = SCALE8_16(static_cast<int32_t>(get_initial_amplitude()));
    s[3] = SCALE8_16(static_cast<int32_t>(get_initial_velocity()));
    s[4] = SCALE8_16(static_cast<int32_t>(get_retrigger_bounces()));

    apply_cv_mapping(BB_SETTING_CV1, cvs, s);
    apply_cv_mapping(BB_SETTING_CV2, cvs, s);
    apply_cv_mapping(BB_SETTING_CV3, cvs, s);
    apply_cv_mapping(BB_SETTING_CV4, cvs, s);

    s[0] = USAT16(s[0]);
    s[1] = USAT16(s[1]);
    s[2] = USAT16(s[2]);
    s[3] = USAT16(s[3]);
    s[4] = USAT16(s[4]);

    bb_.Configure(s) ;

    // hard reset forces the bouncing ball to start at level_[0] on rising gate.
    bb_.set_hard_reset(get_hard_reset());

    OC::DigitalInput trigger_input = get_trigger_input();
    uint8_t gate_state = 0;
    if (triggers & DIGITAL_INPUT_MASK(trigger_input))
      gate_state |= peaks::CONTROL_GATE_RISING;

    bool gate_raised = ioframe->digital_inputs.raised(trigger_input);
    if (gate_raised)
      gate_state |= peaks::CONTROL_GATE;
    else if (gate_raised_)
      gate_state |= peaks::CONTROL_GATE_FALLING;
    gate_raised_ = gate_raised;

    // TODO[PLD] Scale range or offset?
    ioframe->outputs.set_unipolar_value(
        dac_channel,
        bb_.ProcessSingleSample(
            gate_state,
            OC::DAC::get_unipolar_max(dac_channel)));
  }


private:
  peaks::BouncingBall bb_;
  bool gate_raised_;
  int32_t s[kMaxBouncingBallParameters];

  // TOTAL EEPROM SIZE: 4 * 9 bytes
  SETTINGS_ARRAY_DECLARE() {{
    { 128, 0, 255, "Gravity", NULL, settings::STORAGE_TYPE_U8 },
    { 96, 0, 255, "Bounce loss", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 255, "Amplitude", NULL, settings::STORAGE_TYPE_U8 },
    { 228, 0, 255, "Velocity", NULL, settings::STORAGE_TYPE_U8 },
    { OC::DIGITAL_INPUT_1, OC::DIGITAL_INPUT_1, OC::DIGITAL_INPUT_4, "Trigger input", OC::Strings::trigger_input_names, settings::STORAGE_TYPE_U8 },
    { 0, 0, 255, "Retrigger", NULL, settings::STORAGE_TYPE_U8 },
    { BB_CV_MAPPING_NONE, BB_CV_MAPPING_NONE, BB_CV_MAPPING_LAST - 1, "CV1 -> ", bb_cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { BB_CV_MAPPING_NONE, BB_CV_MAPPING_NONE, BB_CV_MAPPING_LAST - 1, "CV2 -> ", bb_cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { BB_CV_MAPPING_NONE, BB_CV_MAPPING_NONE, BB_CV_MAPPING_LAST - 1, "CV3 -> ", bb_cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { BB_CV_MAPPING_NONE, BB_CV_MAPPING_NONE, BB_CV_MAPPING_LAST - 1, "CV4 -> ", bb_cv_mapping_names, settings::STORAGE_TYPE_U4 },
    { 0, 0, 1, "Hard reset", OC::Strings::no_yes, settings::STORAGE_TYPE_U8 },
  }};
};
SETTINGS_ARRAY_DEFINE(BouncingBall);

void BouncingBall::Init(OC::DigitalInput default_trigger) {
  InitDefaults();
  apply_value(BB_SETTING_TRIGGER_INPUT, default_trigger);
  bb_.Init();
  gate_raised_ = false;
}

namespace OC {

OC_APP_TRAITS(AppQuadBouncingBalls, TWOCCS("BB"), "Dialectic Ping Pong", "Balls");
class OC_APP_CLASS(AppQuadBouncingBalls) {
public:
  OC_APP_INTERFACE_DECLARE(AppQuadBouncingBalls);
  OC_APP_STORAGE_SIZE(4 * BouncingBall::storageSize());

private:
  static constexpr int32_t kCvSmoothing = 16;

  enum LeftEditMode {
    MODE_SELECT_CHANNEL,
    MODE_EDIT_SETTINGS
  };

  struct {
    LeftEditMode left_edit_mode;
    int left_encoder_value;

    int selected_channel;
    int selected_segment;
    menu::ScreenCursor<menu::kScreenLines> cursor;
  } ui;

  BouncingBall &selected() { return balls_[ui.selected_channel];  }
  const BouncingBall &selected() const { return balls_[ui.selected_channel]; }

  void HandleTopButton();
  void HandleTowerButton();

  BouncingBall balls_[4];

  SmoothedValue<int32_t, kCvSmoothing> cv1;
  SmoothedValue<int32_t, kCvSmoothing> cv2;
  SmoothedValue<int32_t, kCvSmoothing> cv3;
  SmoothedValue<int32_t, kCvSmoothing> cv4;
};

void AppQuadBouncingBalls::Init() {
  int input = OC::DIGITAL_INPUT_1;
  for (auto &bb : balls_) {
    bb.Init(static_cast<OC::DigitalInput>(input));
    ++input;
  }

  ui.left_encoder_value = 0;
  ui.left_edit_mode = MODE_EDIT_SETTINGS;
  ui.selected_channel = 0;
  ui.selected_segment = 0;
  ui.cursor.Init(BB_SETTING_GRAVITY, BB_SETTING_LAST - 1);
}

void FASTRUN AppQuadBouncingBalls::Process(OC::IOFrame *ioframe) {
// TODO[PLD] Do we need this excessive smoothing?
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

  balls_[0].Update(ioframe, triggers, cvs, DAC_CHANNEL_A);
  balls_[1].Update(ioframe, triggers, cvs, DAC_CHANNEL_B);
  balls_[2].Update(ioframe, triggers, cvs, DAC_CHANNEL_C);
  balls_[3].Update(ioframe, triggers, cvs, DAC_CHANNEL_D);
}

size_t AppQuadBouncingBalls::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  for (auto &bb : balls_)
    bb.Save(stream_buffer);
  return stream_buffer.written();
}

size_t AppQuadBouncingBalls::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  for (auto &bb : balls_)
    bb.Restore(stream_buffer);

  return stream_buffer.read();
}

void AppQuadBouncingBalls::HandleAppEvent(OC::AppEvent event) {
  switch (event) {
    case OC::APP_EVENT_RESUME:
      ui.cursor.set_editing(false);
      break;
    case OC::APP_EVENT_SUSPEND:
    case OC::APP_EVENT_SCREENSAVER_ON:
    case OC::APP_EVENT_SCREENSAVER_OFF:
      break;
  }
}

void AppQuadBouncingBalls::Loop() {
}

void AppQuadBouncingBalls::DrawMenu() const {

  menu::QuadTitleBar::Draw();
  for (uint_fast8_t i = 0; i < 4; ++i) {
    menu::QuadTitleBar::SetColumn(i);
    graphics.print((char)('A' + i));
  }
  menu::QuadTitleBar::Selected(ui.selected_channel);

  auto const &bb = selected();
  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(ui.cursor);
  menu::SettingsListItem list_item;
  while (settings_list.available()) {
    const int current = settings_list.Next(list_item);
    list_item.DrawDefault(bb.get_value(current), BouncingBall::value_attributes(current));
  }
}

void AppQuadBouncingBalls::HandleTopButton() {
  auto &selected_bb = selected();
  selected_bb.change_value(BB_SETTING_GRAVITY + ui.selected_segment, 32);
}

void AppQuadBouncingBalls::HandleTowerButton() {
  auto &selected_bb = selected();
  selected_bb.change_value(BB_SETTING_GRAVITY + ui.selected_segment, -32);
}

void AppQuadBouncingBalls::HandleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case OC::CONTROL_BUTTON_UP:
        HandleTopButton();
        break;
      case OC::CONTROL_BUTTON_DOWN:
        HandleTowerButton();
        break;
      case OC::CONTROL_BUTTON_L:
        break;
      case OC::CONTROL_BUTTON_R:
        ui.cursor.toggle_editing();
        break;
    }
  }
}

void AppQuadBouncingBalls::HandleEncoderEvent(const UI::Event &event) {

  if (OC::CONTROL_ENCODER_L == event.control) {
    int left_value = ui.selected_channel + event.value;
    CONSTRAIN(left_value, 0, 3);
    ui.selected_channel = left_value;
  } else if (OC::CONTROL_ENCODER_R == event.control) {
    if (ui.cursor.editing()) {
      auto &selected_ball = selected();
      selected_ball.change_value(ui.cursor.cursor_pos(), event.value);
    } else {
      ui.cursor.Scroll(event.value);
    }
  }
}

void AppQuadBouncingBalls::DrawScreensaver() const {
  OC::scope_render();
}

void AppQuadBouncingBalls::GetIOConfig(OC::IOConfig &ioconfig) const
{
  char label[kMaxIOLabelLength + 1] = {0}; // oversized, truncate later...

  for (int di = DIGITAL_INPUT_1; di <= DIGITAL_INPUT_4; ++di ) {
    char *l = label;
    int channel = 0;
    for (const auto &ball : balls_) {
      ++channel;
      if (di == ball.get_trigger_input()) {
        if (label == l)
          l += sprintf(l, "Ball%d", channel);
        else
          l += sprintf(l, ",%d", channel);
      }
    }
    *l = 0;
    ioconfig.digital_inputs[di].set(label);
  }

  for (int cv = ADC_CHANNEL_1; cv <= ADC_CHANNEL_4; ++cv) {
    char *l = label;
    int channel = 0;
    for (const auto &ball : balls_) {
      ++channel;
      auto mapping = ball.get_value(BB_SETTING_CV1 + cv);
      if (mapping) {
        if (l != label)
          l += sprintf(l, ",%d:%s", channel, bb_cv_mapping_names[mapping]);
        else
          l += sprintf(l, "%d:%s", channel, bb_cv_mapping_names[mapping]);
      }
      // Alternate: "1:*grav, 2, 3, 4"
    }
    *l = 0;
    ioconfig.cv[cv].set(label);
  }

  ioconfig.outputs[DAC_CHANNEL_A].set("Ball1", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[DAC_CHANNEL_B].set("Ball2", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[DAC_CHANNEL_C].set("Ball3", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[DAC_CHANNEL_D].set("Ball4", OC::OUTPUT_MODE_UNI);
}

void AppQuadBouncingBalls::DrawDebugInfo() const {
#ifdef BBGEN_DEBUG
  graphics.setPrintPos(2, 12);
  graphics.print(cv1.value());
  graphics.setPrintPos(32, 12);
  graphics.print(balls_[0].get_channel_retrigger_bounces());
  graphics.setPrintPos(2, 22);
  graphics.print(cv2.value());
  graphics.setPrintPos(2, 32);
  graphics.print(cv3.value());
  graphics.setPrintPos(2, 42);
  graphics.print(cv4.value());
#endif // BBGEN_DEBUG
}

} // namespace OC
