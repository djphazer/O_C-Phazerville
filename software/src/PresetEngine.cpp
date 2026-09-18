// Whole-module preset engine for the 200e preset bus. See PresetEngine.h.
//
// ROUND 4 -- the Store crash is CLOSED, and it was never in this file. The
// fault is in the USB audio input path; the full write-up lives at the top of
// src/Audio/USB_F32.cpp. Short version: a Store blocks loop() for seconds
// across LittleFS program-flash writes, the USB audio transport's stream-stall
// recovery (USBAudioInInterface::resetBuffer) then computes its ring write
// index from a smoothed DWT timestamp with no modulo and no bound, and the
// receive ISR dereferences whatever .bss word that out-of-range index lands
// on. Nothing below needs to change; the guards are in USB_F32.cpp, where the
// buffers actually live. The round-1..3 history is kept below because the
// ruling-out is still valid and worth not repeating.
#if defined(ARDUINO_TEENSY41)

#include <Arduino.h>
#include "src/extern/dspinst.h"  // before <Audio.h>: same include guard,
                                 // and Audio's copy lacks some intrinsics
#include <Audio.h>
#include <SD.h>

#include "PresetEngine.h"
#include "PresetStage.h"
#include "RtStats.h"
#include "OC_apps.h"
#include "OC_app_switcher.h"
#include "OC_storage.h"
#include "OC_core.h"
#include "OC_digital_inputs.h"
#include "OC_gpio.h"
#include "OC_ui.h"
#include "PhzConfig.h"
#include "PresetBus.h"   // GetStats(): bus-slave ISR count, for the save report
#include "Buchla200eWriteGuard.h"  // Buchla200eCrc32, for the snapshot
#include "HSUtils.h"
#include "PresetOpQueue.h"
#include "src/drivers/FreqMeasure/OC_FreqMeasure.h"

extern uint_fast8_t MENU_REDRAW;  // Main.cpp
// ---------------------------------------------------------------------------
// The Store-crash investigation, round 3. Read this before theorising again.
//
// SYMPTOM: an ordinary Store resets the module, reliably, at the same point:
// right after Captain's CAPTAIN.DAT save prints its last chunk checksum --
// i.e. inside save_config()'s close/remove/rename tail, or step 4's copy_file
// chain immediately after it. Both are LittleFS-on-program-flash write paths.
//
// WHAT IS RULED OUT, with evidence:
//
// * WDOG timeout. SRC_SRSR bit 4 was never set. Still true. The watchdog_feed()
//   calls below stay as cheap insurance, but they were never the fix.
//
// * "SRSR 0x02 proves a CPU lockup." It does NOT. RT1060 RM 21.6.3 names the
//   bit lockup_sysresetreq: set by a CPU lockup OR by any software write of
//   SYSRESETREQ to SCB_AIRCR -- which is exactly how the Teensy core's own
//   default fault handler reboots (startup.c, after parking ~8s). The core
//   itself disambiguates with SRC_GPR5 == 0x0BAD00F1 (CrashReport.cpp). We
//   were never reading GPR5; Main.cpp now captures it at boot and 't' prints
//   [FAULT-REBOOT] vs [LOCKUP or SW-RESET].
//
// * "CrashReport has nothing for this one." It may well have had plenty:
//   CrashReportClass::printTo() calls clear() on its way OUT, so Main.cpp's
//   print-to-Serial-then-print-to-buffer sequence threw the report away and
//   wrote "No Crash Data To Report" into CRASH.LOG every single time. Fixed
//   in Main.cpp (capture first, echo the buffer).
//
// * Round 2's fix: CORE::app_isr_enabled left true across
//   DispatchAppEvent(FLUSH)/(RESUME). Flashed and tested live -- SAME CRASH,
//   same point. The freeze is kept below (it matches RecallSlot and is
//   defensible on its own), but it is NOT the cause, and the premise was
//   unsupported: nothing CORE_timer_ISR reaches touches PhzConfig at all.
//
// * ISR-vs-main-thread data races generally. Every interrupt live on a T4.1
//   build was audited: CORE timer, UI timer (ui.Poll -> GPIO + event ring
//   only, never gated by anything), LPI2C1 general-call slave, ADC DMA,
//   display LPSPI, audio I2S DMA + software_isr, USB device/host, Serial8.
//   NONE of them touch PhzConfig::cfg_store, PresetEngine state, `capture`,
//   LittleFS, or the heap. In particular PresetBus's lpi2c1_slave_isr only
//   pushes into its own SPSC ring (and, while card-serving, into the RAM-only
//   BusCard image) -- it shares nothing with SaveSlot's call chain.
//
// * "The race is inside the flash window." There is no window: LittleFS's
//   program-flash driver (eepromemu_flash_write/erase_sector in the core's
//   eeprom.c) runs from ITCM with __disable_irq() held across the whole
//   erase/program, so every interrupt above is masked while flash is busy.
//
// * Stack exhaustion. Measured, not estimated: the crashing boot's SelfTest
//   reported 6200 bytes of DTCM stack never touched, nowhere near the guard.
//
// WHAT IT ACTUALLY WAS (round 4, from the first real CrashReport):
//
//   reset_cause SRC_SRSR=00000002 SRC_GPR5=0BAD00F1  (a CAUGHT fault, not a
//   lockup -- GPR5 is the core fault handler's own marker)
//   Code was executing from 0x8E8, CFSR 0x82 = DACCVIOL + MMARVALID,
//   accessed 0x80000C on one run and 0x1B4 on another.
//
//   addr2line against the exact ELF: AudioInputUSB_F32::copy_to_buffers, and
//   0x8E8 is the `vstr s15, [lr, #4]` that writes rxBuffer[bIdx][j]->data[].
//   Not our call chain at all -- an ISR that keeps running while a Store
//   blocks loop(), on a wild block pointer. See src/Audio/USB_F32.cpp.
//
// So the save path is a TRIGGER, not the bug: it is simply the longest thing
// in the firmware that stops loop() and repeatedly masks interrupts, which is
// exactly the condition the transport's stall recovery gets wrong. Guards are
// in USB_F32.cpp; the 't' selftest prints their trip counts. If a Store ever
// crashes again, check that line first -- non-zero counts with no crash means
// the guards are doing their job, zero counts means look somewhere new.
// ---------------------------------------------------------------------------
extern void watchdog_feed();      // Main.cpp
// Unused DTCM stack, in bytes, for the save's diagnostic line. The stack
// painting that answers it properly lives in the T4 infra work; where that is
// absent this reports "unknown" (0xFFFFFFFF) rather than pull the feature in.
__attribute__((weak)) uint32_t stack_low_water() { return 0xFFFFFFFFu; }

namespace OC {

// from OC_apps.cpp
extern void BuildAppData(AppData &data);
extern void ApplyAppData(const AppData &data);
extern void BuildGlobalSettingsValues();
extern void RestoreGlobalSettingsFromConfig(uint8_t scala_loaded_mask);
extern size_t ResolveAppIndexByID(uint16_t app_id);

namespace PresetEngine {

static constexpr uint16_t kQuadrantsAppId = TWOCCS("QS");
static constexpr uint8_t kScratchBank = 255;    // reserved: recall staging

// slot manifest keys, in the PRESETBUS namespace of PB_NN_G.CFG
static constexpr uint16_t kManifestKey = 8 << 8;    // == PRESETBUS_KEY
static constexpr uint16_t kSchemaKey  = kManifestKey | 0;
static constexpr uint16_t kFlagsKey   = kManifestKey | 1;
// The slot's name, 16 bytes packed little-endian into two values, so it
// travels inside the container: a preset exported to the card and imported
// on another module (or after a reflash) arrives with the name it was given.
// The local store (PBNAMES.BIN) stays authoritative for local recalls; an
// import copies the container's name over it. Containers written before
// these keys existed carry neither, and an import leaves their local name
// alone -- absent is "unknown", not "unnamed".
static constexpr uint16_t kNameKey0   = kManifestKey | 2;
static constexpr uint16_t kNameKey1   = kManifestKey | 3;
static constexpr uint64_t kSchemaVersion = 1;
// Where firmware before the EEPROM record (below) kept the current slot: a
// key in GLOBALS.CFG. Read once at boot to migrate; never written again. The
// key may linger in old GLOBALS.CFG files and in the 'G' section of old
// containers, and is ignored there.
static constexpr uint16_t kCurSlotKey = kManifestKey | 0x13;

// The current record: what the module was doing at power-down, persisted
// (debounced) so boot can put it back -- the bus slot the case was on (200e
// power-up semantics) and the app that was on screen. Eight bytes of
// emulated EEPROM (EEPROM_PRESETBUS_START, OC_config.h):
//
//   magic 'P', slot, ~slot, app lo, app hi, ~app lo, ~app hi, spare
//
// Each field carries its own complement so it validates on its own. Erased
// flash reads 0xFF, and 0xFF is not ~0xFF, so a never-written record fails
// the check instead of decoding as slot 255; "no slot" is written as 0xFF
// with a real complement (0x00) and rejected by the range check either way.
// The record was four bytes (slot only) before it carried the app: an old
// record has erased bytes where the app goes, so the app reads as absent and
// boot falls back to the app GLOBALS.CFG names, exactly as before.
//
// Not a file. A LittleFS write costs a flash erase with interrupts off
// (~270 ms of dead audio, USB and bus), and this write lands three seconds
// after every preset change -- i.e. while the performer is playing the sound
// they just recalled. An emulated-EEPROM byte is a single 2-byte flash program
// and is skipped when the value has not changed (cores/teensy4/eeprom.c).
// The app used to be written through GLOBALS.CFG on every switch; a
// cross-app bus recall measured 286 ms wall, 277 of them that write.
static constexpr uint8_t kCurRecMagic = 'P';

// slot: -1 = none (or absent/invalid); app: 0 = absent. Returns whether the
// record exists at all (magic), which is what the boot migration asks.
FLASHMEM static bool cur_rec_load(int8_t *slot, uint16_t *app) {
  uint8_t rec[EEPROM_PRESETBUS_SIZE];
  EEPROMStorage::read(EEPROM_PRESETBUS_START, rec, sizeof(rec));
  *slot = -1;
  *app = 0;
  if (rec[0] != kCurRecMagic) return false;
  if (rec[2] == (uint8_t)~rec[1] && rec[1] < kNumSlots) *slot = (int8_t)rec[1];
  if (rec[5] == (uint8_t)~rec[3] && rec[6] == (uint8_t)~rec[4])
    *app = (uint16_t)(rec[3] | (rec[4] << 8));
  return true;
}

FLASHMEM static void cur_rec_store(int8_t slot, uint16_t app) {
  const uint8_t s = slot < 0 ? 0xFF : (uint8_t)slot;
  const uint8_t lo = (uint8_t)app, hi = (uint8_t)(app >> 8);
  const uint8_t rec[EEPROM_PRESETBUS_SIZE] = {
    kCurRecMagic, s, (uint8_t)~s, lo, hi, (uint8_t)~lo, (uint8_t)~hi, 0xFF,
  };
  EEPROMStorage::update(EEPROM_PRESETBUS_START, rec, sizeof(rec));
}

// Quadrants writes its live preset id under this bare (bank-globals) key
// when handling APP_EVENT_FLUSH, so the extractor knows which preset block
// to pull out of the bank map. 253 is unused in the bank key map.
static constexpr uint16_t kQuadLivePresetKey = 253;

enum ContentFlags : uint8_t {
  CONTENT_BANK    = 1 << 0,   // bank section present (Quadrants was active)
  CONTENT_SCENERY = 1 << 1,
  CONTENT_CAPTAIN = 1 << 2,
};
// The manifest bits double as the public "about to be replaced" mask.
static_assert(int(CONTENT_BANK) == int(REPLACES_BANK) &&
              int(CONTENT_SCENERY) == int(REPLACES_SCENERY) &&
              int(CONTENT_CAPTAIN) == int(REPLACES_CAPTAIN),
              "RecallReplaces mirrors ContentFlags");

// The filesystem's erase block, which is also its allocation unit: every
// file consumes whole blocks however few bytes it holds. It is whatever
// PhzConfig::XenoFS mounted with -- 4 KB on T4.1 (1024 blocks in the 4 MB
// partition), the core's 32 KB on a T4.0 -- read at run time so the space
// guard below cannot drift from the geometry. Under the core's original
// 64 KB blocks that partition was only 64 blocks, which is what forced one
// container per slot (see the slot-container comment).
static uint32_t lfs_block_bytes() {
  const uint32_t b = PhzConfig::myfs.blockBytes();
  return b ? b : 4096;
}
// A save's worst case in bytes: the container (sections bounded by sec_buf
// plus one AppData plus two small files, well under 32 KB) and the scratch
// copy it renames through, plus metadata-pair slack. Turned into blocks of
// the mounted size at the guard.
static constexpr uint32_t kSaveBytesNeeded = 65536;

// ---- state -----------------------------------------------------------------

// Request queue, drained in arrival order by Process().
//
// This replaced a pair of last-wins slots (pending_save / pending_recall),
// which the bench caught reordering the bus: a save stalls loop() for
// seconds while the bus keeps delivering, so SAVE 29 / RECALL 9 / SAVE 5
// arriving in one stall ran as save 29, save 5, recall 9 -- slot 5 got the
// state from BEFORE the recall, and a second SAVE in the same window would
// have replaced the first outright. A preset manager's "save what you hear
// after this recall" is exactly that sequence.
//
// Sized for a bus, not a firehose: one entry per command a manager could
// plausibly send in the ~3 s a save takes. When it is full the newest request
// is refused and counted rather than an older one being dropped, because the
// older one is the one the sender has already moved on from.
enum ReqOp : uint8_t { REQ_SAVE, REQ_RECALL, REQ_BOOT_RECALL };
// The merge rule (a recall replaces a trailing recall, saves never merge)
// lives in PresetOpQueue.h, shared with the transport's broadcast queue.
static PresetOpQueue<8, REQ_RECALL> req_q;

static int8_t last_slot = -1;
// The slot the case is on, as distinct from the slot this module has loaded.
// Set the moment a save or recall is attempted, refused or not: when the bus
// says RECALL 7 and slot 7 is empty here, nothing is loaded and last_slot
// stays put -- but every other module in the case is now on 7, and the next
// NEXT pulse has to step to 8, not to last_slot + 1. -1 = none yet.
static int8_t bus_slot = -1;
static bool last_was_save = false;
static uint32_t cur_slot_dirty_ms = 0;   // 0 = clean; stamps slot AND app changes
// What the EEPROM record holds, mirrored so persist_cur_record can skip the
// common case (a bus save, or the manager re-sending the current preset)
// without a read. This engine is the record's only writer. -1 / 0 = unknown
// or absent.
static int8_t persisted_slot = -1;
static uint16_t persisted_app = 0;
static uint32_t op_count = 0;            // completed save/recall operations
static bool last_save_ok = false;
// Why the most recent recall refused, or nullptr if it succeeded. A recall
// that fails validation used to exit WITHOUT bumping op_count, so the
// overlay's completion watch waited out its whole 4-second timeout and then
// showed a generic RECALL FAILED -- for what was actually an empty slot.
// That misdirection cost a night: the owner read it as a bus fault when the
// truth was that the 15s Teensy restores had wiped every PB_* file (a full
// restore erases ALL flash including LittleFS; a reflash does not).
static const char *last_recall_err = nullptr;
static bool busy = false;
static int quad_recall_hint = -1;
// When Process() first held a request back for a bank transfer in flight
// (0 = not holding). See the deferral note in Process().
//
// The cap is sized from the bench, not from the sim: a real 251e BACKUP ran
// 10.8 s from the encR tap to "card image saved" (2026-09-02), so a write --
// RESTORE plus the verify BACKUP the app chains straight after it -- is ~22 s
// of transfer with nothing wrong, and two jobs at the master's 15 s hard cap
// are 30 s. 45 s is past any chain the app starts on its own; only a user
// starting job after job reaches it, and then the manager's request has
// waited long enough.
static uint32_t deferred_since = 0;
static constexpr uint32_t kDeferCapMs = 45000;
// True only while the power-up recall runs. That recall restores the
// preset's state but keeps two things that are the module's own rather
// than the scene's: the live Captain config and the app on screen.
static bool boot_recall = false;
static uint8_t recall_replacing = 0;       // RecallReplacing(): step 2 only

static DMAMEM AppData capture;               // RAM capture buffer (~4KB)

// RAM staging for the PhzConfig-format sections ('G' globals+manifest and
// 'B' bank extract at save; 'G' at recall). Sized well above what either
// holds -- GLOBALS.CFG is ~2.1 KB on the bench and a bank extract is one
// preset plus the bank globals, a few KB -- because overflow fails the save
// loudly rather than quietly, and OCRAM has the room.
static constexpr uint32_t kSecBufBytes = 16384;
static DMAMEM uint8_t sec_buf[kSecBufBytes];

// Zero-write recall. The file-backed sections a slot carries for the apps
// ('B' bank extract, 'S', 'C' Captain) used to be extracted to their live
// files on every recall: each one a flash write with interrupts masked,
// hundreds of ms of dead audio, USB and bus. They are now copied into this
// RAM2 arena and registered in RecallStage(); PhzConfig::load_config serves
// them by name. The disk catches up in stage_sync_pump() at idle. A section
// too large for the arena or the temp buffer falls back to the old write.
static constexpr uint32_t kStageArenaBytes = 24576;
static DMAMEM uint8_t stage_arena[kStageArenaBytes];
static PresetStage recall_stage(stage_arena, kStageArenaBytes);
static constexpr uint32_t kStageTmpBytes = 12288;
static DMAMEM uint8_t stage_tmp[kStageTmpBytes];
static uint32_t last_recall_ms = 0;   // the sync pump waits 3 s after this
// OC::RecallStage() is defined after the namespaces close, at the file end.

// active Quadrants preset during bank extraction (predicate/remap context)
static uint8_t extract_preset;

// ---- helpers ---------------------------------------------------------------

// A wall clock that keeps running while LittleFS writes flash.
//
// millis() does not. Every program and erase on the program flash goes
// through eepromemu_flash_write/erase_64K_block (cores/teensy4/eeprom.c),
// which hold __disable_irq() for the whole FlexSPI operation because code is
// executing from that same chip; systick is masked with everything else. The
// bench caught the gap: a save the module reported as 188 ms took 2748 ms by
// the host's clock, and a recall reported at 40 ms took 630 ms. The report
// was not merely imprecise, it was measuring exactly the part of the work
// that was NOT the problem.
//
// The Cortex-M7 DWT cycle counter (enabled at startup) is unaffected by
// PRIMASK. It wraps every ~7 s at 600 MHz, so it is read as laps -- each one
// shorter than any single flash op can be (a 64 KB block erase, the core's
// geometry, is 2 s worst case) -- and summed in 64 bits.
struct WallClock {
  uint32_t last;
  uint64_t cycles;
  void start() { last = ARM_DWT_CYCCNT; cycles = 0; }
  uint32_t lap_ms() {
    const uint32_t now = ARM_DWT_CYCCNT;
    const uint32_t d = now - last;
    last = now;
    cycles += d;
    return d / (F_CPU_ACTUAL / 1000);
  }
  uint32_t total_ms() const { return (uint32_t)(cycles / (F_CPU_ACTUAL / 1000)); }
};

// Where preset containers live: ALWAYS internal flash, card or no card.
//
// This used to be `SDcard_Ready ? SD : myfs`, which made the instrument's
// preset memory depend on whether a card happened to be seated. Inserting one
// made all 30 slots read "Empty preset"; pulling it brought them back. Nothing
// was lost, but a musician cannot tell that from the front panel, and a preset
// store that answers differently depending on an accessory is not a preset
// store. The card is for carrying presets between modules, not for holding
// them -- see ExportSlot/ImportSlot at the end of this file.
//
// It fits: 30 containers of ~5 KB is 60 of the 1024 4 KB blocks XenoFS gives
// us on T4.1 (it fit even under the core's 64 KB blocks, at 30 of 64). The
// old routing was never a capacity decision anyway; SD was simply the newer,
// roomier disk.
static FS &preset_fs() { return PhzConfig::myfs; }

// Where QUADRANTS keeps its banks, which is a genuinely different question:
// banks are numerous and it prefers SD when there is one (Quadrants.h:99-103,
// 2104-2107). The preset engine only touches this to hand a recalled bank back
// to the place Quadrants will look for it.
static FS &quad_fs() { return SDcard_Ready ? (FS &)SD : (FS &)PhzConfig::myfs; }

// Removable media the export/import path uses. Null when no card is seated.
static FS *card_fs() { return SDcard_Ready ? (FS *)&SD : nullptr; }
static void load_names();  // defined with the name store below
static void pack_name(uint8_t slot, uint64_t out[2]);

FLASHMEM static void slot_name(char *buf, uint8_t slot, char kind, const char *ext) {
  // PB_NN_K.EXT
  buf[0] = 'P'; buf[1] = 'B'; buf[2] = '_';
  buf[3] = '0' + slot / 10; buf[4] = '0' + slot % 10;
  buf[5] = '_'; buf[6] = kind; buf[7] = '.';
  buf[8] = ext[0]; buf[9] = ext[1]; buf[10] = ext[2]; buf[11] = 0;
}

// Source and destination filesystems are separate on purpose. Slot files live
// on preset_fs() (always internal flash), but the live names they restore to
// belong to whichever FS the owning APP actually reads: Captain and Scenery
// only ever read myfs, Quadrants prefers SD. Copying within a single FS is
// what silently put Captain's and Scenery's state on the card, where neither
// of them ever looks.
FLASHMEM static bool copy_file(FS &sfs, const char *from, FS &dfs, const char *to) {
  File src = sfs.open(from);
  if (!src) return false;
  dfs.remove(to);
  File dst = dfs.open(to, FILE_WRITE_BEGIN);
  if (!dst) { src.close(); return false; }
  uint8_t buf[512];
  bool ok = true;
  int n;
  while ((n = src.read(buf, sizeof(buf))) > 0) {
    if (dst.write(buf, n) != (size_t)n) { ok = false; break; }
  }
  src.close();
  dst.close();
  if (!ok) dfs.remove(to);
  return ok;
}

// PB_NN_A.BIN framing
struct AppDataHeader {
  uint32_t fourcc;    // 'OCPB'
  uint16_t version;
  uint16_t used;
  uint16_t checksum;  // 16-bit sum over payload
  uint16_t reserved;
};
static constexpr uint32_t kAppDataFourcc = 0x4243504FUL;  // "OPCB" LE = 'O','P','C','B'... spelled: bytes O C P B
static uint16_t sum16(const uint8_t *p, size_t n) {
  uint16_t s = 0;
  while (n--) s += *p++;
  return s;
}

// Parse an app-data section from wherever the file cursor currently sits, so
// the legacy PB_NN_A.BIN reader and the container's 'A' section share one
// validator instead of drifting apart.
FLASHMEM static bool read_appdata_stream(File &f) {
  AppDataHeader h;
  bool ok = f.read((uint8_t *)&h, sizeof(h)) == sizeof(h) &&
            h.fourcc == kAppDataFourcc && h.version == 1 &&
            h.used <= AppData::kAppDataSize;
  if (ok) {
    ok = f.read(capture.data, h.used) == h.used &&
         sum16(capture.data, h.used) == h.checksum;
    capture.used = h.used;
  }
  return ok;
}

// `fs` defaults to preset_fs() so every existing call site (the legacy
// internal-flash recall fallback below) is unchanged; RecoverLegacyFromCard
// is the one caller that passes card_fs() explicitly, to validate a legacy
// PB_NN_A.BIN sitting on the card the same way this already validates one on
// internal flash -- same fourcc/version/checksum bar, same `capture` result.
FLASHMEM static bool read_appdata_file(uint8_t slot, FS &fs = preset_fs()) {
  char name[12];
  slot_name(name, slot, 'A', "BIN");
  File f = fs.open(name);
  if (!f) return false;
  const bool ok = read_appdata_stream(f);
  f.close();
  return ok;
}

// ---- slot container --------------------------------------------------------
//
// WHY ONE FILE PER SLOT. LittleFS allocates whole erase blocks, and when
// this was written the partition used the core's geometry: 65536-byte
// sectors on Teensy 4.1 (SECTOR_SIZE in the core's LittleFS.cpp; 32768 on a
// T4.0). Every file costs at least one whole block no matter how little it
// holds -- the 588-byte PB_04_A.BIN measured on the bench occupied 64 KB.
// With a 4 MB partition that was only 64 blocks TOTAL, so the old layout's
// 3-5 files per slot could not survive its own 30 slots: 30 x 3 = 90 blocks
// against a 64-block filesystem, i.e. the filesystem ran out somewhere
// around slot 20 while reporting well over a megabyte free.
//
// Packing every section into one file made a slot cost one block instead of
// three-to-five, which is what made 30 slots fit. PhzConfig::XenoFS has
// since remounted the partition in 4 KB blocks, so the pressure is gone;
// the container stays because it is also one open, one checksum and one
// rename per recall. Layout:
//
//   0x00  ContainerHeader                     16 bytes
//   0x10  SectionEntry[kMaxSections]          6 x 12 = 72 bytes
//   0x58  section payloads, in write order
//
// The file is written in ONE forward pass: every section is planned first
// (source, length, checksum -- files are read once for that, RAM images are
// summed), then header, directory and payloads go down in order. It used to
// reserve the directory and seek back to backfill it once the lengths were
// known. On littlefs a write behind the end of an open file relocates the
// block -- a fresh erase with interrupts off, then a byte-by-byte copy of
// everything after the write -- so that one backfill cost as much as the
// whole container. Reads are cheap here (memory-mapped flash); erases are
// what a save is made of, and the plan/write split gets it down to the one
// the container's own block needs.
//
// Sections are opaque byte ranges: a PhzConfig-format section is exactly
// what save_config() would have written, taken from PhzConfig::serialize()
// in RAM and handed back to deserialize() at recall. No scratch file in
// either direction.
// ---------------------------------------------------------------------------

static constexpr uint32_t kContainerFourcc = 0x53425058UL;  // 'X','P','B','S'
static constexpr uint16_t kContainerVersion = 1;
static constexpr int kMaxSections = 6;

struct ContainerHeader {
  uint32_t fourcc;
  uint16_t version;
  uint16_t count;
  uint32_t reserved0;
  uint32_t reserved1;
};
struct SectionEntry {
  uint8_t  kind;      // 'G','A','B','S','C'; 0 = unused
  uint8_t  pad;
  uint16_t checksum;  // sum16 over the payload
  uint32_t offset;    // absolute, from file start
  uint32_t length;
};
static_assert(sizeof(ContainerHeader) == 16, "container header must be 16 bytes");
static_assert(sizeof(SectionEntry) == 12, "section entry must be 12 bytes");

static constexpr uint32_t kPayloadStart =
    sizeof(ContainerHeader) + kMaxSections * sizeof(SectionEntry);  // 88

// "PB_NN.PBS"
FLASHMEM static void container_name(char *buf, uint8_t slot) {
  buf[0] = 'P'; buf[1] = 'B'; buf[2] = '_';
  buf[3] = '0' + slot / 10; buf[4] = '0' + slot % 10;
  buf[5] = '.'; buf[6] = 'P'; buf[7] = 'B'; buf[8] = 'S'; buf[9] = 0;
}

// A planned section: where its bytes come from when the container is
// written. Files are re-read at write time (they were read once already to
// measure and checksum them); RAM sections may be two pieces so the app-data
// section can carry its AppDataHeader without a copy.
struct SectionSrc {
  FS *fs;
  const char *path;
  const uint8_t *mem, *mem2;
  uint32_t len, len2;
};

struct ContainerWriter {
  SectionEntry sec[kMaxSections];
  SectionSrc src[kMaxSections];
  int n;
  uint32_t pos;     // next payload offset
  bool ok;
};

FLASHMEM static void cw_begin(ContainerWriter &w) {
  w.n = 0;
  w.pos = kPayloadStart;
  w.ok = true;
}

// Plan a whole file as one section. An absent or empty source is NOT a
// failure -- it just means the slot has no such content (no Scenery file
// yet, Quadrants not active) and no entry is recorded for it.
FLASHMEM static bool cw_plan_file(ContainerWriter &w, char kind,
                                  FS &fs, const char *path) {
  if (!w.ok || w.n >= kMaxSections) return false;
  File s = fs.open(path, FILE_READ);
  if (!s) return false;
  const uint32_t want = (uint32_t)s.size();
  uint8_t buf[256];
  uint16_t sum = 0;
  uint32_t len = 0;
  int r;
  while ((r = s.read(buf, sizeof(buf))) > 0) {
    for (int i = 0; i < r; ++i) sum += buf[i];
    len += (uint32_t)r;
  }
  s.close();
  if (!len) return false;
  // A short read here is a source we cannot trust; refusing is cheaper than
  // a container that validates clean and recalls with state missing.
  if (len != want) {
    serial_printf("PresetEngine: section '%c' short: %lu of %lu bytes\n",
                  kind, (unsigned long)len, (unsigned long)want);
    w.ok = false;
    return false;
  }
  w.sec[w.n] = { (uint8_t)kind, 0, sum, w.pos, len };
  w.src[w.n] = { &fs, path, nullptr, nullptr, 0, 0 };
  w.n++;
  w.pos += len;
  return true;
}

FLASHMEM static bool cw_plan_mem(ContainerWriter &w, char kind,
                                 const uint8_t *a, uint32_t la,
                                 const uint8_t *b = nullptr, uint32_t lb = 0) {
  if (!w.ok || w.n >= kMaxSections || !la) return false;
  const uint16_t sum = (uint16_t)(sum16(a, la) + (b ? sum16(b, lb) : 0));
  w.sec[w.n] = { (uint8_t)kind, 0, sum, w.pos, la + lb };
  w.src[w.n] = { nullptr, nullptr, a, b, la, lb };
  w.n++;
  w.pos += la + lb;
  return true;
}

// The app-data section carries the same AppDataHeader framing the standalone
// PB_NN_A.BIN used, so read_appdata_stream() validates either source. The
// header has to outlive the plan, hence static.
static AppDataHeader appdata_hdr;
FLASHMEM static bool cw_plan_appdata(ContainerWriter &w) {
  appdata_hdr = { kAppDataFourcc, 1, (uint16_t)capture.used,
                  sum16(capture.data, capture.used), 0 };
  return cw_plan_mem(w, 'A', (const uint8_t *)&appdata_hdr, sizeof(appdata_hdr),
                     capture.data, capture.used);
}

FLASHMEM static bool cw_put(File &f, const uint8_t *p, uint32_t n) {
  return n == 0 || f.write(p, n) == n;
}

FLASHMEM static bool cw_copy_file(File &f, FS &fs, const char *path, uint32_t want) {
  File s = fs.open(path, FILE_READ);
  if (!s) return false;
  uint8_t buf[256];
  uint32_t len = 0;
  int r;
  bool ok = true;
  while (ok && (r = s.read(buf, sizeof(buf))) > 0) {
    ok = f.write(buf, r) == (size_t)r;
    len += (uint32_t)r;
    watchdog_feed();
  }
  s.close();
  // The source changed size between plan and write: the directory would lie.
  return ok && len == want;
}

// Write header, directory and payloads in one pass, then publish via rename
// so a torn write can never replace a good slot (same discipline as
// PhzConfig::save_config).
FLASHMEM static bool cw_commit(ContainerWriter &w, const char *tmp, const char *final_name) {
  if (!w.ok) return false;
  preset_fs().remove(tmp);
  File f = preset_fs().open(tmp, FILE_WRITE_BEGIN);
  if (!f) return false;

  const ContainerHeader h = { kContainerFourcc, kContainerVersion,
                              (uint16_t)w.n, 0, 0 };
  bool ok = cw_put(f, (const uint8_t *)&h, sizeof(h));
  for (int i = 0; i < kMaxSections && ok; ++i) {
    const SectionEntry e = (i < w.n) ? w.sec[i] : SectionEntry{ 0, 0, 0, 0, 0 };
    ok = cw_put(f, (const uint8_t *)&e, sizeof(e));
  }
  for (int i = 0; i < w.n && ok; ++i) {
    const SectionSrc &src = w.src[i];
    if (src.fs) ok = cw_copy_file(f, *src.fs, src.path, w.sec[i].length);
    else ok = cw_put(f, src.mem, src.len) && cw_put(f, src.mem2, src.len2);
    watchdog_feed();
  }
  f.close();
  if (!ok) { preset_fs().remove(tmp); return false; }
  // verify the bytes actually landed: LittleFS has produced 0-byte files
  // while reporting success on a degraded FS
  File v = preset_fs().open(tmp, FILE_READ);
  ok = v && (uint32_t)v.size() == w.pos;
  if (v) v.close();
  if (!ok) { preset_fs().remove(tmp); return false; }
  // Rename FIRST. littlefs's rename atomically replaces an existing
  // destination, so on internal flash the old slot is never absent: it is
  // either the previous container or the new one. The previous
  // remove-then-rename opened a window where a power cut left the slot with
  // no container at all -- and for a migrated slot, no legacy files either,
  // since those were retired on the earlier commit. The only survivor was
  // the temp file, which the next save of ANY slot deletes in cw_commit.
  // The fallback remove is for SD, where replace-on-rename is not promised.
  ok = preset_fs().rename(tmp, final_name);
  if (!ok) {
    preset_fs().remove(final_name);
    ok = preset_fs().rename(tmp, final_name);
    // If THAT failed we have already removed the old container, so the tmp
    // file is now the only copy of this slot in existence. Leave it alone:
    // the next save reclaims the name, which is a block we can spare, and
    // until then a human has something to recover. Removing it here -- as
    // the first version of this did -- deleted the old container and the
    // freshly verified new one in the same breath, which is exactly the
    // slot loss the rename-first ordering exists to prevent, just moved to
    // a rarer path. Only the first-rename failure is safe to clean up
    // after, because there the old container is still in place.
    if (!ok)
      serial_printf("PresetEngine: %s left in place -- it is the only copy "
                    "of this slot\n", tmp);
    return ok;
  }
  return ok;
}

// Open a slot container and read its directory. Returns false when the file
// is absent or not a container -- the caller then falls back to the legacy
// multi-file layout.
FLASHMEM static bool container_parse(File &f, SectionEntry *sec, int &n) {
  ContainerHeader h;
  if (f.read((uint8_t *)&h, sizeof(h)) != sizeof(h) ||
      h.fourcc != kContainerFourcc || h.version != kContainerVersion ||
      h.count > kMaxSections)
    return false;
  n = h.count;
  for (int i = 0; i < n; ++i) {
    if (f.read((uint8_t *)&sec[i], sizeof(sec[i])) != sizeof(sec[i]))
      return false;
    // a section must lie inside the file
    if (sec[i].offset < kPayloadStart ||
        (uint64_t)sec[i].offset + sec[i].length > (uint64_t)f.size())
      return false;
  }
  return true;
}

FLASHMEM static bool container_open(uint8_t slot, File &f, SectionEntry *sec, int &n) {
  char name[12];
  container_name(name, slot);
  f = preset_fs().open(name, FILE_READ);
  if (!f) return false;
  if (!container_parse(f, sec, n)) {
    f.close();
    return false;
  }
  return true;
}

// Retire the pre-container files for a slot once its container is safely on
// disk. This is what actually reclaims the blocks: each of these cost a whole
// erase block (64 KB under the geometry they were written in), so a converted
// slot hands back two to four of them.
FLASHMEM static void remove_legacy_slot(uint8_t slot) {
  static const char kinds[] = { 'G', 'A', 'B', 'S', 'C' };
  static const char *const exts[] = { "CFG", "BIN", "DAT", "DAT", "DAT" };
  char name[12];
  for (unsigned i = 0; i < sizeof(kinds); ++i) {
    slot_name(name, slot, kinds[i], exts[i]);
    preset_fs().remove(name);
  }
}

FLASHMEM static const SectionEntry *find_section(const SectionEntry *sec, int n, char kind) {
  for (int i = 0; i < n; ++i)
    if (sec[i].kind == (uint8_t)kind) return &sec[i];
  return nullptr;
}

// Read one section into RAM, checksum-verified. Returns false when it does
// not fit or does not add up; `buf` holds nothing trustworthy then.
FLASHMEM static bool section_to_mem(File &f, const SectionEntry &e,
                                    uint8_t *buf, uint32_t cap) {
  if (e.length > cap || !f.seek(e.offset)) return false;
  if (f.read(buf, e.length) != (int)e.length) return false;
  return sum16(buf, e.length) == e.checksum;
}

// Checksum a section in place, without a buffer. Reads only, off
// memory-mapped flash, so a few KB costs well under a millisecond -- cheap
// enough to run on every recall, for every section the container carries.
FLASHMEM static bool section_sum_ok(File &f, const SectionEntry &e) {
  if (!f.seek(e.offset)) return false;
  uint8_t buf[128];
  uint32_t left = e.length;
  uint16_t sum = 0;
  while (left) {
    const uint32_t want = left < sizeof(buf) ? left : sizeof(buf);
    if (f.read(buf, want) != (int)want) return false;
    sum += sum16(buf, want);
    left -= want;
  }
  return sum == e.checksum;
}

// Everything a recall would check before applying, on an arbitrary file:
// directory parses, every section adds up, and the two sections no preset
// is complete without are present. This is the bar for a container that
// arrives from OUTSIDE (a card import) before it may replace a local slot,
// and for proving an export actually landed on the card.
FLASHMEM static bool container_verify(FS &fs, const char *name) {
  File f = fs.open(name, FILE_READ);
  if (!f) return false;
  SectionEntry sec[kMaxSections];
  int n = 0;
  bool ok = container_parse(f, sec, n);
  for (int i = 0; ok && i < n; ++i) ok = section_sum_ok(f, sec[i]);
  if (ok) ok = find_section(sec, n, 'A') && find_section(sec, n, 'G');
  f.close();
  return ok;
}

// Does `dest` already hold exactly the section's bytes? Reads only, which
// on memory-mapped flash costs nothing next to the sector erase the rewrite
// would take -- a recall that changes nothing about Captain's setup should
// not stall the module re-writing CAPTAIN.DAT.
//
// Answers with WHERE the two differ rather than a bare no, because a bare
// no hid a bug: through a whole soak two slots carrying the same-sized
// Captain image compared unequal on every recall, and nothing on the
// console said whether the content differed or only the record order
// (PhzConfig serialized in unordered_map order; it now sorts by key).
// Reading the offset: the chunk header is [sig 2][count 2][xor of values
// 8], so "+4" means the xor differs -- real content, since xor does not
// care about order -- while a first difference at +12 or later under an
// equal header would be order alone. The bench pair turned out to be +4.
// kSectionSame when identical; kSectionNoFile when there is nothing to
// compare against; otherwise the first differing offset, with a length
// mismatch counted as differing at the shorter length.
static constexpr int32_t kSectionSame = -1;
static constexpr int32_t kSectionNoFile = -2;
FLASHMEM static int32_t section_diff_offset(File &f, const SectionEntry &e,
                                            const char *dest, FS &fs,
                                            uint32_t *file_size) {
  File d = fs.open(dest, FILE_READ);
  if (!d) { *file_size = 0; return kSectionNoFile; }
  *file_size = d.size();
  const uint32_t common = *file_size < e.length ? *file_size : e.length;
  int32_t result = kSectionSame;
  if (!f.seek(e.offset)) result = 0;
  uint8_t a[128], b[128];
  uint32_t pos = 0;
  while (result == kSectionSame && pos < common) {
    const uint32_t want = common - pos < sizeof(a) ? common - pos : sizeof(a);
    if (f.read(a, want) != (int)want || d.read(b, want) != (int)want) {
      result = (int32_t)pos;
      break;
    }
    for (uint32_t i = 0; i < want; ++i) {
      if (a[i] != b[i]) { result = (int32_t)(pos + i); break; }
    }
    pos += want;
  }
  if (result == kSectionSame && *file_size != e.length) result = (int32_t)common;
  d.close();
  return result;
}

// Extract one section back out to a standalone file. Verifying the checksum
// before the destination is touched is not possible in one pass without a
// buffer, so the copy is written to a scratch name and only then renamed.
//
// Where a recall's time goes, measured: the write+close is the whole cost.
// Under the core's 64 KB blocks a 1794-byte CAPTAIN.DAT took 250-270 ms
// there (one block erase, interrupts off) with the remove/open/rename around
// it at 0/2/6-8 ms, which is why the scratch-and-rename was never worth
// dropping; under XenoFS's 4 KB blocks the same write is ~58 ms. The line
// printed at the end is what keeps that visible on the console.
FLASHMEM static bool section_to_file(File &f, const SectionEntry &e,
                                     const char *dest, FS &fs) {
  uint32_t on_flash = 0;
  const int32_t diff = section_diff_offset(f, e, dest, fs, &on_flash);
  if (diff == kSectionSame) return true;
  if (!f.seek(e.offset)) return false;
  WallClock wall;
  wall.start();
  static const char *const kScratch = "PB_XTR.TMP";
  fs.remove(kScratch);
  File d = fs.open(kScratch, FILE_WRITE_BEGIN);
  if (!d) return false;
  uint8_t buf[256];
  uint32_t left = e.length;
  uint16_t sum = 0;
  bool ok = true;
  while (left) {
    const uint32_t want = left < sizeof(buf) ? left : sizeof(buf);
    const int r = f.read(buf, want);
    if (r != (int)want) { ok = false; break; }
    if (d.write(buf, r) != (size_t)r) { ok = false; break; }
    for (int i = 0; i < r; ++i) sum += buf[i];
    left -= want;
    watchdog_feed();
  }
  d.close();
  const uint32_t write_ms = wall.lap_ms();
  if (ok) ok = (sum == e.checksum);
  if (!ok) { fs.remove(kScratch); return false; }
  fs.remove(dest);
  ok = fs.rename(kScratch, dest);
  if (!ok) fs.remove(kScratch);
  const unsigned long rename_ms = wall.lap_ms();
  if (diff == kSectionNoFile)
    serial_printf("PresetEngine: restored %s (%lu bytes, was absent) write %lu ms, rename %lu ms\n",
                  dest, (unsigned long)e.length, (unsigned long)write_ms, rename_ms);
  else
    serial_printf("PresetEngine: restored %s (%lu bytes, was %lu, differs at +%ld) write %lu ms, rename %lu ms\n",
                  dest, (unsigned long)e.length, (unsigned long)on_flash,
                  (long)diff, (unsigned long)write_ms, rename_ms);
  return ok;
}

// bank-key extraction: keep the active preset's block + the bank globals
// (bare keys 100-255) + VERSION_KEY; remap the preset block to preset 0.
FLASHMEM static bool bank_pred(PhzConfig::KEY k) {
  if (k == 0xFFFF) return true;                     // VERSION_KEY
  const uint16_t block = k >> 11;
  const uint16_t low = k & 0x7FF;
  if (block == 0 && low >= 100 && low < 256) return true;  // bank globals
  if (block == extract_preset && (low < 100 || low >= 256)) return true;
  return false;
}
FLASHMEM static PhzConfig::KEY bank_remap(PhzConfig::KEY k) {
  if (k == 0xFFFF) return k;
  const uint16_t low = k & 0x7FF;
  if ((k >> 11) == extract_preset && (low < 100 || low >= 256))
    return low;  // move to preset block 0
  return k;
}

// ---- save ------------------------------------------------------------------

FLASHMEM bool SaveSlot(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  busy = true;
  bus_slot = (int8_t)slot;
  serial_printf("PresetEngine: save slot %d\n", slot);

  // A save cannot avoid writing flash, and a flash write on this part masks
  // interrupts for the whole erase/program: audio, USB, the display and the
  // bus slave all stop. That is declared here rather than suffered, so the
  // stall is heard as a short dip the player caused by pressing STORE
  // instead of a click into a frozen tone. The window fades out on the way
  // in and back in when it goes out of scope, at every return below.
  //
  // A save is the right place for this and a background write is not: only a
  // write the player asked for may take the audio away.
  const OC::RT::PersistenceWindow window("save");

  // Free-space guard. UNCONDITIONAL now: presets always land on internal
  // flash, so there is no longer an "SD is effectively unbounded" case to
  // skip it for. Leaving the old !SDcard_Ready gate in place would have
  // disabled the only thing standing between a full filesystem and a torn
  // write, for every user with a card seated.
  //
  // This MUST be denominated in blocks, not bytes. LittleFS hands out whole
  // erase blocks -- lfs_block_bytes() -- so a slot that needs one block
  // cannot be placed when one block is not free, however many spare BYTES
  // the filesystem reports. The previous 24 KB threshold was smaller than a
  // single block under the core's 64 KB geometry, so it cheerfully
  // green-lit saves the allocator had no room for; the failure then
  // surfaced as a torn write far downstream instead of an honest refusal
  // here.
  //
  // A save needs the container plus the scratch file it renames through, so
  // require kSaveBytesNeeded worth of blocks rather than exactly one.
  //
  // usedSize() has been observed reporting == totalSize() on a healthy FS,
  // so treat that state as "unknown" and rely on post-write verification.
  {
    const uint64_t total = PhzConfig::myfs.totalSize();
    const uint64_t used = PhzConfig::myfs.usedSize();
    const uint32_t block = lfs_block_bytes();
    const uint32_t blocks_needed = (kSaveBytesNeeded + block - 1) / block;
    if (used < total && (total - used) / block < blocks_needed) {
      HS::PokePopup(HS::MESSAGE_POPUP, "Disk full !!");
      busy = false;
      serial_printf("PresetEngine: save refused, %lu KB free < %lu blocks\n",
                    (unsigned long)((total - used) >> 10),
                    (unsigned long)blocks_needed);
      return false;
    }
  }

  const uint16_t app_id = app_switcher.current_app()->id();
  uint8_t flags = 0;
  // Container staging: renamed into place by cw_commit, so it never costs a
  // block in the steady state.
  static const char *const kCtrTmp = "PB_CTR.TMP";
  char final_name[12];
  container_name(final_name, slot);

  // Instrumentation for the Store-crash hunt (see the note at the top of
  // this file): how long the save blocked loop(), how much interrupt traffic
  // the bus slave took while it did, and -- the number that matters -- how
  // much DTCM stack was still unused afterwards. A save that survives with
  // a low_water in the low hundreds of bytes means the next one may not,
  // and that a fault taken during exception stacking is the mechanism.
  const uint32_t t_start = millis();
  const uint32_t isr_start = PresetBus::GetStats().isr_count;
  WallClock wall;
  wall.start();
  // per-phase wall time, ms: capture, flush, bank, files, globals, appdata,
  // commit, verify, resume
  uint32_t ph[9] = { 0 };

  // 1. capture the app-data chunk stream to RAM (fast; ISR-bracketed).
  // app_isr stays OFF clear through step 7's APP_EVENT_RESUME below, so the
  // app's Process() cannot run on top of its own FLUSH/RESUME handler --
  // the same bracket RecallSlot() already puts around its equivalent region.
  // DEFENSIVE ONLY: this was round 2's candidate fix for the Store crash and
  // it did NOT fix it (see the note at the top of this file). Kept for
  // symmetry with RecallSlot, not as a diagnosis.
  CORE::app_isr_enabled = false;
  delay(1);
  BuildAppData(capture);
  ph[0] = wall.lap_ms();

  // 2. ask the active app to flush its file-backed state (Captain, e.g.,
  // does its own load_config+save_config("CAPTAIN.DAT") right here -- which
  // PhzConfig now skips when nothing in it changed)
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_FLUSH);
  watchdog_feed();
  ph[1] = wall.lap_ms();

  // Steps 3-6 PLAN the slot's sections; cw_commit writes them all at once.
  // Section ORDER is constrained: the content flags must be settled before
  // the 'G' section is built, because the manifest that records them lives
  // inside that map.
  ContainerWriter w;
  cw_begin(w);
  uint32_t sec_used = 0;   // bump allocator over sec_buf

  // 3. Quadrants active: extract its live preset + bank globals from the map
  if (app_id == kQuadrantsAppId) {
    uint64_t p = 0;
    if (PhzConfig::getValue(kQuadLivePresetKey, p) && p < 32) {
      extract_preset = (uint8_t)p;
      const size_t n = PhzConfig::serialize(sec_buf, kSecBufBytes,
                                            bank_pred, bank_remap);
      if (n && cw_plan_mem(w, 'B', sec_buf, n)) {
        flags |= CONTENT_BANK;
        sec_used = n;
      } else if (!n) {
        serial_printf("PresetEngine: bank extract exceeds %lu bytes\n",
                      (unsigned long)kSecBufBytes);
        w.ok = false;
      }
    }
    watchdog_feed();
  }
  ph[2] = wall.lap_ms();

  // 4. the file-backed app stores go straight in -- they are already files.
  //
  // These come from myfs EXPLICITLY, not preset_fs(). Scenery and Captain call
  // PhzConfig::load_config/save_config without an FS argument, which defaults
  // to myfs -- so those two files only ever exist on internal flash, whatever
  // preset_fs() happens to be. Sourcing them from preset_fs() meant that with an
  // SD card inserted the open simply failed and both apps' state was silently
  // absent from every preset. (The old !SDcard_Ready fallback below could not
  // catch it: when !SDcard_Ready, preset_fs() ALREADY is myfs, so it re-tried
  // the call that had just failed, and when SD was present -- the only case
  // that needed a fallback -- the condition blocked it.)
  //
  // BANK_255.DAT is the exception, and it goes to quad_fs(): the recalled
  // bank has to land where QUADRANTS will look for it, and Quadrants genuinely
  // prefers SD (Quadrants.h:99-103, 2104-2107). The container itself stays on
  // internal flash like every other part of a slot.
  if (cw_plan_file(w, 'S', PhzConfig::myfs, "SCENERY.DAT")) flags |= CONTENT_SCENERY;
  watchdog_feed();
  if (cw_plan_file(w, 'C', PhzConfig::myfs, "CAPTAIN.DAT")) flags |= CONTENT_CAPTAIN;
  watchdog_feed();
  ph[3] = wall.lap_ms();

  // 5. globals + manifest, serialized in RAM behind the bank extract
  PhzConfig::load_config();  // base map = GLOBALS.CFG
  BuildGlobalSettingsValues();
  PhzConfig::setValue(kSchemaKey, kSchemaVersion);
  PhzConfig::setValue(kFlagsKey, flags);
  {
    uint64_t nm[2];
    pack_name(slot, nm);
    PhzConfig::setValue(kNameKey0, nm[0]);
    PhzConfig::setValue(kNameKey1, nm[1]);
  }
  bool ok = false;
  {
    const size_t n = PhzConfig::serialize(sec_buf + sec_used,
                                          kSecBufBytes - sec_used);
    if (n) ok = cw_plan_mem(w, 'G', sec_buf + sec_used, n);
    else serial_printf("PresetEngine: globals exceed %lu bytes\n",
                       (unsigned long)(kSecBufBytes - sec_used));
  }
  watchdog_feed();
  ph[4] = wall.lap_ms();

  // 6. app-data chunk stream, straight from the RAM capture
  bool ok2 = cw_plan_appdata(w);
  watchdog_feed();
  ph[5] = wall.lap_ms();

  // Publish the container, then retire the pre-container files this replaces.
  // cw_commit verifies the bytes landed and renames into place, so a torn
  // write leaves the previous slot intact rather than half-overwritten.
  const bool committed = ok && ok2 && cw_commit(w, kCtrTmp, final_name);
  ok = committed;
  ok2 = committed;
  ph[6] = wall.lap_ms();

  // Retiring the legacy files DELETES the only other copy of this preset, so
  // the bar for doing it is not "the container is the right size" -- 88 bytes
  // of anything passes that. Re-open and parse it: container_open validates
  // the fourcc, the version, the section count and every section's bounds, at
  // the cost of one directory read. The degraded filesystem that motivated
  // cw_commit's size check can just as easily return a right-sized file whose
  // directory bytes never landed, and that slot would then read as empty with
  // no legacy files left to fall back to.
  if (committed) {
    File v;
    SectionEntry vsec[kMaxSections];
    int vn = 0;
    if (container_open(slot, v, vsec, vn)) {
      v.close();
      remove_legacy_slot(slot);
    } else {
      serial_printf("PresetEngine: slot %d container did not re-open; "
                    "legacy files kept\n", slot);
    }
  }
  watchdog_feed();
  ph[7] = wall.lap_ms();

  // 7. hand the config map back to the active app (Resume reloads its file).
  // Same hazard as step 2 -- Captain's Resume() also touches PhzConfig's
  // cfg_store and setups[] -- so app_isr stays off until this returns too.
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
  CORE::app_isr_enabled = true;
  watchdog_feed();
  ph[8] = wall.lap_ms();

  last_slot = slot;
  last_was_save = true;
  last_save_ok = ok && ok2;
  cur_slot_dirty_ms = StampMs();
  op_count++;
  busy = false;
  HS::PokePopup(HS::MESSAGE_POPUP, (ok && ok2) ? "Bus save OK" : "Bus save ERR");
  serial_printf("PresetEngine: save slot %d %s (flags %02x)\n",
                slot, (ok && ok2) ? "ok" : "FAILED", flags);
  // Both clocks are printed on purpose: their difference is the time the
  // module spent with interrupts off, i.e. audio, USB and the bus stalled.
  serial_printf("PresetEngine: save took %lums wall (millis saw %lums), "
                "bus ISRs %lu, stack unused %lu bytes\n",
                (unsigned long)wall.total_ms(),
                (unsigned long)(millis() - t_start),
                (unsigned long)(PresetBus::GetStats().isr_count - isr_start),
                (unsigned long)stack_low_water());
  serial_printf("PresetEngine:   capture %lu flush %lu bank %lu files %lu "
                "globals %lu appdata %lu commit %lu verify %lu resume %lu\n",
                (unsigned long)ph[0], (unsigned long)ph[1], (unsigned long)ph[2],
                (unsigned long)ph[3], (unsigned long)ph[4], (unsigned long)ph[5],
                (unsigned long)ph[6], (unsigned long)ph[7], (unsigned long)ph[8]);
  return ok && ok2;
}

// ---- recall ----------------------------------------------------------------

enum RecallStage : uint8_t { STAGE_OK = 0, STAGE_EMPTY = 1, STAGE_BAD = 2 };

// The validate-first half of a recall: app-data into `capture`, the slot's
// globals + manifest into the config map, WITHOUT touching live state.
// Reads a container when there is one and the pre-container files otherwise,
// so slots written before the layout change still recall.
FLASHMEM static RecallStage recall_stage_head(uint8_t slot, bool &from_container) {
  File f;
  SectionEntry sec[kMaxSections];
  int n = 0;
  from_container = container_open(slot, f, sec, n);

  if (from_container) {
    const SectionEntry *a = find_section(sec, n, 'A');
    const SectionEntry *g = find_section(sec, n, 'G');
    bool ok = a && g && f.seek(a->offset) && read_appdata_stream(f);
    if (ok) ok = section_to_mem(f, *g, sec_buf, kSecBufBytes);
    // The file-backed sections too, HERE, before anything live is touched.
    // They used to be checked only as section_to_file streamed them out in
    // step 4, with the app world already frozen and the outgoing app's
    // files already being replaced: a bad C section then failed quietly,
    // the previous slot's CAPTAIN.DAT stayed in place, and the recall
    // reported "done" with one section belonging to another preset. A
    // preset is applied whole or refused whole.
    for (int i = 0; ok && i < n; ++i) {
      if (sec[i].kind == 'A' || sec[i].kind == 'G') continue;
      ok = section_sum_ok(f, sec[i]);
    }
    f.close();
    // The container exists, so a failure here is corruption, not emptiness.
    if (!ok) return STAGE_BAD;
    // Straight from RAM: this used to go through a scratch file, which was
    // a block erase with interrupts off on every recall for nothing.
    return PhzConfig::deserialize(sec_buf, g->length) ? STAGE_OK : STAGE_BAD;
  }

  // legacy multi-file layout
  if (!read_appdata_file(slot)) return STAGE_EMPTY;
  char name[12];
  slot_name(name, slot, 'G', "CFG");
  return PhzConfig::load_config(name, preset_fs()) ? STAGE_OK : STAGE_BAD;
}

// Copy one section's bytes into the recall stage instead of a file. Reads
// from the container (memory-mapped flash, cheap), verifies the checksum,
// registers the image under the live file's name. False = not staged (too
// big, or the registry refused): the caller falls back to section_to_file.
FLASHMEM static bool stage_section(File &f, const SectionEntry &e, const char *dest) {
  if (e.length > kStageTmpBytes) return false;
  if (!f.seek(e.offset)) return false;
  uint32_t left = e.length, pos = 0;
  uint16_t sum = 0;
  while (left) {
    const uint32_t want = left < 256 ? left : 256;
    const int r = f.read(stage_tmp + pos, want);
    if (r != (int)want) return false;
    for (int i = 0; i < r; ++i) sum += stage_tmp[pos + i];
    pos += want;
    left -= want;
  }
  if (sum != e.checksum) return false;
  if (!recall_stage.put(dest, stage_tmp, e.length)) return false;
  serial_printf("PresetEngine: staged %s (%lu bytes) in RAM, disk syncs at idle\n",
                dest, (unsigned long)e.length);
  return true;
}

// Does `name` on `fs` already hold exactly these bytes? Reads only.
FLASHMEM static bool file_matches(FS &fs, const char *name, const uint8_t *data, uint32_t len) {
  File d = fs.open(name, FILE_READ);
  if (!d) return false;
  bool same = d.size() == len;
  uint8_t b[128];
  uint32_t pos = 0;
  while (same && pos < len) {
    const uint32_t want = len - pos < sizeof(b) ? len - pos : sizeof(b);
    if (d.read(b, want) != (int)want || memcmp(b, data + pos, want) != 0) same = false;
    pos += want;
  }
  d.close();
  return same;
}

// Write a RAM image to its live file the way section_to_file does: scratch
// name, then rename, so a power loss mid-write cannot leave a torn file.
FLASHMEM static bool bytes_to_file(FS &fs, const char *dest, const uint8_t *data, uint32_t len) {
  if (file_matches(fs, dest, data, len)) return true;
  static const char *const kScratch = "PB_XTR.TMP";
  fs.remove(kScratch);
  File d = fs.open(kScratch, FILE_WRITE_BEGIN);
  if (!d) return false;
  bool ok = true;
  uint32_t pos = 0;
  while (ok && pos < len) {
    const uint32_t want = len - pos < 256 ? len - pos : 256;
    if (d.write(data + pos, want) != (size_t)want) ok = false;
    pos += want;
    watchdog_feed();
  }
  d.close();
  if (!ok) { fs.remove(kScratch); return false; }
  fs.remove(dest);
  ok = fs.rename(kScratch, dest);
  if (!ok) fs.remove(kScratch);
  return ok;
}

// Stage the file-backed stores under their live names. Runs with the app
// world already frozen, so it only moves bytes around, and since the recall
// stage landed it moves them into RAM, not onto flash.
FLASHMEM static void recall_stage_files(uint8_t slot, bool from_container,
                                        uint64_t flags) {
  recall_stage.begin_recall();   // counts anything the last sync never finished
  if (from_container) {
    File f;
    SectionEntry sec[kMaxSections];
    int n = 0;
    if (!container_open(slot, f, sec, n)) return;
    // Destination FS per file, mirroring the save side: the bank goes where
    // Quadrants looks (quad_fs() -- SD when a card is present), Scenery and
    // Captain go where THEY look (myfs, always). Restoring those two to the
    // card put them somewhere neither app ever reads. The stage serves by
    // name regardless of FS; the FS matters again when the sync pump writes.
    const SectionEntry *e;
    if ((flags & CONTENT_BANK) && (e = find_section(sec, n, 'B')) != nullptr) {
      if (stage_section(f, *e, "BANK_255.DAT") ||
          section_to_file(f, *e, "BANK_255.DAT", quad_fs()))
        quad_recall_hint = kScratchBank;
      watchdog_feed();
    }
    if ((flags & CONTENT_SCENERY) && (e = find_section(sec, n, 'S')) != nullptr) {
      if (!stage_section(f, *e, "SCENERY.DAT"))
        section_to_file(f, *e, "SCENERY.DAT", PhzConfig::myfs);
      watchdog_feed();
    }
    // Boot recall deliberately keeps the LIVE Captain config -- see the note
    // on the legacy branch below.
    if ((flags & CONTENT_CAPTAIN) && !boot_recall &&
        (e = find_section(sec, n, 'C')) != nullptr) {
      if (!stage_section(f, *e, "CAPTAIN.DAT"))
        section_to_file(f, *e, "CAPTAIN.DAT", PhzConfig::myfs);
      watchdog_feed();
    }
    f.close();
    return;
  }

  char name[12];
  if (flags & CONTENT_BANK) {
    slot_name(name, slot, 'B', "DAT");
    copy_file(preset_fs(), name, quad_fs(), "BANK_255.DAT");
    quad_recall_hint = kScratchBank;
    watchdog_feed();
  }
  if (flags & CONTENT_SCENERY) {
    slot_name(name, slot, 'S', "DAT");
    copy_file(preset_fs(), name, PhzConfig::myfs, "SCENERY.DAT");
    watchdog_feed();
  }
  // Boot recall deliberately keeps the LIVE Captain config: it's the
  // module's MIDI-interface setup (autosaved continuously), not scene
  // state - restoring the slot's snapshot at power-up silently rewound
  // the owner's mapping edits. Explicit recalls still restore it.
  if ((flags & CONTENT_CAPTAIN) && !boot_recall) {
    slot_name(name, slot, 'C', "DAT");
    copy_file(preset_fs(), name, PhzConfig::myfs, "CAPTAIN.DAT");
    watchdog_feed();
  }
}

// A refused recall is still a FINISHED op: op_count bumps so the overlay's
// completion watch reports the reason at once instead of timing out into a
// generic failure, and the serial log gets a closing line instead of a
// dangling "recall slot N". last_slot is deliberately untouched -- nothing
// was recalled, so what this module has loaded has not changed. bus_slot
// was already moved by RecallSlot: the case is on the refused slot now.
FLASHMEM static bool recall_refused(uint8_t slot, const char *why) {
  last_recall_err = why;
  last_was_save = false;
  op_count++;
  busy = false;
  serial_printf("PresetEngine: recall slot %d refused (%s)\n", slot, why);
  return false;
}

FLASHMEM bool RecallSlot(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  busy = true;
  bus_slot = (int8_t)slot;
  serial_printf("PresetEngine: recall slot %d\n", slot);
  const uint32_t t_start = millis();
  WallClock wall;
  wall.start();
  uint32_t ph[5] = { 0 };   // validate, suspend, files, apply, resume

  // 1. validate everything before touching live state
  bool from_container = false;
  const RecallStage stage = recall_stage_head(slot, from_container);
  ph[0] = wall.lap_ms();
  if (stage == STAGE_EMPTY) {
    HS::PokePopup(HS::MESSAGE_POPUP, "Empty preset");
    // put the map back (staging doesn't touch it on this path, but stay safe)
    return recall_refused(slot, "EMPTY SLOT");
  }
  if (stage == STAGE_BAD) {
    // map may now be cleared/partial: restore base-map ownership, then let
    // the app reload its own file so no later writer inherits slot content
    PhzConfig::load_config();
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset");
    return recall_refused(slot, "BAD PRESET");
  }
  uint64_t schema = 0, flags = 0, meta = 0;
  PhzConfig::getValue(kSchemaKey, schema);
  PhzConfig::getValue(kFlagsKey, flags);
  PhzConfig::getValue((uint16_t)(1 << 8) /* METADATA_KEY */, meta);
  const uint16_t slot_app_id = meta & 0xFFFF;
  if (schema != kSchemaVersion) {
    PhzConfig::load_config();  // drop slot content, re-own the base map
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset ver");
    return recall_refused(slot, "OLD PRESET");
  }

  // 2. suspend the outgoing app, ISR still live, exactly as the app menu
  // and SwitchToApp do. Suspend is where an app drops transient state and
  // persists what it owns, and a recall used to skip it: the 200e app kept
  // a scan running (its quiet flag latched, so the console's 'q' printed
  // nothing afterwards) and kept a write/snapshot confirm ARMED -- recall
  // away from that prompt, recall back, and the first encR press would
  // have shipped a whole-bank write into a 251e. Captain's modal Clock
  // Router and unsaved edits, and Quadrants'/Scenery's auto-save, were
  // skipped the same way. Only after validation, deliberately: an empty
  // slot is the common case in a case where other modules own most slots,
  // and it must not cost the running app a Suspend/Resume round trip (for
  // Quadrants that is an audio-graph rebuild).
  //
  // Tell the handler which of its files step 4 is about to overwrite, so it
  // can skip an auto-save that would be erased before anyone read it (a
  // dirty Captain paid a flash erase, interrupts off, for exactly that).
  // Mirrors what recall_stage_files will do, including the boot-recall
  // Captain exception: only the stores the slot carries, per its manifest.
  recall_replacing = (uint8_t)(flags & (CONTENT_BANK | CONTENT_SCENERY | CONTENT_CAPTAIN));
  if (boot_recall) recall_replacing &= (uint8_t)~CONTENT_CAPTAIN;
  recall_replacing |= RECALL_SUSPEND;
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_SUSPEND);
  recall_replacing = 0;
  // Those handlers persist through the shared PhzConfig map (bank file,
  // SCENERY.DAT, CAPTAIN.DAT), so the slot's G section staged in step 1 may
  // be gone by now. Stage it again: RAM plus one container open, ~1 ms.
  if (recall_stage_head(slot, from_container) != STAGE_OK) {
    PhzConfig::load_config();
    app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
    HS::PokePopup(HS::MESSAGE_POPUP, "Bad preset");
    return recall_refused(slot, "BAD PRESET");
  }
  ph[1] = wall.lap_ms();

  // 3. freeze the app world
  CORE::app_isr_enabled = false;
  CORE::app_loop_enabled = false;
  delay(1);

  // 4. stage the file-backed stores (same multi-write LittleFS chain as
  // SaveSlot's step 4 -- see the watchdog note at the top of this file)
  recall_stage_files(slot, from_container, flags);
  ph[2] = wall.lap_ms();

  // 5. apply global settings (map still holds PB_NN_G.CFG) + app chunks
  RestoreGlobalSettingsFromConfig(0);
  Scales::Validate();
  Chords::Validate();
  for (int i = 0; i < HS::TURING_MACHINE_COUNT; ++i)
    HS::user_turing_machines[i].Validate();
  ApplyAppData(capture);
  watchdog_feed();
  ph[3] = wall.lap_ms();

  // 6. switch to the slot's app (missing app: stay put, partial recall).
  // The boot recall keeps the app the module was in instead, for the same
  // reason it keeps the live Captain config: which app is on screen is the
  // module's own state, written down whenever it changes (the app menu, and
  // the runtime recall below). A power cycle used to come back in the
  // preset's app after the user had deliberately switched away from it --
  // in the 200e app, back up in Captain. global_settings.current_app_id
  // still holds the boot app here; nothing in step 5 touches it.
  const size_t idx = ResolveAppIndexByID(boot_recall ? global_settings.current_app_id
                                                     : slot_app_id);

  FreqMeasure.end();
  DigitalInputs::reInit();

  app_switcher.set_current_app(idx);
  // The app on screen is remembered through the current record, stamped
  // dirty with the slot below -- one debounced EEPROM write for both. NOT
  // through GLOBALS.CFG: that made a cross-app recall 286 ms, 277 of them a
  // LittleFS block erase with interrupts off, against 10 ms for a same-app
  // recall. The RESUME that follows hands PhzConfig's map back.

  AudioNoInterrupts();
  app_switcher.current_app()->DispatchAppEvent(APP_EVENT_RESUME);
  AudioInterrupts();
  watchdog_feed();

  // 7. run
  CORE::app_isr_enabled = true;
  CORE::app_loop_enabled = true;
  ::MENU_REDRAW = 1;
  ph[4] = wall.lap_ms();

  last_slot = slot;
  last_was_save = false;
  last_recall_err = nullptr;
  cur_slot_dirty_ms = StampMs();
  op_count++;
  busy = false;
  last_recall_ms = StampMs();   // stage_sync_pump() waits 3 s from here
  HS::PokePopup(HS::MESSAGE_POPUP, "Bus recall OK");
  serial_printf("PresetEngine: recall slot %d done (app %04x)\n", slot, slot_app_id);
  serial_printf("PresetEngine: recall took %lums wall (millis saw %lums): "
                "validate %lu suspend %lu files %lu apply %lu resume %lu\n",
                (unsigned long)wall.total_ms(),
                (unsigned long)(millis() - t_start),
                (unsigned long)ph[0], (unsigned long)ph[1],
                (unsigned long)ph[2], (unsigned long)ph[3],
                (unsigned long)ph[4]);
  return true;
}

// ---- service ---------------------------------------------------------------

// Deferred sync of staged recall images: one file per pass, 3 s after the
// recall, never while a request or a bus transfer is in flight. Until Track
// C's persistence window lands this write still masks interrupts; it just
// no longer sits inside the recall gesture, and `T` counts it as a
// background stall. A file that already holds the bytes is not rewritten.
static constexpr uint32_t kStageSyncDelayMs = 3000;
FLASHMEM static void stage_sync_pump() {
  if (!recall_stage.pending()) return;
  if (busy || !req_q.empty()) return;
  if (PresetBus::MasterTransferring()) return;
  if (!last_recall_ms || millis() - last_recall_ms < kStageSyncDelayMs) return;
  const PresetStage::Image *img = recall_stage.next_unsynced();
  if (!img) return;
  char name[PresetStage::kNameMax + 1];
  strncpy(name, img->name, PresetStage::kNameMax);
  name[PresetStage::kNameMax] = 0;
  FS &fs = strcmp(name, "BANK_255.DAT") == 0 ? quad_fs() : PhzConfig::myfs;
  WallClock wall;
  wall.start();
  const bool ok = bytes_to_file(fs, name, img->data, img->len);
  recall_stage.mark_synced(name);
  serial_printf("PresetEngine: synced %s to disk %s in %lu ms wall\n", name,
                ok ? "ok" : "FAILED", (unsigned long)wall.lap_ms());
}

FLASHMEM void Init() {
  req_q.clear();
  quad_recall_hint = -1;
  load_names();

  // Say what is on the card, if anything.
  //
  // Presets used to follow SDcard_Ready, so any saved while a card was seated
  // are sitting on that card and are no longer where the engine looks. They
  // are not lost -- ImportSlot() pulls one back into the same numbered slot --
  // but nothing else would ever mention them, and a preset you cannot see is
  // indistinguishable from a preset you do not have. This line is how someone
  // finds out they have something to import.
  const int on_card = CardSlotCount();
  if (on_card > 0)
    serial_printf("PresetEngine: %d preset%s on the card, import to load "
                  "%s into internal storage\n",
                  on_card, on_card == 1 ? "" : "s",
                  on_card == 1 ? "it" : "them");
}

static bool enqueue(uint8_t op, uint8_t slot) {
  if (slot >= kNumSlots) return false;
  if (req_q.push(op, slot)) return true;
  serial_printf("PresetEngine: queue full, %s %d refused\n",
                op == REQ_SAVE ? "save" : "recall", slot);
  return false;
}
bool RequestSave(uint8_t slot) { return enqueue(REQ_SAVE, slot); }
bool RequestRecall(uint8_t slot) { return enqueue(REQ_RECALL, slot); }
uint32_t RequestsDropped() { return req_q.dropped; }

// persist the current record ~3s after activity settles, so trigger-driven
// preset cycling never write-hammers it. Touches neither PhzConfig's map nor
// the app: nothing here to hand back or RESUME.
FLASHMEM static void persist_cur_record() {
  cur_slot_dirty_ms = 0;
  const uint16_t app = global_settings.current_app_id;
  if (persisted_slot == last_slot && persisted_app == app) return;
  cur_rec_store(last_slot, app);
  int8_t s = -1;
  uint16_t a = 0;
  const bool ok = cur_rec_load(&s, &a) && s == last_slot && a == app;
  persisted_slot = ok ? last_slot : -1;
  persisted_app = ok ? app : 0;
  serial_printf("PresetEngine: slot %d app %04x persisted%s\n", last_slot, app,
                ok ? "" : " FAILED");
}

void NoteAppOnScreen() { cur_slot_dirty_ms = StampMs(); }

FLASHMEM bool BootAppChoice(uint16_t *app_id) {
  int8_t slot = -1;
  uint16_t app = 0;
  if (!cur_rec_load(&slot, &app) || !app) return false;
  *app_id = app;
  return true;
}

FLASHMEM void ForgetCurrent() {
  cur_rec_store(-1, 0);
  persisted_slot = -1;
  persisted_app = 0;
  cur_slot_dirty_ms = 0;
}

FLASHMEM void BootRecall() {
  int8_t slot = -1;
  uint16_t app = 0;
  if (!cur_rec_load(&slot, &app)) {
    // No record yet: either a fresh module or firmware that kept the slot in
    // GLOBALS.CFG, which is still the loaded map right after boot restore.
    // Migrate it into the record so this branch runs once.
    uint64_t v = 0;
    PhzConfig::load_config();
    if (!PhzConfig::getValue(kCurSlotKey, v) || v >= kNumSlots) return;
    slot = (int8_t)v;
    cur_rec_store(slot, 0);
    serial_printf("PresetEngine: slot %d migrated from GLOBALS.CFG\n", slot);
  }
  // Seed the mirror from the one read boot does. An app that failed to
  // resolve (removed from the build) is on screen as the default now, and
  // the mirror disagreeing with it is what gets the record corrected.
  persisted_slot = slot;
  persisted_app = app;
  if (app && app != global_settings.current_app_id) cur_slot_dirty_ms = StampMs();
  if (slot < 0 || !SlotUsed((uint8_t)slot)) return;
  serial_printf("PresetEngine: boot recall slot %d\n", slot);
  // Local only -- no bus broadcast at power-up. Queued as its own op so the
  // keep-live-Captain rule travels with THIS request rather than sitting in
  // a static that whatever recall ran first would consume.
  enqueue(REQ_BOOT_RECALL, slot);
}

FLASHMEM void Process() {
  // The queue needs no interrupt lock, and it is worth saying why rather
  // than adding one defensively.
  //
  // Every producer of the queue is in LOOP context, not an ISR. The bus slave
  // ISR (PresetBus.cpp lpi2c1_slave_isr) only pushes bytes into its own SPSC
  // ring; the parser that turns those bytes into a SAVE or RECALL runs from
  // PresetBus::Task(), and its cb_save/cb_recall callbacks call RequestSave/
  // RequestRecall from there. The console commands and BootRecall() are loop
  // context too. So this function and every producer are the same thread and
  // cannot interleave.
  //
  // An earlier version of this comment claimed the slave ISR was a producer
  // and bracketed the dequeue in noInterrupts(). The lock was harmless but the
  // reason was invented, and a wrong comment about concurrency in this file
  // is worse than no comment -- the round-3 audit block above is the thing
  // people trust when they are debugging a preset-bus fault at 2am.
  //
  // ONE request per pass, in arrival order. Draining the whole queue here
  // would keep loop() -- and with it the UI, USB and the bus parser that
  // feeds this queue -- starved for as long as the bus kept sending.
  //
  // ...but not while the 200e app has a whole-bank transfer on the wire. The
  // target module is moving 63,120 bytes into or out of our card slave, and a
  // recall (or save) here blocks loop() for 70-150 ms and masks every
  // interrupt for each flash erase inside that. The slave hardware
  // clock-stretches through it (RXSTALL/TXDSTALL), but a 251e that gives up
  // waiting on a RESTORE leaves the module holding half a bank -- exactly the
  // failure the app's verify pass then has to catch and offer an undo for --
  // and on the verify BACKUP itself it reads as a false BAD over a write
  // that landed. The request is not dropped: it stays at the head of the
  // queue (a newer recall still replaces it) and runs the moment the
  // transfer ends. Between the RESTORE and the verify BACKUP the app chains
  // after it there is no pass where the master looks idle: the FSM steps to
  // DONE in PresetBus::Task(), which runs after this function, and the app's
  // loop starts the read-back before this function runs again. Capped
  // (kDeferCapMs, sized above) so a user starting job after job cannot hold
  // a manager's RECALL hostage; the FSM's own timeouts close every single
  // job well inside that.
  if (!req_q.empty() && PresetBus::MasterTransferring()) {
    if (!deferred_since) {
      deferred_since = StampMs();
      serial_printf("PresetEngine: request deferred, bank transfer on the wire\n");
    }
    if (millis() - deferred_since < kDeferCapMs) return;
    serial_printf("PresetEngine: transfer still on the wire after %lu s; "
                  "running the request anyway\n",
                  (unsigned long)(kDeferCapMs / 1000));
  }
  if (!req_q.empty()) {
    if (deferred_since) {
      serial_printf("PresetEngine: deferred request runs, waited %lu ms\n",
                    (unsigned long)(millis() - deferred_since));
      deferred_since = 0;
    }
    const auto r = req_q.front();
    req_q.pop();
    switch (r.op) {
      case REQ_SAVE:
        SaveSlot(r.slot);
        break;
      case REQ_RECALL:
        RecallSlot(r.slot);
        boot_recall = false;   // belt and braces: only REQ_BOOT_RECALL sets it
        break;
      case REQ_BOOT_RECALL:
        boot_recall = true;
        RecallSlot(r.slot);
        boot_recall = false;
        break;
    }
  }
  // The EEPROM write behind this is small, but on a T4.1 EEPROM is emulated
  // in flash and now and then a byte costs a sector erase with interrupts
  // masked. Arriving in the 200e app and tapping Read inside 3 s puts that
  // erase under the 251e's first bytes; the same hold as the request above,
  // and the stamp keeps, so the record lands once the job closes.
  if (cur_slot_dirty_ms && millis() - cur_slot_dirty_ms > 3000 &&
      !PresetBus::MasterTransferring())
    persist_cur_record();
  stage_sync_pump();
  // Names an import left in the cache. The console's batch flushes itself;
  // this is the net under any caller that does not.
  FlushSlotNames();
}

uint8_t RecallReplacing() { return recall_replacing; }

FLASHMEM int ConsumeQuadrantsRecallHint() {
  const int h = quad_recall_hint;
  quad_recall_hint = -1;
  return h;
}

uint32_t StampMs() { const uint32_t t = millis(); return t ? t : 1; }

int8_t LastSlot() { return last_slot; }
int8_t BusSlot() { return bus_slot; }
uint32_t OpCount() { return op_count; }
bool LastSaveOk() { return last_save_ok; }
const char *LastRecallError() { return last_recall_err; }

// ---- slot names --------------------------------------------------------
// One flat 30x16 byte file, whole thing cached in RAM. Deliberately not a
// PhzConfig file: reads/writes must never disturb the shared config map.
static char name_cache[kNumSlots][kNameLen + 1];  // +1: always NUL-safe

FLASHMEM static void load_names() {
  memset(name_cache, 0, sizeof(name_cache));
  File f = preset_fs().open("PBNAMES.BIN", FILE_READ);
  if (!f) return;
  for (int i = 0; i < kNumSlots; ++i) {
    if (f.read((uint8_t *)name_cache[i], kNameLen) != kNameLen) break;
    name_cache[i][kNameLen] = 0;
  }
  f.close();
}

const char *SlotName(uint8_t slot) {
  return (slot < kNumSlots) ? name_cache[slot] : "";
}

// The 16 name bytes as two config values (manifest kNameKey0/1), and back.
// Little-endian byte order is fixed by hand rather than by memcpy so the
// container reads the same on any host that parses it.
static void pack_name(uint8_t slot, uint64_t out[2]) {
  out[0] = out[1] = 0;
  for (size_t i = 0; i < kNameLen; ++i)
    out[i / 8] |= (uint64_t)(uint8_t)name_cache[slot][i] << (8 * (i % 8));
}

static void unpack_name(const uint64_t in[2], char *out /* kNameLen + 1 */) {
  for (size_t i = 0; i < kNameLen; ++i)
    out[i] = (char)(uint8_t)(in[i / 8] >> (8 * (i % 8)));
  out[kNameLen] = 0;
}

// Whole store to flash in one write. A LittleFS write is a block erase with
// interrupts off, so callers that touch many names (a 30-slot import) set
// the cache and flush ONCE; the panel's single rename flushes immediately.
static bool names_dirty = false;

FLASHMEM static void names_flush() {
  File f = preset_fs().open("PBNAMES.BIN", FILE_WRITE_BEGIN);
  if (!f) return;
  for (int i = 0; i < kNumSlots; ++i)
    f.write((const uint8_t *)name_cache[i], kNameLen);
  f.close();
  names_dirty = false;
}

// Cache only; the caller decides when the store is written.
static void name_set_cached(uint8_t slot, const char *name) {
  memset(name_cache[slot], 0, sizeof(name_cache[slot]));
  strncpy(name_cache[slot], name, kNameLen);
  // trim trailing spaces so "unnamed" stays honest
  for (int i = (int)strlen(name_cache[slot]) - 1;
       i >= 0 && name_cache[slot][i] == ' '; --i)
    name_cache[slot][i] = 0;
  names_dirty = true;
}

FLASHMEM void SetSlotName(uint8_t slot, const char *name) {
  if (slot >= kNumSlots) return;
  name_set_cached(slot, name);
  names_flush();
}

// An imported container's name, if it carries one, into the cache. Reads the
// G section into the staging buffer and peeks the manifest there -- never
// through the config map, which belongs to the running app at import time.
FLASHMEM static void name_from_container(uint8_t slot) {
  File f;
  SectionEntry sec[kMaxSections];
  int n = 0;
  if (!container_open(slot, f, sec, n)) return;
  const SectionEntry *g = find_section(sec, n, 'G');
  const bool have = g && section_to_mem(f, *g, sec_buf, kSecBufBytes);
  f.close();
  if (!have) return;
  uint64_t nm[2];
  if (!PhzConfig::peek(sec_buf, g->length, kNameKey0, nm[0]) ||
      !PhzConfig::peek(sec_buf, g->length, kNameKey1, nm[1]))
    return;   // pre-name container: keep whatever the local store says
  char name[kNameLen + 1];
  unpack_name(nm, name);
  if (strcmp(name, name_cache[slot]) != 0) name_set_cached(slot, name);
}

FLASHMEM void FlushSlotNames() {
  if (names_dirty) names_flush();
}

FLASHMEM bool SlotUsed(uint8_t slot) {
  if (slot >= kNumSlots) return false;
  char name[12];
  container_name(name, slot);
  {
    File c = preset_fs().open(name, FILE_READ);
    const bool have = c && c.size() >= (int)kPayloadStart;
    if (c) c.close();
    if (have) return true;
  }
  // pre-container layout
  slot_name(name, slot, 'G', "CFG");
  File f = preset_fs().open(name, FILE_READ);
  const bool used = f && f.size() > 16;
  if (f) f.close();
  return used;
}
bool LastWasSave() { return last_was_save; }
bool Busy() { return busy; }

// ---- pre-write bank snapshot -----------------------------------------------
//
// The last good copy of a 200e module's bank, taken before we overwrite it.
//
// Why this exists: the bus has no per-slot write, so editing one preset means
// re-sending all thirty. The app is careful about that -- it fingerprints the
// bank at Read, re-checks at Save, proves the patch byte-for-byte, and reads
// the whole bank back to verify. What it could not do was RECOVER. CommitWrite
// keeps only hashes of the other 29 slots, and the verify read-back overwrites
// the card image with the module's now-corrupt contents, so the last correct
// copy was destroyed by the verification step itself. The app could detect
// collateral damage, could never locate it (a CRC cannot name a slot), and
// could never repair it: the screen said "BAD: OTHER PRESETS!" and one press
// later said "No changes to write".
//
// A 251e bank is 63,120 bytes: 16 of XenoFS's 4 KB blocks (it was 96.3% of
// one of the core's 64 KB blocks, exactly one of the 64 the partition then
// had, which only became affordable when slots stopped costing 3-5 files
// each -- the same discovery paying off twice).
//
// Deliberately ONE snapshot, not a history: it is a safety net for the write
// happening right now, not a backup system. Naming it after the address it
// came from is what stops it being restored onto the wrong module.

static constexpr uint32_t kSnapFourcc = 0x50414E53UL;  // 'S','N','A','P'
struct SnapHeader {
  uint32_t fourcc;
  uint8_t  addr;        // the module this came from
  uint8_t  pad[3];
  uint32_t length;      // payload bytes
  uint32_t crc;         // over the payload
};
static_assert(sizeof(SnapHeader) == 16, "SnapHeader must stay 16 bytes");
static const char *const kSnapFile = "PBSNAP.BIN";

FLASHMEM bool SnapshotBank(uint8_t addr, const uint8_t *bank, uint32_t len,
                           uint32_t crc) {
  if (!bank || !len) return false;
  static const char *const kSnapTmp = "PB_SNP.TMP";
  preset_fs().remove(kSnapTmp);
  File f = preset_fs().open(kSnapTmp, FILE_WRITE_BEGIN);
  if (!f) return false;
  const SnapHeader h = { kSnapFourcc, addr, {0, 0, 0}, len, crc };
  bool ok = f.write((const uint8_t *)&h, sizeof(h)) == sizeof(h);
  uint32_t left = len;
  const uint8_t *p = bank;
  while (ok && left) {
    const uint32_t n = left < 512 ? left : 512;
    ok = f.write(p, n) == n;
    p += n; left -= n;
    watchdog_feed();
  }
  f.close();
  if (!ok) { preset_fs().remove(kSnapTmp); return false; }
  // Rename-first, as everywhere else here: a torn snapshot must not replace
  // a good one, and a failed publish must not leave us with neither.
  if (!preset_fs().rename(kSnapTmp, kSnapFile)) {
    preset_fs().remove(kSnapFile);
    if (!preset_fs().rename(kSnapTmp, kSnapFile)) return false;
  }
  serial_printf("PresetEngine: snapshot %lu bytes from %02X\n",
                (unsigned long)len, addr);
  return true;
}

FLASHMEM bool SnapshotInfo(uint8_t *addr_out, uint32_t *len_out) {
  File f = preset_fs().open(kSnapFile, FILE_READ);
  if (!f) return false;
  SnapHeader h;
  const bool ok = f.read((uint8_t *)&h, sizeof(h)) == (int)sizeof(h) &&
                  h.fourcc == kSnapFourcc && h.length &&
                  (uint64_t)h.length + sizeof(h) <= (uint64_t)f.size();
  f.close();
  if (!ok) return false;
  if (addr_out) *addr_out = h.addr;
  if (len_out) *len_out = h.length;
  return true;
}

FLASHMEM bool LoadSnapshot(uint8_t addr, uint8_t *dest, uint32_t cap,
                           uint32_t *len_out) {
  if (!dest) return false;
  File f = preset_fs().open(kSnapFile, FILE_READ);
  if (!f) return false;
  SnapHeader h;
  bool ok = f.read((uint8_t *)&h, sizeof(h)) == (int)sizeof(h) &&
            h.fourcc == kSnapFourcc && h.length && h.length <= cap &&
            (uint64_t)h.length + sizeof(h) <= (uint64_t)f.size();
  // Refuse to hand a bank back to a module it did not come from. Restoring
  // 63,120 bytes of a 251e's presets into whatever now answers at a different
  // address is the single worst thing this file could do.
  if (ok && h.addr != addr) ok = false;
  if (ok) ok = (uint32_t)f.read(dest, h.length) == h.length;
  f.close();
  if (!ok) return false;

  // CHECK THE CRC. It was written and never read, which made this the one
  // write path in the instrument whose payload nothing verified -- and it is
  // the path that exists specifically to run after something has already gone
  // wrong once. A bit-rotted or torn PBSNAP.BIN would have gone on the wire,
  // and the verify read-back would then compare the module against the same
  // corrupt image and stamp it WROTE + VERIFIED. The header comment naming
  // the field "crc over the payload" read as verification, which is exactly
  // how it survived review.
  if (Buchla200eCrc32(dest, h.length) != h.crc) {
    serial_printf("PresetEngine: snapshot CRC mismatch, refusing\n");
    return false;
  }
  if (len_out) *len_out = h.length;
  return true;
}

FLASHMEM void DiscardSnapshot() { preset_fs().remove(kSnapFile); }

// ---- export / import -------------------------------------------------------
//
// The SD card's actual job, now that it is not the preset store. A container
// is self-contained -- header, section directory, payloads, checksums -- so
// moving one between filesystems is a byte copy and nothing else. That is the
// whole reason the one-file-per-slot layout was worth building: the old 3-5
// files per slot had no single thing to hand somebody.
//
// Names are the same on both sides (PB_NN.PBS), so a card carrying slot 07
// drops into slot 07 on another module. Both directions verify by re-opening
// and parsing the destination: a copy that lands corrupt must not be reported
// as success, and on import it must never retire a good local container.

// The container to the card with the slot's CURRENT name in its manifest.
// SaveSlot packs the name at save time, and a rename since then only touched
// the name store -- rewriting a 10 KB container for sixteen bytes of name
// would cost a block erase on every rename. So the copy is where the two
// meet: the G section is read into the staging buffer, the name records
// poked (chunk checksum kept right by PhzConfig::poke), the section sum
// recomputed, and the bytes streamed out with that section and the patched
// directory substituted. A container from before the name keys has nothing
// to poke and is copied as it is. The card copy is verified afterwards, so
// a mistake here reads as EXPORT_FAILED, never as a bad file on the card.
FLASHMEM static bool export_container(uint8_t slot, FS &card, const char *name) {
  File f;
  SectionEntry sec[kMaxSections];
  int n = 0;
  if (!container_open(slot, f, sec, n)) return false;
  SectionEntry *g = nullptr;
  for (int i = 0; i < n; ++i)
    if (sec[i].kind == 'G') g = &sec[i];
  bool patch = g && section_to_mem(f, *g, sec_buf, kSecBufBytes);
  if (patch) {
    uint64_t nm[2];
    pack_name(slot, nm);
    patch = PhzConfig::poke(sec_buf, g->length, kNameKey0, nm[0]) &&
            PhzConfig::poke(sec_buf, g->length, kNameKey1, nm[1]);
    if (patch) g->checksum = sum16(sec_buf, g->length);
  }
  // A poke that found the first key and not the second has half-patched the
  // buffer -- SaveSlot writes both or neither, so that is a damaged
  // manifest. Re-read the section rather than export the mixture.
  if (!patch && g && !section_to_mem(f, *g, sec_buf, kSecBufBytes)) {
    f.close();
    return false;
  }

  ContainerHeader h;
  if (!f.seek(0) || f.read((uint8_t *)&h, sizeof(h)) != sizeof(h)) {
    f.close();
    return false;
  }
  card.remove(name);
  File dst = card.open(name, FILE_WRITE_BEGIN);
  if (!dst) { f.close(); return false; }
  bool ok = cw_put(dst, (const uint8_t *)&h, sizeof(h));
  for (int i = 0; i < kMaxSections && ok; ++i) {
    const SectionEntry e = (i < n) ? sec[i] : SectionEntry{ 0, 0, 0, 0, 0 };
    ok = cw_put(dst, (const uint8_t *)&e, sizeof(e));
  }
  // Payload, in file order, with the G byte range taken from the buffer.
  const uint32_t size = (uint32_t)f.size();
  uint32_t pos = kPayloadStart;
  ok = ok && f.seek(pos);
  uint8_t buf[512];
  while (ok && pos < size) {
    const uint32_t want = (size - pos) < sizeof(buf) ? (size - pos) : sizeof(buf);
    ok = f.read(buf, want) == (int)want;
    if (ok && g) {
      const uint32_t lo = pos > g->offset ? pos : g->offset;
      const uint32_t hi = (pos + want) < (g->offset + g->length)
                              ? (pos + want) : (g->offset + g->length);
      if (lo < hi) memcpy(buf + (lo - pos), sec_buf + (lo - g->offset), hi - lo);
    }
    ok = ok && dst.write(buf, want) == want;
    pos += want;
    watchdog_feed();
  }
  f.close();
  dst.close();
  if (!ok) card.remove(name);
  return ok;
}

FLASHMEM ExportResult ExportSlot(uint8_t slot) {
  if (slot >= kNumSlots) return EXPORT_BAD_SLOT;
  FS *card = card_fs();
  if (!card) return EXPORT_NO_CARD;
  if (!SlotUsed(slot)) return EXPORT_EMPTY;

  char name[12];
  container_name(name, slot);

  // A container is the portable unit; the pre-container layout is not.
  // SlotUsed() answers for BOTH -- it has to, so old slots still recall --
  // so a legacy slot arrives here looking exportable and then fails on a
  // source file that was never going to exist. Found on hardware: a module
  // holding seven pre-container presets reported "7 failed", which names no
  // cause and reads like the card is broken. Say what is actually true, and
  // what to do about it: re-saving the slot converts it.
  {
    File c; SectionEntry sec[kMaxSections]; int n = 0;
    if (!container_open(slot, c, sec, n)) return EXPORT_LEGACY;
    c.close();
  }
  // A local container whose checksums no longer hold would copy fine and
  // fail the card-side verify below, reported as FAILED -- which sends
  // someone looking at the card for a slot that is damaged on the module
  // (and that a recall would refuse). Say which side is broken.
  if (!container_verify(preset_fs(), name)) return EXPORT_BAD_FILE;

  if (!export_container(slot, *card, name)) return EXPORT_FAILED;

  // Prove it landed, whole. A card that reports a successful write and
  // stores nothing is the exact failure this whole engine is built to
  // notice -- and a size check alone passed a right-sized file of garbage.
  if (!container_verify(*card, name)) { card->remove(name); return EXPORT_FAILED; }

  serial_printf("PresetEngine: exported slot %d to card\n", slot);
  return EXPORT_OK;
}

FLASHMEM ExportResult ImportSlot(uint8_t slot) {
  if (slot >= kNumSlots) return EXPORT_BAD_SLOT;
  FS *card = card_fs();
  if (!card) return EXPORT_NO_CARD;

  char name[12];
  container_name(name, slot);
  {
    File s = card->open(name, FILE_READ);
    const bool have = s && s.size() >= (int)kPayloadStart;
    if (s) s.close();
    if (!have) return EXPORT_EMPTY;
  }

  // Stage through a scratch name and only publish after the copy verifies
  // as a container a recall would accept: directory, every checksum, the A
  // and G sections present. Writing straight to PB_NN.PBS would destroy a
  // good local preset on the strength of a card we have not read yet -- and
  // so would checking only the 16-byte header, which is all this did: a
  // card file truncated or bit-rotted past that point kept its fourcc,
  // replaced the local slot, and the next recall said BAD PRESET with the
  // good copy already gone.
  static const char *const kImpTmp = "PB_IMP.TMP";
  preset_fs().remove(kImpTmp);
  if (!copy_file(*card, name, preset_fs(), kImpTmp)) {
    preset_fs().remove(kImpTmp);
    return EXPORT_FAILED;
  }
  if (!container_verify(preset_fs(), kImpTmp)) {
    preset_fs().remove(kImpTmp);
    return EXPORT_BAD_FILE;
  }

  // Rename-first, same discipline as cw_commit: on LittleFS the slot is
  // never absent, and if the fallback also fails the scratch file is left
  // standing rather than deleted alongside the original.
  if (!preset_fs().rename(kImpTmp, name)) {
    preset_fs().remove(name);
    if (!preset_fs().rename(kImpTmp, name)) return EXPORT_FAILED;
  }
  remove_legacy_slot(slot);   // an imported container supersedes them
  // The name rides in the container's manifest; cache it here and let the
  // caller's batch (or Process) write the store once, not per slot.
  name_from_container(slot);
  serial_printf("PresetEngine: imported slot %d from card\n", slot);
  return EXPORT_OK;
}

FLASHMEM int CardSlotCount() {
  FS *card = card_fs();
  if (!card) return -1;
  int n = 0;
  char name[12];
  for (int i = 0; i < kNumSlots; ++i) {
    container_name(name, (uint8_t)i);
    File f = card->open(name, FILE_READ);
    if (f && f.size() >= (int)kPayloadStart) ++n;
    if (f) f.close();
  }
  return n;
}

// ---- legacy recovery -------------------------------------------------------
//
// Presets a firmware from before the container format saved to SD -- back
// when preset_fs() sometimes WAS the card -- are five files per slot,
// PB_NN_G.CFG/_A.BIN/_B.DAT/_S.DAT/_C.DAT, sitting on the card with nothing
// to read them: ImportSlot only understands PB_NN.PBS, and the engine's own
// legacy reader (recall_stage_head, above) only ever looks at preset_fs().
// This builds the container recall_stage_head would have needed, straight
// from the card's copies, and lands it on internal flash as if ImportSlot
// had succeeded.
//
// G is deliberately NOT read through PhzConfig::load_config()/getValue(),
// the way recall_stage_head's legacy branch reads it. That function owns the
// one shared live config map every app's Suspend/Resume also reads and
// writes; calling it here, outside a suspended-app recall, would silently
// swap out whatever the running app currently has loaded -- for a background
// "recover everything on the card" sweep that runs from an ordinary menu
// press, not a recall. Instead G travels exactly like Scenery or Captain's
// files already do at save time (cw_plan_file, below): an opaque, checksummed
// byte range. The header comment on the slot container says a 'G' section
// "is exactly what save_config() would have written" -- a legacy PB_NN_G.CFG
// *is* one of those, byte for byte, so no repacking is needed. Correctness of
// its contents (schema version, the manifest) is exactly as unverified here
// as it is for a card import via ImportSlot, which also only checks the
// container's shape, not the G payload's meaning -- the real check happens
// once, in RecallSlot, whatever the container's origin.
//
// A DOES go through the existing legacy validator (read_appdata_file, now
// FS&-parameterized): it is cheap, it is already exactly the right check
// (fourcc/version/checksum on the AppData stream), and reusing it here is
// what pulls the payload into `capture` so cw_plan_appdata can build the
// section the same way SaveSlot does.
//
// SAFETY: never overwrites a slot that already holds anything recallable.
// SlotUsed() is the same bar the rest of this file uses for "is there a
// preset here" (container OR legacy-on-internal-flash); recovery only ever
// fills a slot SlotUsed() calls empty. This does mean a slot holding a
// CORRUPT local container is left alone rather than repaired from a possibly
// -good card copy -- deliberately: which of two damaged-looking things wins
// is a decision for a human with ExportSlot/ImportSlot, not a sweep that runs
// off one menu press. Nothing on the card is ever deleted or modified: a
// recovered slot's source files stay right where they were, as a second copy
// -- unlike remove_legacy_slot(), which retires INTERNAL flash legacy files
// once a save/import supersedes them (that is a block-reclaiming decision;
// the card was never block-starved the same way, and destroying someone's
// only remaining backup on a recovery pass would be exactly the loss this
// whole feature exists to prevent).
FLASHMEM RecoverResult RecoverLegacyFromCard(uint8_t slot) {
  if (slot >= kNumSlots) return RECOVER_BAD_SLOT;
  FS *card = card_fs();
  if (!card) return RECOVER_NO_CARD;
  if (SlotUsed(slot)) return RECOVER_OCCUPIED;

  char gname[12], aname[12], bname[12], sname[12], cname[12];
  slot_name(gname, slot, 'G', "CFG");
  slot_name(aname, slot, 'A', "BIN");
  slot_name(bname, slot, 'B', "DAT");
  slot_name(sname, slot, 'S', "DAT");
  slot_name(cname, slot, 'C', "DAT");

  // Presence first, cheap and read-only: G and A are the floor a slot cannot
  // recall without (recall_stage_head refuses STAGE_EMPTY on a missing A;
  // load_config on a missing G). Nothing legacy-shaped here at all is not a
  // failure, it is "the card has nothing for this slot" -- RECOVER_EMPTY,
  // the same answer ImportSlot gives an absent PB_NN.PBS.
  {
    File g = card->open(gname, FILE_READ);
    const bool g_have = (bool)g;
    if (g) g.close();
    File a = card->open(aname, FILE_READ);
    const bool a_have = (bool)a;
    if (a) a.close();
    if (!g_have || !a_have) return RECOVER_EMPTY;
  }

  // A validates through the real AppData-stream check; a failure here means
  // something IS on the card under this name but is not a legacy set worth
  // trusting -- report it, don't silently package garbage into a container
  // that would only fail later, at recall, with no card left to explain why.
  if (!read_appdata_file(slot, *card)) return RECOVER_BAD_FILE;

  ContainerWriter w;
  cw_begin(w);
  const bool g_ok = cw_plan_file(w, 'G', *card, gname);
  const bool a_ok = cw_plan_appdata(w);   // from `capture`, populated above
  if (!g_ok || !a_ok || !w.ok) return RECOVER_BAD_FILE;
  // B/S/C are optional per-content, exactly as the flags bitmask/
  // recall_stage_files already treat them -- cw_plan_file no-ops on an
  // absent source without failing the whole plan. A PRESENT-but-short one
  // does fail it: a preset is built whole or refused whole, same rule as
  // every other container path in this file.
  cw_plan_file(w, 'B', *card, bname);
  cw_plan_file(w, 'S', *card, sname);
  cw_plan_file(w, 'C', *card, cname);
  if (!w.ok) return RECOVER_BAD_FILE;

  static const char *const kRecTmp = "PB_REC.TMP";
  char final_name[12];
  container_name(final_name, slot);
  if (!cw_commit(w, kRecTmp, final_name)) return RECOVER_FAILED;

  // Same bar SaveSlot holds its own fresh container to before trusting it
  // enough to retire anything: re-open and parse the directory. There is
  // nothing to retire here (the source lives on the card, untouched), but an
  // unreadable PB_NN.PBS is worse than no container at all -- SlotUsed()
  // would then call the slot occupied and no future import/recovery would
  // ever touch it again.
  {
    File v;
    SectionEntry vsec[kMaxSections];
    int vn = 0;
    if (!container_open(slot, v, vsec, vn)) {
      preset_fs().remove(final_name);
      return RECOVER_FAILED;
    }
    v.close();
  }
  name_from_container(slot);   // pick up a name if this era's G carries one
  serial_printf("PresetEngine: recovered legacy slot %d from card\n", slot);
  return RECOVER_OK;
}

FLASHMEM int RecoverAllLegacyFromCard() {
  if (!card_fs()) return -1;
  int n = 0;
  for (uint8_t i = 0; i < kNumSlots; ++i) {
    if (RecoverLegacyFromCard(i) == RECOVER_OK) ++n;
    watchdog_feed();
  }
  // One flush for the whole sweep, not one per recovered slot -- the same
  // batching reasoning as ImportSlot's own comment on name caching (a 30-slot
  // sweep that flushed PBNAMES.BIN on every hit would pay up to 30 block
  // erases for something one write settles).
  FlushSlotNames();
  return n;
}

}  // namespace PresetEngine
}  // namespace OC

PresetStage &OC::RecallStage() { return OC::PresetEngine::recall_stage; }

#endif  // ARDUINO_TEENSY41
