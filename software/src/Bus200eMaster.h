#ifndef BUS200EMASTER_H_
#define BUS200EMASTER_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Transient bus-master orchestration for card-based BACKUP/RESTORE against a
// FOREIGN 200e module (e.g. capturing a 251e's preset dump for a browser
// applet). Nothing in PresetBus200e.cpp issues these commands -- it only
// ever reacts to them. This module is the piece that does: pick a card
// address nothing else on the bus owns, master the BACKUP/RESTORE command
// naming that address as the target module's destination, and watch the
// resulting card traffic (served by PresetBusCard, elsewhere) to know when
// the transfer is done.
//
// BSP-free and host-testable, same split as PresetBus200e/PresetBusCard: no
// hardware access here. The Bus200eMasterOps callbacks below are the entire
// transport surface; PresetBus.cpp supplies the real Wire/BusCardStats-backed
// implementation (see MasterBackup/MasterRestore there) and is the only
// caller compiled against real hardware. Every FSM decision -- frame
// building, retry/timeout bookkeeping, activity-based done-detection -- is
// exercised on host in test_bus200e_master.cpp against a fake ops table.
//
// The bus's electrical safety (level shifting) has since been verified on
// real hardware, and this project's existing tx_gate_open()/suppress-echo
// master pattern (PresetBus.cpp: pump_broadcast, pump_midi_tx, the QUERY
// reply) is proven-obeyed by the physical case -- notably the sibling
// Orin_Fun rig's ember.py masters 0xFA/0xFC realtime bytes at the 251e's
// sequencer B transport through this exact QueueMidiTx/pump_midi_tx path,
// "proven obeyed" per its own commit history (2026-08-28). That is real
// corroboration for the gating/echo-suppression *mechanism* this module
// reuses.
//
// UPDATE (2026-08-31): BACKUP/RESTORE itself is now bench-CONFIRMED, not
// just host-tested -- repeated live MasterBackup/MasterRestore cycles
// against a real 251e (mod_addr 0x5C), including a full 198-byte-patch,
// 4-sequence composition written and independently read back byte-exact.
// The timing constants below turned out generous enough in practice (no
// activity/hard-cap timeouts observed across that session), but they were
// never bench-TRACED to see how close a real 251e actually runs to them --
// treat them as "proven sufficient", not "proven tight". Retune only with
// real trace data, not by guessing:
//   - the timing constants below (how fast a real 225e/251e starts touching
//     the card after a BACKUP/RESTORE command, and how long its own
//     internal per-preset write/read gaps run).
//   - the address model: PresetBus.cpp's card-serving hardware (SAMR ADDR1)
//     is now parameterized -- slave_reconfig() takes the 7-bit address to
//     claim -- and MasterBackup()/MasterRestore() there offer
//     Bus200eMasterFindFreeCard() the candidate list {0x00, 0x01} (0x50
//     then 0x51), so a capture can still succeed at 0x51 when a live WPM
//     holds 0x50. Ordinary manual card serving (CardServeEnable(), the
//     console 'e' toggle) is unchanged and still only ever claims 0x50 --
//     it emulates the canonical card address other modules expect, not a
//     transient capture. Every candidate past 0x50 has no cached presence
//     flag of its own (unlike wpm_present for 0x50), so it is probed fresh
//     via Bus200eMasterOps::probe_card before being claimed.
// ---------------------------------------------------------------------------

#include "PresetBus200e.h"  // BUS200E_OP_BACKUP / BUS200E_OP_RESTORE

// FSM states, exposed for status reporting (UI, USB bridge, tests).
//
// Fixed underlying type on purpose. Without one an enum's valid range stops
// at the smallest bit-field holding its largest enumerator -- 0..7 here -- so
// merely LOADING a value past that is undefined, and the tests that prove no
// state can strand the caller do exactly that by design. uint8_t makes every
// value they can pass a legal one. (Caught by UBSan, 2026-09-14.)
typedef enum : uint8_t {
  BUS200E_MASTER_IDLE = 0,
  BUS200E_MASTER_FINDING_CARD,   // probing candidate card addresses
  BUS200E_MASTER_SENDING,        // waiting for a quiet bus to master the cmd
  BUS200E_MASTER_WAIT_ACTIVITY,  // command sent; waiting for the target to
                                  // start touching the card
  BUS200E_MASTER_TRANSFERRING,   // card activity seen; waiting for it to
                                  // go quiet (transfer done)
  BUS200E_MASTER_DONE,
  BUS200E_MASTER_FAILED,
} Bus200eMasterState;

// Bus200eMasterLastError() values while state == BUS200E_MASTER_FAILED.
typedef enum {
  BUS200E_MASTER_ERR_NONE = 0,
  BUS200E_MASTER_ERR_BUSY,          // a job was already active
  BUS200E_MASTER_ERR_BAD_ARGS,      // ops incomplete, or a bad request
  BUS200E_MASTER_ERR_NO_FREE_CARD,  // every candidate card address is claimed
  BUS200E_MASTER_ERR_SEND_TIMEOUT,  // bus never went quiet / send kept failing
  BUS200E_MASTER_ERR_NO_RESPONSE,   // command sent, target never touched the card
} Bus200eMasterError;

typedef struct {
  // Millisecond clock, shared with the rest of the transport (PresetBus.cpp
  // feeds Bus200eSetNow() from the same source on target).
  uint32_t (*now_ms)(void);

  // True if the bus is quiet enough to safely master a frame right now
  // (mirrors PresetBus.cpp's tx_gate_open(): no RX in flight, no card
  // transfer window open, bus not wedged).
  int (*tx_gate_open)(void);

  // Probe one 7-bit card address (BUS200E_CARD_BASE | card_lo): 1 = already
  // ACKed by something else on the bus (claimed), 0 = free. May be a cached
  // presence flag (e.g. WpmPresent()) rather than a fresh bus probe -- the
  // caller decides the risk/staleness tradeoff; this module only consumes
  // the answer.
  int (*probe_card)(uint8_t card_addr7);

  // Master `n` bytes onto the general call (Wire.beginTransmission(0) +
  // write + endTransmission(), on target). Returns 0 on a clean ACK'd send,
  // nonzero on any I2C error (NAK, arbitration loss, ...).
  int (*send_frame)(const uint8_t *bytes, uint8_t n);

  // Register a just-sent frame for self-echo suppression (Bus200eSuppressFrame
  // on target) so our own slave doesn't re-parse what we just mastered.
  void (*suppress_echo)(const uint8_t *bytes, uint8_t n);

  // Monotonically increasing count of bytes the local card slave has moved
  // in the direction relevant to the active job (bytes_written for a
  // BACKUP -- the target is writing into our card; bytes_read for a
  // RESTORE -- the target is reading out of it). Only the delta matters;
  // this module never inspects the absolute value.
  uint32_t (*card_activity)(void);
} Bus200eMasterOps;

// Reset all state and store `ops`. All fields of `ops` must be non-NULL
// (NULL = untested/unsupported here, unlike PresetBus200e's log-only mode --
// there's no meaningful "master with no transport" behaviour).
void Bus200eMasterInit(const Bus200eMasterOps *ops);

// Probe candidates[0..n) via ops->probe_card(BUS200E_CARD_BASE | candidates[i])
// in order; the first one that comes back free (0) is written to *out_card_lo
// and this returns 1. Returns 0 if every candidate is claimed (or n == 0).
// Pure lookup -- does not touch FSM state, safe to call any time.
int Bus200eMasterFindFreeCard(const uint8_t *candidates, uint8_t n,
                               uint8_t *out_card_lo);

// Start a BACKUP (target module -> our card at card_addr) or RESTORE (our
// card -> target module) job. card_lo must already be a card address this
// module is (about to be) serving -- callers choose it via
// Bus200eMasterFindFreeCard() beforehand, or a known-good constant while the
// hardware only offers one candidate. Returns 0 if accepted (job now
// running), <0 (a Bus200eMasterError, negated) if refused outright: busy,
// or ops incomplete.
int Bus200eMasterBackup(uint8_t mod_addr, uint8_t card_lo);
int Bus200eMasterRestore(uint8_t mod_addr, uint8_t card_lo);

// Pump the FSM. Call every loop tick (or every host-test tick) once a job is
// running; a no-op while IDLE/DONE/FAILED.
void Bus200eMasterTask(void);

Bus200eMasterState Bus200eMasterGetState(void);
Bus200eMasterError Bus200eMasterLastError(void);
uint8_t Bus200eMasterCardAddr(void);   // card_lo of the active/last job
uint8_t Bus200eMasterModAddr(void);    // target module of the active/last job
int Bus200eMasterIsRestore(void);      // 1 = RESTORE, 0 = BACKUP, of the active/last job

// Bytes moved by ops->card_activity() during the active/last job -- the
// delta between the value sampled right after the command was sent and the
// most recent value observed (frozen once the job leaves TRANSFERRING, so
// it still reads correctly after DONE/FAILED, right up until the NEXT job
// reaches WAIT_ACTIVITY and rebaselines). 0 before any job has started, and
// 0 for a job that failed before WAIT_ACTIVITY (SEND_TIMEOUT/BAD_ARGS) --
// start_job() clears both samples so a stale previous-job delta can never
// leak through. This is deliberately NOT BusCardStats::bytes_written/
// bytes_read -- those are lifetime cumulative counters across every
// transaction ever served, not "how much this job moved" (see
// PresetBusCard.h). Callers wanting "how big was the capture I just did"
// (e.g. PresetBus.cpp's DumpCard()) want this, not BusCardGetStats().
uint32_t Bus200eMasterBytesTransferred(void);

// The target module announced the end of its transfer (BUS200E_OP_XFER_DONE,
// cmd 0x0A -- see parse_frame() in PresetBus200e.cpp for the one bench
// observation this rests on). Fed by the ops->xfer_done adapter in
// PresetBus.cpp. Only an announcement from THIS job's module, while the job
// is in WAIT_ACTIVITY or TRANSFERRING, counts; anything else is ignored.
// It never ends the job by itself: activity still has to go quiet, only for
// BUS200E_MASTER_QUIET_ACKED_MS instead of BUS200E_MASTER_QUIET_DONE_MS.
// A module is the I2C master of both its card writes and this frame, so the
// frame cannot overtake its own last write -- but a module that announces
// early, or one whose announcement means something else, still gets the
// short quiet check rather than an instant DONE.
void Bus200eMasterXferDone(uint8_t from_addr);
// 1 once the current/last job's module has announced (cleared by start_job).
int Bus200eMasterAcked(void);

// Acknowledge a DONE/FAILED result and return to IDLE. Starting a new job
// (Backup/Restore) also implicitly does this once the FSM allows it (only
// from DONE/FAILED/IDLE -- see Bus200eMasterError::BUSY).
void Bus200eMasterReset(void);

// Timing constants (see the UNVERIFIED note above). Exposed so tests and
// callers can reason about worst-case job duration without duplicating them.
#define BUS200E_MASTER_SEND_TIMEOUT_MS     2000  // give up trying to grab the bus
#define BUS200E_MASTER_ACTIVITY_TIMEOUT_MS 3000  // target must start touching
                                                  // the card within this long
#define BUS200E_MASTER_QUIET_DONE_MS       1500  // this much silence after
                                                  // activity started = done
                                                  // (matches PresetBus.cpp's
                                                  // own card-transfer holdoff)
#define BUS200E_MASTER_QUIET_ACKED_MS       200  // ...or this much once the
                                                  // module has announced it is
                                                  // done (Bus200eMasterXferDone)
#define BUS200E_MASTER_HARD_CAP_MS        15000  // absolute safety net

// ---------------------------------------------------------------------------
// QUERY: ask ONE module who it is, capture the version string it answers with.
//
// A deliberate SIBLING of the BACKUP/RESTORE FSM above rather than another
// state inside it: a QUERY is a single request and a single reply frame, with
// no card to claim, no card address to probe, no multi-kilobyte transfer to
// watch go quiet. Folding it into Bus200eMasterState would mean four of that
// FSM's states and three of its timing constants never applying. It shares
// what it genuinely shares -- the same Bus200eMasterOps table installed by
// Bus200eMasterInit(), so there is nothing extra to wire up -- and uses only
// four of the six ops (now_ms, tx_gate_open, send_frame, suppress_echo);
// probe_card and card_activity are meaningless here.
//
// The two FSMs hold independent state and neither blocks the other; the
// shared tx_gate_open() is what keeps their bus access from colliding, the
// same way pump_broadcast/pump_midi_tx/try_query_reply already coexist in
// PresetBus.cpp.
//
// THE REPLY ARRIVES BY A DIFFERENT ROUTE THAN THE REQUEST LEFT BY. We master
// the request; the answer comes back as the queried module's OWN general-call
// broadcast, which our slave ISR hears like any other bus frame. So the reply
// path is: ISR -> event ring -> Bus200eFeedEvent -> parse_frame's QUERY REPLY
// case -> Bus200eOps::query_reply -> (PresetBus.cpp adapter) ->
// Bus200eMasterQueryReply() below. There is no polling of the target and no
// second bus transaction.
//
// BENCH-CONFIRMED 2026-08-31 against a live 200e bus (251e at 0x5C, a second
// module at 0x28, a WPM also on the bus): the request frame was always right
// -- both modules answered it -- but the reply we listened for was not. Real
// modules answer with command byte 0x1C and a single 0xFF payload byte
// ([04 22 5C 1C FF]), where this file previously expected 0x13 and a version
// string, a shape invented here that no module speaks. That is why every
// query before the fix timed out with stray=0: the answer was on the wire
// and reached parse_frame(), which classified it as an UNKNOWN short/V2
// frame instead of a reply. See PresetBus200e.h for the captured bytes.
//
// Consequence for callers: the "version string" a reply carries is, on real
// hardware, one opaque byte. A DONE query means "that address is occupied
// and answering"; it does not identify the module.
typedef enum : uint8_t {   // fixed underlying type, see Bus200eMasterState
  BUS200E_QUERY_IDLE = 0,
  BUS200E_QUERY_SENDING,   // waiting for a quiet bus to master the request
  BUS200E_QUERY_WAITING,   // request sent; waiting for the module's reply
  BUS200E_QUERY_DONE,      // reply captured (Bus200eMasterQueryVersion())
  BUS200E_QUERY_FAILED,    // see Bus200eMasterQueryLastError()
} Bus200eQueryState;

// Reuses Bus200eMasterError: BUSY (a query is already in flight), BAD_ARGS
// (ops incomplete, or mod_addr 0 -- broadcast, see Bus200eBuildQueryFrame),
// SEND_TIMEOUT (never got a quiet bus), NO_RESPONSE (no reply in time).
#define BUS200E_MASTER_QUERY_SEND_TIMEOUT_MS  2000  // matches the BACKUP send
                                                    // gate: same bus, same
                                                    // contention story
#define BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS 1000  // bus turnaround only --
                                                    // no card work follows,
                                                    // so nowhere near
                                                    // ACTIVITY_TIMEOUT's 3s

// Start a QUERY against mod_addr. Returns 0 if accepted, <0 (a negated
// Bus200eMasterError) if refused: busy, ops incomplete, or mod_addr 0.
int Bus200eMasterQuery(uint8_t mod_addr);

// Pump the QUERY FSM. Call every loop tick alongside Bus200eMasterTask();
// a no-op while IDLE/DONE/FAILED.
void Bus200eMasterQueryTask(void);

// Feed a QUERY reply seen on the bus (the Bus200eOps::query_reply hook).
// Ignored unless a query is in flight; a reply from an address we did not
// ask counts as a stray (Bus200eMasterQueryStrayReplies) and is NOT taken as
// the answer -- reporting some other module's identity as the queried one's
// would be worse than timing out.
void Bus200eMasterQueryReply(uint8_t from_addr, const uint8_t *ver, uint8_t n);

Bus200eQueryState  Bus200eMasterQueryGetState(void);
Bus200eMasterError Bus200eMasterQueryLastError(void);
uint8_t  Bus200eMasterQueryModAddr(void);       // target of the active/last query
uint32_t Bus200eMasterQueryStrayReplies(void);  // replies from other addresses

// Copy up to `cap` captured version bytes into `out` (NOT NUL-terminated --
// the bytes are whatever the module sent, spaces and all). Returns how many
// were written; 0 when no reply has been captured.
uint8_t Bus200eMasterQueryVersion(uint8_t *out, uint8_t cap);

// Acknowledge a DONE/FAILED query and return to IDLE. Starting a new query
// also implicitly does this (only from DONE/FAILED/IDLE -- see BUSY).
void Bus200eMasterQueryReset(void);

#endif  // BUS200EMASTER_H_
