// Copyright (c) 2016-2022 Patrick Dowling
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <Arduino.h>
#include <string.h>

#include "weegfx.h"
#include "../../util/util_macros.h"

#include <cstdarg>
#include <cstdio>
#include <utility>

namespace weegfx {

// TODO
// - Bench templated draw_pixel_row (inlined versions) vs. function pointers
// - Offer specialized functions w/o clipping or specific draw mode (e.g. text overwrite)
// - Remainder masks as LUT or switch
// - 32bit ops? Should be possible along x-axis (use SIMD instructions?) but not y (page stride)
// - Clipping for x, y < 0
// - Support 16 bit text characters?
// - Kerning/BBX etc.
// - print(string) -> print(char) can re-use variables
// - etc.

#define CLIPX(x, w)                   \
  if (x + w > kWidth) w = kWidth - x; \
  if (x < 0) {                        \
    w += x;                           \
    x = 0;                            \
  }                                   \
  if (w <= 0) return;                 \
  do {                                \
  } while (0)

#define CLIPY(y, h)                     \
  if (y + h > kHeight) h = kHeight - y; \
  if (y < 0) {                          \
    h += y;                             \
    y = 0;                              \
  }                                     \
  if (h <= 0) return;                   \
  do {                                  \
  } while (0)

// clang-format off
template <PIXEL_OP op>
inline uint8_t pixel_op_impl(uint8_t a, uint8_t n) __attribute__((always_inline));
template <> inline uint8_t pixel_op_impl<PIXEL_OP_OR>(uint8_t a, uint8_t b) { return a | b; }
template <> inline uint8_t pixel_op_impl<PIXEL_OP_XOR>(uint8_t a, uint8_t b) { return a ^ b; }
template <> inline uint8_t pixel_op_impl<PIXEL_OP_SRC>(uint8_t, uint8_t b) { return b; }
template <> inline uint8_t pixel_op_impl<PIXEL_OP_NAND>(uint8_t a, uint8_t b) { return a & ~b; }

template <PIXEL_OP pixel_op> inline void draw_pixel_row(uint8_t *dst, coord_t count, uint8_t mask) __attribute__((always_inline));
template <PIXEL_OP pixel_op> inline void draw_pixel_row(uint8_t *dst, coord_t count, const uint8_t *src) __attribute__((always_inline));
template <PIXEL_OP pixel_op> inline void draw_pixel_row_lshift(uint8_t *dst, coord_t count, const uint8_t *src, int shift) __attribute__((always_inline));
template <PIXEL_OP pixel_op> inline void draw_pixel_row_rshift(uint8_t *dst, coord_t count, const uint8_t *src, int shift) __attribute__((always_inline));
template <PIXEL_OP pixel_op> inline void draw_rect(uint8_t *buf, coord_t y, coord_t w, coord_t h) __attribute__((always_inline));
template <PIXEL_OP pixel_op> inline void blit(uint8_t *dst, coord_t y, coord_t w, coord_t h, const uint8_t *src);
// clang-format on

template <PIXEL_OP pixel_op>
inline void draw_pixel_row(uint8_t *dst, coord_t count, uint8_t mask)
{
  while (count--) {
    *dst = pixel_op_impl<pixel_op>(*dst, mask);
    ++dst;
  }
}

template <PIXEL_OP pixel_op>
inline void draw_pixel_row(uint8_t *dst, coord_t count, const uint8_t *src)
{
  while (count--) {
    *dst = pixel_op_impl<pixel_op>(*dst, *src);
    ++dst;
    ++src;
  }
}

template <PIXEL_OP pixel_op>
inline void draw_pixel_row_lshift(uint8_t *dst, coord_t count, const uint8_t *src, int shift)
{
  while (count--) {
    *dst = pixel_op_impl<pixel_op>(*dst, *src << shift);
    ++dst;
    ++src;
  }
}

template <PIXEL_OP pixel_op>
inline void draw_pixel_row_rshift(uint8_t *dst, coord_t count, const uint8_t *src, int shift)
{
  while (count--) {
    *dst = pixel_op_impl<pixel_op>(*dst, *src >> shift);
    ++dst;
    ++src;
  }
}

template <PIXEL_OP pixel_op>
inline void draw_rect(uint8_t *buf, coord_t y, coord_t w, coord_t h)
{
  coord_t remainder = y & 0x7;
  if (remainder) {
    remainder = 8 - remainder;
    uint8_t mask = ~(0xff >> remainder);
    if (h < remainder) {
      mask &= (0xff >> (remainder - h));
      h = 0;
    } else {
      h -= remainder;
    }

    draw_pixel_row<pixel_op>(buf, w, mask);
    buf += Graphics::kWidth;
  }

  remainder = h & 0x7;
  h >>= 3;
  while (h--) {
    draw_pixel_row<pixel_op>(buf, w, 0xff);
    buf += Graphics::kWidth;
  }

  if (remainder) { draw_pixel_row<pixel_op>(buf, w, ~(0xff << remainder)); }
}

template <PIXEL_OP pixel_op>
inline void blit(uint8_t *dst, coord_t y, coord_t w, coord_t h, const uint8_t *src)
{
  coord_t remainder = y & 0x7;
  if (!remainder) {
    draw_pixel_row<pixel_op>(dst, w, src);
  } else {
    draw_pixel_row_lshift<pixel_op>(dst, w, src, remainder);
    if (h >= 8) {
      dst += Graphics::kWidth;
      draw_pixel_row_rshift<pixel_op>(dst, w, src, 8 - remainder);
    }
  }
}

void Graphics::Begin(uint8_t *frame, CLEAR_FRAME clear_frame)
{
  frame_ = frame;
  if (clear_frame) memset(frame_, 0, kFrameSize);

  setPrintPos(0, 0);
}

void Graphics::End()
{
  frame_ = NULL;
}

void Graphics::drawRect(coord_t x, coord_t y, coord_t w, coord_t h)
{
  CLIPX(x, w);
  CLIPY(y, h);
  draw_rect<PIXEL_OP_OR>(get_frame_ptr(x, y), y, w, h);
}

void Graphics::clearRect(coord_t x, coord_t y, coord_t w, coord_t h)
{
  CLIPX(x, w);
  CLIPY(y, h);
  draw_rect<PIXEL_OP_NAND>(get_frame_ptr(x, y), y, w, h);
}

void Graphics::invertRect(coord_t x, coord_t y, coord_t w, coord_t h)
{
  CLIPX(x, w);
  CLIPY(y, h);
  draw_rect<PIXEL_OP_XOR>(get_frame_ptr(x, y), y, w, h);
}

void Graphics::drawFrame(coord_t x, coord_t y, coord_t w, coord_t h)
{
  // Obvious candidate for optimizing
  // TODO Check w/h
  drawHLine(x, y, w);
  drawVLine(x, y + 1, h - 1);
  drawVLine(x + w - 1, y + 1, h - 1);
  drawHLine(x, y + h - 1, w);
}

void Graphics::drawHLine(coord_t x, coord_t y, coord_t w)
{
  coord_t h = 1;
  CLIPX(x, w);
  CLIPY(y, h);
  uint8_t *start = get_frame_ptr(x, y);

  draw_pixel_row<PIXEL_OP_OR>(start, w, 0x1 << (y & 0x7));
}

void Graphics::drawVLine(coord_t x, coord_t y, coord_t h)
{
  coord_t w = 1;
  CLIPX(x, w);
  CLIPY(y, h);
  uint8_t *buf = get_frame_ptr(x, y);

  // unaligned start
  coord_t remainder = y & 0x7;
  if (remainder) {
    remainder = 8 - remainder;
    uint8_t mask = ~(0xff >> remainder);
    if (h < remainder) {
      mask &= (0xff >> (remainder - h));
      h = 0;
    } else {
      h -= remainder;
    }

    *buf |= mask;
    buf += kWidth;
  }

  // aligned loop
  remainder = h & 0x7;
  h >>= 3;
  while (h--) {
    *buf = 0xff;
    buf += kWidth;
  }

  // unaligned remainder
  if (remainder) { *buf |= ~(0xff << remainder); }
}

void Graphics::drawVLinePattern(coord_t x, coord_t y, coord_t h, uint8_t pattern)
{
  CLIPY(y, h);
  uint8_t *buf = get_frame_ptr(x, y);

  // unaligned start
  coord_t remainder = y & 0x7;
  if (remainder) {
    remainder = 8 - remainder;
    uint8_t mask = ~(0xff >> remainder);
    if (h < remainder) {
      mask &= (pattern >> (remainder - h));
      h = 0;
    } else {
      h -= remainder;
    }

    *buf |= (mask & pattern);
    buf += kWidth;
  }

  // aligned loop
  remainder = h & 0x7;
  h >>= 3;
  while (h--) {
    *buf = pattern;  // FIXME this is probably not aligned right
    buf += kWidth;
  }

  // unaligned remainder
  if (remainder) { *buf |= ~(pattern << remainder); }
}

void Graphics::drawHLinePattern(coord_t x, coord_t y, coord_t w, uint8_t skip)
{
  uint8_t h = 1;
  CLIPX(x, w);
  CLIPY(y, h);

  uint8_t *buf = get_frame_ptr(x, y);
  auto end = buf + w;
  uint8_t mask = 0x1 << (y & 0x7);
  while (buf < end) {
    *buf |= mask;
    buf += skip;
  }
}

void Graphics::drawBitmap8(coord_t x, coord_t y, coord_t w, const uint8_t *data)
{
  if (x + w > kWidth) w = kWidth - x;
  if (x < 0) {
    data += x;
    w += x;
  }
  if (w <= 0) return;

  coord_t h = 8;
  CLIPY(y, h);

  blit<PIXEL_OP_OR>(get_frame_ptr(x, y), y, w, h, data);
}

void Graphics::writeBitmap8(coord_t x, coord_t y, coord_t w, const uint8_t *data)
{
  if (x + w > kWidth) w = kWidth - x;
  if (x < 0) {
    data += x;
    w += x;
  }
  if (w <= 0) return;

  coord_t h = 8;
  CLIPY(y, h);

  blit<PIXEL_OP_SRC>(get_frame_ptr(x, y), y, w, h, data);
}

// p = period. Draw a dotted line with a pixel every p
void Graphics::drawLine(coord_t x0, coord_t y0, coord_t x1, coord_t y1, const uint8_t p) {
  uint8_t c = 0;
  coord_t dx, dy;
  if (x0 > x1)
    dx = x0 - x1;
  else
    dx = x1 - x0;
  if (y0 > y1)
    dy = y0 - y1;
  else
    dy = y1 - y0;

  bool steep = false;
  if (dy > dx) {
    steep = true;
    std::swap(dx, dy);
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (x0 > x1) {
    std::swap(x0, x1);
    std::swap(y0, y1);
  }
  coord_t err = dx >> 1;
  coord_t ystep = (y1 > y0) ? 1 : -1;
  coord_t y = y0;

  // OPTIMIZE Generate mask/buffer offset before loop and update on-the-fly instead of setPixeling
  // OPTIMIZE Generate spans of pixels to draw

  if (steep) {
    for(coord_t x = x0; x <= x1; x++ ) {
      if (++c % p == 0) setPixel(y, x);
      err -= dy;
      if (err < 0) {
        y += ystep;
        err += dx;
      }
    }
  } else {
    for(coord_t x = x0; x <= x1; x++ ) {
      if (++c % p == 0) setPixel(x, y);
      err -= dy;
      if (err < 0) {
        y += ystep;
        err += dx;
      }
    }
  }
}

void Graphics::drawCircle(coord_t center_x, coord_t center_y, coord_t r)
{
  coord_t f = 1 - r;
  coord_t ddF_x = 1;
  coord_t ddF_y = -2 * r;
  coord_t x = 0;
  coord_t y = r;

  setPixel(center_x, center_y + r);
  setPixel(center_x, center_y - r);
  setPixel(center_x + r, center_y);
  setPixel(center_x - r, center_y);

  while (x < y) {
    if (f >= 0) {
      y--;
      ddF_y += 2;
      f += ddF_y;
    }
    x++;
    ddF_x += 2;
    f += ddF_x;

    setPixel(center_x + x, center_y + y);
    setPixel(center_x - x, center_y + y);
    setPixel(center_x + x, center_y - y);
    setPixel(center_x - x, center_y - y);
    setPixel(center_x + y, center_y + x);
    setPixel(center_x - y, center_y + x);
    setPixel(center_x + y, center_y - x);
    setPixel(center_x - y, center_y - x);
  }
}

#include "../../extern/gfx_font_6x8.h"
static inline font_glyph get_char_glyph(char c) __attribute__((always_inline));
static inline font_glyph get_char_glyph(char c)
{
  return ssd1306xled_font6x8 + kFixedFontW * (c - 32);
}

static char print_buf[128] = {0};

// OPTIMIZE When printing strings, all chars will have the same y/remainder
// This will probably only save a few cycles, if any. Also the clipping can
// be made optional (template?)
template <PIXEL_OP pixel_op>
void Graphics::blit_char(char c, coord_t x, coord_t y)
{
  if (!c) c = '0';
  if (c <= 32 || c > 127) return;

  coord_t w = kFixedFontW;
  coord_t h = kFixedFontH;
  font_glyph data = get_char_glyph(c);
  if (x + w > kWidth) w = kWidth - x;
  if (x < 0) {
    w += x;
    data += x;
  }
  if (w <= 0 || w > kFixedFontW) return;
  CLIPY(y, h);

  blit<pixel_op>(get_frame_ptr(x, y), y, w, h, data);
}

template <PIXEL_OP pixel_op>
void Graphics::print_impl(const char *s)
{
  coord_t x = text_x_;
  coord_t y = text_y_;

  // TODO Track position, only clip when necessary or early-out?
  while (*s) {
    blit_char<pixel_op>(*s++, x, y);
    x += kFixedFontW;
  }

  text_x_ = x;
}

void Graphics::print(char c)
{
  blit_char<PIXEL_OP_OR>(c, text_x_, text_y_);
  text_x_ += kFixedFontW;
}

template <typename type, bool pretty>
char *itos(type value, char *buf, size_t buflen)
{
  char *pos = buf + buflen;
  *--pos = '\0';
  if (!value) {
    *--pos = '0';
    if (pretty)  // avoid jump when 0 -> +1 or -1
      *--pos = ' ';
  } else {
    char sign = 0;
    if (value < 0) {
      sign = '-';
      value = -value;
    } else if (pretty) {
      sign = '+';
    }

    while (value) {
      *--pos = '0' + value % 10;
      value /= 10;
    }
    if (sign) *--pos = sign;
  }

  return pos;
}

void Graphics::print(int value)
{
  print(itos<int, false>(value, print_buf, sizeof(print_buf)));
}

void Graphics::print(long value)
{
  print(itos<long, false>(value, print_buf, sizeof(print_buf)));
}

void Graphics::pretty_print(int value)
{
  print(itos<int, true>(value, print_buf, sizeof(print_buf)));
}

void Graphics::print(int value, unsigned width)
{
  char *str = itos<int, false>(value, print_buf, sizeof(print_buf));
  while (str > print_buf && (unsigned)(str - print_buf) >= sizeof(print_buf) - width) *--str = ' ';
  print_impl<PIXEL_OP_OR>(str);
}

void Graphics::write(int value, unsigned width)
{
  char *str = itos<int, false>(value, print_buf, sizeof(print_buf));
  while (str > print_buf && (unsigned)(str - print_buf) >= sizeof(print_buf) - width) *--str = ' ';
  print_impl<PIXEL_OP_SRC>(str);
}

void Graphics::print(uint16_t value, unsigned width)
{
  char *str = itos<uint16_t, false>(value, print_buf, sizeof(print_buf));
  while (str > print_buf && (unsigned)(str - print_buf) >= sizeof(print_buf) - width) *--str = ' ';
  print(str);
}

void Graphics::print(uint32_t value, size_t width)
{
  char *str = itos<uint32_t, false>(value, print_buf, sizeof(print_buf));
  while (str > print_buf && (size_t)(str - print_buf) >= sizeof(print_buf) - width) *--str = ' ';
  print(str);
}

void Graphics::pretty_print(int value, unsigned width)
{
  char *str = itos<int, true>(value, print_buf, sizeof(print_buf));

  while (str > print_buf && (unsigned)(str - print_buf) >= sizeof(print_buf) - width) *--str = ' ';
  print(str);
}

void Graphics::pretty_print_right(int value)
{
  coord_t x = text_x_ - kFixedFontW;
  coord_t y = text_y_;

  if (!value) {
    blit_char<PIXEL_OP_OR>('0', x, y);
  } else {
    char sign;
    if (value < 0) {
      value = -value;
      sign = '-';
    } else {
      sign = '+';
    }

    while (value) {
      blit_char<PIXEL_OP_OR>('0' + value % 10, x, y);
      x -= kFixedFontW;
      value /= 10;
    }
    if (sign) blit_char<PIXEL_OP_OR>(sign, x, y);
  }
}

void Graphics::print(const char *s)
{
  print_impl<PIXEL_OP_OR>(s);
}

void Graphics::print(const char *s, unsigned len)
{
  coord_t x = text_x_;
  coord_t y = text_y_;
  while (*s && len--) {
    blit_char<PIXEL_OP_OR>(*s++, x, y);
    x += kFixedFontW;
  }

  text_x_ = x;
}

void Graphics::print_right(const char *s)
{
  coord_t x = text_x_;
  coord_t y = text_y_;
  const char *c = s;
  while (*c) ++c;  // find end

  while (c > s) {
    x -= kFixedFontW;
    blit_char<PIXEL_OP_OR>(*--c, x, y);
  }
}

void Graphics::write_right(const char *s)
{
  auto x = text_x_;
  auto y = text_y_;
  auto c = s;
  while (*c) ++c;
  while (c > s) {
    x -= kFixedFontW;
    blit_char<PIXEL_OP_SRC>(*--c, x, y);
  }
}

void Graphics::printf(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vsnprintf(print_buf, sizeof(print_buf), fmt, args);
  va_end(args);
  print(print_buf);
}

void Graphics::drawStr(coord_t x, coord_t y, const char *s)
{
  while (*s) {
    blit_char<PIXEL_OP_OR>(*s++, x, y);
    x += kFixedFontW;
  }
}

}  // namespace weegfx
