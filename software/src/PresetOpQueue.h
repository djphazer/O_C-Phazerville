// A small FIFO of preset-bus ops with ONE merge rule, shared by the engine's
// request queue (PresetEngine.cpp) and the transport's broadcast queue
// (PresetBus.cpp) so the two can never disagree about it:
//
//   a RECALL queued behind a RECALL replaces it in place. Only the last of
//   two back-to-back recalls is ever heard -- the case applies the last one
//   -- so the first is dead weight, and merging it bounds the queue against
//   a manager sweeping through presets. SAVEs are never merged: each one is
//   a distinct "keep this", and dropping any of them is data loss. Nothing
//   merges ACROSS a save, either: "recall 3, save 3, recall 5" must run all
//   three, or slot 3 gets preset 5's state.
//
// The UI leans on this rule: PresetBusUI's completion watch answers a
// pending RECALL with whatever recall the engine ran, because a recall
// queued behind ours is what the engine (and the bus) actually did.
//
// Host-tested: test/test_preset_op_queue.cpp. No Arduino here.
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

  // false = refused (full); the caller says so out loud, this counts it
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
