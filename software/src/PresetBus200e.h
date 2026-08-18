#ifndef PRESETBUS200E_H_
#define PRESETBUS200E_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Buchla 200e preset-bus command engine (bus slave role).
//
// Ported from the MARF project's bus200e.{c,h} (same author/system); protocol
// ground truth is github.com/studiohsoftware/2WIRELESS (Studio H), whose
// source implements both framings against real 225e/206e hardware.
//
// BSP-free and host-testable: this layer never touches hardware. The transport
// (the LPI2C1 slave ISR on target, the test suite on host) feeds it a stream
// of BUS200E_EV_* events; actions run through the Bus200eOps callbacks
// supplied at init. Passing NULL ops gives the RX-log-only behaviour required
// for the first enabled build: every decoded command lands in the debug ring
// and nothing else happens.
//
// Commands arrive as I2C writes to the general-call address in one of two
// framings, both parsed here:
//   PRIMO (current 225e firmware) — the LONG frame:
//     [nBytes] [destAddr] [srcAddr=0x22] [cmd] [args...]
//     where nBytes counts the bytes after itself; destAddr 0x00 = broadcast.
//   V2 / pre-PRIMO — the SHORT frame: first byte is the command.
// (NOTE: the MARF/288r design notes label these two backwards; the 2WIRELESS
// source is authoritative. The disambiguation below is framing-order safe.)
// ---------------------------------------------------------------------------

// Event encoding fed from the transport. Low byte = data byte when no flag is
// set; flags mark transaction boundaries and transport trouble.
#define BUS200E_EV_START 0x0100u  // general-call transaction opened
#define BUS200E_EV_STOP  0x0200u  // transaction closed: parse the frame
#define BUS200E_EV_OVF   0x0400u  // transport lost events: poison this frame

// Our identity in command payloads (NOT the I2C wire address -- commands ride
// the general call; module addresses are payload bytes).
// UNCONFIRMED default: chosen to dodge every address in the 2WIRELESS preset
// dumps (0x10, 0x20, 0x21, 0x28, 0x29, 0x32, 0x37, 0x39, 0x44, 0x48, 0x5C)
// but not validated against a real system's enumeration (a 225e shows a
// module's address on remote-enable hold). Runtime-adjustable; persisted by
// the preset engine.
#define BUS200E_DEFAULT_MODULE_ADDR 0x3C

// The bus preset space is 0-29 (wire values; the 225e UI shows 1-30).
#define BUS200E_BUS_PRESETS 30

// Remote-enable state at boot. The 200e etiquette (does a 225e expect modules
// to come up enabled?) is unverified on real hardware.
#ifndef BUS200E_REMOTE_DEFAULT
#define BUS200E_REMOTE_DEFAULT 1
#endif

// Storage cards slave at 0x50|cardLo and speak a 24xx-EEPROM-style protocol.
#define BUS200E_CARD_BASE 0x50

// Decoded operations, for the debug ring and dispatch.
typedef enum {
  BUS200E_OP_NONE = 0,
  BUS200E_OP_RECALL,      // arg = preset number
  BUS200E_OP_SAVE,        // arg = preset number
  BUS200E_OP_REMOTE_EN,
  BUS200E_OP_REMOTE_DIS,
  BUS200E_OP_POLL_DONE,   // "polling complete" (module-init trigger; no action)
  BUS200E_OP_QUERY,       // mod_addr = queried module; ours sets query_pending
  BUS200E_OP_BACKUP,      // mod_addr/card_lo/mem_off populated (phase 2)
  BUS200E_OP_RESTORE,     // mod_addr/card_lo/mem_off populated (phase 2)
  BUS200E_OP_MIDI,        // arg = status byte (log only)
  BUS200E_OP_CLOCK,       // arg = 0xF8/0xFA/0xFB/0xFC (log only)
  BUS200E_OP_UNKNOWN,     // arg = first frame byte
  BUS200E_OP_DROPPED,     // frame poisoned/truncated/preempted; arg = length
} Bus200eOp;

typedef struct {
  uint8_t  op;        // Bus200eOp
  uint8_t  arg;       // preset number / status byte / first byte, per op
  uint8_t  mod_addr;  // target module address (BACKUP/RESTORE/QUERY/long dest)
  uint8_t  card_lo;   // card address low byte (BACKUP/RESTORE)
  uint16_t mem_off;   // card memory offset (BACKUP/RESTORE)
} Bus200eCmd;

// Action callbacks. Any pointer (or the whole struct) may be NULL: the
// corresponding action is skipped while logging continues.
// Card transfers (phase 2) exchange opaque per-slot blobs of record_size
// bytes; both hooks must be set (with card hooks) for a job to run.
typedef struct {
  // Live-state save/recall, the bus equivalent of the front-panel gesture.
  void (*save_preset)(uint8_t slot);    // slot 0-29
  void (*recall_preset)(uint8_t slot);  // slot 0-29
  // Phase-2 card transfer hooks (all four + record_size required to activate).
  uint32_t record_size;
  int (*slot_read)(uint8_t slot, uint8_t *out, uint32_t cap);      // 0 = ok
  // slot_write validates the incoming record: 0 = accepted, >0 = rejected
  // (skip this record, keep going -- counted in stats), <0 = abort transfer.
  int (*slot_write)(uint8_t slot, const uint8_t *in, uint32_t n);
  int (*card_write)(uint8_t card7, uint32_t off, const uint8_t *d, uint32_t n);
  int (*card_read)(uint8_t card7, uint32_t off, uint8_t *d, uint32_t n);
} Bus200eOps;

typedef struct {
  uint32_t frames;           // general-call frames parsed
  uint32_t dropped;          // frames discarded (poisoned/preempted/overlong)
  uint32_t job_errors;       // card transfers aborted on an ops error
  uint32_t restore_rejects;  // restore records rejected by slot_write
} Bus200eStats;

// Reset all state (parser, remote-enable, job, log, stats) and store `ops`.
// NULL ops = RX-log-only.
void Bus200eInit(const Bus200eOps *ops);

// Feed one transport event. Call from the main loop, never from an ISR.
void Bus200eFeedEvent(uint16_t ev);

// Run pending card backup/restore work: one slot record per call.
void Bus200eTask(void);

int Bus200eRemoteEnabled(void);
int Bus200eJobActive(void);
const Bus200eStats *Bus200eGetStats(void);

// Module payload address (runtime-adjustable; see BUS200E_DEFAULT_MODULE_ADDR).
void    Bus200eSetModuleAddress(uint8_t addr);
uint8_t Bus200eModuleAddress(void);

// Set when a QUERY (long cmd 0x1A) addressed to our module address arrives.
// The transport answers by mastering the reply frame, then clears it.
int  Bus200eQueryPending(void);
void Bus200eClearQueryPending(void);

// Debug ring of decoded commands (the RX-log build's whole output).
#define BUS200E_LOG_SIZE 32
uint32_t Bus200eLogTotal(void);
int Bus200eLogRead(uint32_t n_back, Bus200eCmd *out);   // 1 = ok, 0 = gone

#endif  // PRESETBUS200E_H_
