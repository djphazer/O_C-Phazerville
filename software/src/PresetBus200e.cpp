// Buchla 200e preset-bus engine: frame parser (both command framings),
// remote-enable state, save/recall dispatch and chunked card transfer jobs.
// Pure logic -- no hardware includes; see PresetBus200e.h for the contract.
//
// Ported from the MARF project's bus200e.c; protocol ground truth is
// github.com/studiohsoftware/2WIRELESS. Framing labels corrected relative to
// the MARF source comments (PRIMO = long frame, V2/pre-PRIMO = short frame).

#include <string.h>

// On target, keep this cold code out of ITCM; host builds compile it bare.
#if defined(__IMXRT1062__) || defined(__MK20DX256__)
#include <Arduino.h>
#define BUS_CODE FLASHMEM
#else
#define BUS_CODE
#endif

#include "PresetBus200e.h"

// Longest meaningful general-call frame is the 8-byte long-form card command;
// anything longer than this is not a command we know and is dropped.
#define FRAME_MAX 12

static const Bus200eOps *bus_ops;

// ---- parser state ----------------------------------------------------------
static uint8_t  frame[FRAME_MAX];
static uint8_t  frame_len;
static uint8_t  in_frame;
static uint8_t  frame_poisoned;   // OVF or overlong: drop at STOP

static uint8_t  remote_enabled;
static uint8_t  module_addr = BUS200E_DEFAULT_MODULE_ADDR;
static uint8_t  query_pending;

// ---- card transfer job -----------------------------------------------------
static struct {
  uint8_t  active;
  uint8_t  is_restore;
  uint8_t  card_lo;
  uint8_t  next_slot;
  uint16_t mem_off;
} job;

// ---- debug ring + stats ----------------------------------------------------
static Bus200eCmd  log_ring[BUS200E_LOG_SIZE];
static uint32_t    log_total;
static Bus200eStats stats;

static void log_cmd(const Bus200eCmd *c) {
  log_ring[log_total % BUS200E_LOG_SIZE] = *c;
  log_total++;
}

BUS_CODE uint32_t Bus200eLogTotal(void) { return log_total; }

BUS_CODE int Bus200eLogRead(uint32_t n_back, Bus200eCmd *out) {
  if (n_back >= log_total || n_back >= BUS200E_LOG_SIZE) return 0;
  *out = log_ring[(log_total - 1 - n_back) % BUS200E_LOG_SIZE];
  return 1;
}

int Bus200eRemoteEnabled(void) { return remote_enabled; }
int Bus200eJobActive(void) { return job.active; }
const Bus200eStats *Bus200eGetStats(void) { return &stats; }

void    Bus200eSetModuleAddress(uint8_t addr) { module_addr = addr & 0x7F; }
uint8_t Bus200eModuleAddress(void) { return module_addr; }

int  Bus200eQueryPending(void) { return query_pending; }
void Bus200eClearQueryPending(void) { query_pending = 0; }

BUS_CODE void Bus200eInit(const Bus200eOps *ops) {
  bus_ops = ops;
  frame_len = 0;
  in_frame = 0;
  frame_poisoned = 0;
  remote_enabled = BUS200E_REMOTE_DEFAULT;
  query_pending = 0;
  memset(&job, 0, sizeof(job));
  memset(&stats, 0, sizeof(stats));
  memset(log_ring, 0, sizeof(log_ring));
  log_total = 0;
}

// ---- dispatch --------------------------------------------------------------

BUS_CODE static void dispatch(Bus200eCmd *c) {
  log_cmd(c);

  switch (c->op) {
    case BUS200E_OP_REMOTE_EN:  remote_enabled = 1; break;
    case BUS200E_OP_REMOTE_DIS: remote_enabled = 0; break;

    case BUS200E_OP_RECALL:
      if (remote_enabled && c->arg < BUS200E_BUS_PRESETS &&
          bus_ops && bus_ops->recall_preset)
        bus_ops->recall_preset(c->arg);
      break;

    case BUS200E_OP_SAVE:
      if (remote_enabled && c->arg < BUS200E_BUS_PRESETS &&
          bus_ops && bus_ops->save_preset)
        bus_ops->save_preset(c->arg);
      break;

    case BUS200E_OP_QUERY:
      if (c->mod_addr == module_addr) query_pending = 1;
      break;

    case BUS200E_OP_BACKUP:
    case BUS200E_OP_RESTORE:
      if (c->mod_addr != module_addr) break;
      if (job.active) {   // one transfer at a time; a second request is dropped
        Bus200eCmd d = { BUS200E_OP_DROPPED, c->op, 0, 0, 0 };
        log_cmd(&d);
        stats.dropped++;
        break;
      }
      job.active = 1;
      job.is_restore = (c->op == BUS200E_OP_RESTORE);
      job.card_lo = c->card_lo;
      job.mem_off = c->mem_off;
      job.next_slot = 0;
      break;

    default:  // POLL_DONE / MIDI / CLOCK / UNKNOWN: log only
      break;
  }
}

// ---- frame parsing ---------------------------------------------------------

BUS_CODE static void parse_frame(void) {
  Bus200eCmd c = { BUS200E_OP_NONE, 0, 0, 0, 0 };
  const uint8_t *f = frame;
  uint8_t n = frame_len;

  stats.frames++;

  // LONG / PRIMO framing: [nBytes, destAddr, srcAddr=0x22, cmd, args...],
  // where nBytes counts the bytes that follow it. No short command collides
  // with this shape.
  if (n >= 4 && f[2] == 0x22 && f[0] == n - 1) {
    c.mod_addr = f[1];
    switch (f[3]) {
      case 0x01: c.op = BUS200E_OP_RECALL; c.arg = (n > 4) ? f[4] : 0; break;
      case 0x02: c.op = BUS200E_OP_SAVE;   c.arg = (n > 4) ? f[4] : 0; break;
      case 0x14: c.op = BUS200E_OP_POLL_DONE;  break;
      case 0x16: c.op = BUS200E_OP_REMOTE_EN;  break;
      case 0x17: c.op = BUS200E_OP_REMOTE_DIS; break;
      case 0x1A: c.op = BUS200E_OP_QUERY;      break;
      case 0x04:  // dump presets to card: [.., modAddr, cardLo, memLSB, memMSB]
      case 0x05:  // restore presets from card, same argument order
        if (n >= 8) {
          c.op = (f[3] == 0x04) ? BUS200E_OP_BACKUP : BUS200E_OP_RESTORE;
          c.mod_addr = f[4];
          c.card_lo = f[5];
          c.mem_off = (uint16_t) (f[6] | (f[7] << 8));
        } else {
          c.op = BUS200E_OP_UNKNOWN; c.arg = f[3];
        }
        break;
      default:
        c.op = BUS200E_OP_UNKNOWN; c.arg = f[3];
        break;
    }
    dispatch(&c);
    return;
  }

  // SHORT / V2 (pre-PRIMO) framing: first byte is the command.
  // NOTE the card-op argument order differs from the long framing.
  switch (f[0]) {
    case 0x00: c.op = BUS200E_OP_RECALL; c.arg = (n > 1) ? f[1] : 0; break;
    case 0x01: c.op = BUS200E_OP_SAVE;   c.arg = (n > 1) ? f[1] : 0; break;
    case 0x14: c.op = BUS200E_OP_REMOTE_EN;  break;
    case 0x15: c.op = BUS200E_OP_REMOTE_DIS; break;
    case 0x2D:  // dump presets to card: [0x2D, modAddr, memLSB, memMSB, cardLo]
    case 0x2E:  // restore presets from card, same argument order
      if (n >= 5) {
        c.op = (f[0] == 0x2D) ? BUS200E_OP_BACKUP : BUS200E_OP_RESTORE;
        c.mod_addr = f[1];
        c.mem_off = (uint16_t) (f[2] | (f[3] << 8));
        c.card_lo = f[4];
      } else {
        c.op = BUS200E_OP_UNKNOWN; c.arg = f[0];
      }
      break;
    default:
      if (f[0] & 0x80) {
        // Bus MIDI (status-first) and realtime clock ride the same bus.
        c.op = (f[0] >= 0xF8) ? BUS200E_OP_CLOCK : BUS200E_OP_MIDI;
        c.arg = f[0];
      } else {
        c.op = BUS200E_OP_UNKNOWN; c.arg = f[0];
      }
      break;
  }
  dispatch(&c);
}

BUS_CODE static void drop_frame(uint8_t why_len) {
  Bus200eCmd c = { BUS200E_OP_DROPPED, why_len, 0, 0, 0 };
  log_cmd(&c);
  stats.dropped++;
}

BUS_CODE void Bus200eFeedEvent(uint16_t ev) {
  if (ev & BUS200E_EV_OVF) {
    frame_poisoned = 1;
    return;
  }
  if (ev & BUS200E_EV_START) {
    // A new transaction while a frame is open (repeated START, or a lost
    // STOP): the partial frame is not trustworthy -- drop it.
    if (in_frame && frame_len) drop_frame(frame_len);
    in_frame = 1;
    frame_len = 0;
    frame_poisoned = 0;
    return;
  }
  if (ev & BUS200E_EV_STOP) {
    if (in_frame && !frame_poisoned && frame_len > 0 && frame_len <= FRAME_MAX)
      parse_frame();
    else if (in_frame && frame_len)
      drop_frame(frame_len);
    in_frame = 0;
    frame_len = 0;
    frame_poisoned = 0;
    return;
  }
  // data byte
  if (!in_frame) return;
  if (frame_len >= FRAME_MAX) { frame_poisoned = 1; return; }
  frame[frame_len++] = (uint8_t) ev;
}

// ---- card transfer job (phase 2: runs only when all hooks are supplied) ----

BUS_CODE void Bus200eTask(void) {
  if (!job.active) return;

  if (!bus_ops || !bus_ops->record_size ||
      (job.is_restore && !(bus_ops->card_read && bus_ops->slot_write)) ||
      (!job.is_restore && !(bus_ops->card_write && bus_ops->slot_read))) {
    job.active = 0;   // RX-log-only build: transfers are logged, never run
    return;
  }

  uint8_t card7 = (uint8_t) ((BUS200E_CARD_BASE | job.card_lo) & 0x7F);
  uint32_t off = (uint32_t) job.mem_off +
                 (uint32_t) job.next_slot * bus_ops->record_size;
  static uint8_t rec[512];  // record staging; record_size must fit
  if (bus_ops->record_size > sizeof(rec)) { job.active = 0; return; }

  if (job.is_restore) {
    if (bus_ops->card_read(card7, off, rec, bus_ops->record_size) != 0) {
      stats.job_errors++;
      job.active = 0;
      return;
    }
    const int w = bus_ops->slot_write(job.next_slot, rec, bus_ops->record_size);
    if (w < 0) {
      stats.job_errors++;
      job.active = 0;
      return;
    }
    if (w > 0) stats.restore_rejects++;  // invalid record: skip, keep going
  } else {
    if (bus_ops->slot_read(job.next_slot, rec, sizeof(rec)) != 0 ||
        bus_ops->card_write(card7, off, rec, bus_ops->record_size) != 0) {
      stats.job_errors++;
      job.active = 0;
      return;
    }
  }

  job.next_slot++;
  if (job.next_slot >= BUS200E_BUS_PRESETS) job.active = 0;
}
