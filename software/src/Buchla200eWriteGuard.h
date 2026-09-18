#ifndef BUCHLA200EWRITEGUARD_H_
#define BUCHLA200EWRITEGUARD_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Whether it is safe to push the resident card image back to a 200e module.
//
// WHY THIS IS ITS OWN FILE, AND PURE: MasterRestore transfers the ENTIRE bank
// -- all 30 slots -- not the one slot being edited. The card image is the unit
// of transfer. So a write built on a stale, short, or wrong-module image
// destroys 29 presets the user never touched. The module itself offers no
// undo; the app now takes a pre-write snapshot to internal flash so a BAD
// verdict has a way back, but that net is taken AFTER this decision and is
// no reason to relax it. That makes the permit/refuse decision the
// highest-consequence logic in the app, so it
// lives here as a pure function over a plain struct, with no bus, no UI and no
// hardware, and is exercised by test_buchla200e_write_guard.cpp.
//
// The short-read case is not theoretical: one real MasterBackup during
// bring-up returned 957 bytes instead of 990 -- exactly one record short --
// while reporting DONE with error=NONE. Harmless on a read. Unrecoverable on
// a write.
// ---------------------------------------------------------------------------

enum Buchla200eWriteBlock : uint8_t {
  BUCHLA200E_WRITE_OK = 0,
  BUCHLA200E_WRITE_NO_READ,       // nothing read this session
  BUCHLA200E_WRITE_WRONG_MODULE,  // image belongs to a different module
  BUCHLA200E_WRITE_SHORT_READ,    // transfer < a full bank
  BUCHLA200E_WRITE_NO_IMAGE,      // card image gone (CardServing dropped)
  BUCHLA200E_WRITE_BUSY,          // a scan/probe/read/write is in flight
  BUCHLA200E_WRITE_NO_CHANGES,    // diff is empty; nothing to send
  BUCHLA200E_WRITE_IMAGE_CHANGED, // image no longer the bank we read
  BUCHLA200E_WRITE_PATCH_RANGE,   // a patch offset falls outside the slot
  BUCHLA200E_WRITE_BUILD_FAILED,  // constructed bank != what was intended
};

// Everything the decision depends on, gathered by the caller. Deliberately
// plain data: the point is that this can be constructed in a test.
struct Buchla200eWriteContext {
  bool     have_read;            // a read COMPLETED this session
  uint8_t  read_addr;            // module address that read came from
  uint8_t  read_type;            // module type that read was decoded as
  uint8_t  target_addr;          // module we are about to write
  uint8_t  target_type;
  uint32_t bytes_transferred;    // Bus200eMasterBytesTransferred()
  uint32_t expected_bank_bytes;  // slots * record size for this module type
  bool     card_serving;         // CardServing()
  bool     image_valid;          // MasterCardImage() != NULL
  bool     master_idle;          // no scan/probe/read/write in flight
  int      changed_bytes;        // Buchla251eDiffSlot() result; <0 = overflow
  // The card image is SHARED MUTABLE STATE: the console 'w' command patches
  // arbitrary bytes into it, Bus200eBridge writes browser SysEx chunks into
  // it, and any other MasterBackup overwrites it wholesale. bytes_transferred
  // is a counter, not a fingerprint -- it cannot notice any of that. So the
  // caller re-hashes the image at commit time and reports whether it is still
  // byte-for-byte the bank that was read.
  bool     image_matches_read;
  // Every patch offset lies inside the slot being edited. A diff bug that
  // widened the list is exactly the failure that would rewrite a neighbour.
  bool     patches_in_range;
};

// The order matters: report the most fundamental problem first, so the user
// is told "you have not read this module" rather than "nothing changed".
Buchla200eWriteBlock Buchla200eCheckWrite(const Buchla200eWriteContext &ctx);

// Short enough for a 128px line.
const char *Buchla200eWriteBlockText(Buchla200eWriteBlock b);

// --- bank fingerprinting ---------------------------------------------------
// CRC-32 (reflected, poly 0xEDB88320) over a byte range. Chosen over a sum
// because a sum cannot see a transposition, and a swapped pair of records is
// precisely the shape a mis-seated transfer takes.
uint32_t Buchla200eCrc32(const uint8_t *data, uint32_t len);

// Two hashes of one bank in a single pass: the whole thing, and everything
// EXCEPT a hole (the slot about to be rewritten).
//
// The pair is what makes a whole-bank write provable without keeping a second
// 63,120-byte copy, which this build has nowhere to put:
//   * `whole` taken at read time, re-taken at commit, says the shared image is
//     still the bank we read.
//   * `outside` taken either side of applying the patch says the other 29
//     slots did not move -- and the edited slot is then checked byte-for-byte
//     against a freshly encoded copy, so both halves of the bank are proven
//     by different means.
// hole_len == 0 means no hole, and `outside` then equals `whole`.
struct Buchla200eBankHash {
  uint32_t whole;
  uint32_t outside;
};
Buchla200eBankHash Buchla200eHashBank(const uint8_t *bank, uint32_t bank_len,
                                      uint32_t hole_off, uint32_t hole_len);

#endif  // BUCHLA200EWRITEGUARD_H_
