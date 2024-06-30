// Copyright 2019 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//

#include "../../util/util_settings.h"

namespace settings {

static constexpr uint16_t kNibbleValid = 0xf000;
struct StreamState {
  StreamState(const ValueAttributes *attrs) : nibbles(), attributes(attrs) { }

  uint16_t nibbles;
  const ValueAttributes *attributes;
};

void flush_nibbles(StreamState &stream_state, util::StreamBufferWriter &stream_writer) {
  if (stream_state.nibbles) {
    stream_writer.Write<uint8_t>(stream_state.nibbles & 0xff);
    stream_state.nibbles = 0;
  }
}

void write_nibble(StreamState &stream_state, util::StreamBufferWriter &stream_writer, int value) {
  if (stream_state.nibbles) {
    stream_state.nibbles |= (value & 0x0f);
    flush_nibbles(stream_state, stream_writer);
  } else {
    // Ensure correct packing for reads even if there's an odd number of nibbles;
    // the first nibble is assumed to be in the msbits.
    stream_state.nibbles = kNibbleValid | ((value & 0x0f) << 4);
  }
}

template <typename storage_type>
void write_value(StreamState &stream_state, util::StreamBufferWriter &stream_writer, int value) {
  flush_nibbles(stream_state, stream_writer);
  stream_writer.Write(static_cast<storage_type>(value));
}

int read_nibble(StreamState &stream_state, util::StreamBufferReader &stream_reader) {
  uint8_t value;
  if (stream_state.nibbles) {
    value = stream_state.nibbles & 0x0f;
    stream_state.nibbles = 0;
  } else {
    value = stream_reader.Read<uint8_t>();
    stream_state.nibbles = kNibbleValid | value;
    value >>= 4;
  }
  return value;
}

template <typename storage_type>
int read_value(StreamState &stream_state, util::StreamBufferReader &stream_reader) {
  stream_state.nibbles = 0;
  storage_type value = stream_reader.Read<storage_type>();
  return static_cast<int>(value);
}

/*static*/
size_t SettingsRW::Write(const int values[], const ValueAttributes attributes[], size_t num_values, util::StreamBufferWriter &stream_writer)
{
  auto stream_state = StreamState{attributes};
  for (size_t v = 0; v < num_values; ++v) {
    const int value = values[v];
    switch(attributes[v].storage_type) {
      case STORAGE_TYPE_U4: write_nibble(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_I8: write_value<int8_t>(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_U8: write_value<uint8_t>(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_I16: write_value<int16_t>(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_U16: write_value<uint16_t>(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_I32: write_value<int32_t>(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_U32: write_value<uint32_t>(stream_state, stream_writer, value); break;
      case STORAGE_TYPE_NOP: break;
    }
  }
  flush_nibbles(stream_state, stream_writer);
  return stream_writer.written();
}

/*static*/
size_t SettingsRW::Read(int values[], const ValueAttributes attributes[], size_t num_values, util::StreamBufferReader &stream_reader)
{
  auto stream_state = StreamState{attributes};
  for (size_t v = 0; v < num_values; ++v) {
    auto attr = attributes[v];
    int value = 0;
    switch(attr.storage_type) {
      case STORAGE_TYPE_U4: value = read_nibble(stream_state, stream_reader); break;
      case STORAGE_TYPE_I8: value = read_value<int8_t>(stream_state, stream_reader); break;
      case STORAGE_TYPE_U8: value = read_value<uint8_t>(stream_state, stream_reader); break;
      case STORAGE_TYPE_I16: value = read_value<int16_t>(stream_state, stream_reader); break;
      case STORAGE_TYPE_U16: value = read_value<uint16_t>(stream_state, stream_reader); break;
      case STORAGE_TYPE_I32: value = read_value<int32_t>(stream_state, stream_reader); break;
      case STORAGE_TYPE_U32: value = read_value<uint32_t>(stream_state, stream_reader); break;
      case STORAGE_TYPE_NOP: break;
    }
    if (STORAGE_TYPE_NOP != attr.storage_type)
      values[v] = attributes[v].clamp(value);
  }
  return stream_reader.read();
}
  
} // settings
