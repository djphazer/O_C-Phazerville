#ifndef PRESETBUS200E_H_
#define PRESETBUS200E_H_

#include <stdint.h>

#define BUS200E_EV_START 0x0100u
#define BUS200E_EV_STOP  0x0200u
#define BUS200E_EV_OVF   0x0400u

#define BUS200E_DEFAULT_MODULE_ADDR 0x3C

#define BUS200E_BUS_PRESETS 30

#ifndef BUS200E_REMOTE_DEFAULT
#define BUS200E_REMOTE_DEFAULT 1
#endif

#define BUS200E_CARD_BASE 0x50

typedef enum {
  BUS200E_OP_NONE = 0,
  BUS200E_OP_RECALL,
  BUS200E_OP_SAVE,
  BUS200E_OP_REMOTE_EN,
  BUS200E_OP_REMOTE_DIS,
  BUS200E_OP_POLL_DONE,
  BUS200E_OP_QUERY,
  BUS200E_OP_BACKUP,
  BUS200E_OP_RESTORE,
  BUS200E_OP_MIDI,
  BUS200E_OP_CLOCK,
  BUS200E_OP_UNKNOWN,
  BUS200E_OP_DROPPED,
  BUS200E_OP_QUERY_REPLY,
  BUS200E_OP_XFER_DONE,
  BUS200E_OP_LOAD_ACK,
} Bus200eOp;

typedef struct {
  uint8_t  op;
  uint8_t  arg;
  uint8_t  mod_addr;
  uint8_t  card_lo;
  uint16_t mem_off;
} Bus200eCmd;

typedef struct {
  void (*save_preset)(uint8_t slot);
  void (*recall_preset)(uint8_t slot);
  uint32_t record_size;
  int (*slot_read)(uint8_t slot, uint8_t *out, uint32_t cap);
  int (*slot_write)(uint8_t slot, const uint8_t *in, uint32_t n);
  int (*card_write)(uint8_t card7, uint32_t off, const uint8_t *d, uint32_t n);
  int (*card_read)(uint8_t card7, uint32_t off, uint8_t *d, uint32_t n);
  void (*midi_rx)(uint8_t status, uint8_t data1, uint8_t data2);
  void (*query_reply)(uint8_t from_addr, const uint8_t *ver, uint8_t n);
  void (*xfer_done)(uint8_t from_addr);
  void (*load_ack)(uint8_t from_addr);
} Bus200eOps;

typedef struct {
  uint32_t frames;
  uint32_t frames_long;
  uint32_t frames_short;
  uint32_t dropped;
  uint32_t job_errors;
  uint32_t restore_rejects;
} Bus200eStats;

void Bus200eInit(const Bus200eOps *ops);

void Bus200eFeedEvent(uint16_t ev);

uint32_t Bus200eLastTransferMs(void);
void Bus200eSetNow(uint32_t now_ms);

void Bus200eSuppressFrame(const uint8_t *bytes, uint8_t n);

void Bus200eTask(void);

int Bus200eRemoteEnabled(void);
int Bus200eJobActive(void);
const Bus200eStats *Bus200eGetStats(void);

void    Bus200eSetModuleAddress(uint8_t addr);
uint8_t Bus200eModuleAddress(void);

int  Bus200eQueryPending(void);
void Bus200eClearQueryPending(void);

#define BUS200E_LOG_SIZE 32
uint32_t Bus200eLogTotal(void);
int Bus200eLogRead(uint32_t n_back, Bus200eCmd *out);

#define BUS200E_XFER_FRAME_LEN 8

int Bus200eBuildTransferFrame(uint8_t op, uint8_t mod_addr, uint8_t card_lo,
                               uint16_t mem_off, uint8_t *out, uint8_t cap);

#define BUS200E_QUERY_FRAME_LEN 5
#define BUS200E_QUERY_VER_MAX 8

int Bus200eBuildQueryFrame(uint8_t mod_addr, uint8_t *out, uint8_t cap);

#endif
