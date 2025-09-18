#pragma once

static constexpr int CLOCKDIV_MAX = 63;
static constexpr int CLOCKDIV_MIN = -16;

struct ClkDivMult {
  int8_t steps = 1; // positive for division, negative for multiplication
  uint8_t clock_count = 0; // Number of clocks since last output (for clock divide)
  uint32_t next_clock = 0; // Tick number for the next output (for clock multiply)
  uint32_t last_clock = 0;
  int cycle_time = 0; // Cycle time between the last two clock inputs

  void Set(int s) {
    steps = constrain(s, CLOCKDIV_MIN, CLOCKDIV_MAX);
  }
  bool Tick(bool clocked = 0) {
    if (steps == 0) return false;
    bool trigout = 0;
    const uint32_t this_tick = OC::CORE::ticks;

    if (clocked) {
      cycle_time = this_tick - last_clock;
      last_clock = this_tick;

      if (steps > 0) { // Positive value indicates clock division
          clock_count++;
          if (clock_count == 1) trigout = 1; // fire on first step
          if (clock_count >= steps) clock_count = 0; // Reset on last step
      }
      if (steps < 0) {
          // Calculate next clock for multiplication on each clock
          int tick_interval = (cycle_time / -steps);
          next_clock = this_tick + tick_interval;
          clock_count = 0;
          trigout = 1;
      }
    }

    // Handle clock multiplication
    if (steps < 0 && next_clock > 0) {
        if ( this_tick >= next_clock && clock_count+1 < -steps) {
            int tick_interval = (cycle_time / -steps);
            next_clock += tick_interval;
            ++clock_count;
            trigout = 1;
        }
    }
    return trigout;
  }
  void Reset() {
    clock_count = 0;
    next_clock = 0;
  }
};

template <size_t NUM_STEPS = 5>
struct DivSequence {
  int step_index = -1;
  int clock_count = 0;
  ClkDivMult divmult[NUM_STEPS]; // separate DividerMultiplier for each step
  uint16_t muted = 0x0; // bitmask
  uint32_t last_clock = 0;

  int Get(int s) {
    return divmult[s].steps;
  }
  void Set(int s, int div) {
    divmult[s].Set(div);
  }
  void ToggleStep(int idx) {
    muted ^= (0x01 << idx);
  }
  void SetMute(int idx, bool set = true) {
    muted = (muted & ~(0x01 << idx)) | ((set & 0x01) << idx);
  }
  bool Muted(int idx) {
    return (muted >> idx) & 0x01;
  }
  bool StepActive(int idx) {
    return divmult[idx].steps != 0 && !Muted(idx);
  }
  bool Poke(bool clocked = 0) {
    if (step_index < 0) {
      // reset case
      if (clocked) {
          step_index = 0;
          divmult[step_index].last_clock = last_clock;
          last_clock = OC::CORE::ticks;
          return divmult[step_index].Tick(true) && StepActive(step_index);
      }
      return false; // reset and not ready
    }

    bool trigout = divmult[step_index].Tick(clocked);

    if (clocked)
    {
      if (divmult[step_index].steps < 0 || ++clock_count >= divmult[step_index].steps)
      {
        // special case to proceed to next step
        clock_count = 0;

        size_t i = 0;
        do {
            ++step_index %= NUM_STEPS;
            ++i;
        } while (!StepActive(step_index) && i < NUM_STEPS);

        if (StepActive(step_index)) {
          divmult[step_index].Reset();
          divmult[step_index].last_clock = last_clock;
          trigout = divmult[step_index].Tick(true);
        }
      }

      last_clock = OC::CORE::ticks;
    }


    return trigout;
  }
  void Reset() {
    step_index = -1;
    clock_count = 0;
    for (auto &d : divmult) d.Reset();
  }

};
