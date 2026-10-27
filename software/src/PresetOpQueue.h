#pragma once
#include <stdint.h>

template <uint8_t kSize, uint8_t kMergeOp>
struct PresetOpQueue {
  struct Entry { uint8_t op; uint8_t slot; };

  Entry q[kSize];
  uint8_t head = 0, count = 0;
  uint32_t dropped = 0;

  bool empty() const { return count == 0; }
  uint8_t size() const { return count; }
  const Entry &front() const { return q[head]; }

  bool push(uint8_t op, uint8_t slot) {
    if (op == kMergeOp && count) {
      Entry &tail = q[(uint8_t)(head + count - 1) % kSize];
      if (tail.op == kMergeOp) { tail.slot = slot; return true; }
    }
    if (count >= kSize) { dropped++; return false; }
    q[(uint8_t)(head + count) % kSize] = { op, slot };
    count++;
    return true;
  }

  void pop() {
    if (!count) return;
    head = (uint8_t)(head + 1) % kSize;
    count--;
  }

  void clear() { head = count = 0; }
};
