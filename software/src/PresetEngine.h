#pragma once
// ---------------------------------------------------------------------------
// Whole-module preset engine for the 200e preset bus (Xenomorpher, T4.1).
//
// 30 slots (bus presets 0-29), each a full snapshot of module state, and each
// ONE FILE: PB_NN.PBS. A 16-byte header, a directory of up to 6 sections, then
// the payloads:
//   'G'  global settings values + slot manifest (PhzConfig format)
//   'A'  the per-app chunk stream (OC::AppData serialization + header)
//   'B'  active Quadrants preset extracted from its bank (preset keys remapped
//        to preset 0) + bank globals + audio applet graph
//   'S'  Scenery       'C'  Captain MIDI
//
// One file per slot is not tidiness, it is arithmetic. Every file costs whole
// erase blocks however few bytes it holds, and under the core's LittleFS
// geometry (64 KB blocks, so the 4 MB partition was 64 BLOCKS) the previous
// layout's 3-5 files per slot -- 90-150 blocks for 30 slots -- would have
// exhausted the filesystem around slot 20 while cheerfully reporting well
// over a megabyte free. PhzConfig::XenoFS has since moved the partition to
// 4 KB blocks (1024 of them), which makes this comfortable rather than
// necessary; one container per slot stays because it is also one open, one
// checksum and one rename per recall. Sections are opaque byte ranges, which
// is what let this change stay entirely out of PhzConfig.
//
// Slots live on INTERNAL FLASH, always, card or no card. This used to follow
// SDcard_Ready, which meant inserting a card made all 30 read empty and
// pulling it brought them back -- nothing lost, but indistinguishable from
// loss at the front panel. The card carries presets between modules instead:
// see ExportSlot/ImportSlot. Quadrants' own bank files still prefer SD, which
// is a genuine capacity case and a different question.
//
// Calibration is per-module and never in a preset.
//
// RequestSave/RequestRecall queue work in WIRE ORDER; Process() drains the
// queue from loop() context. They are loop-context only (the bus parser runs
// in PresetBus::Task, not the slave ISR), so no lock is involved. Order
// matters because a save stalls loop() for seconds and the bus keeps
// delivering: SAVE 29, RECALL 9, SAVE 5 arriving during one stall must leave
// slot 5 holding what was live AFTER recall 9, not before it. Recall applies
// LIVE: validate, stop the app ISR, restore chunks + globals, switch app,
// RESUME under AudioNoInterrupts. See docs in the plan / bring-up notes.
// ---------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>

namespace OC {
namespace PresetEngine {

#if defined(ARDUINO_TEENSY41)

static constexpr int kNumSlots = 30;

void Init();                  // call after app_switcher.Init (boot)
void Process();               // call from loop(); runs pending save/recall

// Queue a save/recall in arrival order; loop() context only (bus parser,
// console, boot). A recall queued directly behind another recall replaces it
// (only the last one would be audible anyway); saves are never coalesced
// away. Returns false only when the queue is full, which is counted.
bool RequestSave(uint8_t slot);
bool RequestRecall(uint8_t slot);
uint32_t RequestsDropped();   // queue-full refusals since boot

// Direct operations; loop() context only.
bool SaveSlot(uint8_t slot);
bool RecallSlot(uint8_t slot);

// millis() as a "0 = idle" timer stamp, for the deferred-persist and the
// overlay's hold timers. NOT `millis() | 1`: that rounds an even reading UP
// by one, so a check in the same millisecond -- the next loop pass, at
// ~300 kHz -- sees now - stamp wrap to 0xFFFFFFFF, and a 3 s deferral or a
// 250 ms hold has "elapsed" at once. A tap of encR in the overlay fired a
// bus-wide RECALL about half the time that way.
uint32_t StampMs();

// Quadrants recall interplay: after a recall, Resume() consumes this hint
// (>= 0 = load this bank number fresh from disk, preset 0) exactly once.
int ConsumeQuadrantsRecallHint();

// What the recall in progress is about to overwrite. Non-zero only while
// RecallSlot has the outgoing app in APP_EVENT_SUSPEND, and only for the
// stores the slot actually carries (a slot saved on a module that never ran
// Scenery has no 'S' section, and SCENERY.DAT then survives the recall).
// A Suspend handler's auto-save to one of these files is replaced by the
// slot's copy a millisecond later: a flash erase with interrupts off
// (250-295 ms of frozen audio and MIDI as measured under 64 KB blocks; ~45 ms
// a sector under 4 KB ones) for bytes nobody will ever read.
// The app-menu Suspend, where that save is the whole point, sees 0.
// RECALL_SUSPEND is set for every recall Suspend whatever the slot carries:
// for work that belongs to the menu gesture and not to a performance
// event, such as Captain's dump-to-host, which must not wait on USB.
enum RecallReplaces : uint8_t {
  REPLACES_BANK    = 1 << 0,   // BANK_255.DAT on quad_fs()
  REPLACES_SCENERY = 1 << 1,   // SCENERY.DAT
  REPLACES_CAPTAIN = 1 << 2,   // CAPTAIN.DAT (never at the boot recall)
  RECALL_SUSPEND   = 1 << 7,   // this Suspend is a recall, not the app menu
};
uint8_t RecallReplacing();

// status for UI / debug
int8_t LastSlot();            // slot this module has LOADED; -1 = none yet
int8_t BusSlot();             // slot the CASE is on: last save/recall
                              // attempted, refused or not; -1 = none yet
uint32_t OpCount();           // bumps when a save/recall finishes
bool LastSaveOk();            // result of the most recent save
const char *LastRecallError();  // why the last recall refused; nullptr = ok
// Boot-time restore of the last bus preset (200e power-up semantics):
// reads the persisted current slot and queues a local recall if it still
// validates. Call once from setup(), after the apps have started.
void BootRecall();  // skips Captain-config restore: boot keeps live edits
// The app on screen is part of the same power-down record as the slot: a
// power cycle comes back in it, whether it got there by the app menu, a
// console switch or a bus recall into another app. NoteAppOnScreen() marks
// the record dirty (written ~3 s later, one cheap EEPROM program, never a
// GLOBALS.CFG block erase); every app switch calls it. BootAppChoice()
// hands AppSwitcher::Init the recorded app before it resumes anything --
// false when the record has none, and the GLOBALS.CFG app stands.
// ForgetCurrent() blanks the record: a settings reset.
void NoteAppOnScreen();
bool BootAppChoice(uint16_t *app_id);
void ForgetCurrent();
bool SlotUsed(uint8_t slot);  // slot has a stored preset on disk

// slot names: flat PBNAMES.BIN sidecar (16 chars max, RAM-cached at Init;
// independent of the slot data so renames never touch preset content)
static constexpr size_t kNameLen = 16;
const char *SlotName(uint8_t slot);              // "" when unnamed
void SetSlotName(uint8_t slot, const char *name);  // persists immediately
// Names an import batch cached (ImportSlot reads each container's manifest
// name into RAM, not flash) -- one write for the whole batch. Process() also
// calls this, so a caller that forgets only defers the write one loop pass.
void FlushSlotNames();
bool LastWasSave();
bool Busy();

// ---- pre-write bank snapshot -----------------------------------------------
// One file holding the last good copy of a module's bank, taken before
// a write goes on the wire. The 200e write path can DETECT that it damaged a
// preset the user never touched, but until this existed it could not repair
// one: only hashes of the other 29 slots were kept, and the verify read-back
// overwrote the card image with the module's corrupt contents, destroying the
// last correct copy as a side effect of checking. Tagged with the address it
// came from, because restoring a 251e bank into whatever answers at some
// other address is the worst thing this engine could do.
bool SnapshotBank(uint8_t addr, const uint8_t *bank, uint32_t len, uint32_t crc);
bool SnapshotInfo(uint8_t *addr_out, uint32_t *len_out);   // is one available?
bool LoadSnapshot(uint8_t addr, uint8_t *dest, uint32_t cap, uint32_t *len_out);
void DiscardSnapshot();

// ---- export / import -------------------------------------------------------
// Presets live on internal flash, always. The card carries them between
// modules instead of holding them: one container is one self-contained file,
// so an export is a byte copy that another Xenomorpher can import into the
// same numbered slot. Both directions verify the destination before reporting
// success, and an import stages through a scratch name so a bad card can
// never destroy a good local preset.
enum ExportResult : uint8_t {
  EXPORT_OK = 0,
  EXPORT_NO_CARD,    // no card seated
  EXPORT_EMPTY,      // nothing in that slot on the source side
  EXPORT_BAD_SLOT,   // slot >= kNumSlots
  EXPORT_BAD_FILE,   // the file is damaged: on import the card's copy, on
                     // export the module's own slot (a recall would refuse it)
  EXPORT_LEGACY,     // pre-container slot; re-save it to convert, then export
  EXPORT_FAILED,     // the copy did not land
};
ExportResult ExportSlot(uint8_t slot);   // internal -> card
ExportResult ImportSlot(uint8_t slot);   // card -> internal
int CardSlotCount();                     // containers on the card; -1 = no card

// ---- legacy recovery -------------------------------------------------------
// Presets saved to SD by a build where preset_fs() used to BE the card (see
// the header comment above) are orphaned: nothing reads them, because
// ImportSlot only understands the current one-container format and the
// engine's own legacy multi-file reader (recall_stage_head, PresetEngine.cpp)
// only ever looks at internal flash. This is the recovery path: find a
// PB_NN_G.CFG + PB_NN_A.BIN (+ optional _B/_S/_C) set on the card, repackage
// it as a proper PB_NN.PBS container on internal flash, exactly as if it had
// arrived through ImportSlot -- without ever touching a slot that already
// holds something.
enum RecoverResult : uint8_t {
  RECOVER_OK = 0,
  RECOVER_NO_CARD,    // no card seated
  RECOVER_EMPTY,      // no legacy G+A set on the card for this slot
  RECOVER_BAD_SLOT,   // slot >= kNumSlots
  RECOVER_OCCUPIED,   // slot already holds a preset (container or legacy on
                       // internal flash); left alone on purpose, not recovered
  RECOVER_BAD_FILE,   // a legacy set was found but is damaged/incomplete
  RECOVER_FAILED,     // the container did not land
};
RecoverResult RecoverLegacyFromCard(uint8_t slot);
// Sweeps every slot. Returns the number actually recovered; -1 = no card
// seated (mirrors CardSlotCount()'s convention), so a caller can tell "no
// card" from "card present, nothing legacy to find" from "recovered N".
int RecoverAllLegacyFromCard();

#else  // non-T4.1 builds: inert stubs

static constexpr int kNumSlots = 30;
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
inline bool SnapshotBank(uint8_t, const uint8_t *, uint32_t, uint32_t) { return false; }
inline bool SnapshotInfo(uint8_t *, uint32_t *) { return false; }
inline bool LoadSnapshot(uint8_t, uint8_t *, uint32_t, uint32_t *) { return false; }
inline void DiscardSnapshot() {}

#endif

}  // namespace PresetEngine
}  // namespace OC
