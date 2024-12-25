#ifndef OC_DAC_H_
#define OC_DAC_H_

#include <algorithm>
#include <iterator>
#include <stdint.h>
#include <string.h>
#include "OC_config.h"
#include "OC_options.h"
#include "OC_gpio.h"
#include "util/util_math.h"
#include "util/util_macros.h"
#if defined(__IMXRT1062__)
#include <SPI.h>
#endif

#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
static inline void dac8568_raw_write(uint32_t data) {
  LPSPI4_TDR = data; // assume writes always at pace SPI FIFO can absorb
}
static inline void dac8568_set_channel(uint32_t channel, uint32_t data) {
  dac8568_raw_write(0x03000000 | ((channel & 0x07) << 20) | ((data & 0xFFFF) << 4));
}
#endif

class DAC8565 {
public:

  static constexpr uint16_t kMaxValue = 65535U;

#if defined(__IMXRT1062__)
  inline static void WriteChannelA(uint32_t data) {
    LPSPI4_TCR = (LPSPI4_TCR & 0xF8000000) | LPSPI_TCR_FRAMESZ(23)
      | LPSPI_TCR_PCS(0) | LPSPI_TCR_RXMSK;
    Write(kChannelCommand[0], data);
  }
  inline static void WriteChannelB(uint32_t data) { Write(kChannelCommand[1], data); }
  inline static void WriteChannelC(uint32_t data) { Write(kChannelCommand[2], data); }
  inline static void WriteChannelD(uint32_t data) {
    LPSPI4_SR = LPSPI_SR_TCF; //  clear transmit complete flag before last write to FIFO
    Write(kChannelCommand[3], data);
  }
#else
  inline static void WriteChannelA(uint32_t data) { Write(kChannelCommand[0], data); }
  inline static void WriteChannelB(uint32_t data) { Write(kChannelCommand[1], data); }
  inline static void WriteChannelC(uint32_t data) { Write(kChannelCommand[2], data); }
  inline static void WriteChannelD(uint32_t data) { Write(kChannelCommand[3], data); }
#endif

private:
  static void Write(uint32_t cmd, uint32_t data);

  static constexpr uint32_t kChannelCommand[4] = {
#ifdef FLIP_180
    0b00010110, 0b00010100, 0b00010010, 0b00010000 
#else
    0b00010000, 0b00010010, 0b00010100, 0b00010110
#endif
  };
};

extern void SPI_init();

using DAC_CHANNEL = int;

extern DAC_CHANNEL DAC_CHANNEL_A, DAC_CHANNEL_B, DAC_CHANNEL_C, DAC_CHANNEL_D;
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
extern DAC_CHANNEL DAC_CHANNEL_E, DAC_CHANNEL_F, DAC_CHANNEL_G, DAC_CHANNEL_H;
#endif

// backward compat [deprecated]
static constexpr int DAC_CHANNEL_LAST = DAC_CHANNEL_COUNT;

namespace OC {

enum OutputVoltageScaling {
  VOLTAGE_SCALING_1V_PER_OCT,    // 0
  VOLTAGE_SCALING_CARLOS_ALPHA,  // 1
  VOLTAGE_SCALING_CARLOS_BETA,   // 2
  VOLTAGE_SCALING_CARLOS_GAMMA,  // 3
  VOLTAGE_SCALING_BOHLEN_PIERCE, // 4
  VOLTAGE_SCALING_QUARTERTONE,   // 5
  VOLTAGE_SCALING_1_2V_PER_OCT,  // 6
  VOLTAGE_SCALING_2V_PER_OCT,    // 7
  VOLTAGE_SCALING_LAST  
} ;


class DAC {
public:
  static constexpr size_t kHistoryDepth = 8;
  static constexpr uint16_t MAX_VALUE = DAC8565::kMaxValue; // DAC fullscale 

#if defined(ARDUINO_TEENSY41) || defined(VOR)
  static int kOctaveZero;
#elif defined(NORTHERNLIGHT)
  static constexpr int kOctaveZero = 0;
#else
  static constexpr int kOctaveZero = 3;
#endif
#if defined(VOR)
  static constexpr int VBiasUnipolar = 3900;   // onboard DAC @ Vref 1.2V (internal), 1.75x gain
  static constexpr int VBiasBipolar = 2000;    // onboard DAC @ Vref 1.2V (internal), 1.75x gain
  static constexpr int VBiasAsymmetric = 2760; // onboard DAC @ Vref 1.2V (internal), 1.75x gain
#endif

#ifdef NORTHERNLIGHT
  static constexpr int32_t kMillvoltsPerOctave = 1200;
  static constexpr int32_t kOctaveGateHigh = 8;
#else
  static constexpr int32_t kMillvoltsPerOctave = 1000;
  static constexpr int32_t kOctaveGateHigh = 9;
#endif

  #if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  static void DAC8568_Vref_enable();
  #endif

  // Base calibration data for each octave on each channel
  struct CalibrationData {
    // -3V to +7V or 0V to +10V
    uint16_t calibrated_octaves[DAC_CHANNEL_COUNT][OCTAVES + 1];

    // TODO: -10V to +10V for updated hardware
    //uint16_t calibrated_octaves[DAC_CHANNEL_COUNT][2*OCTAVES + 1];
    // An alternate approach would be to store 16-bit values for one channel,
    // and 8-bit offsets for all other channels
  };

  // Making this into a distinct type might avoid some unintentional blunders.
  struct AutotuneChannelCalibrationData {
    bool valid;
    uint16_t calibrated_octaves[OCTAVES + 1];

    void set_calibration_point(int octave, uint16_t calibration_point) {
      calibrated_octaves[octave] = calibration_point;
    }

    void CopyFrom(const uint16_t *src) {
      std::copy(src, src + OCTAVES + 1, calibrated_octaves);
      valid = true;
    }

    void Reset() {
      valid = false;
      std::fill(std::begin(calibrated_octaves), std::end(calibrated_octaves), 0);
    }

    bool is_valid() const {
      return valid;
    }
  };

  struct AutotuneCalibrationData {
    AutotuneChannelCalibrationData channels[DAC_CHANNEL_COUNT];

    void Reset() {
      for (auto &ch : channels) ch.Reset();
    }

    uint32_t valid_mask() const {
      uint32_t mask = 0;
      for (auto &ch : channels) {
        mask >>= 8;
        mask |= (ch.is_valid() ? 0xFF000000 : 0xF7000000);
      }
      return mask;
    }
  };

  // Init internals and SPI interface
  //
  // Any changes to calibration_data and autotune_calibration_data are managed
  // externally.
  static void Init(const CalibrationData *calibration_data, const AutotuneCalibrationData *autotune_calibration_data, bool flip180 = false);

  static void set_Vbias(uint32_t data);
  static void init_Vbias();
  
  static void set_all(uint32_t value) {
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i)
      values_[i] = USAT16(value);
  }

  template <DAC_CHANNEL &channel>
  static void set(uint32_t value) {
    values_[channel] = USAT16(value);
  }

  static void set(DAC_CHANNEL channel, uint32_t value) {
    values_[channel] = USAT16(value);
  }

  static uint32_t value(size_t index) {
    return values_[index];
  }

  // Calculate DAC value from pitch value with 12 * 128 bit per octave for a
  // specific channel.
  // 0 = C1 = 0V, C2 = 24 << 7 = 1V etc.
  // Respects channel's current scaling settings and calibration data.
  // If autotune_enabled is true, use the autotune calibration if it's valid,
  // otherwise use the default values (which are assumed to be sane).
  //
  // @return DAC output value
  template <DAC_CHANNEL &channel>
  static int32_t PitchToScaledDAC(int32_t pitch, OutputVoltageScaling scaling, bool autotune_enabled)
  {
    pitch = Scale(pitch, scaling);
    pitch += kOctaveZero * 12 << 7;
    CONSTRAIN(pitch, 0, (120 << 7));

    const int32_t octave = pitch / (12 << 7);
    const int32_t fractional = pitch - octave * (12 << 7);

    const uint16_t *calibrated_octaves =
      (!autotune_enabled || !autotune_calibration_data_->channels[channel].is_valid())
      ? calibration_data_->calibrated_octaves[channel]
      : autotune_calibration_data_->channels[channel].calibrated_octaves;

    int32_t sample = calibrated_octaves[octave];
    if (fractional) {
      int32_t span = calibrated_octaves[octave + 1] - sample;
      sample += (fractional * span) / (12 << 7);
    }
    return sample;
  }

  // Scale output value given channel scaling
  static int32_t Scale(int32_t pitch, OutputVoltageScaling scaling)
  {
    switch (scaling) {
      case VOLTAGE_SCALING_1V_PER_OCT:    // 1V/oct
          break;
      case VOLTAGE_SCALING_CARLOS_ALPHA:  // Wendy Carlos alpha scale - scale by 0.77995
          pitch = (pitch * 25548) >> 15;  // 2^15 * 0.77995 = 25547.571
          break;
      case VOLTAGE_SCALING_CARLOS_BETA:   // Wendy Carlos beta scale - scale by 0.63833 
          pitch = (pitch * 20917) >> 15;  // 2^15 * 0.63833 = 20916.776
          break;
      case VOLTAGE_SCALING_CARLOS_GAMMA:  // Wendy Carlos gamma scale - scale by 0.35099
          pitch = (pitch * 11501) >> 15;  // 2^15 * 0.35099 = 11501.2403
          break;
      case VOLTAGE_SCALING_BOHLEN_PIERCE: // Bohlen-Pierce macrotonal scale - scale by 1.585
          pitch = (pitch * 25969) >> 14;  // 2^14 * 1.585 = 25968.64
          break;
      case VOLTAGE_SCALING_QUARTERTONE:   // Quartertone scaling (just down-scales to 0.5V/oct)
          pitch = pitch >> 1;
          break;
      case VOLTAGE_SCALING_1_2V_PER_OCT:  // 1.2V/oct
          pitch = (pitch * 19661) >> 14;  // 2^14 * 1.2 = 19660,8
          break;
      case VOLTAGE_SCALING_2V_PER_OCT:    // 2V/oct
          pitch = pitch << 1;
          break;
      default: 
          break;
    }
    return pitch;
  }

  template <DAC_CHANNEL &channel>
  static int32_t GateToDAC(int32_t value)
  {
    return calibration_data_->calibrated_octaves[channel][value ? kOctaveGateHigh : kOctaveZero];
  }

  // Set integer voltage value, where 0 = 0V, 1 = 1V
  static void set_octave(DAC_CHANNEL channel, int v) {
    set(channel, calibration_data_->calibrated_octaves[channel][kOctaveZero + v]);
  }

  // Set all channels to integer voltage value, where 0 = 0V, 1 = 1V
  static void set_all_octave(int v) {
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i)
      set_octave(DAC_CHANNEL(i), v);
  }

  static uint32_t get_zero_offset(DAC_CHANNEL channel) {
    return calibration_data_->calibrated_octaves[channel][kOctaveZero];
  }

  static uint32_t get_octave_offset(DAC_CHANNEL channel, int octave) {
    return calibration_data_->calibrated_octaves[channel][kOctaveZero + octave];
  }

  static uint16_t get_unipolar_max(DAC_CHANNEL channel) {
    return MAX_VALUE - calibration_data_->calibrated_octaves[channel][kOctaveZero];
  }

  static void Update() {
    #if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
      if (DAC8568_Uses_SPI) {
        if (NorthernLightModular) {
          dac8568_set_channel(0, values_[0]);
          dac8568_set_channel(1, values_[1]);
          dac8568_set_channel(2, values_[2]);
          dac8568_set_channel(3, values_[3]);
          dac8568_set_channel(4, values_[4]);
          dac8568_set_channel(5, values_[5]);
          dac8568_set_channel(6, values_[6]);
          dac8568_set_channel(7, values_[7]);
        } else {
          dac8568_set_channel(0, MAX_VALUE - values_[0]);
          dac8568_set_channel(1, MAX_VALUE - values_[1]);
          dac8568_set_channel(2, MAX_VALUE - values_[2]);
          dac8568_set_channel(3, MAX_VALUE - values_[3]);
          dac8568_set_channel(4, MAX_VALUE - values_[4]);
          dac8568_set_channel(5, MAX_VALUE - values_[5]);
          dac8568_set_channel(6, MAX_VALUE - values_[6]);
          dac8568_set_channel(7, MAX_VALUE - values_[7]);
        }
      } else {
    #endif
        DAC8565::WriteChannelA(values_[0]);
        DAC8565::WriteChannelB(values_[1]);
        DAC8565::WriteChannelC(values_[2]);
        DAC8565::WriteChannelD(values_[3]);
    #if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
      }
    #endif

    size_t tail = history_tail_;
    for (int i = 0; i < DAC_CHANNEL_COUNT; ++i)
      history_[i][tail] = values_[i];
    history_tail_ = (tail + 1) % kHistoryDepth;
  }

  static void getHistory(int channel, uint16_t *dst){
    size_t head = (history_tail_ + 1) % kHistoryDepth;

    size_t count = kHistoryDepth - head;
    const uint16_t *src = history_[channel] + head;
    while (count--)
      *dst++ = *src++;

    count = head;
    src = history_[channel];
    while (count--)
      *dst++ = *src++;
  }

private:
  static const CalibrationData *calibration_data_;
  static const AutotuneCalibrationData *autotune_calibration_data_;

  static uint32_t values_[DAC_CHANNEL_COUNT];
  static uint16_t history_[DAC_CHANNEL_COUNT][kHistoryDepth];
  static volatile size_t history_tail_;
};

}; // namespace OC

#endif // OC_DAC_H_
