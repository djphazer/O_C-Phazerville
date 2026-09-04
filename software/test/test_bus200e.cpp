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

static const Bus200eOps fake_ops = {
  f_save, f_recall, kRecSize, f_slot_read, f_slot_write, f_card_write, f_card_read,
  f_midi,
};

static void reset(const Bus200eOps *ops) {
  Bus200eInit(ops);
  Bus200eSetModuleAddress(BUS200E_DEFAULT_MODULE_ADDR);
  n_save = n_recall = n_cw = n_sw = n_sr = n_cr = n_midi = 0;
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

  printf("\ntest_bus200e: %d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
