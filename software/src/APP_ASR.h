// Copyright (c) 2014-2017 Max Stadler, Patrick Dowling
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

#include "util/util_settings.h"
#include "util/util_trigger_delay.h"
#include "util/util_turing.h"
#include "util/util_ringbuffer.h"
#include "util/util_integer_sequences.h"
#include "OC_DAC.h"
#include "OC_menus.h"
#include "OC_pitch_utils.h"
#include "OC_cv_utils.h"
#include "OC_scales.h"
#include "OC_scale_edit.h"
#include "OC_strings.h"
#include "OC_visualfx.h"
#include "src/extern/peaks_bytebeat.h"

extern uint_fast8_t MENU_REDRAW;

#define NUM_ASR_CHANNELS 0x4
#define ASR_MAX_ITEMS 256 // = ASR ring buffer size.
#define ASR_HOLD_BUF_SIZE ASR_MAX_ITEMS / NUM_ASR_CHANNELS // max. delay size

enum ASRSettings {
  ASR_SETTING_SCALE,
  ASR_SETTING_OCTAVE,
  ASR_SETTING_ROOT,
  ASR_SETTING_MASK,
  ASR_SETTING_INDEX,
  ASR_SETTING_MULT,
  ASR_SETTING_DELAY,
  ASR_SETTING_BUFFER_LENGTH,
  ASR_SETTING_CV_SOURCE,
  ASR_SETTING_CV4_DESTINATION,
  ASR_SETTING_TURING_LENGTH,
  ASR_SETTING_TURING_PROB,
  ASR_SETTING_TURING_CV_SOURCE,
  ASR_SETTING_BYTEBEAT_EQUATION,
  ASR_SETTING_BYTEBEAT_P0,
  ASR_SETTING_BYTEBEAT_P1,
  ASR_SETTING_BYTEBEAT_P2,
  ASR_SETTING_BYTEBEAT_CV_SOURCE,
  ASR_SETTING_INT_SEQ_INDEX,
  ASR_SETTING_INT_SEQ_MODULUS,
  ASR_SETTING_INT_SEQ_START,
  ASR_SETTING_INT_SEQ_LENGTH,
  ASR_SETTING_INT_SEQ_DIR,
  ASR_SETTING_FRACTAL_SEQ_STRIDE,
  ASR_SETTING_INT_SEQ_CV_SOURCE,
  ASR_SETTING_LAST
};

enum ASRChannelSource {
  ASR_CHANNEL_SOURCE_CV1,
  ASR_CHANNEL_SOURCE_TURING,
  ASR_CHANNEL_SOURCE_BYTEBEAT,
  ASR_CHANNEL_SOURCE_INTEGER_SEQUENCES,
  ASR_CHANNEL_SOURCE_LAST
};

enum ASR_CV4_DEST {
  ASR_DEST_OCTAVE,
  ASR_DEST_ROOT,
  ASR_DEST_TRANSPOSE,
  ASR_DEST_BUFLEN,
  ASR_DEST_INPUT_SCALING,
  ASR_DEST_LAST
};

const char* const asr_input_sources[] = {
  "CV1", "TM", "ByteB", "IntSq"
};

const char* const asr_cv4_destinations[] = {
  "oct", "root", "trns", "buf.l", "igain"
};

const char* const bb_CV_destinations[] = {
  "igain", "eqn", "P0", "P1", "P2"
};

const char* const int_seq_CV_destinations[] = {
  "igain", "seq", "strt", "len", "strd", "mod"
};

typedef int16_t ASR_pitch;

class ASR
: public settings::SettingsBase<ASR, ASR_SETTING_LAST>
, public OC::ScaleEditorEventHandler {
public:
  static constexpr size_t kHistoryDepth = 5;

#ifdef ARDUINO_TEENSY41
  static constexpr ADC_CHANNEL CVInput1 = ADC_CHANNEL_5;
  static constexpr ADC_CHANNEL CVInput2 = ADC_CHANNEL_6;
  static constexpr ADC_CHANNEL CVInput3 = ADC_CHANNEL_7;
  static constexpr ADC_CHANNEL CVInput4 = ADC_CHANNEL_8;
#else
  static constexpr ADC_CHANNEL CVInput1 = ADC_CHANNEL_1;
  static constexpr ADC_CHANNEL CVInput2 = ADC_CHANNEL_2;
  static constexpr ADC_CHANNEL CVInput3 = ADC_CHANNEL_3;
  static constexpr ADC_CHANNEL CVInput4 = ADC_CHANNEL_4;
#endif

  // ScaleEditorEventHandler
  int get_scale(int /*slot_index*/) const final {
    return get_scale();
  }

  uint16_t get_scale_mask(int /*slot_index*/) const final {
    return get_scale_mask();
  }

  void update_scale_mask(uint16_t mask, int /*slot_index*/) final {
    apply_value(ASR_SETTING_MASK, mask); // Should automatically be updated
  }

  void scale_changed() final {
    force_update_ = true;
  }

  void set_scale(int scale) {
    if (scale != get_scale()) {
      const OC::Scale &scale_def = OC::Scales::GetScale(scale);
      uint16_t mask = get_scale_mask();
      if (0 == (mask & ~(0xffff << scale_def.num_notes)))
        mask |= 0x1;
      apply_value(ASR_SETTING_MASK, mask);
      apply_value(ASR_SETTING_SCALE, scale);
    }
  }
  // End ScaleEditorEventHandler

  int get_scale() const {
    return values_[ASR_SETTING_SCALE];
  }

  uint16_t get_scale_mask() const {
    return values_[ASR_SETTING_MASK];
  }

  uint8_t get_buffer_length() const {
    return values_[ASR_SETTING_BUFFER_LENGTH];
  }

  int get_root() const {
    return values_[ASR_SETTING_ROOT];
  }

  int get_index() const {
    return values_[ASR_SETTING_INDEX];
  }

  int get_octave() const {
    return values_[ASR_SETTING_OCTAVE];
  }

  bool octave_toggle() {
    octave_toggle_ = !octave_toggle_;
    return octave_toggle_;
  }

  bool poke_octave_toggle() const {
    return octave_toggle_;
  }

  int get_mult() const {
    return values_[ASR_SETTING_MULT];
  }

  int get_cv_source() const {
    return values_[ASR_SETTING_CV_SOURCE];
  }

  uint8_t get_cv4_destination() const {
    return values_[ASR_SETTING_CV4_DESTINATION];
  }

  uint8_t get_turing_length() const {
    return values_[ASR_SETTING_TURING_LENGTH];
  }

  uint8_t get_turing_display_length() const {
    return turing_display_length_;
  }

  uint8_t get_turing_probability() const {
    return values_[ASR_SETTING_TURING_PROB];
  }

  uint8_t get_turing_CV() const {
    return values_[ASR_SETTING_TURING_CV_SOURCE];
  }

  uint32_t get_shift_register() const {
    return turing_machine_.get_shift_register();
  }

  uint16_t get_trigger_delay() const {
    return values_[ASR_SETTING_DELAY];
  }

  uint8_t get_bytebeat_equation() const {
    return values_[ASR_SETTING_BYTEBEAT_EQUATION];
  }

  uint8_t get_bytebeat_p0() const {
    return values_[ASR_SETTING_BYTEBEAT_P0];
  }

  uint8_t get_bytebeat_p1() const {
    return values_[ASR_SETTING_BYTEBEAT_P1];
  }

  uint8_t get_bytebeat_p2() const {
    return values_[ASR_SETTING_BYTEBEAT_P2];
  }

  uint8_t get_bytebeat_CV() const {
    return values_[ASR_SETTING_BYTEBEAT_CV_SOURCE];
  }

  uint8_t get_int_seq_index() const {
    return values_[ ASR_SETTING_INT_SEQ_INDEX];
  }

  uint8_t get_int_seq_modulus() const {
    return values_[ ASR_SETTING_INT_SEQ_MODULUS];
  }

  int16_t get_int_seq_start() const {
    return static_cast<int16_t>(values_[ASR_SETTING_INT_SEQ_START]);
  }

  int16_t get_int_seq_length() const {
    return static_cast<int16_t>(values_[ASR_SETTING_INT_SEQ_LENGTH] - 1);
  }

  bool get_int_seq_dir() const {
    return static_cast<bool>(values_[ASR_SETTING_INT_SEQ_DIR]);
  }

  int16_t get_fractal_seq_stride() const {
    return static_cast<int16_t>(values_[ASR_SETTING_FRACTAL_SEQ_STRIDE]);
  }

  uint8_t get_int_seq_CV() const {
    return values_[ASR_SETTING_INT_SEQ_CV_SOURCE];
  }

  int16_t get_int_seq_k() const {
    return int_seq_.get_k();
  }

  int16_t get_int_seq_l() const {
    return int_seq_.get_l();
  }

  int16_t get_int_seq_i() const {
    return int_seq_.get_i();
  }

  int16_t get_int_seq_j() const {
    return int_seq_.get_j();
  }

  int16_t get_int_seq_n() const {
    return int_seq_.get_n();
  }

  void toggle_delay_mechanics() {
    delay_type_ = !delay_type_;
  }

  bool get_delay_type() const {
    return delay_type_;
  }
  
  void toggle_manual_freeze() {
    manual_freeze_ = !manual_freeze_;
  }

  void clear_freeze() {
    manual_freeze_ = false;
  }

  bool freeze_state() const {
    return manual_freeze_ || freeze_;
  }

  ASRSettings enabled_setting_at(int index) const {
    return enabled_settings_[index];
  }

  int num_enabled_settings() const {
    return num_enabled_settings_;
  }

  void init() {

    force_update_ = false;
    last_scale_ = -0x1;
    delay_type_ = false;
    octave_toggle_ = false;
    manual_freeze_ = false;
    freeze_ = false;
    set_scale(OC::Scales::SCALE_SEMI);
    last_mask_ = 0x0;
    quantizer_.Init();
    update_scale(true, 0x0);

    _ASR.Init();
    clock_display_.Init();
    for (auto &sh : scrolling_history_)
      sh.Init();
    update_enabled_settings();

    trigger_delay_.Init();
    turing_machine_.Init();
    turing_display_length_ = get_turing_length();
    bytebeat_.Init();
    int_seq_.Init(get_int_seq_start(), get_int_seq_length());
 }

  bool update_scale(bool force, int32_t mask_rotate) {
    
    const int scale = get_scale();
    uint16_t mask = get_scale_mask();
    if (mask_rotate)
      mask = OC::ScaleEditor::RotateMask(mask, OC::Scales::GetScale(scale).num_notes, mask_rotate);

    if (force || (last_scale_ != scale || last_mask_ != mask)) {

      last_scale_ = scale;
      last_mask_ = mask;
      quantizer_.Configure(OC::Scales::GetScale(scale), mask);
      return true;
    } else {
      return false;
    }
  }

  void force_update() {
    force_update_ = true;
  }

  uint16_t get_rotated_mask() const {
    return last_mask_;
  }

  void set_display_mask(uint16_t mask) {
    last_mask_ = mask;
  }

  void update_enabled_settings() {

    ASRSettings *settings = enabled_settings_;

    *settings++ = ASR_SETTING_ROOT;
    *settings++ = ASR_SETTING_MASK;
    *settings++ = ASR_SETTING_OCTAVE;
    *settings++ = ASR_SETTING_INDEX;
    *settings++ = ASR_SETTING_BUFFER_LENGTH;
    *settings++ = ASR_SETTING_DELAY;
    *settings++ = ASR_SETTING_MULT;

    *settings++ = ASR_SETTING_CV4_DESTINATION;
    *settings++ = ASR_SETTING_CV_SOURCE;

    switch (get_cv_source()) {
      case ASR_CHANNEL_SOURCE_TURING:
        *settings++ = ASR_SETTING_TURING_LENGTH;
        *settings++ = ASR_SETTING_TURING_PROB;
        *settings++ = ASR_SETTING_TURING_CV_SOURCE;
       break;
      case ASR_CHANNEL_SOURCE_BYTEBEAT:
        *settings++ = ASR_SETTING_BYTEBEAT_EQUATION;
        *settings++ = ASR_SETTING_BYTEBEAT_P0;
        *settings++ = ASR_SETTING_BYTEBEAT_P1;
        *settings++ = ASR_SETTING_BYTEBEAT_P2;
        *settings++ = ASR_SETTING_BYTEBEAT_CV_SOURCE;
      break;
       case ASR_CHANNEL_SOURCE_INTEGER_SEQUENCES:
        *settings++ = ASR_SETTING_INT_SEQ_INDEX;
        *settings++ = ASR_SETTING_INT_SEQ_MODULUS;
        *settings++ = ASR_SETTING_INT_SEQ_START;
        *settings++ = ASR_SETTING_INT_SEQ_LENGTH;
        *settings++ = ASR_SETTING_INT_SEQ_DIR;
        *settings++ = ASR_SETTING_FRACTAL_SEQ_STRIDE;
        *settings++ = ASR_SETTING_INT_SEQ_CV_SOURCE;
       break;
     default:
      break;
    }

    num_enabled_settings_ = settings - enabled_settings_;
  }

  void updateASR_indexed(int32_t *_asr_buf, int32_t _sample, int16_t _index, bool _freeze, OC::IOFrame *ioframe) {
      int16_t _delay = _index, _offset;

      if (_freeze) {
        int32_t _buflen = get_buffer_length();
        if (get_cv4_destination() == ASR_DEST_BUFLEN) {
          _buflen += ioframe->cv.ScaledValue<64>(CVInput4);
          CONSTRAIN(_buflen, NUM_ASR_CHANNELS, ASR_HOLD_BUF_SIZE - 0x1);
        }
        _ASR.Freeze(_buflen);
      }
      else
        _ASR.Write(_sample);

      // update outputs:
      _offset = _delay;
      *(_asr_buf + DAC_CHANNEL_A) = _ASR.Poke(_offset++);
      // delay mechanics ...
      _delay = delay_type_ ? 0x0 : _delay;
      // continue updating
      _offset +=_delay;
      *(_asr_buf + DAC_CHANNEL_B) = _ASR.Poke(_offset++);
      _offset +=_delay;
      *(_asr_buf + DAC_CHANNEL_C) = _ASR.Poke(_offset++);
      _offset +=_delay;
      *(_asr_buf + DAC_CHANNEL_D) = _ASR.Poke(_offset++);
  }

  inline void update(OC::IOFrame *ioframe) {

     bool update = ioframe->digital_inputs.triggered<OC::DIGITAL_INPUT_1>();
     clock_display_.Update(1, update);

     trigger_delay_.Update();
     if (update)
      trigger_delay_.Push(OC::trigger_delay_ticks[get_trigger_delay()]);     
     update = trigger_delay_.triggered();

     if (update) {
         bool freeze = ioframe->digital_inputs.raised(OC::DIGITAL_INPUT_2);
         int8_t _root  = get_root();
         int32_t _index = get_index() + ioframe->cv.ScaledValue<64>(CVInput2);
         int8_t _octave = get_octave();
         int8_t _transpose = 0;
         int8_t _mult = get_mult();
         int32_t _pitch = ioframe->cv.pitch_values[CVInput1];
         int32_t _asr_buffer[NUM_ASR_CHANNELS];

         bool forced_update = force_update_;
         force_update_ = false;
         update_scale(forced_update, ioframe->cv.ScaledValue<16>(CVInput3));

         // cv4 destination, defaults to octave:
         switch(get_cv4_destination()) {
            case ASR_DEST_OCTAVE:
              _octave += ioframe->cv.ScaledValue<8>(CVInput4);
            break;
            case ASR_DEST_ROOT:
              _root += ioframe->cv.ScaledValue<16>(CVInput4);
              CONSTRAIN(_root, 0, 11);
            break;
            case ASR_DEST_TRANSPOSE:
              _transpose += ioframe->cv.ScaledValue<32>(CVInput4);
              CONSTRAIN(_transpose, -12, 12); 
            break;
            case ASR_DEST_INPUT_SCALING:
              _mult += ioframe->cv.ScaledValue<64>(CVInput4);
              CONSTRAIN(_mult, 0, OC::CVUtils::kMultSteps - 1);
            break;
            // CV for buffer length happens in updateASR_indexed
            default:
            break;
         }
         
         // freeze?
         freeze |= manual_freeze_;
         freeze_ = freeze;

         // use built in CV sources? 
         if (!freeze) {
           switch (get_cv_source()) {

            case ASR_CHANNEL_SOURCE_TURING:
              {
                int16_t _length = get_turing_length();
                int16_t _probability = get_turing_probability();

                // _pitch can do other things now --
                switch (get_turing_CV()) {

                    case 1: // mult
                     _mult += ((_pitch + 63) >> 7);
                    break;
                    case 2:  // LEN, 1-32
                     _length += ((_pitch + 255) >> 9);
                     CONSTRAIN(_length, 1, 32);
                    break;
                    case 3:  // P
                     _probability += ((_pitch + 7) >> 4);
                     CONSTRAIN(_probability, 0, 255);
                    break;
                    default:
                    break;
                }

                turing_machine_.set_length(_length);
                turing_machine_.set_probability(_probability);
                turing_display_length_ = _length;

                _pitch = turing_machine_.Clock();

                // scale LFSR output (0 - 4095) / compensate for length
                if (_length < 12)
                  _pitch = _pitch << (12 -_length);
                else
                  _pitch = _pitch >> (_length - 12);
                _pitch &= 0xFFF;
              }
              break;
              case ASR_CHANNEL_SOURCE_BYTEBEAT:
             {
               int32_t _bytebeat_eqn = get_bytebeat_equation() << 12;
               int32_t _bytebeat_p0 = get_bytebeat_p0() << 8;
               int32_t _bytebeat_p1 = get_bytebeat_p1() << 8;
               int32_t _bytebeat_p2 = get_bytebeat_p2() << 8;

                 // _pitch can do other things now --
                switch (get_bytebeat_CV()) {

                    case 1:  //  0-15
                     _bytebeat_eqn += (_pitch << 4);
                     _bytebeat_eqn = USAT16(_bytebeat_eqn);
                    break;
                    case 2:  // P0
                     _bytebeat_p0 += (_pitch << 4);
                     _bytebeat_p0 = USAT16(_bytebeat_p0);
                    break;
                    case 3:  // P1
                     _bytebeat_p1 += (_pitch << 4);
                     _bytebeat_p1 = USAT16(_bytebeat_p1);
                    break;
                    case 4:  // P4
                     _bytebeat_p2 += (_pitch << 4);
                     _bytebeat_p2 = USAT16(_bytebeat_p2);
                    break;
                    default: // mult
                     _mult += ((_pitch + 63) >> 7);
                    break;
                }

                bytebeat_.set_equation(_bytebeat_eqn);
                bytebeat_.set_p0(_bytebeat_p0);
                bytebeat_.set_p0(_bytebeat_p1);
                bytebeat_.set_p0(_bytebeat_p2);

                int32_t _bb = (static_cast<int16_t>(bytebeat_.Clock()) & 0xFFF);
                _pitch = _bb;
              }
              break;
              case ASR_CHANNEL_SOURCE_INTEGER_SEQUENCES:
             {
               int16_t _int_seq_index = get_int_seq_index() ;
               int16_t _int_seq_modulus = get_int_seq_modulus() ;
               int16_t _int_seq_start = get_int_seq_start() ;
               int16_t _int_seq_length = get_int_seq_length();
               int16_t _fractal_seq_stride = get_fractal_seq_stride();
               bool _int_seq_dir = get_int_seq_dir();

               // _pitch can do other things now --
               switch (get_int_seq_CV()) {

                  case 1:  // integer sequence, 0-8
                   _int_seq_index += ((_pitch + 255) >> 9);
                   CONSTRAIN(_int_seq_index, 0, 8);
                  break;
                   case 2:  // sequence start point, 0 to kIntSeqLen - 2
                   _int_seq_start += ((_pitch + 15) >> 5);
                   CONSTRAIN(_int_seq_start, 0, kIntSeqLen - 2);
                  break;
                   case 3:  // sequence loop length, 1 to kIntSeqLen - 1
                   _int_seq_length += ((_pitch + 15) >> 5);
                   CONSTRAIN(_int_seq_length, 1, kIntSeqLen - 1);
                  break;
                   case 4:  // fractal sequence stride length, 1 to kIntSeqLen - 1
                   _fractal_seq_stride += ((_pitch + 15) >> 5);
                   CONSTRAIN(_fractal_seq_stride, 1, kIntSeqLen - 1);
                  break;
                   case 5:  // fractal sequence modulus
                   _int_seq_modulus += ((_pitch + 15) >> 5);
                   CONSTRAIN(_int_seq_modulus, 2, 121);
                  break;
                  default: // mult
                   _mult += ((_pitch + 63) >> 7);
                  break;
                }

                int_seq_.set_loop_start(_int_seq_start);
                int_seq_.set_loop_length(_int_seq_length);
                int_seq_.set_int_seq(_int_seq_index);
                int_seq_.set_int_seq_modulus(_int_seq_modulus);
                int_seq_.set_loop_direction(_int_seq_dir);
                int_seq_.set_fractal_stride(_fractal_seq_stride);

                int32_t _is = (static_cast<int16_t>(int_seq_.Clock()) & 0xFFF);
                _pitch = _is;
              }
              break;
              default:
              break;
           }
         }
         else {
          // we hold, so we only need the multiplication CV:
            switch (get_cv_source()) {

              case ASR_CHANNEL_SOURCE_TURING:
                if (get_turing_CV() == 0x0)
                  _mult += ((_pitch + 63) >> 7);
              break;
              case ASR_CHANNEL_SOURCE_INTEGER_SEQUENCES:
                if (get_int_seq_CV() == 0x0)
                  _mult += ((_pitch + 63) >> 7);
              break;
              case ASR_CHANNEL_SOURCE_BYTEBEAT:
                if (get_bytebeat_CV() == 0x0)
                  _mult += ((_pitch + 63) >> 7);
              break;
              default:
              break;
           }
         }
         // limit gain factor.
         CONSTRAIN(_mult, 0, OC::CVUtils::kMultSteps - 1);
         // .. and index
         CONSTRAIN(_index, 0, ASR_HOLD_BUF_SIZE - 0x1);
         // push sample into ring-buffer and/or freeze buffer: 
         updateASR_indexed(_asr_buffer, _pitch, _index, freeze, ioframe);

         // get octave offset :
         if (ioframe->digital_inputs.raised<OC::DIGITAL_INPUT_3>())
            _octave++;
         else if (ioframe->digital_inputs.raised<OC::DIGITAL_INPUT_4>())
            _octave--;

         // quantize buffer outputs:
         for (int i = 0; i < NUM_ASR_CHANNELS; ++i) {
             int32_t _sample = _asr_buffer[i];
             _sample = OC::CVUtils::Attenuate(_sample, _mult);
             _sample = quantizer_.Process(_sample, _root << 7, _transpose);
             _sample = OC::PitchUtils::PitchAddOctaves(_sample, _octave);
             scrolling_history_[i].Push(_sample);
             _asr_buffer[i] = _sample;
         }

        // ... and write to DAC
        ioframe->outputs.set_pitch_values(_asr_buffer);       
        MENU_REDRAW = 0x1;
      }
      for (auto &sh : scrolling_history_)
        sh.Update();
  }

  uint8_t clockState() const {
    return clock_display_.getState();
  }

  const OC::vfx::ScrollingHistory<int32_t, kHistoryDepth> &history(int i) const {
    return scrolling_history_[i];
  }

private:
  bool force_update_;
  bool delay_type_;
  bool octave_toggle_;
  bool manual_freeze_;
  bool freeze_;
  int last_scale_;
  uint16_t last_mask_;
  braids::Quantizer quantizer_;
  OC::DigitalInputDisplay clock_display_;
  util::TriggerDelay<OC::kMaxTriggerDelayTicks> trigger_delay_;
  util::TuringShiftRegister turing_machine_;
  util::RingBuffer<ASR_pitch, ASR_MAX_ITEMS> _ASR;
  int8_t turing_display_length_;
  peaks::ByteBeat bytebeat_ ;
  util::IntegerSequence int_seq_ ;
  OC::vfx::ScrollingHistory<int32_t, kHistoryDepth> scrolling_history_[NUM_ASR_CHANNELS];
  int num_enabled_settings_;
  ASRSettings enabled_settings_[ASR_SETTING_LAST];

  // TOTAL EEPROM SIZE: 23 bytes
  SETTINGS_ARRAY_DECLARE() {{
    { OC::Scales::SCALE_SEMI, 0, OC::Scales::NUM_SCALES - 1, "Scale", OC::scale_names_short, settings::STORAGE_TYPE_U8 },
    { 0, -5, 5, "octave", NULL, settings::STORAGE_TYPE_I8 }, // octave
    { 0, 0, 11, "root", OC::Strings::note_names_unpadded, settings::STORAGE_TYPE_U8 },
    { 65535, 1, 65535, "mask", NULL, settings::STORAGE_TYPE_U16 }, // mask
    { 0, 0, ASR_HOLD_BUF_SIZE - 1, "buf.index", NULL, settings::STORAGE_TYPE_U8 },
    { OC::CVUtils::kMultOne, 0, OC::CVUtils::kMultSteps - 1, "input gain", OC::Strings::mult, settings::STORAGE_TYPE_U8 },
    { 0, 0, OC::kNumDelayTimes - 1, "trigger delay", OC::Strings::trigger_delay_times, settings::STORAGE_TYPE_U8 },
    { 4, 4, ASR_HOLD_BUF_SIZE - 1, "hold (buflen)", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, ASR_CHANNEL_SOURCE_LAST -1, "CV source", asr_input_sources, settings::STORAGE_TYPE_U4 },
    { 0, 0, ASR_DEST_LAST - 1, "CV4 dest. ->", asr_cv4_destinations, settings::STORAGE_TYPE_U4 },
    { 16, 1, 32, "> LFSR length", NULL, settings::STORAGE_TYPE_U8 },
    { 128, 0, 255, "> LFSR p", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 3, "> LFSR CV1", OC::Strings::TM_aux_cv_destinations, settings::STORAGE_TYPE_U8 }, // ??
    { 0, 0, 15, "> BB eqn", OC::Strings::bytebeat_equation_names, settings::STORAGE_TYPE_U8 },
    { 8, 1, 255, "> BB P0", NULL, settings::STORAGE_TYPE_U8 },
    { 12, 1, 255, "> BB P1", NULL, settings::STORAGE_TYPE_U8 },
    { 14, 1, 255, "> BB P2", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 4, "> BB CV1", bb_CV_destinations, settings::STORAGE_TYPE_U4 },
    { 0, 0, 9, "> IntSeq", OC::Strings::integer_sequence_names, settings::STORAGE_TYPE_U4 },
    { 24, 2, 121, "> IntSeq modul", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, kIntSeqLen - 2, "> IntSeq start", NULL, settings::STORAGE_TYPE_U8 },
    { 8, 2, kIntSeqLen, "> IntSeq len", NULL, settings::STORAGE_TYPE_U8 },
    { 1, 0, 1, "> IntSeq dir", OC::Strings::integer_sequence_dirs, settings::STORAGE_TYPE_U4 },
    { 1, 1, kIntSeqLen - 1, "> Fract stride", NULL, settings::STORAGE_TYPE_U8 },
    { 0, 0, 5, "> IntSeq CV1", int_seq_CV_destinations, settings::STORAGE_TYPE_U4 }
  }};
};
SETTINGS_ARRAY_DEFINE(ASR);

/* -------------------------------------------------------------------*/

namespace OC {

OC_APP_TRAITS(AppASR, TWOCCS("AS"), "CopierMaschine", "ASR");
class OC_APP_CLASS(AppASR) {
public:
  OC_APP_INTERFACE_DECLARE(AppASR);
  OC_APP_STORAGE_SIZE(ASR::storageSize());

private:

  ASR asr_;
  int left_encoder_value_;
  menu::ScreenCursor<menu::kScreenLines> cursor_;
  ScaleEditor scale_editor_;

  inline bool editing() const {
    return cursor_.editing();
  }

  inline int cursor_pos() const {
    return cursor_.cursor_pos();
  }

  void HandleTopButton();
  void HandleLowerButton();
  void HandleRightButton();
  void HandleLeftButton();
  void HandleLeftButtonLong();
  void HandleDownButtonLong();
};

void AppASR::Init() {
  asr_.InitDefaults();
  asr_.init();

  left_encoder_value_ = Scales::SCALE_SEMI;
  cursor_.Init(ASR_SETTING_SCALE, ASR_SETTING_LAST - 1);
  scale_editor_.Init(false);

  asr_.update_enabled_settings();
  cursor_.AdjustEnd(asr_.num_enabled_settings() - 1);
}

size_t AppASR::SaveAppData(util::StreamBufferWriter &stream_buffer) const {
  asr_.Save(stream_buffer);

  return stream_buffer.written();
}

size_t AppASR::RestoreAppData(util::StreamBufferReader &stream_buffer) {
  asr_.Restore(stream_buffer);

  // init nicely
  left_encoder_value_ = asr_.get_scale(); 
  asr_.set_scale(left_encoder_value_);
  asr_.clear_freeze();
  asr_.set_display_mask(asr_.get_scale_mask());
  asr_.update_enabled_settings();
  cursor_.AdjustEnd(asr_.num_enabled_settings() - 1);

  return stream_buffer.read();
}

void AppASR::HandleAppEvent(AppEvent event) {
  switch (event) {
    case APP_EVENT_RESUME:
      cursor_.set_editing(false);
      scale_editor_.Close();
      break;
    case APP_EVENT_SUSPEND:
    case APP_EVENT_SCREENSAVER_ON:
    case APP_EVENT_SCREENSAVER_OFF:
      asr_.update_enabled_settings();
      cursor_.AdjustEnd(asr_.num_enabled_settings() - 1);
      break;
  }
}

void AppASR::Loop() {
}

void AppASR::Process(IOFrame *ioframe) {
  asr_.update(ioframe);
}

void AppASR::GetIOConfig(IOConfig &ioconfig) const
{
  ioconfig.digital_inputs[DIGITAL_INPUT_1].set("Clock");
  ioconfig.digital_inputs[DIGITAL_INPUT_2].set("Freeze");
  ioconfig.digital_inputs[DIGITAL_INPUT_3].set("Oct+");
  ioconfig.digital_inputs[DIGITAL_INPUT_4].set("Oct-");

  const char *src = asr_input_sources[asr_.get_cv_source()];
  const char *mod = nullptr;
  switch (asr_.get_cv_source()) {
    case ASR_CHANNEL_SOURCE_TURING:
      mod = Strings::TM_aux_cv_destinations[asr_.get_turing_CV()];
      break;
    case ASR_CHANNEL_SOURCE_BYTEBEAT:
      mod = bb_CV_destinations[asr_.get_bytebeat_CV()];
      break;
    case ASR_CHANNEL_SOURCE_INTEGER_SEQUENCES:
      mod = int_seq_CV_destinations[asr_.get_int_seq_CV()];
      break;
    default:
      break;
  }
  auto &cv_ch1 = ioconfig.cv[CVInput1];
  if (mod)
    cv_ch1.set_printf("*%s:%s", src, mod);
  else
    cv_ch1.set_printf("S&H");
  ioconfig.cv[CVInput2].set("*index");
  ioconfig.cv[CVInput3].set("*scale");
  ioconfig.cv[CVInput4].set_printf("*%s", asr_cv4_destinations[asr_.get_cv4_destination()]);

  ioconfig.outputs[DAC_CHANNEL_A].set("ASR Tap1", OUTPUT_MODE_PITCH);
  ioconfig.outputs[DAC_CHANNEL_B].set("ASR Tap2", OUTPUT_MODE_PITCH);
  ioconfig.outputs[DAC_CHANNEL_C].set("ASR Tap3", OUTPUT_MODE_PITCH);
  ioconfig.outputs[DAC_CHANNEL_D].set("ASR Tap4", OUTPUT_MODE_PITCH);
}

void AppASR::HandleButtonEvent(const UI::Event &event) {
  if (scale_editor_.active()) {
    scale_editor_.HandleButtonEvent(event);
    return;
  }

  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
      case CONTROL_BUTTON_UP:
        HandleTopButton();
        break;
      case CONTROL_BUTTON_DOWN:
        HandleLowerButton();
        break;
      case CONTROL_BUTTON_L:
        HandleLeftButton();
        break;
      case CONTROL_BUTTON_R:
        HandleRightButton();
        break;
    }
  } else if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
    if (CONTROL_BUTTON_L == event.control)
      HandleLeftButtonLong();
    else if (CONTROL_BUTTON_DOWN == event.control)
      HandleDownButtonLong();  
  }
}

void AppASR::HandleEncoderEvent(const UI::Event &event) {

  if (scale_editor_.active()) {
    scale_editor_.HandleEncoderEvent(event);
    return;
  }

  if (CONTROL_ENCODER_L == event.control) {
    
    int value = left_encoder_value_ + event.value;
    CONSTRAIN(value, 0, Scales::NUM_SCALES - 1);
    left_encoder_value_ = value;
    
  } else if (CONTROL_ENCODER_R == event.control) {

    if (editing()) {  

    ASRSettings setting = asr_.enabled_setting_at(cursor_pos());

    if (ASR_SETTING_MASK != setting) {

          if (asr_.change_value(setting, event.value))
             asr_.force_update();
             
          switch(setting) {

            case ASR_SETTING_CV_SOURCE:
              asr_.update_enabled_settings();
              cursor_.AdjustEnd(asr_.num_enabled_settings() - 1);
              // hack/hide extra options when default CV source is selected
              if (!asr_.get_cv_source()) 
                cursor_.Scroll(cursor_pos());
            break;
            default:
            break;
          }
        }
    } else {
      cursor_.Scroll(event.value);
    }
  }
}


void AppASR::HandleTopButton() {
  if (asr_.octave_toggle())
    asr_.change_value(ASR_SETTING_OCTAVE, 1);
  else 
    asr_.change_value(ASR_SETTING_OCTAVE, -1);
}

void AppASR::HandleLowerButton() {
   asr_.toggle_manual_freeze();
}

void AppASR::HandleRightButton() {

  switch (asr_.enabled_setting_at(cursor_pos())) {

      case ASR_SETTING_MASK: {
        int scale = asr_.get_scale();
        if (Scales::SCALE_NONE != scale)
          scale_editor_.Edit(&asr_, scale);
        }
      break;
      default:
        cursor_.toggle_editing();
      break;  
  }
}

void AppASR::HandleLeftButton() {

  if (left_encoder_value_ != asr_.get_scale())
    asr_.set_scale(left_encoder_value_);
}

void AppASR::HandleLeftButtonLong() {

  int scale = left_encoder_value_;
  asr_.set_scale(left_encoder_value_);
  if (scale != Scales::SCALE_NONE) 
      scale_editor_.Edit(&asr_, scale);
}

void AppASR::HandleDownButtonLong() {
   asr_.toggle_delay_mechanics();
}

void AppASR::DrawMenu() const {

  menu::TitleBar<0, 4, 0>::Draw(io_settings_status_mask());

  int scale = left_encoder_value_;
  graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
  graphics.print(scale_names[scale]);

  if (asr_.get_scale() == scale)
    graphics.drawBitmap8(1, menu::QuadTitleBar::kTextY, 4, bitmap_indicator_4x8);

  if (asr_.freeze_state())
    graphics.drawBitmap8(102, menu::QuadTitleBar::kTextY, 4, bitmap_hold_indicator_4x8);  

  if (asr_.poke_octave_toggle()) {
    graphics.setPrintPos(110, 2);
    graphics.print("+");
  }

  if (asr_.get_delay_type())
    graphics.drawBitmap8(118, menu::QuadTitleBar::kTextY, 4, bitmap_hold_indicator_4x8);
  else 
    graphics.drawBitmap8(118, menu::QuadTitleBar::kTextY, 4, bitmap_indicator_4x8);

  uint8_t clock_state = (asr_.clockState() + 3) >> 2;
  if (clock_state)
    graphics.drawBitmap8(124, 2, 4, bitmap_gate_indicators_8 + (clock_state << 2));

  menu::SettingsList<menu::kScreenLines, 0, menu::kDefaultValueX> settings_list(cursor_);
  menu::SettingsListItem list_item;

  while (settings_list.available()) {

    const int setting = asr_.enabled_setting_at(settings_list.Next(list_item));
    const int value = asr_.get_value(setting);
    const auto &attr = ASR::value_attributes(setting); 

    switch (setting) {

      case ASR_SETTING_MASK:
      menu::DrawMask<false, 16, 8, 1>(menu::kDisplayWidth, list_item.y, asr_.get_rotated_mask(), Scales::GetScale(asr_.get_scale()).num_notes);
      list_item.DrawNoValue<false>(value, attr);
      break;
      case ASR_SETTING_CV_SOURCE:
      if (asr_.get_cv_source() == ASR_CHANNEL_SOURCE_TURING) {
       
          int turing_length = asr_.get_turing_display_length();
          int w = turing_length >= 16 ? 16 * 3 : turing_length * 3;

          menu::DrawMask<true, 16, 8, 1>(menu::kDisplayWidth, list_item.y, asr_.get_shift_register(), turing_length);
          list_item.valuex = menu::kDisplayWidth - w - 1;
          list_item.DrawNoValue<true>(value, attr);
       } else
       list_item.DrawDefault(value, attr);
      break;
      default:
      list_item.DrawDefault(value, attr);
      break;
    }
  }
  if (scale_editor_.active())
    scale_editor_.Draw(); 
}

void AppASR::DrawScreensaver() const {

// Possible variants (w x h)
// 4 x 32x64 px
// 4 x 64x32 px
// "Somehow" overlapping?
// Normalize history to within one octave? That would make steps more visisble for small ranges
// "Zoomed view" to fit range of history...

  int32_t channel_history[ASR::kHistoryDepth];
  for (int i = 0; i < NUM_ASR_CHANNELS; ++i) {
    asr_.history(i).Read(channel_history);

    std::transform(
        std::begin(channel_history), std::end(channel_history),
        std::begin(channel_history),
        [](int32_t pitch) { return IO::pitch_rel_to_abs(pitch); } );

    uint32_t scroll_pos = asr_.history(i).get_scroll_pos() >> 5;

    int pos = 0;
    weegfx::coord_t x = i * 32, y;

    // Was: 10oct => 65536 / 1024 = 64px
    // Now: 10oct => 1536 x 10 / 256 = 60px

    y = 64 - ((channel_history[pos++] >> 8) & 0x3f);
    graphics.drawHLine(x, y, scroll_pos);
    x += scroll_pos;
    graphics.drawVLine(x, y, 3);

    weegfx::coord_t last_y = y;
    for (int c = 0; c < 3; ++c) {
      y = 64 - ((channel_history[pos++] >> 8) & 0x3f);
      graphics.drawHLine(x, y, 8);
      if (y == last_y)
        graphics.drawVLine(x, y, 2);
      else if (y < last_y)
        graphics.drawVLine(x, y, last_y - y + 1);
      else
        graphics.drawVLine(x, last_y, y - last_y + 1);
      x += 8;
      last_y = y;
//      graphics.drawVLine(x, y, 3);

    }

    y = 64 - ((channel_history[pos++] >> 8) & 0x3f);
//    graphics.drawHLine(x, y, 8 - scroll_pos);
    graphics.drawRect(x, y, 8 - scroll_pos, 2);
    if (y == last_y)
      graphics.drawVLine(x, y, 3);
    else if (y < last_y)
      graphics.drawVLine(x, y, last_y - y + 1);
    else
      graphics.drawVLine(x, last_y, y - last_y + 1);
//    x += 8 - scroll_pos;
//    graphics.drawVLine(x, y, 3);
  }
}

void AppASR::DrawDebugInfo() const {
#ifdef ASR_DEBUG
  for (int i = 0; i < 1; ++i) { 
    uint8_t ypos = 10*(i + 1) + 2 ; 
    graphics.setPrintPos(2, ypos);
    graphics.print(asr_.get_int_seq_i());
    graphics.setPrintPos(32, ypos);
    graphics.print(asr_.get_int_seq_l());
    graphics.setPrintPos(62, ypos);
    graphics.print(asr_.get_int_seq_j());
    graphics.setPrintPos(92, ypos);
    graphics.print(asr_.get_int_seq_k());
    graphics.setPrintPos(122, ypos);
    graphics.print(asr_.get_int_seq_n());
 }
#endif // ASR_DEBUG
}

} // namespace OC
