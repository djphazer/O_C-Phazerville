// Buchla 200e preset-bus engine: frame parser (both command framings),
// remote-enable state, save/recall dispatch and chunked card transfer jobs.
// Pure logic -- no hardware includes; see PresetBus200e.h for the contract.
//
// Ported from the MARF project's bus200e.c; protocol ground truth is
// github.com/studiohsoftware/2WIRELESS. Framing labels corrected relative to
// the MARF source comments (PRIMO = long frame, V2/pre-PRIMO = short frame).

#include <string.h>

// On target, keep this cold code out of ITCM; host builds compile it bare.
#if defined(__IMXRT1062__)
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
static uint32_t now_ms;
static uint32_t last_transfer_ms;
static uint8_t  suppress_len;
static uint8_t  suppress_buf[FRAME_MAX];
static uint32_t suppress_at_ms;
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
uint32_t Bus200eLastTransferMs(void) { return last_transfer_ms; }
void Bus200eSetNow(uint32_t ms) { now_ms = ms; }

BUS_CODE void Bus200eSuppressFrame(const uint8_t *bytes, uint8_t n) {
  if (n > FRAME_MAX) { suppress_len = 0; return; }
  memcpy(suppress_buf, bytes, n);
  suppress_len = n;
  suppress_at_ms = now_ms;
}
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
  last_transfer_ms = 0;
  suppress_len = 0;
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
      // any module: the card window is open. 0 means "never", so a zero
      // reading becomes 1 -- not `| 1`, which would round an even reading
      // up and make now - last_transfer_ms wrap for the rest of that ms.
      last_transfer_ms = now_ms ? now_ms : 1;
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

  // self-echo: our own mastered frame arrives back through our slave.
  // Expired suppressions are discarded so a stale registration can never
  // eat a genuine identical frame later (e.g. the manager's own recall).
  if (suppress_len && now_ms - suppress_at_ms > 50) suppress_len = 0;
  if (suppress_len && suppress_len == n && !memcmp(suppress_buf, f, n)) {
    suppress_len = 0;
    return;
  }

  // QUERY REPLY: a queried module answers by mastering a general-call frame
  // of its own, with the dest/src columns SWAPPED relative to a command --
  // [nBytes, destAddr=0x22, srcAddr=replier, cmd, payload...] -- the
  // manager's identity 0x22 being the ADDRESSEE rather than the sender.
  //
  // The reply command byte is 0x1C. Traced off real hardware at the bench: a
  // Buchla 251e at 0x5C answered [04 5C 22 1A FF] with [04 22 5C 1C FF], and
  // the module at 0x28 answered identically bar its own address. 0x13 is
  // accepted alongside it for two reasons: it is what this firmware itself
  // replied with before the capture, so an older Xenomorpher is still
  // understood, and it is a real opcode in its own right -- the module
  // firmware-display frame both module firmwares build and the WPM decodes
  // (see the note on BUS200E_QUERY_FRAME_LEN). A QUERY just never draws it.
  //
  // Checked BEFORE the long/PRIMO branch, but guarded with f[2] != 0x22 so a
  // command frame (srcAddr 0x22) can never be stolen from that branch no
  // matter what it is addressed to -- every frame that parsed as a command
  // before still parses as one now.
  if (n >= 4 && f[0] == n - 1 && f[1] == 0x22 && f[2] != 0x22 &&
      (f[3] == 0x13 || f[3] == 0x1C)) {
    stats.frames_long++;   // same PRIMO dialect as the command framing below
    const uint8_t vn = (uint8_t) (n - 4);
    c.op = BUS200E_OP_QUERY_REPLY;
    c.mod_addr = f[2];                    // who answered
    c.arg = vn;                           // version-string length
    c.card_lo = (vn > 0) ? f[4] : 0;      // first three version chars, for the
    c.mem_off = (uint16_t) (((vn > 1) ? f[5] : 0) |   // debug ring (field
                            (((vn > 2) ? f[6] : 0) << 8));  // reuse, as above)
    if (bus_ops && bus_ops->query_reply) bus_ops->query_reply(f[2], f + 4, vn);
    dispatch(&c);
    return;
  }

  // TRANSFER DONE: [04 dest src 0A src] -- a module announcing, once per
  // job, that it has finished with the card. Seen on the bench 2026-09-02
  // from both module types we have, right after the last byte of a BACKUP
  // we had asked for landed in our card:
  //   251e at 0x5C, 63120 bytes:  [04 22 5C 0A 5C]   (dest = manager 0x22)
  //   259e at 0x28,   990 bytes:  [04 00 28 0A 28]   (dest = general call)
  // Same command byte, same one-byte payload (the module's own address
  // again), different destination column -- so both are accepted. Neither
  // the 2WIRELESS source nor any prior note knows this frame (a WPM in card
  // mode swallows it into FRAM as data). Bus200eMaster treats it as
  // evidence, not proof -- it shortens the quiet wait, it does not replace
  // it. Whether a RESTORE ends the same way has not been seen. No short
  // command starts with 0x04, so the shape cannot shadow one.
  if (n == 5 && f[0] == 4 && (f[1] == 0x22 || f[1] == 0x00) &&
      f[2] != 0x22 && f[3] == 0x0A) {
    stats.frames_long++;
    c.op = BUS200E_OP_XFER_DONE;
    c.mod_addr = f[2];
    c.arg = f[4];
    if (bus_ops && bus_ops->xfer_done) bus_ops->xfer_done(f[2]);
    dispatch(&c);
    return;
  }

  // LOAD ACK: [04 22 addr 03 xx] -- a module's poll reply, mastered once per
  // preset load, in the same module->manager form the QUERY reply and the
  // transfer announcement use. Decoded from the 259e firmware (builder at
  // 0x9179, payload 0xFF, sent when polling mode is on and the module is
  // remote-enabled; the latch that arms it is cleared on every preset load
  // and save) and confirmed on the bench 2026-09-10: a panel recall of slot
  // 1 drew [04 22 28 03 FF] from the 259e and [04 22 20 03 FF] from the
  // 210e, one frame each. The 251e's builder (0x80008920) never writes
  // frame[4] and ships uninitialised stack there -- observed as 0x00 -- so
  // the payload is logged and otherwise ignored.
  //
  // This is the only positive confirmation the bus offers that a broadcast
  // RECALL was acted on: PresetBus counts the distinct addresses that answer
  // within a window of our own broadcast. Guarded like the branches above so
  // a command frame (srcAddr 0x22) can never be stolen from the long branch.
  if (n == 5 && f[0] == 4 && f[1] == 0x22 && f[2] != 0x22 && f[3] == 0x03) {
    stats.frames_long++;
    c.op = BUS200E_OP_LOAD_ACK;
    c.mod_addr = f[2];
    c.arg = f[4];   // not meaningful; kept for the debug ring
    if (bus_ops && bus_ops->load_ack) bus_ops->load_ack(f[2]);
    dispatch(&c);
    return;
  }

  // LONG / PRIMO framing: [nBytes, destAddr, srcAddr=0x22, cmd, args...],
  // where nBytes counts the bytes that follow it. No short command collides
  // with this shape.
  if (n >= 4 && f[2] == 0x22 && f[0] == n - 1) {
    stats.frames_long++;
    c.mod_addr = f[1];
    switch (f[3]) {
      case 0x01: c.op = BUS200E_OP_RECALL; c.arg = (n > 4) ? f[4] : 0; break;
      case 0x02: c.op = BUS200E_OP_SAVE;   c.arg = (n > 4) ? f[4] : 0; break;
      case 0x14: c.op = BUS200E_OP_POLL_DONE;  break;
      case 0x16: c.op = BUS200E_OP_REMOTE_EN;  break;
      case 0x17: c.op = BUS200E_OP_REMOTE_DIS; break;
      case 0x1A: c.op = BUS200E_OP_QUERY;      break;
      case 0x0F:  // bus MIDI: [.., status|busmask, 0x00, data1, data2, 0x00]
        if (n >= 8) {
          c.op = (f[4] >= 0xF8) ? BUS200E_OP_CLOCK : BUS200E_OP_MIDI;
          c.arg = f[4];
          c.card_lo = f[6];             // data1 (field reuse for the log)
          c.mem_off = f[7];             // data2 (field reuse for the log)
          if (bus_ops && bus_ops->midi_rx)
            bus_ops->midi_rx(f[4], f[6], f[7]);
        } else {
          c.op = BUS200E_OP_UNKNOWN; c.arg = f[3];
        }
        break;
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
  stats.frames_short++;
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
        if (bus_ops && bus_ops->midi_rx)
          bus_ops->midi_rx(f[0], (n > 1) ? f[1] : 0, (n > 2) ? f[2] : 0);
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

// ---- master-side frame building (new) --------------------------------------

BUS_CODE int Bus200eBuildTransferFrame(uint8_t op, uint8_t mod_addr,
                                        uint8_t card_lo, uint16_t mem_off,
                                        uint8_t *out, uint8_t cap) {
  if (cap < BUS200E_XFER_FRAME_LEN) return -1;
  if (op != BUS200E_OP_BACKUP && op != BUS200E_OP_RESTORE) return -1;
  out[0] = BUS200E_XFER_FRAME_LEN - 1;  // nBytes: bytes after itself
  out[1] = 0x00;                        // destAddr: broadcast
  out[2] = 0x22;                        // srcAddr: matches every other master path
  out[3] = (op == BUS200E_OP_BACKUP) ? 0x04 : 0x05;
  out[4] = mod_addr & 0x7F;
  out[5] = card_lo & 0x7F;
  out[6] = (uint8_t) (mem_off & 0xFF);
  out[7] = (uint8_t) (mem_off >> 8);
  return BUS200E_XFER_FRAME_LEN;
}

BUS_CODE int Bus200eBuildQueryFrame(uint8_t mod_addr, uint8_t *out,
                                     uint8_t cap) {
  if (cap < BUS200E_QUERY_FRAME_LEN) return -1;
  // destAddr 0 is the broadcast address: a broadcast QUERY would have every
  // module on the bus answer at once, colliding. Refuse it rather than
  // wedging the bus (see PresetBus200e.h).
  if ((mod_addr & 0x7F) == 0) return -1;
  out[0] = BUS200E_QUERY_FRAME_LEN - 1;  // nBytes: bytes after itself
  out[1] = mod_addr & 0x7F;              // destAddr: the module being asked
  out[2] = 0x22;                         // srcAddr: us, asserting manager identity
  out[3] = 0x1A;                         // QUERY
  // Argument byte. A real 251e and the module at 0x28 both ignore it: five
  // different values (00-04, FF) all drew the identical reply, so 0xFF is
  // kept only because that is what a real manager was seen sending.
  out[4] = 0xFF;
  return BUS200E_QUERY_FRAME_LEN;
}

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
