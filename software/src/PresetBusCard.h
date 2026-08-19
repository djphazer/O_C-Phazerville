#ifndef PRESETBUSCARD_H_
#define PRESETBUSCARD_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// 200e storage-card slave emulator (address 0x50).
//
// Real Buchla storage cards are 24xx-series I2C EEPROMs; the 2WIRELESS WPM
// serves the same address from FRAM. Both speak the same wire protocol:
//   write txn:  [ptr_hi][ptr_lo][data...]   -- 2-byte big-endian pointer,
//               remaining bytes stored at ptr++ (linear, wraps at card size;
//               FRAM-style, no EEPROM page wrap -- matches the WPM)
//   read txn:   bytes streamed from ptr++ (wraps at card size); the pointer
//               survives transactions, so pointer-write then repeated-start
//               read is the normal random-access sequence.
// A 0/1-byte write moves (or half-moves: hi byte only, lo forced 0) the
// pointer and stores nothing -- the WPM presence probe (empty write) is
// exactly this and must stay side-effect-free on the image.
//
// BSP-free and host-testable, same pattern as PresetBus200e: the transport
// (LPI2C1 slave ISR on target, the test suite on host) drives these entry
// points. All of them are ISR-context safe: no allocation, no I/O, O(1).
//
// SERVING IS HARD-GATED AT THE TRANSPORT: this state machine never decides
// whether to ACK 0x50 -- the transport must not route events here (nor match
// the address in hardware) while a real preset manager owns 0x50.
// ---------------------------------------------------------------------------

#define BUSCARD_SIZE 32768u  // 24LC256 / WPM FRAM window

typedef struct {
  uint32_t txns_write;   // write transactions (incl. pointer-only)
  uint32_t txns_read;
  uint32_t bytes_written;
  uint32_t bytes_read;
} BusCardStats;

// Attach backing storage (caller owns it; NULL detaches). Resets pointer,
// transaction state and stats; leaves the image contents untouched.
// size must be a power of two >= 4 (pointer wrap is a mask); returns 0 on
// success, -1 on a bad size (card left detached).
int BusCardInit(uint8_t *image, uint32_t size);

// Address 0x50 matched. is_read = R/W bit of the address byte.
void BusCardStart(int is_read);

// Master wrote a byte to us (write transaction only).
void BusCardRxByte(uint8_t b);

// Master reads a byte from us; returns the byte to put on the wire.
// Reads past the image (no card attached) return 0xFF like a floating bus.
uint8_t BusCardTxByte(void);

// STOP or repeated-START: close the open transaction.
void BusCardStop(void);

// Transport prefetch correction: a hardware TX register is one byte deep,
// so the transport feeds byte N+1 while N shifts out; when the master NAKs,
// that prefetched byte was never sent. Rewinding once on read-leg close
// keeps the pointer identical to a real 24xx address counter, so chunked
// current-address reads (no pointer rewrite per chunk) stay aligned.
void BusCardTxRewind(void);

int  BusCardAttached(void);
int  BusCardDirty(void);        // image modified since last ClearDirty
void BusCardClearDirty(void);
uint32_t BusCardPointer(void);  // current memory pointer (debug/tests)
const BusCardStats *BusCardGetStats(void);

#endif  // PRESETBUSCARD_H_
