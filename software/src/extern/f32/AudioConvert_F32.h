
#ifndef _AudioConvert_I16toF32_h
#define _AudioConvert_I16toF32_h

#include <AudioStream_F32.h>
#include "../../Audio/f32_int_convert.h"

class AudioConvert_I16toF32 : public AudioStream_F32 //receive Int and transmits Float
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioConvert_I16toF32(void) : AudioStream_F32(1, inputQueueArray_f32) { };
	AudioConvert_I16toF32(const AudioSettings_F32 &settings) : AudioStream_F32(1, inputQueueArray_f32) { };
	
    void update(void) {	
      //get the Int16 block
      audio_block_t *int_block;
      int_block = AudioStream::receiveReadOnly(); //int16 data block
      if (int_block==NULL) return;

      //allocate a float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::allocate_f32(); 
      if (float_block == NULL) {
      	  AudioStream::release(int_block);
      	  return;
      }
      
      //convert to float
      convertAudio_I16toF32(int_block, float_block, float_block->length);

      //transmit the audio and return it to the system
      AudioStream_F32::transmit(float_block,0);
      AudioStream_F32::release(float_block);
      AudioStream::release(int_block);
    };
    
    static void convertAudio_I16toF32(audio_block_t *in, audio_block_f32_t *out, int len) {
      //WEA Method.  Should look at CMSIS arm_q15_to_float instead: https://www.keil.com/pack/doc/CMSIS/DSP/html/group__q15__to__x.html#gaf8b0d2324de273fc430b0e61ad4e9eb2
      const float MAX_INT = 32768.0;
      for (int i = 0; i < len; i++) out->data[i] = (float)(in->data[i]);
      arm_scale_f32(out->data, 1.0/MAX_INT, out->data, out->length); //divide by 32768 to get -1.0 to +1.0
    }    
    
  private:
    audio_block_f32_t *inputQueueArray_f32[1]; //2 for stereo

};

class AudioConvert_I16x2toF32 : public AudioStream_F32 //receive Int and transmits Float
{
  //GUI: inputs:2, outputs:1  //this line used for automatic generation of GUI node    
    audio_block_t *inputQueueArray[2]; 

  public:
    AudioConvert_I16x2toF32(void) 
      : AudioStream_F32(0, nullptr, 2, inputQueueArray) { };
    AudioConvert_I16x2toF32(const AudioSettings_F32 &settings) 
      : AudioStream_F32(0, nullptr, 2, inputQueueArray) { };
	
    void update(void) {	
      //get the Int16 blocks
      audio_block_t *int_blockH, *int_blockL;
      rxInt16block(int_blockH);
      rxInt16block(int_blockL, 1);

      //allocate a float block
      audio_block_f32_t *float_block = AudioStream_F32::allocate_f32(); 

      // process, as long as we have all blocks
      if (nullptr != int_blockH && nullptr != int_blockL && nullptr != float_block) 
      {
        //convert to float
        convertAudio_I16x2toF32(int_blockH, int_blockL, float_block, float_block->length);

        //transmit the audio and return it to the system
        AudioStream_F32::transmit(float_block,0);
      }
      if( nullptr != float_block) AudioStream_F32::release(float_block);
      releaseInt16block(int_blockH);
      releaseInt16block(int_blockL);
    };
    
    static void convertAudio_I16x2toF32(audio_block_t *inH, audio_block_t *inL, audio_block_f32_t *out, int len) 
    {
      const float MAX_INT = 32768.0f*65536.0f;
      for (int i = 0; i < len; i++)
      {
        // reassemble a 32-bit signed value from two 16-bit values
        int32_t sample = ((int32_t) inH->data[i] << 16) | (((int32_t) inL->data[i]) & 0xFFFF);
        out->data[i] = (float)(sample);
      }
      arm_scale_f32(out->data, 1.0/MAX_INT, out->data, out->length); //divide by 32768*64k to get -1.0 to +1.0
    }
    
    // Receive an I16 block, or create a silent one if it's NULL
    void rxInt16block(audio_block_t*& blk, unsigned int index = 0)
    {
      blk = AudioStream::receiveReadOnly(index);
      if (nullptr == blk)
      {
        blk = AudioStream::allocate();
        if (nullptr != blk) 
          memset(blk->data, 0, sizeof blk->data);
      }
    }

    static void releaseInt16block(audio_block_t*& blk)
    {
      if (nullptr != blk )
        AudioStream::release(blk);
    }
};

class AudioConvert_F32toI16 : public AudioStream_F32 //receive Float and transmits Int
{
  //GUI: inputs:1, outputs:1  //this line used for automatic generation of GUI node
  public:
    AudioConvert_F32toI16(void) : AudioStream_F32(1, inputQueueArray_Float) {};
    void update(void) {
      //get the float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::receiveReadOnly_f32(); //float data block
      if (!float_block) return;

      //allocate a Int16 block
      audio_block_t *int_block;
      int_block = AudioStream::allocate(); 
      if (int_block == NULL) {
      	  AudioStream_F32::release(float_block);
      	  return;
      }
      
      //convert back to int16
      convertAudio_F32toI16(float_block, int_block, float_block->length);

      //return audio to the system
      AudioStream::transmit(int_block);
      AudioStream::release(int_block);
      AudioStream_F32::release(float_block);
    };

    static void convertAudio_F32toI16(audio_block_f32_t* in, audio_block_t* out, int len) {
      // WEA Method.  Should look at CMSIS arm_float_to_q15 instead:
      // https://www.keil.com/pack/doc/CMSIS/DSP/html/group__float__to__x.html#ga215456e35a18db86882e1d3f0d24e1f2
      for (int i = 0; i < len; i++) {
        out->data[i] = f32conv::to_i16(in->data[i]);
      }
    }

  private:
    audio_block_f32_t *inputQueueArray_Float[1];
 
};

class AudioConvert_F32toI16x2 : public AudioStream_F32 //receive Float and transmits Int
{
  //GUI: inputs:1, outputs:2  //this line used for automatic generation of GUI node
    audio_block_f32_t *inputQueueArray_Float[1]; 
  public:
    AudioConvert_F32toI16x2(void) : AudioStream_F32(1, inputQueueArray_Float) {};
    void update(void) {
      //get the float block
      audio_block_f32_t *float_block;
      float_block = AudioStream_F32::receiveReadOnly_f32(); //float data block
      if (!float_block) return;

      //allocate a Int16 block
      audio_block_t *int_blockH, *int_blockL;
      int_blockH = AudioStream::allocate(); 
      if (int_blockH == NULL) 
      {
      	  AudioStream_F32::release(float_block);
      	  return;
      }
      else
      {
        int_blockL = AudioStream::allocate(); 
        if (int_blockL == NULL) 
        {
          AudioStream::release(int_blockH);
          AudioStream_F32::release(float_block);
          return;
        }
      }
      
      //convert back to int16
      convertAudio_F32toI16x2(float_block, int_blockH, int_blockL, float_block->length);

      //return audio to the system
      AudioStream::transmit(int_blockH);
      AudioStream::transmit(int_blockL,1);
      AudioStream::release(int_blockH);
      AudioStream::release(int_blockL);
      AudioStream_F32::release(float_block);
    };

   static void convertAudio_F32toI16x2(audio_block_f32_t *in, audio_block_t *outH, audio_block_t *outL, int len) {
      //WEA Method.  Should look at CMSIS arm_float_to_q15 instead: https://www.keil.com/pack/doc/CMSIS/DSP/html/group__float__to__x.html#ga215456e35a18db86882e1d3f0d24e1f2	
      // Same fix, and the same reason, as convertAudio_F32toI16 above; the
      // 32-bit limits carry the extra trap that 2^31-1 is not representable
      // as a float (see f32_int_convert.h).
      for (int i = 0; i < len; i++) {
        int32_t intValue = f32conv::to_i32(in->data[i]);
        outH->data[i] = (int16_t) ((intValue & 0xFFFF0000)>>16);
        outL->data[i] = (int16_t) ((intValue & 0x0000FFFF));
      }
    }    
};

#endif
