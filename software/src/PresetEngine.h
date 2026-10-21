#pragma once
#include <stddef.h>
#include <stdint.h>

namespace OC {
namespace PresetEngine {

#if defined(ARDUINO_TEENSY41)

static constexpr int kNumSlots = 30;
static constexpr uint8_t kQuadBankCount = 100;

void Init();
void Process();

bool RequestSave(uint8_t slot);
bool RequestRecall(uint8_t slot);
uint32_t RequestsDropped();

bool SaveSlot(uint8_t slot);
bool RecallSlot(uint8_t slot);

uint32_t StampMs();

int ConsumeQuadrantsRecallHint();

enum RecallReplaces : uint8_t {
  REPLACES_BANK    = 1 << 0,
  REPLACES_SCENERY = 1 << 1,
  REPLACES_CAPTAIN = 1 << 2,
  RECALL_SUSPEND   = 1 << 7,
};
uint8_t RecallReplacing();

int8_t LastSlot();
int8_t BusSlot();
uint32_t OpCount();
bool LastSaveOk();
const char *LastRecallError();
void BootRecall();
void NoteAppOnScreen();
bool BootAppChoice(uint16_t *app_id);
void ForgetCurrent();
bool SlotUsed(uint8_t slot);

static constexpr size_t kNameLen = 16;
const char *SlotName(uint8_t slot);
void SetSlotName(uint8_t slot, const char *name);
void FlushSlotNames();
bool LastWasSave();
bool Busy();

bool SnapshotBank(uint8_t addr, const uint8_t *bank, uint32_t len, uint32_t crc);
bool SnapshotInfo(uint8_t *addr_out, uint32_t *len_out);
bool LoadSnapshot(uint8_t addr, uint8_t *dest, uint32_t cap, uint32_t *len_out);
void DiscardSnapshot();

enum ExportResult : uint8_t {
  EXPORT_OK = 0,
  EXPORT_NO_CARD,
  EXPORT_EMPTY,
  EXPORT_BAD_SLOT,
  EXPORT_BAD_FILE,
  EXPORT_LEGACY,
  EXPORT_FAILED,
};
ExportResult ExportSlot(uint8_t slot);
ExportResult ImportSlot(uint8_t slot);
int CardSlotCount();

enum RecoverResult : uint8_t {
  RECOVER_OK = 0,
  RECOVER_NO_CARD,
  RECOVER_EMPTY,
  RECOVER_BAD_SLOT,
  RECOVER_OCCUPIED,
  RECOVER_BAD_FILE,
  RECOVER_FAILED,
};
RecoverResult RecoverLegacyFromCard(uint8_t slot);
int RecoverAllLegacyFromCard();

uint8_t QuadLinkBank();
void SetQuadLinkBank(uint8_t bank);

bool QuadLinkEnabled();
void SetQuadLinkEnabled(bool on);

int CopyBankToSlots(uint8_t bank);
int CopySlotsToBank(uint8_t bank);

void SyncPresetToSlot(uint8_t bank, uint8_t preset_id);

#else

static constexpr int kNumSlots = 30;
static constexpr uint8_t kQuadBankCount = 100;
inline void Init() {}
inline void Process() {}
inline bool RequestSave(uint8_t) { return false; }
inline bool RequestRecall(uint8_t) { return false; }
inline uint32_t RequestsDropped() { return 0; }
inline bool SaveSlot(uint8_t) { return false; }
inline bool RecallSlot(uint8_t) { return false; }
inline int ConsumeQuadrantsRecallHint() { return -1; }
enum RecallReplaces : uint8_t {
  REPLACES_BANK = 1 << 0, REPLACES_SCENERY = 1 << 1, REPLACES_CAPTAIN = 1 << 2,
  RECALL_SUSPEND = 1 << 7,
};
inline uint8_t RecallReplacing() { return 0; }
inline int8_t LastSlot() { return -1; }
inline int8_t BusSlot() { return -1; }
inline uint32_t OpCount() { return 0; }
inline bool LastSaveOk() { return false; }
inline const char *LastRecallError() { return nullptr; }
inline void BootRecall() {}
inline void NoteAppOnScreen() {}
inline bool BootAppChoice(uint16_t *) { return false; }
inline void ForgetCurrent() {}
inline bool SlotUsed(uint8_t) { return false; }
static constexpr size_t kNameLen = 16;
inline const char *SlotName(uint8_t) { return ""; }
inline void SetSlotName(uint8_t, const char *) {}
inline void FlushSlotNames() {}
inline bool LastWasSave() { return false; }
inline bool Busy() { return false; }
enum ExportResult : uint8_t {
  EXPORT_OK = 0, EXPORT_NO_CARD, EXPORT_EMPTY,
  EXPORT_BAD_SLOT, EXPORT_BAD_FILE, EXPORT_LEGACY, EXPORT_FAILED,
};
inline ExportResult ExportSlot(uint8_t) { return EXPORT_NO_CARD; }
inline ExportResult ImportSlot(uint8_t) { return EXPORT_NO_CARD; }
inline int CardSlotCount() { return -1; }
enum RecoverResult : uint8_t {
  RECOVER_OK = 0, RECOVER_NO_CARD, RECOVER_EMPTY, RECOVER_BAD_SLOT,
  RECOVER_OCCUPIED, RECOVER_BAD_FILE, RECOVER_FAILED,
};
inline RecoverResult RecoverLegacyFromCard(uint8_t) { return RECOVER_NO_CARD; }
inline int RecoverAllLegacyFromCard() { return -1; }
inline uint8_t QuadLinkBank() { return 0; }
inline void SetQuadLinkBank(uint8_t) {}
inline bool QuadLinkEnabled() { return false; }
inline void SetQuadLinkEnabled(bool) {}
inline void SyncPresetToSlot(uint8_t, uint8_t) {}
inline int CopyBankToSlots(uint8_t) { return -1; }
inline int CopySlotsToBank(uint8_t) { return -1; }
inline bool SnapshotBank(uint8_t, const uint8_t *, uint32_t, uint32_t) { return false; }
inline bool SnapshotInfo(uint8_t *, uint32_t *) { return false; }
inline bool LoadSnapshot(uint8_t, uint8_t *, uint32_t, uint32_t *) { return false; }
inline void DiscardSnapshot() {}

#endif

}
}
