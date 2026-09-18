// Host tests for the 200e preset-bus engine (src/PresetBus200e.cpp): both
// command framings, remote-enable gating, preset-range gating, QUERY pending,
// frame hygiene and the chunked card jobs against fake ops.
// Vectors ported from the MARF project's test_bus200e.c (framing labels
// corrected: LONG = PRIMO, SHORT = V2/pre-PRIMO per the 2WIRELESS source).
// Standalone (no gtest): g++ -std=c++17 -Wall -Werror -O2 (one line:)
//   -o build/test_bus200e test_bus200e.cpp ../src/PresetBus200e.cpp && ./build/test_bus200e
#include <cassert>
#include <cstdio>
#include <cstring>

#include "../src/PresetBus200e.h"

static int checks = 0, fails = 0;
#define CHECK(cond) do { \
    checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } \
  } while (0)

// ---- fake ops --------------------------------------------------------------

static constexpr uint32_t kRecSize = 40;

static uint8_t save_calls[64], recall_calls[64];
static int n_save, n_recall;

static struct { uint8_t card7; uint32_t off; uint32_t len; } cw_calls[40];
static int n_cw;
static struct { uint8_t slot; } sw_calls[40];
static int n_sw;
static int n_sr, n_cr;
static int fail_card_write;      // fail the n-th card_write (1-based; 0 = never)
static int reject_odd_writes;    // slot_write rejects odd slots (returns 1)

static void f_save(uint8_t slot) { save_calls[n_save++] = slot; }
static void f_recall(uint8_t slot) { recall_calls[n_recall++] = slot; }

static int f_slot_read(uint8_t slot, uint8_t *out, uint32_t cap) {
  if (cap < kRecSize) return -1;
  memset(out, 0, kRecSize);
  out[0] = slot;
  n_sr++;
  return 0;
}

static int f_slot_write(uint8_t slot, const uint8_t *in, uint32_t n) {
  (void) in; (void) n;
  if (reject_odd_writes && (slot & 1)) return 1;   // rejected, keep going
  sw_calls[n_sw++].slot = slot;
  return 0;
}

static int f_card_write(uint8_t card7, uint32_t off, const uint8_t *d, uint32_t n) {
  (void) d;
  if (fail_card_write && n_cw + 1 == fail_card_write) return -1;
  cw_calls[n_cw].card7 = card7;
  cw_calls[n_cw].off = off;
  cw_calls[n_cw].len = n;
  n_cw++;
  return 0;
}

static int f_card_read(uint8_t card7, uint32_t off, uint8_t *d, uint32_t n) {
  (void) card7; (void) off;
  memset(d, 0, n);
  n_cr++;
  return 0;
}

static int n_midi = 0;
static uint8_t midi_last[3];
static void f_midi(uint8_t status, uint8_t d1, uint8_t d2) {
  midi_last[0] = status; midi_last[1] = d1; midi_last[2] = d2;
  n_midi++;
}

static int n_qreply = 0;
static uint8_t qreply_from;
static uint8_t qreply_ver[16];
static uint8_t qreply_len;
static void f_query_reply(uint8_t from_addr, const uint8_t *ver, uint8_t n) {
  qreply_from = from_addr;
  qreply_len = n;
  if (qreply_len > sizeof(qreply_ver)) qreply_len = sizeof(qreply_ver);
  memcpy(qreply_ver, ver, qreply_len);
  n_qreply++;
}

static int n_xdone = 0;
static uint8_t xdone_from;
static void f_xfer_done(uint8_t from_addr) { xdone_from = from_addr; n_xdone++; }

static int n_lack = 0;
static uint8_t lack_from;
static void f_load_ack(uint8_t from_addr) { lack_from = from_addr; n_lack++; }

static const Bus200eOps fake_ops = {
  f_save, f_recall, kRecSize, f_slot_read, f_slot_write, f_card_write, f_card_read,
  f_midi, f_query_reply, f_xfer_done, f_load_ack,
};

static void reset(const Bus200eOps *ops) {
  Bus200eInit(ops);
  Bus200eSetModuleAddress(BUS200E_DEFAULT_MODULE_ADDR);
  n_save = n_recall = n_cw = n_sw = n_sr = n_cr = n_midi = 0;
  n_qreply = 0;
  qreply_from = 0;
  qreply_len = 0;
  n_xdone = 0;
  xdone_from = 0;
  n_lack = 0;
  lack_from = 0;
  memset(qreply_ver, 0, sizeof(qreply_ver));
  fail_card_write = 0;
  reject_odd_writes = 0;
}

// Feed one general-call frame: START, payload bytes, STOP.
static void frame(const uint8_t *bytes, int n) {
  Bus200eFeedEvent(BUS200E_EV_START);
  for (int i = 0; i < n; i++) Bus200eFeedEvent(bytes[i]);
  Bus200eFeedEvent(BUS200E_EV_STOP);
}
#define FRAME(...) do { \
    const uint8_t _f[] = { __VA_ARGS__ }; \
    frame(_f, (int) sizeof(_f)); \
  } while (0)

static int last_op(void) {
  Bus200eCmd c;
  return Bus200eLogRead(0, &c) ? c.op : BUS200E_OP_NONE;
}

// ============================================================================

static void test_short_recall_save(void) {   // V2 / pre-PRIMO framing
  printf("test_short_recall_save\n");
  reset(&fake_ops);
  FRAME(0x00, 5);
  CHECK(n_recall == 1 && recall_calls[0] == 5);
  CHECK(last_op() == BUS200E_OP_RECALL);
  FRAME(0x01, 3);
  CHECK(n_save == 1 && save_calls[0] == 3);
  CHECK(last_op() == BUS200E_OP_SAVE);
  CHECK(Bus200eLogTotal() == 2);
}

static void test_long_recall_save(void) {    // PRIMO framing
  printf("test_long_recall_save\n");
  reset(&fake_ops);
  FRAME(0x04, 0x00, 0x22, 0x01, 7);
  CHECK(n_recall == 1 && recall_calls[0] == 7);
  FRAME(0x04, 0x00, 0x22, 0x02, 2);
  CHECK(n_save == 1 && save_calls[0] == 2);
}

static void test_remote_enable_gating(void) {
  printf("test_remote_enable_gating\n");
  reset(&fake_ops);
  CHECK(Bus200eRemoteEnabled() == BUS200E_REMOTE_DEFAULT);

  FRAME(0x15);                        // short remote disable
  CHECK(!Bus200eRemoteEnabled());
  FRAME(0x00, 4);
  CHECK(n_recall == 0);               // gated...
  CHECK(last_op() == BUS200E_OP_RECALL);  // ...but still logged

  FRAME(0x14);                        // short remote enable
  CHECK(Bus200eRemoteEnabled());
  FRAME(0x00, 4);
  CHECK(n_recall == 1 && recall_calls[0] == 4);

  FRAME(0x04, 0x00, 0x22, 0x17, 0xFF);  // long disable (trailing 0xFF)
  CHECK(!Bus200eRemoteEnabled());
  FRAME(0x04, 0x00, 0x22, 0x16, 0xFF);  // long enable
  CHECK(Bus200eRemoteEnabled());
  FRAME(0x04, 0x00, 0x22, 0x14, 0xFF);  // polling complete: logged, no-op
  CHECK(last_op() == BUS200E_OP_POLL_DONE);
  CHECK(Bus200eRemoteEnabled());
}

static void test_preset_range_gating(void) {
  printf("test_preset_range_gating\n");
  reset(&fake_ops);
  FRAME(0x00, BUS200E_BUS_PRESETS);         // 30: past the bus preset space
  FRAME(0x00, 31);
  CHECK(n_recall == 0);
  CHECK(Bus200eLogTotal() == 2);            // both logged all the same
  FRAME(0x00, BUS200E_BUS_PRESETS - 1);     // 29: last valid
  CHECK(n_recall == 1 && recall_calls[0] == BUS200E_BUS_PRESETS - 1);
  FRAME(0x04, 0x00, 0x22, 0x01, 16);        // long framing, upper half of space
  CHECK(n_recall == 2 && recall_calls[1] == 16);
}

static void test_query_pending(void) {
  printf("test_query_pending\n");
  reset(&fake_ops);
  FRAME(0x04, 0x44, 0x22, 0x1A, 0xFF);      // query some other module
  CHECK(last_op() == BUS200E_OP_QUERY);
  CHECK(!Bus200eQueryPending());
  FRAME(0x04, BUS200E_DEFAULT_MODULE_ADDR, 0x22, 0x1A, 0xFF);  // query us
  CHECK(Bus200eQueryPending());
  Bus200eClearQueryPending();
  CHECK(!Bus200eQueryPending());
  Bus200eSetModuleAddress(0x51);            // runtime address change
  FRAME(0x04, 0x51, 0x22, 0x1A, 0xFF);
  CHECK(Bus200eQueryPending());
  Bus200eClearQueryPending();
  FRAME(0x04, BUS200E_DEFAULT_MODULE_ADDR, 0x22, 0x1A, 0xFF);  // old addr: no
  CHECK(!Bus200eQueryPending());
}

static void test_null_ops_logs_only(void) {
  printf("test_null_ops_logs_only\n");
  reset(NULL);                              // the RX-log-only configuration
  FRAME(0x00, 1);
  FRAME(0x01, 2);
  FRAME(0x2D, BUS200E_DEFAULT_MODULE_ADDR, 0x00, 0x00, 0x00);
  CHECK(Bus200eLogTotal() == 3);
  CHECK(Bus200eJobActive());                // job accepted...
  Bus200eTask();
  CHECK(!Bus200eJobActive());               // ...and quietly dropped: no ops
}

static void test_backup_job(void) {
  printf("test_backup_job\n");
  reset(&fake_ops);
  FRAME(0x2D, BUS200E_DEFAULT_MODULE_ADDR, 0x10, 0x00, 0x02);  // mem 0x0010, card 2
  CHECK(Bus200eJobActive());
  for (int i = 0; i < BUS200E_BUS_PRESETS; i++) Bus200eTask();
  CHECK(!Bus200eJobActive());
  CHECK(n_cw == BUS200E_BUS_PRESETS);
  CHECK(cw_calls[0].card7 == (BUS200E_CARD_BASE | 0x02));
  CHECK(cw_calls[0].off == 0x0010);
  CHECK(cw_calls[1].off == 0x0010 + kRecSize);
  CHECK(cw_calls[0].len == kRecSize);
  Bus200eTask();                            // idle task call is a no-op
  CHECK(n_cw == BUS200E_BUS_PRESETS);
}

static void test_backup_other_module_ignored(void) {
  printf("test_backup_other_module_ignored\n");
  reset(&fake_ops);
  FRAME(0x2D, 0x44, 0x00, 0x00, 0x00);      // a 291e's backup, not ours
  CHECK(!Bus200eJobActive());
  CHECK(last_op() == BUS200E_OP_BACKUP);    // observed in the log though
}

static void test_long_backup_args(void) {
  printf("test_long_backup_args\n");
  reset(&fake_ops);
  // long framing: [n, 0x00, 0x22, 0x04, modAddr, cardLo, memLSB, memMSB]
  FRAME(0x07, 0x00, 0x22, 0x04, BUS200E_DEFAULT_MODULE_ADDR, 0x00, 0x34, 0x12);
  CHECK(Bus200eJobActive());
  Bus200eTask();
  CHECK(n_cw == 1 && cw_calls[0].off == 0x1234);
  CHECK(cw_calls[0].card7 == BUS200E_CARD_BASE);
}

static void test_backup_aborts_on_error(void) {
  printf("test_backup_aborts_on_error\n");
  reset(&fake_ops);
  fail_card_write = 3;
  FRAME(0x2D, BUS200E_DEFAULT_MODULE_ADDR, 0x00, 0x00, 0x00);
  for (int i = 0; i < BUS200E_BUS_PRESETS; i++) Bus200eTask();
  CHECK(!Bus200eJobActive());
  CHECK(n_cw == 2);                         // two good writes, then abort
  CHECK(Bus200eGetStats()->job_errors == 1);
}

static void test_restore_rejects_skip(void) {
  printf("test_restore_rejects_skip\n");
  reset(&fake_ops);
  reject_odd_writes = 1;
  FRAME(0x2E, BUS200E_DEFAULT_MODULE_ADDR, 0x00, 0x00, 0x00);
  CHECK(Bus200eJobActive());
  for (int i = 0; i < BUS200E_BUS_PRESETS; i++) Bus200eTask();
  CHECK(!Bus200eJobActive());
  CHECK(n_cr == BUS200E_BUS_PRESETS);
  CHECK(n_sw == BUS200E_BUS_PRESETS / 2);   // only the even, accepted records
  CHECK(sw_calls[0].slot == 0 && sw_calls[1].slot == 2);
  CHECK(Bus200eGetStats()->restore_rejects == BUS200E_BUS_PRESETS / 2);
}

static void test_second_job_dropped_while_busy(void) {
  printf("test_second_job_dropped_while_busy\n");
  reset(&fake_ops);
  FRAME(0x2D, BUS200E_DEFAULT_MODULE_ADDR, 0x00, 0x00, 0x00);
  FRAME(0x2E, BUS200E_DEFAULT_MODULE_ADDR, 0x00, 0x00, 0x00);  // while busy
  CHECK(last_op() == BUS200E_OP_DROPPED);
  for (int i = 0; i < BUS200E_BUS_PRESETS; i++) Bus200eTask();
  CHECK(n_cw == BUS200E_BUS_PRESETS && n_sw == 0);   // backup ran, not restore
}

static void test_midi_and_clock_log_only(void) {
  printf("test_midi_and_clock_log_only\n");
  reset(&fake_ops);
  FRAME(0x90, 0x3C, 0x64);                  // note on
  CHECK(last_op() == BUS200E_OP_MIDI);
  FRAME(0xF8);                              // clock tick
  CHECK(last_op() == BUS200E_OP_CLOCK);
  CHECK(n_recall == 0 && n_save == 0 && !Bus200eJobActive());
}

static void test_frame_hygiene(void) {
  printf("test_frame_hygiene\n");
  reset(&fake_ops);

  // Repeated START drops the partial frame, keeps the fresh one.
  Bus200eFeedEvent(BUS200E_EV_START);
  Bus200eFeedEvent(0x00);
  Bus200eFeedEvent(BUS200E_EV_START);
  Bus200eFeedEvent(0x00);
  Bus200eFeedEvent(0x05);
  Bus200eFeedEvent(BUS200E_EV_STOP);
  CHECK(n_recall == 1 && recall_calls[0] == 5);
  CHECK(Bus200eGetStats()->dropped == 1);

  // Transport overflow poisons the frame.
  reset(&fake_ops);
  Bus200eFeedEvent(BUS200E_EV_START);
  Bus200eFeedEvent(0x00);
  Bus200eFeedEvent(BUS200E_EV_OVF);
  Bus200eFeedEvent(0x05);
  Bus200eFeedEvent(BUS200E_EV_STOP);
  CHECK(n_recall == 0);
  CHECK(Bus200eGetStats()->dropped == 1);

  // Overlong frame is dropped, not mis-parsed.
  reset(&fake_ops);
  Bus200eFeedEvent(BUS200E_EV_START);
  for (int i = 0; i < 40; i++) Bus200eFeedEvent(0x00);
  Bus200eFeedEvent(BUS200E_EV_STOP);
  CHECK(n_recall == 0);
  CHECK(Bus200eGetStats()->dropped == 1);

  // Bytes outside any frame, and empty frames, are ignored.
  reset(&fake_ops);
  Bus200eFeedEvent(0x00);
  Bus200eFeedEvent(0x05);
  Bus200eFeedEvent(BUS200E_EV_START);
  Bus200eFeedEvent(BUS200E_EV_STOP);
  CHECK(Bus200eLogTotal() == 0 && n_recall == 0);
}

static void test_unknown_commands_logged(void) {
  printf("test_unknown_commands_logged\n");
  reset(&fake_ops);
  FRAME(0x63, 0x01);                        // not a known short command
  CHECK(last_op() == BUS200E_OP_UNKNOWN);
  FRAME(0x04, 0x00, 0x22, 0x7E, 0xFF);      // unknown long subcommand
  CHECK(last_op() == BUS200E_OP_UNKNOWN);
  CHECK(n_recall == 0 && n_save == 0);
}

static void test_log_ring(void) {
  printf("test_log_ring\n");
  reset(&fake_ops);
  for (int i = 0; i < BUS200E_LOG_SIZE + 4; i++) FRAME(0xF8);
  CHECK(Bus200eLogTotal() == (uint32_t) (BUS200E_LOG_SIZE + 4));
  Bus200eCmd c;
  CHECK(Bus200eLogRead(0, &c) && c.op == BUS200E_OP_CLOCK);
  CHECK(Bus200eLogRead(BUS200E_LOG_SIZE - 1, &c));
  CHECK(!Bus200eLogRead(BUS200E_LOG_SIZE, &c));   // aged out of the ring
}

static void test_bus_midi(void) {
  reset(&fake_ops);

  // long/PRIMO note-on to 200e bus A: [08][00][22][0F][98][00][3C][64][00]
  FRAME(0x08, 0x00, 0x22, 0x0F, 0x98, 0x00, 0x3C, 0x64, 0x00);
  CHECK(n_midi == 1);
  CHECK(midi_last[0] == 0x98 && midi_last[1] == 0x3C && midi_last[2] == 0x64);
  CHECK(last_op() == BUS200E_OP_MIDI);

  // long realtime clock: status >= 0xF8 classifies as CLOCK, hook still fires
  FRAME(0x08, 0x00, 0x22, 0x0F, 0xF8, 0x00, 0x00, 0x00, 0x00);
  CHECK(n_midi == 2);
  CHECK(midi_last[0] == 0xF8);
  CHECK(last_op() == BUS200E_OP_CLOCK);

  // short/V2 status-first CC to bus B
  FRAME(0xB4, 0x1F, 0x32);
  CHECK(n_midi == 3);
  CHECK(midi_last[0] == 0xB4 && midi_last[1] == 0x1F && midi_last[2] == 0x32);
  CHECK(last_op() == BUS200E_OP_MIDI);

  // truncated long MIDI frame -> UNKNOWN, no hook call
  FRAME(0x05, 0x00, 0x22, 0x0F, 0x98, 0x00);
  CHECK(n_midi == 3);

  // self-echo suppression: registered frame dropped exactly once
  reset(&fake_ops);
  const uint8_t echo[] = { 0x04, 0x00, 0x22, 0x01, 0x03 };
  Bus200eSuppressFrame(echo, sizeof(echo));
  FRAME(0x04, 0x00, 0x22, 0x01, 0x03);   // our own echo: swallowed
  CHECK(n_recall == 0);
  FRAME(0x04, 0x00, 0x22, 0x01, 0x03);   // a real frame with the same bytes
  CHECK(n_recall == 1);

  // a different frame does NOT clear someone else's suppression... it does
  // by design (arbitration winner processed, ours already gone). Register,
  // let a different frame through, then confirm ours is no longer eaten.
  Bus200eSuppressFrame(echo, sizeof(echo));
  FRAME(0x04, 0x00, 0x22, 0x02, 0x05);   // different frame: processed
  CHECK(n_save == 1);

  // expired suppression never eats a genuine identical frame
  reset(&fake_ops);
  Bus200eSetNow(1000);
  Bus200eSuppressFrame(echo, sizeof(echo));
  Bus200eSetNow(1100);                   // 100ms later: expired
  FRAME(0x04, 0x00, 0x22, 0x01, 0x03);
  CHECK(n_recall == 1);

  // card ops for ANY module stamp the transfer clock
  reset(&fake_ops);
  Bus200eSetNow(1234);
  FRAME(0x07, 0x00, 0x22, 0x04, 0x66, 0x00, 0x00, 0x00);  // foreign module
  CHECK(Bus200eLastTransferMs() != 0);

  // hookless init still parses/logs without crashing
  Bus200eOps no_midi = fake_ops;
  no_midi.midi_rx = 0;
  reset(&no_midi);
  FRAME(0x08, 0x00, 0x22, 0x0F, 0x98, 0x00, 0x3C, 0x64, 0x00);
  CHECK(n_midi == 0);
  CHECK(last_op() == BUS200E_OP_MIDI);
}

static void test_build_transfer_frame(void) {
  printf("test_build_transfer_frame\n");
  uint8_t f[BUS200E_XFER_FRAME_LEN];

  // bad op
  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_RECALL, 0x3C, 0, 0, f, sizeof(f)) == -1);
  // undersized buffer
  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_BACKUP, 0x3C, 0, 0, f, BUS200E_XFER_FRAME_LEN - 1) == -1);

  // exact byte shape
  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_BACKUP, 0x3C, 0x02, 0x1234, f, sizeof(f))
        == BUS200E_XFER_FRAME_LEN);
  const uint8_t want_backup[] = { 0x07, 0x00, 0x22, 0x04, 0x3C, 0x02, 0x34, 0x12 };
  CHECK(memcmp(f, want_backup, sizeof(want_backup)) == 0);

  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_RESTORE, 0x3C, 0x02, 0x1234, f, sizeof(f))
        == BUS200E_XFER_FRAME_LEN);
  const uint8_t want_restore[] = { 0x07, 0x00, 0x22, 0x05, 0x3C, 0x02, 0x34, 0x12 };
  CHECK(memcmp(f, want_restore, sizeof(want_restore)) == 0);

  // args get masked to 7 bits, same as every other payload field in this parser
  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_BACKUP, 0xFF, 0xFF, 0xBEEF, f, sizeof(f))
        == BUS200E_XFER_FRAME_LEN);
  CHECK(f[4] == 0x7F && f[5] == 0x7F && f[6] == 0xEF && f[7] == 0xBE);
}

static void test_build_transfer_frame_round_trips_through_parser(void) {
  printf("test_build_transfer_frame_round_trips_through_parser\n");
  reset(&fake_ops);
  uint8_t f[BUS200E_XFER_FRAME_LEN];

  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_BACKUP, BUS200E_DEFAULT_MODULE_ADDR,
                                   0x03, 0x0100, f, sizeof(f)) == BUS200E_XFER_FRAME_LEN);
  frame(f, sizeof(f));
  CHECK(last_op() == BUS200E_OP_BACKUP);
  CHECK(Bus200eJobActive());
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c));
    CHECK(c.mod_addr == BUS200E_DEFAULT_MODULE_ADDR);
    CHECK(c.card_lo == 0x03);
    CHECK(c.mem_off == 0x0100);
  }
  for (int i = 0; i < BUS200E_BUS_PRESETS; i++) Bus200eTask();
  CHECK(!Bus200eJobActive());
  CHECK(n_cw == BUS200E_BUS_PRESETS);
  CHECK(cw_calls[0].card7 == (BUS200E_CARD_BASE | 0x03));
  CHECK(cw_calls[0].off == 0x0100);

  reset(&fake_ops);
  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_RESTORE, BUS200E_DEFAULT_MODULE_ADDR,
                                   0x00, 0x0000, f, sizeof(f)) == BUS200E_XFER_FRAME_LEN);
  frame(f, sizeof(f));
  CHECK(last_op() == BUS200E_OP_RESTORE);
  for (int i = 0; i < BUS200E_BUS_PRESETS; i++) Bus200eTask();
  CHECK(n_sw == BUS200E_BUS_PRESETS);

  // a backup addressed to a DIFFERENT module is observed but not acted on
  reset(&fake_ops);
  CHECK(Bus200eBuildTransferFrame(BUS200E_OP_BACKUP, 0x44, 0x00, 0, f, sizeof(f))
        == BUS200E_XFER_FRAME_LEN);
  frame(f, sizeof(f));
  CHECK(last_op() == BUS200E_OP_BACKUP);
  CHECK(!Bus200eJobActive());
}

static void test_build_query_frame(void) {
  printf("test_build_query_frame\n");
  uint8_t f[BUS200E_QUERY_FRAME_LEN];

  // undersized buffer
  CHECK(Bus200eBuildQueryFrame(0x28, f, BUS200E_QUERY_FRAME_LEN - 1) == -1);
  // destAddr 0 is the broadcast address: every module would answer at once
  CHECK(Bus200eBuildQueryFrame(0x00, f, sizeof(f)) == -1);
  CHECK(Bus200eBuildQueryFrame(0x80, f, sizeof(f)) == -1);  // masks to 0 too

  // exact byte shape: [0x04][modAddr][0x22][0x1A][0xFF]
  CHECK(Bus200eBuildQueryFrame(0x28, f, sizeof(f)) == BUS200E_QUERY_FRAME_LEN);
  const uint8_t want[] = { 0x04, 0x28, 0x22, 0x1A, 0xFF };
  CHECK(memcmp(f, want, sizeof(want)) == 0);

  // address masked to 7 bits, same as every other payload field
  CHECK(Bus200eBuildQueryFrame(0xC4, f, sizeof(f)) == BUS200E_QUERY_FRAME_LEN);
  CHECK(f[1] == 0x44);
}

static void test_build_query_frame_round_trips_through_parser(void) {
  printf("test_build_query_frame_round_trips_through_parser\n");
  reset(&fake_ops);
  uint8_t f[BUS200E_QUERY_FRAME_LEN];

  // a query aimed at somebody else: decoded, logged, not ours to answer
  CHECK(Bus200eBuildQueryFrame(0x28, f, sizeof(f)) == BUS200E_QUERY_FRAME_LEN);
  frame(f, sizeof(f));
  CHECK(last_op() == BUS200E_OP_QUERY);
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c) && c.mod_addr == 0x28);
  }
  CHECK(!Bus200eQueryPending());

  // the same builder aimed at US sets query_pending, exactly as the
  // hand-written vectors in test_query_pending() do
  CHECK(Bus200eBuildQueryFrame(BUS200E_DEFAULT_MODULE_ADDR, f, sizeof(f))
        == BUS200E_QUERY_FRAME_LEN);
  frame(f, sizeof(f));
  CHECK(last_op() == BUS200E_OP_QUERY);
  CHECK(Bus200eQueryPending());
}

static void test_query_reply_parse(void) {
  printf("test_query_reply_parse\n");
  reset(&fake_ops);

  // The legacy shape this firmware used to master before the reply command
  // byte was traced off real hardware (see the live vectors below): still
  // accepted so an older Xenomorpher on the bus is understood.
  // [0A][22][srcAddr][13][7 version chars]
  FRAME(0x0A, 0x22, 0x28, 0x13, '2', '5', '1', 'e', ' ', ' ', ' ');
  CHECK(last_op() == BUS200E_OP_QUERY_REPLY);
  CHECK(n_qreply == 1);
  CHECK(qreply_from == 0x28);
  CHECK(qreply_len == 7);
  CHECK(memcmp(qreply_ver, "251e   ", 7) == 0);
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c));
    CHECK(c.mod_addr == 0x28);
    CHECK(c.arg == 7);                     // version length
    CHECK(c.card_lo == '2');               // first three chars, field-reused
    CHECK(c.mem_off == (uint16_t)('5' | ('1' << 8)));
  }
  // a reply is PRIMO-dialect framing, counted as such
  CHECK(Bus200eGetStats()->frames_long == 1);
  CHECK(Bus200eGetStats()->frames_short == 0);

  // The vectors captured verbatim off a live 200e bus: a Buchla 251e at 0x5C
  // and a second module at 0x28, each answering [04 <addr> 22 1A FF]. Command
  // byte 0x1C, one payload byte, no version string.
  reset(&fake_ops);
  FRAME(0x04, 0x22, 0x5C, 0x1C, 0xFF);
  CHECK(last_op() == BUS200E_OP_QUERY_REPLY);
  CHECK(n_qreply == 1 && qreply_from == 0x5C && qreply_len == 1);
  CHECK(qreply_ver[0] == 0xFF);
  CHECK(Bus200eGetStats()->frames_long == 1);   // PRIMO dialect, not short/V2
  CHECK(Bus200eGetStats()->frames_short == 0);
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c));
    CHECK(c.mod_addr == 0x5C && c.arg == 1 && c.card_lo == 0xFF);
  }

  reset(&fake_ops);
  FRAME(0x04, 0x22, 0x28, 0x1C, 0xFF);
  CHECK(n_qreply == 1 && qreply_from == 0x28 && qreply_len == 1);

  // our own reply frame shape (module addr in src) parses identically --
  // this is what a second Xenomorpher on the bus would send
  reset(&fake_ops);
  FRAME(0x04, 0x22, BUS200E_DEFAULT_MODULE_ADDR, 0x1C, 0xFF);
  CHECK(n_qreply == 1 && qreply_from == BUS200E_DEFAULT_MODULE_ADDR);
  CHECK(qreply_len == 1 && qreply_ver[0] == 0xFF);
  // ...and hearing one must NOT arm our own reply machinery
  CHECK(!Bus200eQueryPending());

  // a minimal reply with no version bytes at all is still decoded
  reset(&fake_ops);
  FRAME(0x03, 0x22, 0x29, 0x13);
  CHECK(last_op() == BUS200E_OP_QUERY_REPLY);
  CHECK(n_qreply == 1 && qreply_from == 0x29 && qreply_len == 0);
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c) && c.arg == 0 && c.card_lo == 0 && c.mem_off == 0);
  }

  // the longest reply this parser can carry (FRAME_MAX): 8 version bytes
  reset(&fake_ops);
  FRAME(0x0B, 0x22, 0x28, 0x13, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H');
  CHECK(n_qreply == 1 && qreply_len == BUS200E_QUERY_VER_MAX);
  CHECK(memcmp(qreply_ver, "ABCDEFGH", 8) == 0);

  // hookless build: decoded and logged, nothing called
  Bus200eOps no_qr = fake_ops;
  no_qr.query_reply = 0;
  reset(&no_qr);
  FRAME(0x0A, 0x22, 0x28, 0x13, '2', '5', '1', 'e', ' ', ' ', ' ');
  CHECK(n_qreply == 0);
  CHECK(last_op() == BUS200E_OP_QUERY_REPLY);
}

// The reply branch is checked ahead of the long/PRIMO command branch, so it
// must not be able to swallow anything that used to parse as a command.
// The frame a Buchla 251e at 0x5C mastered right after the last byte of a
// BACKUP it had been asked for reached our card (bench, 2026-09-02):
// [04 22 5C 0A 5C]. Module -> manager form, command 0x0A, one payload byte.
static void test_xfer_done_parse(void) {
  printf("test_xfer_done_parse\n");
  reset(&fake_ops);
  FRAME(0x04, 0x22, 0x5C, 0x0A, 0x5C);
  CHECK(last_op() == BUS200E_OP_XFER_DONE);
  CHECK(n_xdone == 1 && xdone_from == 0x5C);
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c));
    CHECK(c.mod_addr == 0x5C);
    CHECK(c.arg == 0x5C);
  }
  CHECK(Bus200eGetStats()->frames_long == 1);

  // the manager's own BACKUP command carries 0x04 in the cmd column and
  // 0x22 in the SRC column: still a command, never mistaken for this
  FRAME(0x07, 0x00, 0x22, 0x04, 0x5C, 0x01, 0x00, 0x00);
  CHECK(last_op() == BUS200E_OP_BACKUP);
  CHECK(n_xdone == 1);

  // a 0x0A with the columns of a command (src 0x22) is not an announcement
  FRAME(0x04, 0x00, 0x22, 0x0A, 0x5C);
  CHECK(last_op() != BUS200E_OP_XFER_DONE);
  CHECK(n_xdone == 1);

  // the 259e's form of the same announcement: general-call destination
  // instead of the manager's 0x22 (bench 2026-09-02, after a 990-byte BACKUP)
  FRAME(0x04, 0x00, 0x28, 0x0A, 0x28);
  CHECK(last_op() == BUS200E_OP_XFER_DONE);
  CHECK(n_xdone == 2 && xdone_from == 0x28);

  // any other destination column stays undecoded
  FRAME(0x04, 0x50, 0x28, 0x0A, 0x28);
  CHECK(last_op() != BUS200E_OP_XFER_DONE);
  CHECK(n_xdone == 2);

  // log-only when the hook is absent
  Bus200eOps no_xd = fake_ops;
  no_xd.xfer_done = 0;
  reset(&no_xd);
  FRAME(0x04, 0x22, 0x28, 0x0A, 0x28);
  CHECK(last_op() == BUS200E_OP_XFER_DONE);
  CHECK(n_xdone == 0);
}

// The poll reply a module masters after it loads a preset: [04 22 addr 03 xx].
// Decoded from the 259e firmware (builder 0x9179, payload 0xFF) and confirmed
// live 2026-09-10, one frame per module per preset load -- a 259e at 0x28 and
// a 210e at 0x20 both answered a panel recall. The 251e's builder never writes
// the payload byte (it ships uninitialised stack, observed as 0x00), so the
// address is the only field worth trusting.
static void test_load_ack_parse(void) {
  printf("test_load_ack_parse\n");
  reset(&fake_ops);
  FRAME(0x04, 0x22, 0x28, 0x03, 0xFF);
  CHECK(last_op() == BUS200E_OP_LOAD_ACK);
  CHECK(n_lack == 1 && lack_from == 0x28);
  {
    Bus200eCmd c;
    CHECK(Bus200eLogRead(0, &c));
    CHECK(c.mod_addr == 0x28);
    CHECK(c.arg == 0xFF);   // diagnostic only: the 251e ships garbage here
  }
  CHECK(Bus200eGetStats()->frames_long == 1);

  // the 251e's form, with its uninitialised payload byte
  FRAME(0x04, 0x22, 0x5C, 0x03, 0x00);
  CHECK(last_op() == BUS200E_OP_LOAD_ACK);
  CHECK(n_lack == 2 && lack_from == 0x5C);

  // a command frame (src 0x22) stays a command, whatever the command byte
  FRAME(0x04, 0x00, 0x22, 0x03, 0x05);
  CHECK(last_op() != BUS200E_OP_LOAD_ACK);
  CHECK(n_lack == 2);

  // an ordinary broadcast RECALL is untouched by the new branch
  FRAME(0x04, 0x00, 0x22, 0x01, 9);
  CHECK(last_op() == BUS200E_OP_RECALL);
  CHECK(n_recall == 1 && recall_calls[0] == 9);
  CHECK(n_lack == 2);

  // not addressed to the manager: undecoded
  FRAME(0x04, 0x00, 0x28, 0x03, 0xFF);
  CHECK(last_op() != BUS200E_OP_LOAD_ACK);
  CHECK(n_lack == 2);

  // wrong length byte for the frame: undecoded
  FRAME(0x05, 0x22, 0x28, 0x03, 0xFF);
  CHECK(last_op() != BUS200E_OP_LOAD_ACK);
  CHECK(n_lack == 2);

  // log-only when the hook is absent
  Bus200eOps no_la = fake_ops;
  no_la.load_ack = 0;
  reset(&no_la);
  FRAME(0x04, 0x22, 0x20, 0x03, 0xFF);
  CHECK(last_op() == BUS200E_OP_LOAD_ACK);
  CHECK(n_lack == 0);
}

static void test_query_reply_does_not_shadow_commands(void) {
  printf("test_query_reply_does_not_shadow_commands\n");
  reset(&fake_ops);

  // a command frame addressed TO the manager (destAddr 0x22, srcAddr 0x22):
  // srcAddr 0x22 keeps it on the command path, where 0x13/0x1C are just
  // unknown commands
  FRAME(0x04, 0x22, 0x22, 0x13, 0xFF);
  CHECK(last_op() == BUS200E_OP_UNKNOWN);
  CHECK(n_qreply == 0);
  FRAME(0x04, 0x22, 0x22, 0x1C, 0xFF);
  CHECK(last_op() == BUS200E_OP_UNKNOWN);
  CHECK(n_qreply == 0);

  // an ordinary broadcast RECALL still recalls
  FRAME(0x04, 0x00, 0x22, 0x01, 7);
  CHECK(n_recall == 1 && recall_calls[0] == 7);
  CHECK(n_qreply == 0);

  // right shape, wrong command byte: not a reply, and not a command either
  // (srcAddr is not 0x22), so it falls through to the short/V2 branch
  reset(&fake_ops);
  FRAME(0x0A, 0x22, 0x28, 0x14, '2', '5', '1', 'e', ' ', ' ', ' ');
  CHECK(n_qreply == 0);
  CHECK(last_op() == BUS200E_OP_UNKNOWN);

  // right command byte, wrong dest (not the manager): also not a reply
  reset(&fake_ops);
  FRAME(0x0A, 0x23, 0x28, 0x13, '2', '5', '1', 'e', ' ', ' ', ' ');
  CHECK(n_qreply == 0);
  CHECK(last_op() == BUS200E_OP_UNKNOWN);

  // length byte inconsistent with the frame: not a reply
  reset(&fake_ops);
  FRAME(0x09, 0x22, 0x28, 0x13, '2', '5', '1', 'e', ' ', ' ', ' ');
  CHECK(n_qreply == 0);

  // a reply we mastered ourselves is dropped by echo suppression, once
  reset(&fake_ops);
  const uint8_t echo[] = { 0x04, 0x22, BUS200E_DEFAULT_MODULE_ADDR,
                           0x1C, 0xFF };
  Bus200eSuppressFrame(echo, sizeof(echo));
  frame(echo, sizeof(echo));
  CHECK(n_qreply == 0);
  frame(echo, sizeof(echo));
  CHECK(n_qreply == 1);
}

int main() {
  test_short_recall_save();
  test_long_recall_save();
  test_remote_enable_gating();
  test_preset_range_gating();
  test_query_pending();
  test_null_ops_logs_only();
  test_backup_job();
  test_backup_other_module_ignored();
  test_long_backup_args();
  test_backup_aborts_on_error();
  test_restore_rejects_skip();
  test_second_job_dropped_while_busy();
  test_midi_and_clock_log_only();
  test_frame_hygiene();
  test_unknown_commands_logged();
  test_log_ring();

  test_bus_midi();

  test_build_transfer_frame();
  test_build_transfer_frame_round_trips_through_parser();

  test_build_query_frame();
  test_build_query_frame_round_trips_through_parser();
  test_query_reply_parse();
  test_xfer_done_parse();
  test_load_ack_parse();
  test_query_reply_does_not_shadow_commands();

  printf("\ntest_bus200e: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
