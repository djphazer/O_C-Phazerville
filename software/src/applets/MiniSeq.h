#pragma once

struct MiniSeq {

  static constexpr int MAX_STEPS = 32;
  static constexpr int MIN_VALUE = 0;
  static constexpr int MAX_VALUE = 63;

  // packed as 6-bit Note Number and two flags
  uint8_t *note = (uint8_t*)(OC::user_patterns[0].notes);
  int step = 0;
  bool reset = 1;

  void SetPattern(int &index) {
    CONSTRAIN(index, 0, 7);
    note = (uint8_t*)(OC::user_patterns[index].notes);
  }
  void Clear() {
      for (int s = 0; s < MAX_STEPS; s++) note[s] = 0x20; // C4 == 0V
  }
  void Randomize(bool keeplength = false) {
    const uint8_t length = keeplength ? GetLength() : MAX_STEPS;
    for (int s = 0; s < length; s++) {
      note[s] = random(0xff);
    }
    if (keeplength) SetLength(length);
  }
  void SowPitches(const uint8_t range = 32) {
    const int length = GetLength();
    for (int s = 0; s < MAX_STEPS; s++) {
      SetNote(random(range), s);
    }
    SetLength(length);
  }
  void Advance(bool reverse = false) {
      if (reset) {
        reset = false;
        return;
      }
      if (reverse) {
        if (--step < 0) step = GetLength() - 1;
      } else {
        if (++step >= GetLength()) step = 0;
      }
  }
  int GetNote(const size_t s_) {
    // lower 6 bits is note value
    // bipolar -32 to +31
    return int(note[s_] & 0x3f) - 32;
  }
  int GetNote() {
    return GetNote(step);
  }
  void SetNote(int nval, const size_t s_) {
    nval += 32;
    CONSTRAIN(nval, MIN_VALUE, MAX_VALUE);
    // keep upper 2 bits
    note[s_] = (note[s_] & 0xC0) | (uint8_t(nval) & 0x3f);
    //note[s_] &= ~(0x01 << 7); // unmute?
  }
  void SetNote(int nval) {
    SetNote(nval, step);
  }
  bool accent(const size_t s_) {
    // second highest bit is accent
    return (note[s_] & (0x01 << 6));
  }
  void SetAccent(const size_t s_, bool on = true) {
    note[s_] &= ~(1 << 6); // clear
    note[s_] |= (on << 6); // set
  }
  bool muted() {
    return muted(step);
  }
  bool muted(const size_t s_) {
    // highest bit is mute
    return (note[s_] & (0x01 << 7));
  }
  void Unmute(const size_t s_) {
    note[s_] &= ~(0x01 << 7);
  }
  void Mute(const size_t s_) {
    if (s_ == MAX_STEPS - 1) note[s_] &= ~(1 << 6); // clear accent
    note[s_] |= (0x01 << 7);
  }
  uint8_t GetLength() {
    if ((note[MAX_STEPS - 1] & 0xE0) == 0xE0)
      return (note[MAX_STEPS-1] & 0b00011111);
    else
      return MAX_STEPS;
  }
  void SetLength(uint8_t length_) {
    if (length_ < MAX_STEPS)
      note[MAX_STEPS - 1] = 0b11100000 | (length_ & 0b00011111);
    else
      note[MAX_STEPS - 1] = 0x20; // reinit last step
  }
  void ToggleAccent(const size_t s_) {
    note[s_] ^= (0x01 << 6);
  }
  void ToggleMute(const size_t s_) {
    if (s_ == MAX_STEPS - 1) note[s_] &= ~(1 << 6); // clear accent
    note[s_] ^= (0x01 << 7);
  }
  void Reset() {
      step = 0;
      reset = true;
  }
};
