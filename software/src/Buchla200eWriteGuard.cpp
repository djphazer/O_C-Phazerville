#if defined(__IMXRT1062__)
#include <Arduino.h>
#define B200E_GUARD_CODE FLASHMEM
#define B200E_GUARD_DATA PROGMEM
#else
#define B200E_GUARD_CODE
#define B200E_GUARD_DATA
#endif

#include "Buchla200eWriteGuard.h"

B200E_GUARD_CODE
Buchla200eWriteBlock Buchla200eCheckWrite(const Buchla200eWriteContext &ctx) {
  if (!ctx.master_idle) return BUCHLA200E_WRITE_BUSY;

  if (!ctx.have_read) return BUCHLA200E_WRITE_NO_READ;

  if (ctx.read_addr != ctx.target_addr || ctx.read_type != ctx.target_type)
    return BUCHLA200E_WRITE_WRONG_MODULE;

  if (ctx.expected_bank_bytes == 0 ||
      ctx.bytes_transferred < ctx.expected_bank_bytes)
    return BUCHLA200E_WRITE_SHORT_READ;

  if (!ctx.card_serving || !ctx.image_valid)
    return BUCHLA200E_WRITE_NO_IMAGE;

  if (!ctx.image_matches_read) return BUCHLA200E_WRITE_IMAGE_CHANGED;

  if (ctx.changed_bytes < 0) return BUCHLA200E_WRITE_SHORT_READ;

  if (!ctx.patches_in_range) return BUCHLA200E_WRITE_PATCH_RANGE;

  if (ctx.changed_bytes == 0) return BUCHLA200E_WRITE_NO_CHANGES;

  return BUCHLA200E_WRITE_OK;
}

static const uint32_t kCrcNibble[16] B200E_GUARD_DATA = {
  0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
  0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
  0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
  0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
};

static inline uint32_t crc_feed(uint32_t crc, uint8_t b) {
  crc ^= b;
  crc = (crc >> 4) ^ kCrcNibble[crc & 0x0F];
  crc = (crc >> 4) ^ kCrcNibble[crc & 0x0F];
  return crc;
}

B200E_GUARD_CODE
uint32_t Buchla200eCrc32(const uint8_t *data, uint32_t len) {
  if (!data) return 0;
  uint32_t crc = 0xFFFFFFFFUL;
  for (uint32_t i = 0; i < len; ++i) crc = crc_feed(crc, data[i]);
  return crc ^ 0xFFFFFFFFUL;
}

B200E_GUARD_CODE
Buchla200eBankHash Buchla200eHashBank(const uint8_t *bank, uint32_t bank_len,
                                      uint32_t hole_off, uint32_t hole_len) {
  Buchla200eBankHash out;
  out.whole = 0;
  out.outside = 0;
  if (!bank) return out;

  const uint32_t hole_end = hole_off + hole_len;
  uint32_t whole = 0xFFFFFFFFUL;
  uint32_t outside = 0xFFFFFFFFUL;
  for (uint32_t i = 0; i < bank_len; ++i) {
    const uint8_t b = bank[i];
    whole = crc_feed(whole, b);
    if (hole_len == 0 || i < hole_off || i >= hole_end)
      outside = crc_feed(outside, b);
  }
  out.whole = whole ^ 0xFFFFFFFFUL;
  out.outside = outside ^ 0xFFFFFFFFUL;
  return out;
}

B200E_GUARD_CODE
const char *Buchla200eWriteBlockText(Buchla200eWriteBlock b) {
  switch (b) {
    case BUCHLA200E_WRITE_OK:           return "ok";
    case BUCHLA200E_WRITE_NO_READ:      return "Read the module first";
    case BUCHLA200E_WRITE_WRONG_MODULE: return "Other module's data";
    case BUCHLA200E_WRITE_SHORT_READ:   return "Read was incomplete";
    case BUCHLA200E_WRITE_NO_IMAGE:     return "Image gone - reread";
    case BUCHLA200E_WRITE_BUSY:         return "Bus busy";
    case BUCHLA200E_WRITE_NO_CHANGES:   return "No changes to write";
    case BUCHLA200E_WRITE_IMAGE_CHANGED:return "Image changed-reread";
    case BUCHLA200E_WRITE_PATCH_RANGE:  return "Patch out of range";
    case BUCHLA200E_WRITE_BUILD_FAILED: return "Build check failed";
    default:                            return "blocked";
  }
}
