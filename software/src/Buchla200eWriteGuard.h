#ifndef BUCHLA200EWRITEGUARD_H_
#define BUCHLA200EWRITEGUARD_H_

#include <stdint.h>

enum Buchla200eWriteBlock : uint8_t {
  BUCHLA200E_WRITE_OK = 0,
  BUCHLA200E_WRITE_NO_READ,
  BUCHLA200E_WRITE_WRONG_MODULE,
  BUCHLA200E_WRITE_SHORT_READ,
  BUCHLA200E_WRITE_NO_IMAGE,
  BUCHLA200E_WRITE_BUSY,
  BUCHLA200E_WRITE_NO_CHANGES,
  BUCHLA200E_WRITE_IMAGE_CHANGED,
  BUCHLA200E_WRITE_PATCH_RANGE,
  BUCHLA200E_WRITE_BUILD_FAILED,
};

struct Buchla200eWriteContext {
  bool     have_read;
  uint8_t  read_addr;
  uint8_t  read_type;
  uint8_t  target_addr;
  uint8_t  target_type;
  uint32_t bytes_transferred;
  uint32_t expected_bank_bytes;
  bool     card_serving;
  bool     image_valid;
  bool     master_idle;
  int      changed_bytes;
  bool     image_matches_read;
  bool     patches_in_range;
};

Buchla200eWriteBlock Buchla200eCheckWrite(const Buchla200eWriteContext &ctx);

const char *Buchla200eWriteBlockText(Buchla200eWriteBlock b);

uint32_t Buchla200eCrc32(const uint8_t *data, uint32_t len);

struct Buchla200eBankHash {
  uint32_t whole;
  uint32_t outside;
};
Buchla200eBankHash Buchla200eHashBank(const uint8_t *bank, uint32_t bank_len,
                                      uint32_t hole_off, uint32_t hole_len);

#endif
