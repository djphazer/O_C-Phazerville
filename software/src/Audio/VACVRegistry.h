// VACVRegistry.h
#pragma once
#include "HSIOFrame.h"

class VACVRegistry {
public:
  static VACVRegistry& I() { static VACVRegistry inst; return inst; }

  // Hand out an ID that is NOT present in owners_ right now.
  uint16_t registerOwner() {
    __disable_irq();
    uint16_t start = next_owner_ ? next_owner_ : 1;
    uint16_t id = start;
    do {
      if (!id) id = 1; // never 0
      bool in_use = false;
      for (int ch = 0; ch < HS::VACV_CHANNEL_COUNT; ++ch) {
        if (owners_[ch] == id) { in_use = true; break; }
      }
      if (!in_use) { next_owner_ = (uint16_t)(id + 1); __enable_irq(); return id; }
      id++;
    } while (id != start);
    __enable_irq();
    return 0; // shouldn't happen unless COUNT==0
  }

  void unregisterOwner(uint16_t owner) {
    if (!owner) return;
    __disable_irq();
    for (int ch = 0; ch < HS::VACV_CHANNEL_COUNT; ++ch)
      if (owners_[ch] == owner) owners_[ch] = 0;
    __enable_irq();
  }

  bool claim(int ch0, uint16_t owner) {
    if (ch0 < 0 || ch0 >= HS::VACV_CHANNEL_COUNT || !owner) return false;
    __disable_irq();
    bool ok = (owners_[ch0] == 0 || owners_[ch0] == owner);
    if (ok) owners_[ch0] = owner;
    __enable_irq();
    return ok;
  }

  void release(int ch0, uint16_t owner) {
    if (ch0 < 0 || ch0 >= HS::VACV_CHANNEL_COUNT || !owner) return;
    __disable_irq();
    if (owners_[ch0] == owner) owners_[ch0] = 0;
    __enable_irq();
  }

  bool isClaimed(int ch0) const {
    if (ch0 < 0 || ch0 >= HS::VACV_CHANNEL_COUNT) return false;
    __disable_irq(); uint16_t o = owners_[ch0]; __enable_irq();
    return o != 0;
  }

  // “Next free” in +1 / -1 direction (wraps). Returns -1 if none.
  int findFree(int start0, int dir) const {
    if (HS::VACV_CHANNEL_COUNT <= 0 || dir == 0) return -1;
    int i = start0;
    for (int step = 0; step < HS::VACV_CHANNEL_COUNT; ++step) {
      i = (i + (dir > 0 ? 1 : -1) + HS::VACV_CHANNEL_COUNT) % HS::VACV_CHANNEL_COUNT;
      __disable_irq(); uint16_t o = owners_[i]; __enable_irq();
      if (o == 0) return i;
    }
    return -1;
  }

private:
  VACVRegistry() { for (int i=0;i<HS::VACV_CHANNEL_COUNT;++i) owners_[i]=0; }

  // NB: if COUNT==0, we still compile by guarding uses with COUNT checks.
  uint16_t owners_[ (HS::VACV_CHANNEL_COUNT>0)?HS::VACV_CHANNEL_COUNT:1 ] = {0};
  uint16_t next_owner_ = 1;
};