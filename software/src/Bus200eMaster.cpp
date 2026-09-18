// Transient bus-master orchestration for foreign-module BACKUP/RESTORE.
// Pure logic -- no hardware includes; see Bus200eMaster.h for the contract.
#include <stddef.h>

// On target, keep this cold code out of ITCM; host builds compile it bare.
#if defined(__IMXRT1062__)
#include <Arduino.h>
#define MASTER_CODE FLASHMEM
#else
#define MASTER_CODE
#endif

#include "Bus200eMaster.h"

static const Bus200eMasterOps *ops;

static Bus200eMasterState state = BUS200E_MASTER_IDLE;
static Bus200eMasterError last_error = BUS200E_MASTER_ERR_NONE;

static uint8_t job_mod_addr;
static uint8_t job_card_lo;
static int     job_is_restore;

static uint32_t send_start_ms;          // job accepted -> SENDING timeout base
static uint32_t phase_start_ms;         // current-state entry time
static uint32_t activity_baseline;      // card_activity() sampled at send time
static uint32_t last_activity_val;
static uint32_t last_activity_change_ms;
static int      job_acked;              // target announced XFER_DONE

// ---- QUERY sibling FSM state (see Bus200eMaster.h) -------------------------
static Bus200eQueryState  q_state = BUS200E_QUERY_IDLE;
static Bus200eMasterError q_error = BUS200E_MASTER_ERR_NONE;
static uint8_t  q_mod_addr;
static uint8_t  q_ver[BUS200E_QUERY_VER_MAX];
static uint8_t  q_ver_len;
static uint32_t q_strays;
static uint32_t q_send_start_ms;
static uint32_t q_phase_start_ms;

static int ops_complete(const Bus200eMasterOps *o) {
  return o && o->now_ms && o->tx_gate_open && o->probe_card &&
         o->send_frame && o->suppress_echo && o->card_activity;
}

// A QUERY needs only four of the six ops: there is no card to probe and no
// card traffic to watch. Requiring the full table would refuse a perfectly
// valid query on the absence of hooks it never calls.
static int query_ops_ready(const Bus200eMasterOps *o) {
  return o && o->now_ms && o->tx_gate_open && o->send_frame && o->suppress_echo;
}

MASTER_CODE void Bus200eMasterInit(const Bus200eMasterOps *o) {
  ops = o;
  q_state = BUS200E_QUERY_IDLE;
  q_error = BUS200E_MASTER_ERR_NONE;
  q_mod_addr = 0;
  q_ver_len = 0;
  q_strays = 0;
  q_send_start_ms = 0;
  q_phase_start_ms = 0;
  state = BUS200E_MASTER_IDLE;
  last_error = BUS200E_MASTER_ERR_NONE;
  job_mod_addr = 0;
  job_card_lo = 0;
  job_is_restore = 0;
  send_start_ms = 0;
  phase_start_ms = 0;
  activity_baseline = 0;
  last_activity_val = 0;
  last_activity_change_ms = 0;
  job_acked = 0;
}

MASTER_CODE int Bus200eMasterFindFreeCard(const uint8_t *candidates, uint8_t n,
                                           uint8_t *out_card_lo) {
  if (!ops || !ops->probe_card || !candidates || !out_card_lo) return 0;
  for (uint8_t i = 0; i < n; ++i) {
    const uint8_t addr7 = (uint8_t) ((BUS200E_CARD_BASE | candidates[i]) & 0x7F);
    if (!ops->probe_card(addr7)) {
      *out_card_lo = candidates[i];
      return 1;
    }
  }
  return 0;
}

MASTER_CODE static int start_job(uint8_t mod_addr, uint8_t card_lo, int is_restore) {
  if (!ops_complete(ops)) return -BUS200E_MASTER_ERR_BAD_ARGS;
  if (state == BUS200E_MASTER_SENDING || state == BUS200E_MASTER_WAIT_ACTIVITY ||
      state == BUS200E_MASTER_TRANSFERRING)
    return -BUS200E_MASTER_ERR_BUSY;

  job_mod_addr = mod_addr & 0x7F;
  job_card_lo = card_lo & 0x7F;
  job_is_restore = is_restore;
  last_error = BUS200E_MASTER_ERR_NONE;
  send_start_ms = ops->now_ms();
  phase_start_ms = send_start_ms;
  // Clear the previous job's activity samples now, not just at the
  // SENDING->WAIT_ACTIVITY transition: if this job fails before ever
  // getting there (SEND_TIMEOUT, BAD_ARGS), Bus200eMasterBytesTransferred()
  // must read 0 rather than silently reporting the prior job's delta.
  activity_baseline = 0;
  last_activity_val = 0;
  last_activity_change_ms = send_start_ms;
  job_acked = 0;
  state = BUS200E_MASTER_SENDING;
  return 0;
}

int Bus200eMasterBackup(uint8_t mod_addr, uint8_t card_lo) {
  return start_job(mod_addr, card_lo, 0);
}
int Bus200eMasterRestore(uint8_t mod_addr, uint8_t card_lo) {
  return start_job(mod_addr, card_lo, 1);
}

MASTER_CODE void Bus200eMasterTask(void) {
  if (!ops_complete(ops)) return;
  if (state != BUS200E_MASTER_SENDING && state != BUS200E_MASTER_WAIT_ACTIVITY &&
      state != BUS200E_MASTER_TRANSFERRING)
    return;

  const uint32_t now = ops->now_ms();

  switch (state) {
    case BUS200E_MASTER_SENDING: {
      if (now - send_start_ms > BUS200E_MASTER_SEND_TIMEOUT_MS) {
        state = BUS200E_MASTER_FAILED;
        last_error = BUS200E_MASTER_ERR_SEND_TIMEOUT;
        return;
      }
      if (!ops->tx_gate_open()) return;  // retry next Task() pass

      uint8_t f[BUS200E_XFER_FRAME_LEN];
      const uint8_t op = job_is_restore ? BUS200E_OP_RESTORE : BUS200E_OP_BACKUP;
      const int n = Bus200eBuildTransferFrame(op, job_mod_addr, job_card_lo, 0,
                                               f, sizeof(f));
      if (n < 0) {  // can't happen with a valid op, but fail closed
        state = BUS200E_MASTER_FAILED;
        last_error = BUS200E_MASTER_ERR_BAD_ARGS;
        return;
      }
      if (ops->send_frame(f, (uint8_t) n) != 0) return;  // NAK/arb loss: retry

      ops->suppress_echo(f, (uint8_t) n);
      activity_baseline = ops->card_activity();
      last_activity_val = activity_baseline;
      phase_start_ms = now;
      state = BUS200E_MASTER_WAIT_ACTIVITY;
      return;
    }

    case BUS200E_MASTER_WAIT_ACTIVITY: {
      const uint32_t act = ops->card_activity();
      if (act != activity_baseline) {
        last_activity_val = act;
        last_activity_change_ms = now;
        state = BUS200E_MASTER_TRANSFERRING;
        return;
      }
      if (now - phase_start_ms > BUS200E_MASTER_ACTIVITY_TIMEOUT_MS) {
        state = BUS200E_MASTER_FAILED;
        last_error = BUS200E_MASTER_ERR_NO_RESPONSE;
      }
      return;
    }

    case BUS200E_MASTER_TRANSFERRING: {
      const uint32_t act = ops->card_activity();
      if (act != last_activity_val) {
        last_activity_val = act;
        last_activity_change_ms = now;
      }
      const uint32_t quiet = job_acked ? BUS200E_MASTER_QUIET_ACKED_MS
                                       : BUS200E_MASTER_QUIET_DONE_MS;
      if (now - last_activity_change_ms > quiet) {
        state = BUS200E_MASTER_DONE;
        return;
      }
      if (now - send_start_ms > BUS200E_MASTER_HARD_CAP_MS) {
        // Safety net: activity never went quiet (a chatty bus, or our
        // quiet-detection missed a gap). We saw real transfer bytes move,
        // so call it done rather than fail a job that likely succeeded.
        state = BUS200E_MASTER_DONE;
      }
      return;
    }

    default:
      return;
  }
}

Bus200eMasterState Bus200eMasterGetState(void) { return state; }
Bus200eMasterError Bus200eMasterLastError(void) { return last_error; }
uint8_t Bus200eMasterCardAddr(void) { return job_card_lo; }
uint8_t Bus200eMasterModAddr(void) { return job_mod_addr; }
int Bus200eMasterIsRestore(void) { return job_is_restore; }

uint32_t Bus200eMasterBytesTransferred(void) {
  return last_activity_val - activity_baseline;
}

void Bus200eMasterXferDone(uint8_t from_addr) {
  if (state != BUS200E_MASTER_WAIT_ACTIVITY &&
      state != BUS200E_MASTER_TRANSFERRING)
    return;
  if ((from_addr & 0x7F) != job_mod_addr) return;
  job_acked = 1;
}

int Bus200eMasterAcked(void) { return job_acked; }

void Bus200eMasterReset(void) {
  if (state == BUS200E_MASTER_DONE || state == BUS200E_MASTER_FAILED)
    state = BUS200E_MASTER_IDLE;
}

// ---- QUERY: master the request, capture the module's reply -----------------

MASTER_CODE int Bus200eMasterQuery(uint8_t mod_addr) {
  if (!query_ops_ready(ops)) return -BUS200E_MASTER_ERR_BAD_ARGS;
  // mod_addr 0 is the broadcast destination -- refused here as well as in
  // Bus200eBuildQueryFrame(), so a caller finds out at request time rather
  // than after a pointless trip through SENDING.
  if ((mod_addr & 0x7F) == 0) return -BUS200E_MASTER_ERR_BAD_ARGS;
  if (q_state == BUS200E_QUERY_SENDING || q_state == BUS200E_QUERY_WAITING)
    return -BUS200E_MASTER_ERR_BUSY;

  q_mod_addr = mod_addr & 0x7F;
  q_error = BUS200E_MASTER_ERR_NONE;
  // Clear the previous query's answer at request time, not on reply: a query
  // that times out must read back as "no version", never as the last
  // module's string (the same stale-result trap Bus200eMasterBytesTransferred
  // documents above).
  q_ver_len = 0;
  q_strays = 0;
  q_send_start_ms = ops->now_ms();
  q_phase_start_ms = q_send_start_ms;
  q_state = BUS200E_QUERY_SENDING;
  return 0;
}

MASTER_CODE void Bus200eMasterQueryTask(void) {
  if (!query_ops_ready(ops)) return;
  if (q_state != BUS200E_QUERY_SENDING && q_state != BUS200E_QUERY_WAITING)
    return;

  const uint32_t now = ops->now_ms();

  if (q_state == BUS200E_QUERY_SENDING) {
    if (now - q_send_start_ms > BUS200E_MASTER_QUERY_SEND_TIMEOUT_MS) {
      q_state = BUS200E_QUERY_FAILED;
      q_error = BUS200E_MASTER_ERR_SEND_TIMEOUT;
      return;
    }
    if (!ops->tx_gate_open()) return;  // retry next Task() pass

    uint8_t f[BUS200E_QUERY_FRAME_LEN];
    const int n = Bus200eBuildQueryFrame(q_mod_addr, f, sizeof(f));
    if (n < 0) {  // can't happen (addr 0 refused at request), but fail closed
      q_state = BUS200E_QUERY_FAILED;
      q_error = BUS200E_MASTER_ERR_BAD_ARGS;
      return;
    }
    if (ops->send_frame(f, (uint8_t) n) != 0) return;  // NAK/arb loss: retry

    ops->suppress_echo(f, (uint8_t) n);
    // The reply window opens now: it is measured from the moment the request
    // actually made it onto the wire, not from when the job was accepted --
    // a long wait for a quiet bus must not eat the module's answering time.
    q_phase_start_ms = now;
    q_state = BUS200E_QUERY_WAITING;
    return;
  }

  // BUS200E_QUERY_WAITING: nothing to do but time out. The answer arrives
  // through Bus200eMasterQueryReply(), driven by the slave RX path.
  if (now - q_phase_start_ms > BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS) {
    q_state = BUS200E_QUERY_FAILED;
    q_error = BUS200E_MASTER_ERR_NO_RESPONSE;
  }
}

MASTER_CODE void Bus200eMasterQueryReply(uint8_t from_addr, const uint8_t *ver,
                                          uint8_t n) {
  if (q_state != BUS200E_QUERY_WAITING) return;  // unsolicited: not our answer
  if ((from_addr & 0x7F) != q_mod_addr) {
    // Someone else's exchange (a live preset manager enumerating the bus
    // while we ask). Count it -- it explains an otherwise mysterious
    // NO_RESPONSE at the bench -- but keep waiting for the one we asked.
    q_strays++;
    return;
  }
  q_ver_len = 0;
  if (ver) {
    while (q_ver_len < n && q_ver_len < BUS200E_QUERY_VER_MAX) {
      q_ver[q_ver_len] = ver[q_ver_len];
      q_ver_len++;
    }
  }
  q_state = BUS200E_QUERY_DONE;
}

Bus200eQueryState  Bus200eMasterQueryGetState(void) { return q_state; }
Bus200eMasterError Bus200eMasterQueryLastError(void) { return q_error; }
uint8_t  Bus200eMasterQueryModAddr(void) { return q_mod_addr; }
uint32_t Bus200eMasterQueryStrayReplies(void) { return q_strays; }

MASTER_CODE uint8_t Bus200eMasterQueryVersion(uint8_t *out, uint8_t cap) {
  if (!out || !cap) return 0;
  const uint8_t n = (q_ver_len < cap) ? q_ver_len : cap;
  for (uint8_t i = 0; i < n; ++i) out[i] = q_ver[i];
  return n;
}

void Bus200eMasterQueryReset(void) {
  if (q_state == BUS200E_QUERY_DONE || q_state == BUS200E_QUERY_FAILED)
    q_state = BUS200E_QUERY_IDLE;
}
