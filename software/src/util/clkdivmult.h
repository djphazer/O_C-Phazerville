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

