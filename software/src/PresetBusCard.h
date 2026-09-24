#ifndef PRESETBUSCARD_H_
#define PRESETBUSCARD_H_

#include <stdint.h>

#define BUSCARD_SIZE 65536u

typedef struct {
  uint32_t txns_write;
  uint32_t txns_read;
  uint32_t bytes_written;
  uint32_t bytes_read;
} BusCardStats;

int BusCardInit(uint8_t *image, uint32_t size);

void BusCardStart(int is_read);

void BusCardRxByte(uint8_t b);

uint8_t BusCardTxByte(void);

void BusCardStop(void);

void BusCardTxRewind(void);

int  BusCardAttached(void);
int  BusCardDirty(void);
void BusCardClearDirty(void);
uint32_t BusCardPointer(void);
const BusCardStats *BusCardGetStats(void);

#endif
