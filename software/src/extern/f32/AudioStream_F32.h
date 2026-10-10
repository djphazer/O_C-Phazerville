/*
 * AudioStream_F32
 *
 * Created: Chip Audette, November 2016
 * Purpose; Extend the Teensy Audio Library's "AudioStream" to permit floating-point audio data.
 *
 * I modeled it directly on the Teensy code in "AudioStream.h" and "AudioStream.cpp", which are
 * available here: https://github.com/PaulStoffregen/cores/tree/master/teensy3
 *
 * Added id to audio_block_f32_t class, per Tympan.  Bob Larkin June 2020
 *
 *
 * Thse classes are derived from their equivalents in Teensyduino. Thus:
 * Teensyduino Core Library
 * http://www.pjrc.com/teensy/
 * Copyright (c) 2017 PJRC.COM, LLC.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * 2. If the Software is incorporated into a build system that allows
 * selection among a list of target devices, then similar target
 * devices manufactured by PJRC.COM must be included in the list of
 * target devices and selectable in the same manner.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * All additions are under  MIT License.  use at your own risk.
*/

#ifndef _AudioStream_F32_h
#define _AudioStream_F32_h

#include <arm_math.h> //ARM DSP extensions.  for speed!
#include <Audio.h> //Teensy Audio Library
#include "AudioSettings_F32.h"


// /////////////// class prototypes
class AudioStream_F32;
class AudioConnection_F32;


// ///////////// class definitions

//create a new structure to hold audio as floating point values.
//modeled on the existing teensy audio block struct, which uses Int16
//https://github.com/PaulStoffregen/cores/blob/268848cdb0121f26b7ef6b82b4fb54abbe465427/teensy3/AudioStream.h
// Added id, per Tympan.  Should not disturb existing programs.  Bob Larkin June 2020
class audio_block_f32_t {
    public:
        audio_block_f32_t(void) {};
        audio_block_f32_t(const AudioSettings_F32 &settings) {
            fs_Hz = settings.sample_rate_Hz;
            length = settings.audio_block_samples;
        };

        unsigned char ref_count;
        unsigned char memory_pool_index;
        unsigned char reserved1;
        unsigned char reserved2;
        float32_t data[AUDIO_BLOCK_SAMPLES]; // AUDIO_BLOCK_SAMPLES is 128, from AudioStream.h
        const int full_length = AUDIO_BLOCK_SAMPLES;
        int length = AUDIO_BLOCK_SAMPLES; // AUDIO_BLOCK_SAMPLES is 128, from AudioStream.h
                                          // For Teensy 4.x, AUDIO_SAMPLE_RATE is 44100
        float fs_Hz = AUDIO_SAMPLE_RATE;  // T3.x AUDIO_SAMPLE_RATE is 44117.64706
        unsigned long id;
};

// Xenomorphicle divergence from upstream OpenAudio: pointer-based src/dst with
// a default constructor, re-connect(), and disconnect() — mirrors the stock
// int16 AudioConnection API so F32 patch cables can be pooled and re-patched
// when applets are hot-swapped.
class AudioConnection_F32
{
  public:
    AudioConnection_F32() :
      src(NULL), dst(NULL), src_index(0), dest_index(0),
      next_dest(NULL), isConnected(false) {}
    AudioConnection_F32(AudioStream_F32 &source, AudioStream_F32 &destination) :
      src(&source), dst(&destination), src_index(0), dest_index(0),
      next_dest(NULL), isConnected(false)
      { connect(); }
    AudioConnection_F32(AudioStream_F32 &source, unsigned char sourceOutput,
      AudioStream_F32 &destination, unsigned char destinationInput) :
      src(&source), dst(&destination),
      src_index(sourceOutput), dest_index(destinationInput),
      next_dest(NULL), isConnected(false)
      { connect(); }
    void connect(AudioStream_F32 &source, unsigned char sourceOutput,
      AudioStream_F32 &destination, unsigned char destinationInput);
    void disconnect(void);
    friend class AudioStream_F32;
  protected:
    void connect(void);
    AudioStream_F32 *src;
    AudioStream_F32 *dst;
    unsigned char src_index;
    unsigned char dest_index;
    AudioConnection_F32 *next_dest;
    bool isConnected;
};


class AudioStream_F32 : public AudioStream {
  public:
    AudioStream_F32(unsigned char n_input_f32, audio_block_f32_t **iqueue, 
        unsigned char ninput_i16, audio_block_t** iqueues_i16)
      : AudioStream(ninput_i16, iqueues_i16),
        num_inputs_f32(n_input_f32), inputQueue_f32(iqueue) 
    {
      //active_f32 = false;
      destination_list_f32 = NULL;
      for (int i=0; i < n_input_f32; i++) {
        inputQueue_f32[i] = NULL;
      }
    };
    AudioStream_F32(unsigned char n_input_f32, audio_block_f32_t **iqueue)
      : AudioStream_F32(n_input_f32, iqueue, 1, inputQueueArray_i16)
      {}

    static void initialize_f32_memory(audio_block_f32_t *data, unsigned int num);
    static void initialize_f32_memory(audio_block_f32_t *data, unsigned int num, const AudioSettings_F32 &settings);
    //virtual void update(audio_block_f32_t *) = 0;
    static uint8_t f32_memory_used;
    static uint8_t f32_memory_used_max;
    static audio_block_f32_t * allocate_f32(void);
    static void release(audio_block_f32_t * block);

  protected:
    //bool active_f32;
    unsigned char num_inputs_f32;
    void transmit(audio_block_f32_t *block, unsigned char index = 0);
    audio_block_f32_t * receiveReadOnly_f32(unsigned int index = 0);
    audio_block_f32_t * receiveWritable_f32(unsigned int index = 0);
    friend class AudioConnection_F32;

  private:
    AudioConnection_F32 *destination_list_f32;
    audio_block_f32_t **inputQueue_f32;
    virtual void update(void) = 0;
    audio_block_t *inputQueueArray_i16[1];  //two for stereo
    static audio_block_f32_t *f32_memory_pool;
    static uint32_t f32_memory_pool_available_mask[6];
};

/*
#define AudioMemory_F32(num) ({ \
  static audio_block_f32_t data_f32[num]; \
  AudioStream_F32::initialize_f32_memory(data_f32, num); \
})
*/

void AudioMemory_F32(const int num);
void AudioMemory_F32(const int num, const AudioSettings_F32 &settings);
#define AudioMemory_F32_wSettings(num,settings) (AudioMemory_F32(num,settings))   //for historical compatibility


#define AudioMemoryUsage_F32() (AudioStream_F32::f32_memory_used)
#define AudioMemoryUsageMax_F32() (AudioStream_F32::f32_memory_used_max)
#define AudioMemoryUsageMaxReset_F32() (AudioStream_F32::f32_memory_used_max = AudioStream_F32::f32_memory_used)


#endif
