// Copyright (c) 2016 Patrick Dowling
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

#ifndef UTIL_SETTINGS_H_
#define UTIL_SETTINGS_H_

#include <algorithm>
#include <array>
#include <stdint.h>
#include <string.h>
#include "util_stream_buffer.h"
#include "util_misc.h"

namespace settings {

enum StorageType {
  STORAGE_TYPE_U4, // nibbles are packed where possible, else aligned to next byte
  STORAGE_TYPE_I8, STORAGE_TYPE_U8,
  STORAGE_TYPE_I16, STORAGE_TYPE_U16,
  STORAGE_TYPE_I32, STORAGE_TYPE_U32,
  STORAGE_TYPE_NOP,
};

// Storage size in bits
static constexpr size_t StorageBitSize[] = {
    4,
    sizeof(int8_t) << 3, sizeof(uint8_t) << 3,
    sizeof(int16_t) << 3, sizeof(uint16_t) << 3,
    sizeof(int32_t) << 3, sizeof(uint32_t) << 3,
    0,
};
static_assert(sizeof(StorageBitSize) / sizeof(size_t) == STORAGE_TYPE_NOP + 1, "Not all StorageType sizes defined");

struct ValueAttributes {

  const int default_;
  int min_;
  int max_;
  const char *name;
  const char * const *value_names;
  const StorageType storage_type;

  constexpr ValueAttributes()
  : default_(0), min_(0), max_(0)
  , name(nullptr), value_names(nullptr)
  , storage_type(STORAGE_TYPE_NOP)
  { }

  constexpr ValueAttributes(int def, int mi, int ma,
                            const char *n, const char * const *vn,
                            StorageType t) 
  : default_(def), min_(mi), max_(ma)
  , name(n), value_names(vn)
  , storage_type(t)
  { }

  int default_value() const {
    return default_;
  }

  int clamp(int value) const {
    if (value < min_) return min_;
    else if (value > max_) return max_;
    else return value;
  }
};

// Calculate storage size constexpr-style, C++11 recursive implementation
// This isn't super elegant, but we want to round up the nibble if a non-
// nibble type is added, as well as rounding up in case there's a dangling
// nibble at the end. Nibble.
// Adding the extra struct in the middle makes it marginally more legible.
struct Nibbler {
  constexpr Nibbler add(StorageType storage_type) const {
    return (value & 0x4 && STORAGE_TYPE_U4 != storage_type)
        ? Nibbler{value + 4 + StorageBitSize[storage_type]}
        : Nibbler{value + StorageBitSize[storage_type]};
  }
  size_t value = 0;
  constexpr Nibbler(size_t v = 0) : value(v) { }
};

template<std::size_t N>
constexpr size_t calc_storage_bitsize(const std::array<ValueAttributes, N> &A, size_t i, Nibbler nibbler) {
    return i < N
      ? calc_storage_bitsize(A, i + 1, nibbler.add(A[i].storage_type))
      : nibbler.value + 4;
}

template<std::size_t N>
constexpr size_t calc_storage_size(const std::array<ValueAttributes, N>& A) {
  return calc_storage_bitsize(A, 0, Nibbler()) >> 3;
}

template <typename T>
constexpr size_t StorageSize() {
  return calc_storage_size(T::value_attributes_array);
}

// Implement the actual reading and writing of settings as external functions.
// This reduces some template bloat by avoiding a separate Save/Restore
// implementation for each class derived from SettingsBase.
class SettingsRW {
public:
  static size_t Write(const int values[], const ValueAttributes attributes[], size_t num_valyes, util::StreamBufferWriter &stream_writer);
  static size_t Read(int values[], const ValueAttributes attributes[], size_t num_values, util::StreamBufferReader &stream_reader);
};

// Provide a very simple "settings" base.
// Settings values are an array of ints that are accessed by index, usually the
// owning class will use an enum for clarity, and provide specific getter
// functions for each value.
//
// The values are decsribed using the per-class value_attr array and allow min,
// max and default values. To set the defaults, call ::InitDefaults at least
// once before using the class. The min/max values are enforced when setting
// or modifying values. Classes shouldn't normally have to access the values_
// directly.
//
// To try and save some storage space, each setting can be stored as a smaller
// type as specified in the attributes. For even more compact representations,
// the owning class can pack things differently if required.
//
// TODO: Save/Restore is still kind of sucky
// TODO: If absolutely necessary, add STORAGE_TYPE_BIT and pack nibbles & bits
//
template <typename clazz, size_t num_settings>
class SettingsBase {
public:
  using ValueArray = std::array<int, num_settings>;
  using ValueAttributesArray = std::array<ValueAttributes, num_settings>;

  // This would be nice, but isn't actually possible (even if gcc compiles it)
  //static constexpr size_t kStorageSize = calc_storage_size(clazz::value_attributes_array);

  int get_value(size_t index) const {
    return values_[index];
  }

  static int clamp_value(size_t index, int value) {
    return clazz::value_attributes_array[index].clamp(value);
  }

  bool apply_value(size_t index, int value) {
    if (index < num_settings) {
      const int clamped = clazz::value_attributes_array[index].clamp(value);
      if (values_[index] != clamped) {
        values_[index] = clamped;
        return true;
      }
    }
    return false;
  }

  bool change_value(size_t index, int delta) {
    return apply_value(index, values_[index] + delta);
  }

  static const ValueAttributes &value_attributes(size_t i) {
    return clazz::value_attributes_array[i];
  }

  void InitDefaults() {
    std::transform(
        std::begin(clazz::value_attributes_array),
        std::end(clazz::value_attributes_array),
        std::begin(values_),
        [](const ValueAttributes &attributes) {
          return attributes.default_value();
        });
  }

  size_t Save(util::StreamBufferWriter &stream_writer) const {
    return SettingsRW::Write(values_.data(),
                             clazz::value_attributes_array.data(),
                             num_settings, stream_writer);
  }

  size_t Restore(util::StreamBufferReader &stream_reader) {
    return SettingsRW::Read(values_.data(),
                            clazz::value_attributes_array.data(),
                            num_settings, stream_reader);
  }

  static constexpr size_t storageSize() {
    return StorageSize<clazz>();
  }

protected:

  ValueArray values_;
};

#define SETTINGS_ARRAY_DECLARE() \
public: \
  static constexpr ValueAttributesArray value_attributes_array

#define SETTINGS_ARRAY_DEFINE(clazz) \
  constexpr clazz::ValueAttributesArray clazz::value_attributes_array

} // namespace settings

#endif // UTIL_SETTINGS_H_
