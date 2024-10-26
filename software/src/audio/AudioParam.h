#pragma once

#include <Audio.h>

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);

template <typename T> class AudioParam {
public:
  AudioParam() : AudioParam(T()) {}

  AudioParam(T v, float lpf_coeff = 1.0f)
      : value(v), prev(v), lpf(v), inc(0), lpf_coeff(lpf_coeff) {}

  AudioParam &operator=(const T &newValue) {
    value = newValue;
    inc = (value - prev) / AUDIO_BLOCK_SAMPLES;
    return *this;
  }

  T ReadNext() {
    prev += inc;
    // check if the sign on inc and value-prev are different to see if we've
    // gone too far
    if (inc * (value - prev) <= 0.0f) {
      inc = 0.0f;
      prev = value;
    }
    ONE_POLE(lpf, prev, lpf_coeff);
    return lpf;
  }

  T Read() { return lpf; }

  void Reset() {
    inc = 0;
    lpf = prev = value;
  }

private:
  T value;
  T prev;
  T lpf;
  T inc;
  float lpf_coeff;
};

template <typename T> class Param {
public:
  Param() : Param(0.0f) {}
  Param(T value) : value(value) {}
  inline T ReadNext() { return value; }
  inline T Read() { return value; }
  inline Param &operator=(const T &newValue) {
    value = newValue;
    return *this;
  }
  inline void Reset() {}

private:
  T value;
};

template <typename P> class OnePole {
public:
  OnePole() : OnePole(P(), 1.0f) {}

  OnePole(float value, float coeff) : OnePole(Param(value), coeff) {}

  OnePole(P param, float coeff)
      : param(param), coeff(coeff), lp_value(param.Read()) {}
  inline OnePole &operator=(float new_value) {
    param = new_value;
    return *this;
  }

  inline float ReadNext() {
    ONE_POLE(lp_value, param.ReadNext(), coeff);
    return lp_value;
  }

  inline void Reset() {
    param.Reset();
    lp_value = param.Read();
  }

  inline float Read() { return param.Read(); }

private:
  P param;
  float coeff;
  float lp_value;
};

class Interpolated {
public:
  Interpolated() : Interpolated(0, 1) {}

  Interpolated(float value, size_t steps)
      : value(value), target(value), steps(steps), inc(0) {}

  inline Interpolated &operator=(float new_value) {
    target = new_value;
    inc = (target - value) / steps;
    return *this;
  }

  inline float ReadNext() {
    value += inc;
    // check if the sign on inc and value-prev are different to see if we've
    // gone too far
    if (inc * (target - value) <= 0.0f) {
      inc = 0.0f;
      value = target;
    }
    return value;
  }

  inline float Read() { return value; }

  inline void Reset() { value = target; }

private:
  float value;
  float target;
  size_t steps;
  float inc;
};

// Based on
// https://github.com/oamodular/time-machine/blob/aaf3410759f43a828c8350bcacb10828af13f3c2/TimeMachine/dsp.h#L109-L146
struct NoiseSuppressor {
  float value;
  float lpf_coeff;
  float noise_floor;
  size_t settle_threshold;
  size_t settle_count;

  float Process(float new_val) {
    float d = new_val - value;
    if (abs(d) < noise_floor) {
      if (settle_count < settle_threshold)
        settle_count++;
      else
        d = 0;
    } else {
      settle_count = 0;
    }
    return value += lpf_coeff * d;
  }
};
