// Xenomorphicle: whole-file guard, F32 core is used on T4.1 audio builds only (see README.md)
#ifdef ARDUINO_TEENSY41

#include "AudioStream_F32.h"

audio_block_f32_t * AudioStream_F32::f32_memory_pool;
uint32_t AudioStream_F32::f32_memory_pool_available_mask[6];

uint8_t AudioStream_F32::f32_memory_used = 0;
uint8_t AudioStream_F32::f32_memory_used_max = 0;

audio_block_f32_t* allocate_f32_memory(const int num) {
    static bool firstTime=true;
    static audio_block_f32_t *data_f32;
    if (firstTime == true) {
        firstTime = false;
        data_f32 = new audio_block_f32_t[num];
    }
    return data_f32;
}
void AudioMemory_F32(const int num) {
    audio_block_f32_t *data_f32 = allocate_f32_memory(num);
    if (data_f32 != NULL) AudioStream_F32::initialize_f32_memory(data_f32, num);
}
void AudioMemory_F32(const int num, const AudioSettings_F32 &settings) {
    audio_block_f32_t *data_f32 = allocate_f32_memory(num);
    if (data_f32 != NULL) AudioStream_F32::initialize_f32_memory(data_f32, num, settings);
}

// Set up the pool of audio data blocks
// placing them all onto the free list
void AudioStream_F32::initialize_f32_memory(audio_block_f32_t *data, unsigned int num)
{
  unsigned int i;

  // Serial.println("AudioStream_F32 initialize_memory");
  // delay(10);
  if (num > 192) num = 192;
  __disable_irq();
  f32_memory_pool = data;
  for (i=0; i < 6; i++) {
    f32_memory_pool_available_mask[i] = 0;
  }
  for (i=0; i < num; i++) {
    f32_memory_pool_available_mask[i >> 5] |= (1 << (i & 0x1F));
  }

  for (i=0; i < num; i++) {
    data[i].memory_pool_index = i;
  }
  __enable_irq();

} // end initialize_memory
void AudioStream_F32::initialize_f32_memory(audio_block_f32_t *data, unsigned int num, const AudioSettings_F32 &settings)
{
 initialize_f32_memory(data,num);
 for (unsigned int i=0; i < num; i++) {
     data[i].fs_Hz = settings.sample_rate_Hz;
     data[i].length = settings.audio_block_samples;
 }
} // end initialize_memory

// Allocate 1 audio data block.  If successful
// the caller is the only owner of this new block
audio_block_f32_t * AudioStream_F32::allocate_f32(void)
{
  uint32_t n, index, avail;
  uint32_t *p;
  audio_block_f32_t *block;
  uint8_t used;

  // f32_memory_pool_available_mask is array of six 32-bit uints with a 1 in position of
  // each available block (max 192).  0 if unavailable (busy) or not allocated at all.
  p = f32_memory_pool_available_mask;


/*
  if(millis() > 1200) {
       Serial.print("AudioStream_F32 ");
       Serial.println((uint32_t)*p, BIN);  // Just first of 6
     }
 */


  __disable_irq();
  do {
    avail = *p; if (avail) break;
    p++; avail = *p; if (avail) break;
    p++; avail = *p; if (avail) break;
    p++; avail = *p; if (avail) break;
    p++; avail = *p; if (avail) break;
    p++; avail = *p; if (avail) break;
    __enable_irq();
    // Serial.println("alloc_f32:null");
    return NULL;
  } while (0);
  n = __builtin_clz(avail);
  *p = avail & ~(0x80000000 >> n);
  used = f32_memory_used + 1;
  f32_memory_used = used;
  __enable_irq();
  index = p - f32_memory_pool_available_mask;
  block = f32_memory_pool + ((index << 5) + (31 - n));
  block->ref_count = 1;
  if (used > f32_memory_used_max) f32_memory_used_max = used;
  // Serial.print("alloc_f32:");  Serial.println((uint32_t)block, HEX);
  return block;
}


// Release ownership of a data block.  If no
// other streams have ownership, the block is
// returned to the free pool
void AudioStream_F32::release(audio_block_f32_t *block)
{
  uint32_t mask = (0x80000000 >> (31 - (block->memory_pool_index & 0x1F)));
  uint32_t index = block->memory_pool_index >> 5;

  __disable_irq();
  if (block->ref_count > 1) {
    block->ref_count--;
  } else {
//Serial.print("release_f32:"); Serial.println((uint32_t)block, HEX);
    f32_memory_pool_available_mask[index] |= mask;
    f32_memory_used--;
  }
  __enable_irq();
}

// Transmit an audio data block
// to all streams that connect to an output.  The block
// becomes owned by all the recepients, but also is still
// owned by this object.  Normally, a block must be released
// by the caller after it's transmitted.  This allows the
// caller to transmit to same block to more than 1 output,
// and then release it once after all transmit calls.
void AudioStream_F32::transmit(audio_block_f32_t *block, unsigned char index)
{
  //Serial.print("AudioStream_F32: transmit().  start...index = ");Serial.println(index);
  for (AudioConnection_F32 *c = destination_list_f32; c != NULL; c = c->next_dest) {
      //Serial.print("  : loop1, c->src_index = ");Serial.println(c->src_index);
    if (c->src_index == index) {
        //Serial.println("  : if1");
      if (c->dst->inputQueue_f32[c->dest_index] == NULL) {
          //Serial.println("  : if2");
        c->dst->inputQueue_f32[c->dest_index] = block;
        block->ref_count++;
          //Serial.print("  : block->ref_count = "); Serial.println(block->ref_count);
      }
    }
  }
  //Serial.println("AudioStream_F32: transmit(). finished.");
}

// Receive block from an input.  The block's data
// may be shared with other streams, so it must not be written
audio_block_f32_t * AudioStream_F32::receiveReadOnly_f32(unsigned int index)
{
  audio_block_f32_t *in;
//Serial.print(index);  Serial.print(" <index   RcvReadOnly()  num_inputs_f32> ");  Serial.println(num_inputs_f32);
  if (index >= num_inputs_f32) return NULL;
  in = inputQueue_f32[index];
  inputQueue_f32[index] = NULL;
  return in;
}

// Receive block from an input.  The block will not
// be shared, so its contents may be changed.
audio_block_f32_t * AudioStream_F32::receiveWritable_f32(unsigned int index)
{
  audio_block_f32_t *in, *p;

  if (index >= num_inputs_f32) return NULL;
  in = inputQueue_f32[index];
  inputQueue_f32[index] = NULL;
  if (in && in->ref_count > 1) {
    p = allocate_f32();
    if (p) memcpy(p->data, in->data, sizeof(p->data));
    in->ref_count--;
    in = p;
  }
  return in;
}

void AudioConnection_F32::connect(void) {
  AudioConnection_F32 *p;

  if (isConnected || !src || !dst) return;
  if (dest_index > dst->num_inputs_f32) return;
  __disable_irq();
  p = src->destination_list_f32;
  if (p == NULL) {
    src->destination_list_f32 = this;
  } else {
    while (p->next_dest) p = p->next_dest;
    p->next_dest = this;
  }
  src->active = true;
  dst->active = true;
  isConnected = true;
  __enable_irq();
}

// Xenomorphicle additions (see header note): re-target and unlink, mirroring
// the stock int16 AudioConnection semantics.
void AudioConnection_F32::connect(AudioStream_F32 &source, unsigned char sourceOutput,
    AudioStream_F32 &destination, unsigned char destinationInput) {
  if (isConnected) disconnect();
  src = &source;
  dst = &destination;
  src_index = sourceOutput;
  dest_index = destinationInput;
  connect();
}

void AudioConnection_F32::disconnect(void) {
  AudioConnection_F32 *p;

  if (!isConnected || !src || !dst) return;
  if (dest_index > dst->num_inputs_f32) return;
  __disable_irq();
  // unlink from the source's destination list
  p = src->destination_list_f32;
  if (p == this) {
    src->destination_list_f32 = next_dest;
  } else {
    while (p && p->next_dest != this) p = p->next_dest;
    if (p) p->next_dest = next_dest;
  }
  next_dest = NULL;
  // drop any block still queued for the destination input
  if (dst->inputQueue_f32[dest_index]) {
    AudioStream_F32::release(dst->inputQueue_f32[dest_index]);
    dst->inputQueue_f32[dest_index] = NULL;
  }
  isConnected = false;
  __enable_irq();
}

#endif // ARDUINO_TEENSY41
