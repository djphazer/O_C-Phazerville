#ifndef PRESETSTAGE_H_
#define PRESETSTAGE_H_

#include <stdint.h>
#include <string.h>

class PresetStage {
 public:
  static const int kSlots = 4;
  static const int kNameMax = 12;

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

  void begin_recall() {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && !img_[i].synced) superseded++;
  }

  bool put(const char *name, const uint8_t *data, uint32_t len) {
    if (strlen(name) > (size_t)kNameMax) { refused++; return false; }
    Image *slot = find_slot(name);
    if (!slot) slot = free_slot();
    if (!slot) { refused++; return false; }
    if (all_synced_except(slot)) top_ = 0;
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

  const Image *find(const char *name) const {
    for (int i = 0; i < kSlots; ++i)
      if (img_[i].used && !img_[i].synced && strcmp(img_[i].name, name) == 0)
        return &img_[i];
    return nullptr;
  }

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
    if (pending() == 0) top_ = 0;
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
PresetStage &RecallStage();
}

#endif
