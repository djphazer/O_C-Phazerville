#ifndef PRESETSTAGE_H_
#define PRESETSTAGE_H_

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Recall staging registry: RAM images that stand in for files.
//
// A preset recall used to extract each section of the slot container into
// its live file (BANK_255.DAT, CAPTAIN.DAT) so the apps could read it back:
// every recall was a flash write with interrupts masked, hundreds of ms of
// dead audio. Now the recall registers each section's bytes here and
// PhzConfig::load_config serves them by name, straight from RAM.
//
// The image stays registered until a deferred sync has written the file at
// idle: Quadrants reloads its bank file on every app switch, so the RAM copy
// must keep answering until the disk agrees with it. Entries are served by
// name only; the filesystem the caller asked for is its own business.
//
// BSP-free and host-tested (test/test_preset_stage.cpp). The arena is the
// caller's (RAM2 on the module); put() copies into it, so the source buffer
// is free the moment put() returns.
// ---------------------------------------------------------------------------
class PresetStage {
 public:
  static const int kSlots = 4;
  static const int kNameMax = 12;   // 8.3 plus NUL fits in 13

  struct Image {
    char name[kNameMax + 1];
    const uint8_t *data;
    uint32_t len;
    bool used;
    bool synced;
  };

  PresetStage(uint8_t *arena, uint32_t cap) : arena_(arena), cap_(cap) { clear(); }

  void clear() {
    for (int i = 0; i < kSlots; ++i) {
      img_[i].name[0] = 0;
      img_[i].data = nullptr;
      img_[i].len = 0;
      img_[i].used = false;
      img_[i].synced = true;
    }
    top_ = 0;
    refused = superseded = 0;
  }

  // A recall is starting: anything still unsynced from the previous one
  // means the disk never caught up. Counted for the `T` report.
  void begin_recall() {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && !img_[i].synced) superseded++;
  }

  // Register (or replace) the image for `name`. False = refused: name too
  // long, no free slot, or no arena room; counted in `refused`.
  bool put(const char *name, const uint8_t *data, uint32_t len) {
    if (strlen(name) > (size_t)kNameMax) { refused++; return false; }
    Image *slot = find_slot(name);
    if (!slot) slot = free_slot();
    if (!slot) { refused++; return false; }
    if (all_synced_except(slot)) top_ = 0;   // nobody else holds arena bytes
    if (top_ + len > cap_) { refused++; return false; }
    memcpy(arena_ + top_, data, len);
    strncpy(slot->name, name, kNameMax);
    slot->name[kNameMax] = 0;
    slot->data = arena_ + top_;
    slot->len = len;
    slot->used = true;
    slot->synced = false;
    top_ += len;
    return true;
  }

  // The image to serve for `name`, or null once it is synced (or unknown).
  const Image *find(const char *name) const {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && !img_[i].synced && strcmp(img_[i].name, name) == 0)
        return &img_[i];
    return nullptr;
  }

  // Oldest unsynced entry for the flusher, in registration order.
  const Image *next_unsynced() const {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && !img_[i].synced) return &img_[i];
    return nullptr;
  }

  void mark_synced(const char *name) {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && strcmp(img_[i].name, name) == 0) {
        img_[i].synced = true;
        img_[i].used = false;
      }
    if (pending() == 0) top_ = 0;   // arena is free again
  }

  int pending() const {
    int n = 0;
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && !img_[i].synced) ++n;
    return n;
  }

  uint32_t refused;
  uint32_t superseded;

 private:
  Image *find_slot(const char *name) {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && strcmp(img_[i].name, name) == 0) return &img_[i];
    return nullptr;
  }
  Image *free_slot() {
    for (int i = 0; i < kSlots; ++i)
      if (!img_[i].used) return &img_[i];
    return nullptr;
  }
  bool all_synced_except(const Image *keep) const {
    for (int i = 0; i < kSlots; ++i)
      if (&img_[i] != keep && img_[i].used && !img_[i].synced) return false;
    return true;
  }

  Image img_[kSlots];
  uint8_t *arena_;
  uint32_t cap_;
  uint32_t top_;
};

namespace OC {
// The module's one registry, owned by PresetEngine.cpp; PhzConfig consults
// it in load_config and drops an entry in save_config (a fresh save makes
// the disk newer than the staged image).
PresetStage &RecallStage();
}

#endif  // PRESETSTAGE_H_
