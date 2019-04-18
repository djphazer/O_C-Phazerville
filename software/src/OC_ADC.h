#ifndef OC_ADC_H_
#define OC_ADC_H_

#include <algorithm>
#include <limits>
#include <stdint.h>
#include <string.h>

#include "src/drivers/ADC/OC_util_ADC.h"
#include "OC_config.h"
#include "OC_options.h"

// If enabled, use an interrupt to track DMA completion; otherwise use polling
//#define OC_ADC_ENABLE_DMA_INTERRUPT

// Grmpfarglbarg
#undef max
#undef min

enum ADC_CHANNEL {
  ADC_CHANNEL_1,
  ADC_CHANNEL_2,
  ADC_CHANNEL_3,
  ADC_CHANNEL_4,
#if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  ADC_CHANNEL_5,
  ADC_CHANNEL_6,
  ADC_CHANNEL_7,
  ADC_CHANNEL_8,
#endif
  ADC_CHANNEL_LAST,
};

#define DMA_BUF_SIZE 16
#define DMA_NUM_CH 4

namespace OC {

class ADC {
public:

  static constexpr uint8_t kAdcResolution = 12;
  static constexpr uint32_t kAdcSmoothing = 4;
  static constexpr uint32_t kAdcSmoothBits = 8; // fractional bits for smoothing
  static constexpr uint16_t kDefaultPitchCVScale = SEMITONES << 7;

  // These values should be tweaked so startSingleRead/readSingle run in main ISR update time
  // 16 bit has best-case 13 bits useable, but we only want 12 so we discard 4 anyway
  static constexpr uint8_t kAdcScanResolution = 16;
  static constexpr uint8_t kAdcScanAverages = 4;
  static constexpr uint8_t kAdcSamplingSpeed = ADC_HIGH_SPEED_16BITS;
  static constexpr uint8_t kAdcConversionSpeed = ADC_HIGH_SPEED;
  static constexpr uint32_t kAdcValueShift = kAdcSmoothBits;


  struct CalibrationData {
    uint16_t offset[ADC_CHANNEL_LAST];
    uint16_t pitch_cv_scale;
    int16_t pitch_cv_offset;
  };

  struct ChannelStats {
    int32_t min, max;
    void Reset() {
      min = std::numeric_limits<int32_t>::max();
      max = std::numeric_limits<int32_t>::min();
    }

    void Push(int32_t value) {
      min = std::min(value, min);
      max = std::max(value, max);
    }
  };
  
  static void Init(CalibrationData *calibration_data);
  #if defined(__IMXRT1062__) && defined(ARDUINO_TEENSY41)
  static void ADC33131D_Vref_calibrate();
  #endif
  static void Init_DMA();
  static void DMA_ISR();
  static void Scan_DMA();

  template <ADC_CHANNEL channel>
  static int32_t value() {
    return calibration_data_->offset[channel] - (smoothed_[channel] >> kAdcValueShift);
  }

  static int32_t value(ADC_CHANNEL channel) {
    return calibration_data_->offset[channel] - (smoothed_[channel] >> kAdcValueShift);
  }

  static int32_t unsmoothed_value(ADC_CHANNEL channel) {
    return calibration_data_->offset[channel] - (raw_[channel] >> kAdcValueShift);
  }

  static uint32_t raw_value(ADC_CHANNEL channel) {
    return raw_[channel] >> kAdcValueShift;
  }

  static uint32_t smoothed_raw_value(ADC_CHANNEL channel) {
    return smoothed_[channel] >> kAdcValueShift;
  }

  static int32_t pitch_value(ADC_CHANNEL channel) {
    return (value(channel) * calibration_data_->pitch_cv_scale) >> 12;
  }

  static int32_t unsmoothed_pitch_value(ADC_CHANNEL channel) {
    int32_t value = calibration_data_->offset[channel] - raw_value(channel);
    return (value * calibration_data_->pitch_cv_scale) >> 12;
  }

  static void CalibratePitch(int32_t c2, int32_t c4);

  static float Read_ID_Voltage();

#ifdef OC_DEBUG_ADC_STATS
  static const ChannelStats &get_channel_stats(ADC_CHANNEL channel) {
    return channel_stats_[channel];
  }
#endif

private:

  template <ADC_CHANNEL channel>
  static void update(uint32_t value) {
    value = (value  >> (kAdcScanResolution - kAdcResolution)) << kAdcSmoothBits;
    raw_[channel] = value;
#ifdef OC_DEBUG_ADC_STATS
    if (stats_ticks_ & 0x3fff)
      channel_stats_[channel].Push(value >> kAdcValueShift);
    else
      channel_stats_[channel].Reset();
#endif
    // division should be shift if kAdcSmoothing is power-of-two
    value = (smoothed_[channel] * (kAdcSmoothing - 1) + value) / kAdcSmoothing;
    smoothed_[channel] = value;
  }

  static ::ADC adc_;
  static CalibrationData *calibration_data_;

  static uint32_t raw_[ADC_CHANNEL_LAST];
  static uint32_t smoothed_[ADC_CHANNEL_LAST];

#ifdef OC_ADC_ENABLE_DMA_INTERRUPT
  static volatile bool ready_;
#endif

#ifdef OC_DEBUG_ADC_STATS
  static ChannelStats channel_stats_[ADC_CHANNEL_LAST];
  static uint32_t stats_ticks_;
#endif

  /*  
   *   below: channel ids for the ADCx_SCA register: we have 4 inputs
   *   CV1 (19) = A5 = 0x4C; CV2 (18) = A4 = 0x4D; CV3 (20) = A6 = 0x46; CV4 (17) = A3 = 0x49
   *   for some reason the IDs must be in order: CV2, CV3, CV4, CV1 resp. (when flipped) CV3, CV2, CV1, CV4
  */
  #ifdef FLIP_180
  static constexpr uint16_t SCA_CHANNEL_ID[DMA_NUM_CH] = { 0x46, 0x4D, 0x4C, 0x49 };
  #else
  static constexpr uint16_t SCA_CHANNEL_ID[DMA_NUM_CH] = { 0x4D, 0x46, 0x49, 0x4C };
  #endif
};

}; // namespace OC

#endif // OC_ADC_H_
