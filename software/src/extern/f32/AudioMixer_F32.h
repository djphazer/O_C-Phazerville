/*
 * AudioMixer
 * 
 * AudioMixer4
 * Created: Patrick Radius, December 2016
 * Purpose: Mix up to 4 audio channels with individual gain controls.
 * Assumes floating-point data.
 *          
 * This processes a single stream fo audio data (ie, it is mono)       
 *
 * Extended to AudioMixer8
 * By: Chip Audette, OpenAudio, Feb 2017
 *          
 * MIT License.  use at your own risk.
*/

#ifndef AUDIOMIXER_F32_H
#define AUDIOMIXER_F32_H

#include <arm_math.h> 
#include <AudioStream_F32.h>

class AudioMixer4_F32 : public AudioStream_F32 {
//GUI: inputs:4, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:Mixer4
public:
    AudioMixer4_F32() : AudioStream_F32(4, inputQueueArray) { setDefaultValues(); }
	AudioMixer4_F32(const AudioSettings_F32 &settings) : AudioStream_F32(4, inputQueueArray) { setDefaultValues(); }
	
	void setDefaultValues(void) {
		for (int i=0; i<4; i++) multiplier[i] = 1.0;
	}
	
    virtual void update(void);

    void gain(unsigned int channel, float gain) {
      if (channel >= 4 || channel < 0) return;
      multiplier[channel] = gain;
    }

  private:
    audio_block_f32_t *inputQueueArray[4];
    float multiplier[4];
};

class AudioMixer8_F32 : public AudioStream_F32 {
//GUI: inputs:8, outputs:1  //this line used for automatic generation of GUI node
//GUI: shortName:Mixer8
public:
    AudioMixer8_F32() : AudioStream_F32(8, inputQueueArray) { setDefaultValues();}
    AudioMixer8_F32(const AudioSettings_F32 &settings) : AudioStream_F32(8, inputQueueArray) { setDefaultValues();}
	
	void setDefaultValues(void) {
      for (int i=0; i<8; i++) multiplier[i] = 1.0;
    }

    virtual void update(void);

    void gain(unsigned int channel, float gain) {
      if (channel >= 8 || channel < 0) return;
      multiplier[channel] = gain;
    }

  private:
    audio_block_f32_t *inputQueueArray[8];
    float multiplier[8];
};

#endif