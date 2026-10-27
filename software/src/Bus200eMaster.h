#ifndef BUS200EMASTER_H_
#define BUS200EMASTER_H_

#include <stdint.h>

#include "PresetBus200e.h"

typedef enum : uint8_t {
  BUS200E_MASTER_IDLE = 0,
  BUS200E_MASTER_FINDING_CARD,
  BUS200E_MASTER_SENDING,
  BUS200E_MASTER_WAIT_ACTIVITY,
  BUS200E_MASTER_TRANSFERRING,
  BUS200E_MASTER_DONE,
  BUS200E_MASTER_FAILED,
} Bus200eMasterState;

typedef enum {
  BUS200E_MASTER_ERR_NONE = 0,
  BUS200E_MASTER_ERR_BUSY,
  BUS200E_MASTER_ERR_BAD_ARGS,
  BUS200E_MASTER_ERR_NO_FREE_CARD,
  BUS200E_MASTER_ERR_SEND_TIMEOUT,
  BUS200E_MASTER_ERR_NO_RESPONSE,
} Bus200eMasterError;

typedef struct {
  uint32_t (*now_ms)(void);

  int (*tx_gate_open)(void);

  int (*probe_card)(uint8_t card_addr7);

  int (*send_frame)(const uint8_t *bytes, uint8_t n);

  void (*suppress_echo)(const uint8_t *bytes, uint8_t n);

  uint32_t (*card_activity)(void);
} Bus200eMasterOps;

void Bus200eMasterInit(const Bus200eMasterOps *ops);

int Bus200eMasterFindFreeCard(const uint8_t *candidates, uint8_t n,
                               uint8_t *out_card_lo);

int Bus200eMasterBackup(uint8_t mod_addr, uint8_t card_lo);
int Bus200eMasterRestore(uint8_t mod_addr, uint8_t card_lo);

void Bus200eMasterTask(void);

Bus200eMasterState Bus200eMasterGetState(void);
Bus200eMasterError Bus200eMasterLastError(void);
uint8_t Bus200eMasterCardAddr(void);
uint8_t Bus200eMasterModAddr(void);
int Bus200eMasterIsRestore(void);

uint32_t Bus200eMasterBytesTransferred(void);

void Bus200eMasterXferDone(uint8_t from_addr);
int Bus200eMasterAcked(void);

void Bus200eMasterReset(void);

#define BUS200E_MASTER_SEND_TIMEOUT_MS     2000
#define BUS200E_MASTER_ACTIVITY_TIMEOUT_MS 3000
#define BUS200E_MASTER_QUIET_DONE_MS       1500
#define BUS200E_MASTER_QUIET_ACKED_MS       200
#define BUS200E_MASTER_HARD_CAP_MS        15000

typedef enum : uint8_t {
  BUS200E_QUERY_IDLE = 0,
  BUS200E_QUERY_SENDING,
  BUS200E_QUERY_WAITING,
  BUS200E_QUERY_DONE,
  BUS200E_QUERY_FAILED,
} Bus200eQueryState;

#define BUS200E_MASTER_QUERY_SEND_TIMEOUT_MS  2000
#define BUS200E_MASTER_QUERY_REPLY_TIMEOUT_MS 1000

int Bus200eMasterQuery(uint8_t mod_addr);

void Bus200eMasterQueryTask(void);

void Bus200eMasterQueryReply(uint8_t from_addr, const uint8_t *ver, uint8_t n);

Bus200eQueryState  Bus200eMasterQueryGetState(void);
Bus200eMasterError Bus200eMasterQueryLastError(void);
uint8_t  Bus200eMasterQueryModAddr(void);
uint32_t Bus200eMasterQueryStrayReplies(void);

uint8_t Bus200eMasterQueryVersion(uint8_t *out, uint8_t cap);

void Bus200eMasterQueryReset(void);

#endif
