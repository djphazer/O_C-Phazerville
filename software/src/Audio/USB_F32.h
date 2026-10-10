#pragma once
// USB audio <-> float32 bridge for the high-speed USB audio transport in the
// framework fork. The stock AudioInputUSB/AudioOutputUSB truncate to int16
// when AUDIO_SUBSLOT_SIZE==3 (24-bit on the bus, LSB dropped/zeroed in
// usb_audio.cpp). These classes install float-block callbacks against the
// same USBAudioIn/OutInterface transport (async endpoints, rate feedback),
// preserving full 24-bit fidelity into audio_block_f32_t.
//
// Constraint inherited from the transport: the callbacks are static, so at
// most ONE flavor of USB audio input (int16 OR F32) and one of output may be
// instantiated per firmware.

#include "usb_desc.h" // defines AUDIO_INTERFACE for USB audio builds

#if defined(ARDUINO_TEENSY41) && defined(AUDIO_INTERFACE)

#include "usb_audio_interface.h"
#include "AudioStream_F32.h"

class AudioInputUSB_F32 : public AudioStream_F32 {
public:
  AudioInputUSB_F32(float kp = 400.f, float ki = .2f);
  virtual void update(void);
  float getBufferedSamples() const { return _usbInterface.getBufferedSamples(); }
  float getBufferedSamplesSmooth() const { return _usbInterface.getBufferedSamplesSmooth(); }
  float getRequestedSamplingFrequ() const { return _usbInterface.getRequestedSamplingFrequ(); }
  float getActualBIntervalUs() const { return _usbInterface.getActualBIntervalUs(); }
  USBAudioInInterface::Status getStatus() const { return _usbInterface.getStatus(); }
  float volume(void) { return _usbInterface.volume(); }

private:
  static void copy_to_buffers(const uint8_t *src, uint16_t bIdx, uint16_t noChannels, unsigned int count, unsigned int len);
  static bool setBlockQuite(uint16_t bIdx, uint16_t channel);
  static void releaseBlock(uint16_t bIdx, uint16_t channel);
  static bool allocateBlock(uint16_t bIdx, uint16_t channel);
  static bool areBlocksReady(uint16_t bIdx, uint16_t noChannels);

  static audio_block_f32_t *rxBuffer[USBAudioInInterface::ringRxBufferSize][USB_AUDIO_MAX_NO_CHANNELS];
  USBAudioInInterface _usbInterface;
};

class AudioOutputUSB_F32 : public AudioStream_F32 {
public:
  AudioOutputUSB_F32(void);
  AudioOutputUSB_F32(int nch);
  virtual void update(void);
  float getBufferedSamples() const { return _usbInterface.getBufferedSamples(); }
  float getBufferedSamplesSmooth() const { return _usbInterface.getBufferedSamplesSmooth(); }
  float getActualBIntervalUs() const { return _usbInterface.getActualBIntervalUs(); }
  USBAudioOutInterface::Status getStatus() const { return _usbInterface.getStatus(); }

private:
  static void copy_from_buffers(uint8_t *dst, uint16_t bIdx, uint16_t noChannels, unsigned int count, unsigned int len);
  static void releaseBlocks(uint16_t bIdx, uint16_t noChannels);
  static bool isBlockReady(uint16_t bIdx, uint16_t channel);

  static audio_block_f32_t *txBuffer[USBAudioOutInterface::ringTxBufferSize][USB_AUDIO_MAX_NO_CHANNELS];
  audio_block_f32_t *inputQueueArray_f32[USB_AUDIO_MAX_NO_CHANNELS];
  USBAudioOutInterface _usbInterface;
};

#endif // ARDUINO_TEENSY41 && AUDIO_INTERFACE
