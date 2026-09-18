// Host tests for the foreign-module BACKUP/RESTORE master orchestration
// (src/Bus200eMaster.cpp): card-address discovery, frame construction (via
// the real Bus200eBuildTransferFrame(), not a re-implementation), the
// send/retry/timeout FSM and activity-based done-detection, all against a
// fake I2C transport (no hardware, no real bus).
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_bus200e_master test_bus200e_master.cpp
//      ../src/Bus200eMaster.cpp ../src/PresetBus200e.cpp &&
//   ./build/test_bus200e_master
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/Bus200eMaster.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

// ---- fake transport ---------------------------------------------------------

static uint32_t fake_now;
static int fake_gate_open;
static uint8_t occupied[128];         // occupied[addr7] = 1 -> probe_card ACKs it
static int send_result;               // 0 = ok, nonzero = simulated I2C error
static int send_fail_countdown;       // fail this many sends first, then succeed
static uint8_t last_sent[16];
static uint8_t last_sent_len;
static int send_calls;
static uint8_t last_suppressed[16];
static uint8_t last_suppressed_len;
static int suppress_calls;
static uint32_t fake_activity;

static uint32_t f_now(void) { return fake_now; }
static int f_gate_open(void) { return fake_gate_open; }
static int f_probe_card(uint8_t addr7) { return occupied[addr7 & 0x7F] ? 1 : 0; }
static int f_send_frame(const uint8_t *b, uint8_t n) {
  send_calls++;
  if (n > sizeof(last_sent)) n = sizeof(last_sent);
  memcpy(last_sent, b, n);
  last_sent_len = n;
  if (send_fail_countdown > 0) { send_fail_countdown--; return 1; }
  return send_result;
}
static void f_suppress_echo(const uint8_t *b, uint8_t n) {
  suppress_calls++;
  if (n > sizeof(last_suppressed)) n = sizeof(last_suppressed);
  memcpy(last_suppressed, b, n);
  last_suppressed_len = n;
}
static uint32_t f_card_activity(void) { return fake_activity; }

static const Bus200eMasterOps fake_ops = {
  f_now, f_gate_open, f_probe_card, f_send_frame, f_suppress_echo, f_card_activity,
};

static void reset(void) {
  fake_now = 0;
  fake_gate_open = 1;
  memset(occupied, 0, sizeof(occupied));
  send_result = 0;
  send_fail_countdown = 0;
  memset(last_sent, 0, sizeof(last_sent));
  last_sent_len = 0;
  send_calls = 0;
  memset(last_suppressed, 0, sizeof(last_suppressed));
  last_suppressed_len = 0;
  suppress_calls = 0;
  fake_activity = 0;
  Bus200eMasterInit(&fake_ops);
}

// ============================================================================

static void test_find_free_card(void) {
  printf("test_find_free_card\n");
  reset();
  const uint8_t candidates[] = { 0, 1, 2, 3 };
  uint8_t got = 0xFF;

  // nothing claimed: first candidate wins
  CHECK(Bus200eMasterFindFreeCard(candidates, 4, &got) == 1);
  CHECK(got == 0);

  // 0 and 1 claimed: first free is 2
  occupied[BUS200E_CARD_BASE | 0] = 1;
  occupied[BUS200E_CARD_BASE | 1] = 1;
  got = 0xFF;
  CHECK(Bus200eMasterFindFreeCard(candidates, 4, &got) == 1);
  CHECK(got == 2);

  // every candidate claimed: none free
  occupied[BUS200E_CARD_BASE | 2] = 1;
  occupied[BUS200E_CARD_BASE | 3] = 1;
  CHECK(Bus200eMasterFindFreeCard(candidates, 4, &got) == 0);

  // empty candidate list: none free
  reset();
  CHECK(Bus200eMasterFindFreeCard(candidates, 0, &got) == 0);
}

static void test_bad_ops_rejected(void) {
  printf("test_bad_ops_rejected\n");
  Bus200eMasterOps incomplete = fake_ops;
  incomplete.send_frame = nullptr;
  Bus200eMasterInit(&incomplete);
  CHECK(Bus200eMasterBackup(0x3C, 0) == -BUS200E_MASTER_ERR_BAD_ARGS);

  Bus200eMasterInit(nullptr);
  CHECK(Bus200eMasterBackup(0x3C, 0) == -BUS200E_MASTER_ERR_BAD_ARGS);
}

static void test_backup_happy_path(void) {
  printf("test_backup_happy_path\n");
  reset();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_IDLE);
  CHECK(Bus200eMasterBytesTransferred() == 0);  // nothing has ever run
  CHECK(Bus200eMasterBackup(0x3C, 0x02) == 0);
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);
  CHECK(Bus200eMasterModAddr() == 0x3C && Bus200eMasterCardAddr() == 0x02);
  CHECK(Bus200eMasterIsRestore() == 0);

  Bus200eMasterTask();  // gate open, sends immediately
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);
  CHECK(send_calls == 1);
  const uint8_t want[] = { 0x07, 0x00, 0x22, 0x04, 0x3C, 0x02, 0x00, 0x00 };
  CHECK(last_sent_len == 8 && memcmp(last_sent, want, 8) == 0);
  CHECK(suppress_calls == 1);
  CHECK(last_suppressed_len == 8 && memcmp(last_suppressed, want, 8) == 0);

  // no activity yet: stays in WAIT_ACTIVITY
  fake_now += 500;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);
  CHECK(Bus200eMasterBytesTransferred() == 0);

  // target starts writing to the card
  fake_activity = 40;
  fake_now += 100;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);
  CHECK(Bus200eMasterBytesTransferred() == 40);

  // more bytes arrive: still transferring, not yet quiet
  fake_activity = 400;
  fake_now += 800;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);
  CHECK(Bus200eMasterBytesTransferred() == 400);

  // bus goes quiet for less than the done threshold: still transferring
  fake_now += (BUS200E_MASTER_QUIET_DONE_MS - 100);
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);

  // quiet past the threshold: done
  fake_now += 200;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_DONE);
  CHECK(Bus200eMasterLastError() == BUS200E_MASTER_ERR_NONE);
  CHECK(Bus200eMasterBytesTransferred() == 400);  // frozen at the DONE value

  // still reads correctly well after DONE, before any new job starts
  fake_now += 5000;
  CHECK(Bus200eMasterBytesTransferred() == 400);
}

// The target's end-of-transfer announcement (Bus200eMasterXferDone) shortens
// the quiet wait to BUS200E_MASTER_QUIET_ACKED_MS; it never ends the job on
// its own, and only the job's own module, during the job, is heard.
static void test_xfer_done_shortens_quiet(void) {
  printf("test_xfer_done_shortens_quiet\n");
  reset();
  // an announcement with no job open is ignored
  Bus200eMasterXferDone(0x3C);
  CHECK(Bus200eMasterAcked() == 0);

  CHECK(Bus200eMasterBackup(0x3C, 0x02) == 0);
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);
  fake_activity = 40; fake_now += 100; Bus200eMasterTask();
  fake_activity = 400; fake_now += 800; Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);

  // some other module's announcement: not ours
  Bus200eMasterXferDone(0x28);
  CHECK(Bus200eMasterAcked() == 0);
  fake_now += BUS200E_MASTER_QUIET_ACKED_MS + 50;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);

  // bytes still moving while the target announces: the short quiet window
  // starts from the LAST activity, not from the announcement
  fake_activity = 500; fake_now += 100; Bus200eMasterTask();
  Bus200eMasterXferDone(0x3C | 0x80);   // high bit masked like everywhere else
  CHECK(Bus200eMasterAcked() == 1);
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);
  fake_activity = 520; fake_now += 100; Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);
  fake_now += BUS200E_MASTER_QUIET_ACKED_MS - 50; Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);
  fake_now += 100; Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_DONE);
  CHECK(Bus200eMasterBytesTransferred() == 520);

  // a new job starts un-acked, and its quiet wait is the long one again
  Bus200eMasterReset();
  CHECK(Bus200eMasterBackup(0x3C, 0x02) == 0);
  CHECK(Bus200eMasterAcked() == 0);
  Bus200eMasterTask();
  fake_activity = 600; fake_now += 100; Bus200eMasterTask();
  fake_now += BUS200E_MASTER_QUIET_ACKED_MS + 100; Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);
  fake_now += BUS200E_MASTER_QUIET_DONE_MS; Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_DONE);
}

// Bus200eMasterBytesTransferred() must report the delta for THIS job only,
// never a stale figure left over from a previous one -- this is exactly the
// bug DumpCard() hit against BusCardStats::bytes_written (a lifetime
// cumulative counter): capturing the same 990-byte module twice back to
// back must report 990 both times, not 990 then 1980.
static void test_bytes_transferred_no_cross_job_leakage(void) {
  printf("test_bytes_transferred_no_cross_job_leakage\n");
  reset();

  // job 1: moves 990 bytes, starting from a nonzero fake_activity baseline
  // (a real card_activity() counter never resets between jobs either).
  fake_activity = 1000;
  CHECK(Bus200eMasterBackup(0x28, 0) == 0);
  Bus200eMasterTask();  // -> WAIT_ACTIVITY, baseline sampled at 1000
  fake_activity = 1000 + 990;
  fake_now += 100;
  Bus200eMasterTask();  // -> TRANSFERRING
  fake_now += BUS200E_MASTER_QUIET_DONE_MS + 1;
  Bus200eMasterTask();  // -> DONE
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_DONE);
  CHECK(Bus200eMasterBytesTransferred() == 990);

  Bus200eMasterReset();  // back to IDLE; DONE-value samples untouched
  CHECK(Bus200eMasterBytesTransferred() == 990);

  // job 2: same module, same 990 bytes again, activity counter keeps
  // climbing from where it left off (1990), never reset to 0
  CHECK(Bus200eMasterBackup(0x28, 0) == 0);
  CHECK(Bus200eMasterBytesTransferred() == 0);  // cleared the instant job 2 starts
  Bus200eMasterTask();  // -> WAIT_ACTIVITY, baseline re-sampled at 1990
  fake_activity += 990;
  fake_now += 100;
  Bus200eMasterTask();  // -> TRANSFERRING
  fake_now += BUS200E_MASTER_QUIET_DONE_MS + 1;
  Bus200eMasterTask();  // -> DONE
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_DONE);
  CHECK(Bus200eMasterBytesTransferred() == 990);  // not 1980
}

// A job that fails before the target ever touches the card (send timeout,
// or bad ops) must report 0, not whatever the previous job left behind.
static void test_bytes_transferred_zero_on_early_failure(void) {
  printf("test_bytes_transferred_zero_on_early_failure\n");
  reset();

  // establish a nonzero "previous job" figure first
  CHECK(Bus200eMasterBackup(0x28, 0) == 0);
  Bus200eMasterTask();  // -> WAIT_ACTIVITY
  fake_activity = 500;
  Bus200eMasterTask();  // -> TRANSFERRING
  fake_now += BUS200E_MASTER_QUIET_DONE_MS + 1;
  Bus200eMasterTask();  // -> DONE
  CHECK(Bus200eMasterBytesTransferred() == 500);
  Bus200eMasterReset();

  // next job never gets past SENDING (bus never quiets)
  fake_gate_open = 0;
  CHECK(Bus200eMasterBackup(0x28, 0) == 0);
  CHECK(Bus200eMasterBytesTransferred() == 0);  // cleared at start_job(), not 500
  fake_now += BUS200E_MASTER_SEND_TIMEOUT_MS + 1;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_FAILED);
  CHECK(Bus200eMasterLastError() == BUS200E_MASTER_ERR_SEND_TIMEOUT);
  CHECK(Bus200eMasterBytesTransferred() == 0);
}

static void test_restore_uses_restore_opcode(void) {
  printf("test_restore_uses_restore_opcode\n");
  reset();
  CHECK(Bus200eMasterRestore(0x3C, 0x00) == 0);
  CHECK(Bus200eMasterIsRestore() == 1);
  Bus200eMasterTask();
  const uint8_t want[] = { 0x07, 0x00, 0x22, 0x05, 0x3C, 0x00, 0x00, 0x00 };
  CHECK(memcmp(last_sent, want, 8) == 0);
}

static void test_busy_rejects_second_job(void) {
  printf("test_busy_rejects_second_job\n");
  reset();
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);
  CHECK(Bus200eMasterBackup(0x44, 1) == -BUS200E_MASTER_ERR_BUSY);
  CHECK(Bus200eMasterRestore(0x44, 1) == -BUS200E_MASTER_ERR_BUSY);
  // the original job's target is untouched
  CHECK(Bus200eMasterModAddr() == 0x3C && Bus200eMasterCardAddr() == 0);

  Bus200eMasterTask();  // sends; now WAIT_ACTIVITY -- still busy
  CHECK(Bus200eMasterBackup(0x44, 1) == -BUS200E_MASTER_ERR_BUSY);
}

static void test_gate_closed_retries(void) {
  printf("test_gate_closed_retries\n");
  reset();
  fake_gate_open = 0;
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);
  CHECK(send_calls == 0);

  fake_now += 500;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);  // still closed
  CHECK(send_calls == 0);

  fake_gate_open = 1;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);
  CHECK(send_calls == 1);
}

static void test_send_retries_then_succeeds(void) {
  printf("test_send_retries_then_succeeds\n");
  reset();
  send_fail_countdown = 2;   // NAK/arb-loss twice, then a clean send
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);

  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);  // 1st attempt failed
  CHECK(send_calls == 1);
  CHECK(suppress_calls == 0);

  fake_now += 10;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);  // 2nd attempt failed
  CHECK(send_calls == 2);

  fake_now += 10;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);  // 3rd: ok
  CHECK(send_calls == 3);
  CHECK(suppress_calls == 1);
}

static void test_send_timeout(void) {
  printf("test_send_timeout\n");
  reset();
  fake_gate_open = 0;   // bus never goes quiet
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);

  fake_now += BUS200E_MASTER_SEND_TIMEOUT_MS - 1;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);

  fake_now += 2;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_FAILED);
  CHECK(Bus200eMasterLastError() == BUS200E_MASTER_ERR_SEND_TIMEOUT);
  CHECK(send_calls == 0);
}

static void test_no_response_timeout(void) {
  printf("test_no_response_timeout\n");
  reset();
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);
  Bus200eMasterTask();  // sent -> WAIT_ACTIVITY
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);

  fake_now += BUS200E_MASTER_ACTIVITY_TIMEOUT_MS - 1;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_WAIT_ACTIVITY);

  fake_now += 2;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_FAILED);
  CHECK(Bus200eMasterLastError() == BUS200E_MASTER_ERR_NO_RESPONSE);
}

static void test_hard_cap_forces_done(void) {
  printf("test_hard_cap_forces_done\n");
  reset();
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);
  Bus200eMasterTask();  // -> WAIT_ACTIVITY
  fake_activity = 1;
  Bus200eMasterTask();  // -> TRANSFERRING
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_TRANSFERRING);

  // activity keeps moving just under the quiet threshold every tick, so it
  // never naturally goes quiet -- the hard cap must still end the job
  for (int i = 0; i < 40; ++i) {
    fake_now += (BUS200E_MASTER_QUIET_DONE_MS - 50);
    fake_activity += 1;
    Bus200eMasterTask();
    if (Bus200eMasterGetState() != BUS200E_MASTER_TRANSFERRING) break;
  }
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_DONE);
  CHECK(Bus200eMasterLastError() == BUS200E_MASTER_ERR_NONE);
}

static void test_reset_only_from_terminal_states(void) {
  printf("test_reset_only_from_terminal_states\n");
  reset();
  Bus200eMasterReset();  // IDLE -> IDLE, harmless
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_IDLE);

  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);
  Bus200eMasterReset();  // busy: no-op
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);

  fake_gate_open = 0;
  fake_now += BUS200E_MASTER_SEND_TIMEOUT_MS + 1;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_FAILED);
  Bus200eMasterReset();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_IDLE);

  // a fresh job is accepted straight from a terminal state too, no Reset()
  // call required first
  reset();
  CHECK(Bus200eMasterBackup(0x3C, 0) == 0);
  fake_gate_open = 0;
  fake_now += BUS200E_MASTER_SEND_TIMEOUT_MS + 1;
  Bus200eMasterTask();
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_FAILED);
  CHECK(Bus200eMasterBackup(0x44, 1) == 0);
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);
  CHECK(Bus200eMasterModAddr() == 0x44 && Bus200eMasterCardAddr() == 1);
}

// ---- QUERY (the sibling FSM) ------------------------------------------------

// Convenience: the exact bytes a QUERY at `addr` must put on the wire.
static void check_query_frame_sent(uint8_t addr) {
  const uint8_t want[] = { 0x04, addr, 0x22, 0x1A, 0xFF };
  CHECK(last_sent_len == BUS200E_QUERY_FRAME_LEN);
  CHECK(memcmp(last_sent, want, sizeof(want)) == 0);
  CHECK(last_suppressed_len == BUS200E_QUERY_FRAME_LEN);
  CHECK(memcmp(last_suppressed, want, sizeof(want)) == 0);
}

static void test_query_happy_path(void) {
  printf("test_query_happy_path\n");
  reset();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_IDLE);
  uint8_t v[BUS200E_QUERY_VER_MAX];
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 0);  // nothing asked yet

  CHECK(Bus200eMasterQuery(0x28) == 0);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);
  CHECK(Bus200eMasterQueryModAddr() == 0x28);

  Bus200eMasterQueryTask();  // gate open: sends immediately
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);
  CHECK(send_calls == 1 && suppress_calls == 1);
  check_query_frame_sent(0x28);

  // nothing back yet, still inside the window
  fake_now += BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS - 1;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);

  // the module answers (arrives via the slave RX path, not a poll)
  Bus200eMasterQueryReply(0x28, (const uint8_t *) "251e   ", 7);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_DONE);
  CHECK(Bus200eMasterQueryLastError() == BUS200E_MASTER_ERR_NONE);
  CHECK(Bus200eMasterQueryStrayReplies() == 0);
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 7);
  CHECK(memcmp(v, "251e   ", 7) == 0);

  // the timeout must not fire after the answer landed
  fake_now += 10 * BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_DONE);
  CHECK(send_calls == 1);   // and nothing is re-sent

  // reading back into a short buffer truncates rather than overruns
  uint8_t small[3] = { 0, 0, 0 };
  CHECK(Bus200eMasterQueryVersion(small, sizeof(small)) == 3);
  CHECK(memcmp(small, "251", 3) == 0);
  CHECK(Bus200eMasterQueryVersion(small, 0) == 0);
  CHECK(Bus200eMasterQueryVersion(nullptr, sizeof(small)) == 0);

  Bus200eMasterQueryReset();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_IDLE);
}

static void test_query_bad_args(void) {
  printf("test_query_bad_args\n");
  reset();
  // address 0 is the broadcast destination: every module would answer at once
  CHECK(Bus200eMasterQuery(0x00) == -BUS200E_MASTER_ERR_BAD_ARGS);
  CHECK(Bus200eMasterQuery(0x80) == -BUS200E_MASTER_ERR_BAD_ARGS);  // masks to 0
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_IDLE);

  // a QUERY needs only four of the six ops -- an ops table with no
  // probe_card/card_activity (useless to a query) must still be accepted
  Bus200eMasterOps no_card = fake_ops;
  no_card.probe_card = nullptr;
  no_card.card_activity = nullptr;
  Bus200eMasterInit(&no_card);
  CHECK(Bus200eMasterQuery(0x28) == 0);
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);
  // ...while a BACKUP through that same table is still refused
  CHECK(Bus200eMasterBackup(0x28, 0) == -BUS200E_MASTER_ERR_BAD_ARGS);

  // but one missing an op a query DOES use is refused
  Bus200eMasterOps no_send = fake_ops;
  no_send.send_frame = nullptr;
  Bus200eMasterInit(&no_send);
  CHECK(Bus200eMasterQuery(0x28) == -BUS200E_MASTER_ERR_BAD_ARGS);
  Bus200eMasterInit(nullptr);
  CHECK(Bus200eMasterQuery(0x28) == -BUS200E_MASTER_ERR_BAD_ARGS);
}

static void test_query_busy_rejects_second(void) {
  printf("test_query_busy_rejects_second\n");
  reset();
  CHECK(Bus200eMasterQuery(0x28) == 0);
  CHECK(Bus200eMasterQuery(0x29) == -BUS200E_MASTER_ERR_BUSY);
  CHECK(Bus200eMasterQueryModAddr() == 0x28);   // original target untouched
  Bus200eMasterQueryTask();                     // -> WAITING, still busy
  CHECK(Bus200eMasterQuery(0x29) == -BUS200E_MASTER_ERR_BUSY);

  // a query in flight does NOT block a BACKUP: independent FSMs
  CHECK(Bus200eMasterBackup(0x29, 0) == 0);
  CHECK(Bus200eMasterGetState() == BUS200E_MASTER_SENDING);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);
}

static void test_query_gate_closed_then_send_timeout(void) {
  printf("test_query_gate_closed_then_send_timeout\n");
  reset();
  fake_gate_open = 0;
  CHECK(Bus200eMasterQuery(0x28) == 0);
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);
  CHECK(send_calls == 0);

  fake_now += BUS200E_MASTER_QUERY_SEND_TIMEOUT_MS - 1;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);

  fake_now += 2;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_FAILED);
  CHECK(Bus200eMasterQueryLastError() == BUS200E_MASTER_ERR_SEND_TIMEOUT);
  CHECK(send_calls == 0);

  // a gate that opens in time instead lets the send through
  reset();
  fake_gate_open = 0;
  CHECK(Bus200eMasterQuery(0x28) == 0);
  Bus200eMasterQueryTask();
  CHECK(send_calls == 0);
  fake_gate_open = 1;
  fake_now += 500;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);
  CHECK(send_calls == 1);
}

static void test_query_send_retries(void) {
  printf("test_query_send_retries\n");
  reset();
  send_fail_countdown = 2;   // NAK/arb-loss twice, then a clean send
  CHECK(Bus200eMasterQuery(0x28) == 0);

  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);
  CHECK(send_calls == 1 && suppress_calls == 0);   // nothing to suppress yet

  fake_now += 10;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);
  CHECK(send_calls == 2 && suppress_calls == 0);

  fake_now += 10;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);
  CHECK(send_calls == 3 && suppress_calls == 1);
}

static void test_query_no_response(void) {
  printf("test_query_no_response\n");
  reset();
  CHECK(Bus200eMasterQuery(0x28) == 0);
  // burn most of the send window before the bus goes quiet: the reply window
  // is measured from the SEND, not from the request, so the module still
  // gets its full turnaround time
  fake_gate_open = 0;
  fake_now += BUS200E_MASTER_QUERY_SEND_TIMEOUT_MS - 100;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);
  fake_gate_open = 1;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);

  fake_now += BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS - 1;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);

  fake_now += 2;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_FAILED);
  CHECK(Bus200eMasterQueryLastError() == BUS200E_MASTER_ERR_NO_RESPONSE);

  // a reply that shows up after the timeout is not retro-fitted onto the
  // failed query
  Bus200eMasterQueryReply(0x28, (const uint8_t *) "251e", 4);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_FAILED);
  uint8_t v[BUS200E_QUERY_VER_MAX];
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 0);
}

static void test_query_stray_replies(void) {
  printf("test_query_stray_replies\n");
  reset();

  // a reply with nobody asking is ignored outright
  Bus200eMasterQueryReply(0x44, (const uint8_t *) "whoami", 6);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_IDLE);
  CHECK(Bus200eMasterQueryStrayReplies() == 0);

  CHECK(Bus200eMasterQuery(0x28) == 0);
  Bus200eMasterQueryTask();  // -> WAITING

  // another module's exchange (a live WPM enumerating the bus) must NOT be
  // reported as the answer to OUR question
  Bus200eMasterQueryReply(0x44, (const uint8_t *) "259e   ", 7);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_WAITING);
  CHECK(Bus200eMasterQueryStrayReplies() == 1);
  Bus200eMasterQueryReply(0x10, (const uint8_t *) "225e   ", 7);
  CHECK(Bus200eMasterQueryStrayReplies() == 2);

  // the real one still lands
  Bus200eMasterQueryReply(0x28, (const uint8_t *) "251e   ", 7);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_DONE);
  uint8_t v[BUS200E_QUERY_VER_MAX];
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 7);
  CHECK(memcmp(v, "251e   ", 7) == 0);
  CHECK(Bus200eMasterQueryStrayReplies() == 2);
}

// A query that fails must not read back the PREVIOUS query's version string
// -- the same stale-result trap Bus200eMasterBytesTransferred() guards
// against for BACKUP. Getting this wrong at the bench would mean confidently
// mis-identifying the module at an address.
static void test_query_no_stale_answer(void) {
  printf("test_query_no_stale_answer\n");
  reset();
  uint8_t v[BUS200E_QUERY_VER_MAX];

  CHECK(Bus200eMasterQuery(0x28) == 0);
  Bus200eMasterQueryTask();
  Bus200eMasterQueryReply(0x28, (const uint8_t *) "251e   ", 7);
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 7);
  Bus200eMasterQueryReset();
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 7);  // still readable

  // next query, at a DIFFERENT address, times out
  CHECK(Bus200eMasterQuery(0x29) == 0);
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 0);  // cleared immediately
  Bus200eMasterQueryTask();
  fake_now += BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS + 1;
  Bus200eMasterQueryTask();
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_FAILED);
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 0);  // not "251e   "

  // a fresh query straight from a terminal state is accepted, no Reset first
  CHECK(Bus200eMasterQuery(0x2A) == 0);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_SENDING);
  CHECK(Bus200eMasterQueryModAddr() == 0x2A);

  // an over-long reply is clipped to the buffer, never overrun
  Bus200eMasterQueryTask();
  const uint8_t huge[32] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J' };
  Bus200eMasterQueryReply(0x2A, huge, sizeof(huge));
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == BUS200E_QUERY_VER_MAX);
  CHECK(memcmp(v, "ABCDEFGH", BUS200E_QUERY_VER_MAX) == 0);

  // and a NULL/zero-length reply is a valid (empty) answer, not a crash
  Bus200eMasterQueryReset();
  CHECK(Bus200eMasterQuery(0x2B) == 0);
  Bus200eMasterQueryTask();
  Bus200eMasterQueryReply(0x2B, nullptr, 4);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_DONE);
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 0);
}

// Init must clear the query FSM too, or a re-Init mid-query would leave a
// stale WAITING state that never times out against the new clock.
static void test_query_init_clears_state(void) {
  printf("test_query_init_clears_state\n");
  reset();
  CHECK(Bus200eMasterQuery(0x28) == 0);
  Bus200eMasterQueryTask();
  Bus200eMasterQueryReply(0x28, (const uint8_t *) "251e", 4);
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_DONE);

  reset();   // Bus200eMasterInit()
  CHECK(Bus200eMasterQueryGetState() == BUS200E_QUERY_IDLE);
  CHECK(Bus200eMasterQueryModAddr() == 0);
  CHECK(Bus200eMasterQueryStrayReplies() == 0);
  uint8_t v[BUS200E_QUERY_VER_MAX];
  CHECK(Bus200eMasterQueryVersion(v, sizeof(v)) == 0);
}

int main() {
  test_find_free_card();
  test_bad_ops_rejected();
  test_backup_happy_path();
  test_xfer_done_shortens_quiet();
  test_bytes_transferred_no_cross_job_leakage();
  test_bytes_transferred_zero_on_early_failure();
  test_restore_uses_restore_opcode();
  test_busy_rejects_second_job();
  test_gate_closed_retries();
  test_send_retries_then_succeeds();
  test_send_timeout();
  test_no_response_timeout();
  test_hard_cap_forces_done();
  test_reset_only_from_terminal_states();

  test_query_happy_path();
  test_query_bad_args();
  test_query_busy_rejects_second();
  test_query_gate_closed_then_send_timeout();
  test_query_send_retries();
  test_query_no_response();
  test_query_stray_replies();
  test_query_no_stale_answer();
  test_query_init_clears_state();

  printf("\ntest_bus200e_master: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
