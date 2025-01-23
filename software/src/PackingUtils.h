#pragma once

template <typename T>
constexpr uint64_t as_unsigned(T value) {
  union {
    std::remove_reference_t<T> value;
    uint64_t unsigned_value;
  } u = {value};
  return u.unsigned_value;
}

/**
 * Convert the first N bits of a 64-bit value to the given type. Will correctly
 * handle signed, negative integers.
 */
template <typename T>
constexpr std::remove_reference_t<T> extract_value(uint64_t value) {
  value <<= (64 - sizeof(T) * 8);
  value >>= (64 - sizeof(T) * 8);
  union {
    uint64_t unsigned_value;
    std::remove_reference_t<T> value;
  } u = {value};
  return u.value;
}

template <typename T, size_t S>
struct Packable {
  T& value;
  static constexpr size_t Size = S;

  static constexpr uint64_t mask() {
    return 0xffffffffffffffffull >> (64 - Size);
  }

  static constexpr uint64_t PackType(T value) {
    // This works for for negative signed values because all leading bits will
    // be 1 if the number is actually in range.
    return as_unsigned(value) & mask();
  }

  uint64_t Pack() const {
    return PackType(value);
  }

  static constexpr T UnpackType(uint64_t data) {
    uint64_t uv = data & mask();
    // Check if its signed and should be negative
    if (std::is_signed_v<T> && (uv >> (Size - 1))) {
      uint64_t typemask = 0xffffffffffffffffull >> (64 - sizeof(T) * 8);
      // fill out leading ones to make actual type negative
      uv |= typemask ^ mask();
    }
    return extract_value<T>(uv);
  }

  void Unpack(uint64_t data) {
    value = UnpackType(data);
  }
};

template <size_t Size, typename T>
constexpr Packable<T, Size> pack(Packable<T, Size> value) {
  return value;
}
template <size_t Size, typename T>
constexpr Packable<T, Size> pack(T& value) {
  return {value};
}
template <typename T>
constexpr Packable<T, sizeof(T) * 8> pack(T& value) {
  return {value};
}

template <typename... Packables>
constexpr uint64_t PackPackables(std::tuple<Packables...> params) {
  return PackPackables<Packables...>(std::get<Packables>(params)...);
}

/**
 * Densely pack multiple values into a uint64_t. If a parameter is a Packable,
 * it will use it will be packed into Size bits. Otherwise, it will be packed
 * into 8 * sizeof(param) bits. The leading packed bit of signed values will
 * indicate if the value is negative. Thus, e.g., the range a 7 bit signed value
 * will be [-64, 63].
 */
template <typename... Packables>
constexpr uint64_t PackPackables(Packables&&... params) {
  static_assert(
    (... + std::decay_t<decltype(pack(params))>::Size) <= 64,
    "Can pack up to 64 bits"
  );
  uint64_t result = 0;
  size_t shift = 0;
  return (
    ...,
    (result
     |= (pack(params).Pack() << ((shift += pack(params).Size) - pack(params).Size))
    )
  );
}

template <typename... Packables>
constexpr void UnpackPackables(uint64_t data, std::tuple<Packables...> params) {
  UnpackPackables<Packables...>(data, std::get<Packables>(params)...);
}

/**
 * Unpack densely packed values from a uint64_t and store them in the given
 * refs. If a parameter is a Packable, it will be unpacked from Size bits.
 * Otherwise, it will be unpacked from 8 * sizeof(param) bits. The leading
 * packed bit of signed values will indicate if the value is negative. Thus,
 * e.g., the range a 7 bit signed value will be [-64, 63].

 * */
template <typename... Packables>
constexpr void UnpackPackables(uint64_t data, Packables&&... params) {
  static_assert(
    (... + std::decay_t<decltype(pack(params))>::Size) <= 64,
    "Can unpack up to 64 bits"
  );
  size_t shift = 0;
  (...,
   (pack(params).Unpack(
     data >> ((shift += pack(params).Size) - pack(params).Size)
   )));
}
