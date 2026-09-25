// 200e storage-card slave emulator. See PresetBusCard.h for the protocol.
#include "PresetBusCard.h"

#include <stddef.h>

static uint8_t *card = NULL;
static uint32_t card_mask = 0;   // size - 1 (power-of-two sizes only)
static uint32_t ptr = 0;         // memory pointer, survives transactions
static uint8_t dirty = 0;

// Open-transaction state. phase counts pointer bytes received this write
// transaction (0 = expecting hi, 1 = expecting lo, 2 = data phase).
static uint8_t in_txn = 0;       // 0 = idle, 1 = write, 2 = read
static uint8_t phase = 0;
static uint32_t ptr_staged = 0;

static BusCardStats stats;

int BusCardInit(uint8_t *image, uint32_t size) {
  card = NULL;
  card_mask = 0;
  ptr = 0;
  dirty = 0;
  in_txn = 0;
  phase = 0;
  stats = BusCardStats();
  if (!image) return 0;  // detach is always fine
  if (size < 4 || (size & (size - 1))) return -1;
  card = image;
  card_mask = size - 1;
  return 0;
}

void BusCardStart(int is_read) {
  if (in_txn) BusCardStop();  // lost STOP: close the old transaction first
  in_txn = is_read ? 2 : 1;
  phase = 0;
  if (is_read) stats.txns_read++; else stats.txns_write++;
}

void BusCardRxByte(uint8_t b) {
  if (in_txn != 1) return;
  if (phase == 0) {            // pointer high byte
    ptr_staged = (uint32_t)b << 8;
    phase = 1;
  } else if (phase == 1) {     // pointer low byte
    ptr = (ptr_staged | b) & card_mask;
    phase = 2;
  } else if (card) {           // data
    card[ptr] = b;
    ptr = (ptr + 1) & card_mask;
    dirty = 1;
    stats.bytes_written++;
  }
}

uint8_t BusCardTxByte(void) {
  if (!card) return 0xFF;
  const uint8_t b = card[ptr];
  ptr = (ptr + 1) & card_mask;
  stats.bytes_read++;
  return b;
}

void BusCardTxRewind(void) {
  if (!card) return;
  ptr = (ptr - 1) & card_mask;
  if (stats.bytes_read) stats.bytes_read--;
}

void BusCardStop(void) {
  // A 1-byte write moved only the staged hi byte: commit it with lo = 0,
  // 24xx-style (the pointer write is what the master asked for; losing it
  // would corrupt a hi-only positioning sequence).
  if (in_txn == 1 && phase == 1) ptr = ptr_staged & card_mask;
  in_txn = 0;
  phase = 0;
}

int BusCardAttached(void) { return card != NULL; }
int BusCardDirty(void) { return dirty; }
void BusCardClearDirty(void) { dirty = 0; }
uint32_t BusCardPointer(void) { return ptr; }
const BusCardStats *BusCardGetStats(void) { return &stats; }
