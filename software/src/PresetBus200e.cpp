
#include <string.h>

#if defined(__IMXRT1062__)
#include <Arduino.h>
#define BUS_CODE FLASHMEM
#else
#define BUS_CODE
#endif

#include "PresetBus200e.h"

#define FRAME_MAX 12

static const Bus200eOps *bus_ops;

static uint8_t  frame[FRAME_MAX];
static uint8_t  frame_len;
static uint8_t  in_frame;
static uint8_t  frame_poisoned;

static uint8_t  remote_enabled;
static uint32_t now_ms;
static uint32_t last_transfer_ms;
static uint8_t  suppress_len;
static uint8_t  suppress_buf[FRAME_MAX];
static uint32_t suppress_at_ms;
static uint8_t  module_addr = BUS200E_DEFAULT_MODULE_ADDR;
static uint8_t  query_pending;

static struct {
  uint8_t  active;
  uint8_t  is_restore;
  uint8_t  card_lo;
  uint8_t  next_slot;
  uint16_t mem_off;
} job;

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
      last_transfer_ms = now_ms ? now_ms : 1;
      if (c->mod_addr != module_addr) break;
      if (job.active) {
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

    default:
      break;
  }
}

BUS_CODE static void parse_frame(void) {
  Bus200eCmd c = { BUS200E_OP_NONE, 0, 0, 0, 0 };
  const uint8_t *f = frame;
  uint8_t n = frame_len;

  stats.frames++;

  if (suppress_len && now_ms - suppress_at_ms > 50) suppress_len = 0;
  if (suppress_len && suppress_len == n && !memcmp(suppress_buf, f, n)) {
    suppress_len = 0;
    return;
  }

  if (n >= 4 && f[0] == n - 1 && f[1] == 0x22 && f[2] != 0x22 &&
      (f[3] == 0x13 || f[3] == 0x1C)) {
    stats.frames_long++;
    const uint8_t vn = (uint8_t) (n - 4);
    c.op = BUS200E_OP_QUERY_REPLY;
    c.mod_addr = f[2];
    c.arg = vn;
    c.card_lo = (vn > 0) ? f[4] : 0;
    c.mem_off = (uint16_t) (((vn > 1) ? f[5] : 0) |
                            (((vn > 2) ? f[6] : 0) << 8));
    if (bus_ops && bus_ops->query_reply) bus_ops->query_reply(f[2], f + 4, vn);
    dispatch(&c);
    return;
  }

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

  if (n == 5 && f[0] == 4 && f[1] == 0x22 && f[2] != 0x22 && f[3] == 0x03) {
    stats.frames_long++;
    c.op = BUS200E_OP_LOAD_ACK;
    c.mod_addr = f[2];
    c.arg = f[4];
    if (bus_ops && bus_ops->load_ack) bus_ops->load_ack(f[2]);
    dispatch(&c);
    return;
  }

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
      case 0x0F:
        if (n >= 8) {
          c.op = (f[4] >= 0xF8) ? BUS200E_OP_CLOCK : BUS200E_OP_MIDI;
          c.arg = f[4];
          c.card_lo = f[6];
          c.mem_off = f[7];
          if (bus_ops && bus_ops->midi_rx)
            bus_ops->midi_rx(f[4], f[6], f[7]);
        } else {
          c.op = BUS200E_OP_UNKNOWN; c.arg = f[3];
        }
        break;
      case 0x04:
      case 0x05:
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

  stats.frames_short++;
  switch (f[0]) {
    case 0x00: c.op = BUS200E_OP_RECALL; c.arg = (n > 1) ? f[1] : 0; break;
    case 0x01: c.op = BUS200E_OP_SAVE;   c.arg = (n > 1) ? f[1] : 0; break;
    case 0x14: c.op = BUS200E_OP_REMOTE_EN;  break;
    case 0x15: c.op = BUS200E_OP_REMOTE_DIS; break;
    case 0x2D:
    case 0x2E:
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
  if (!in_frame) return;
  if (frame_len >= FRAME_MAX) { frame_poisoned = 1; return; }
  frame[frame_len++] = (uint8_t) ev;
}

BUS_CODE int Bus200eBuildTransferFrame(uint8_t op, uint8_t mod_addr,
                                        uint8_t card_lo, uint16_t mem_off,
                                        uint8_t *out, uint8_t cap) {
  if (cap < BUS200E_XFER_FRAME_LEN) return -1;
  if (op != BUS200E_OP_BACKUP && op != BUS200E_OP_RESTORE) return -1;
  out[0] = BUS200E_XFER_FRAME_LEN - 1;
  out[1] = 0x00;
  out[2] = 0x22;
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
  if ((mod_addr & 0x7F) == 0) return -1;
  out[0] = BUS200E_QUERY_FRAME_LEN - 1;
  out[1] = mod_addr & 0x7F;
  out[2] = 0x22;
  out[3] = 0x1A;
  out[4] = 0xFF;
  return BUS200E_QUERY_FRAME_LEN;
}

BUS_CODE void Bus200eTask(void) {
  if (!job.active) return;

  if (!bus_ops || !bus_ops->record_size ||
      (job.is_restore && !(bus_ops->card_read && bus_ops->slot_write)) ||
      (!job.is_restore && !(bus_ops->card_write && bus_ops->slot_read))) {
    job.active = 0;
    return;
  }

  uint8_t card7 = (uint8_t) ((BUS200E_CARD_BASE | job.card_lo) & 0x7F);
  uint32_t off = (uint32_t) job.mem_off +
                 (uint32_t) job.next_slot * bus_ops->record_size;
  static uint8_t rec[512];
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
    if (w > 0) stats.restore_rejects++;
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
