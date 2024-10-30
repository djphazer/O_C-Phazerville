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

#ifndef WEEGFX_H_
#define WEEGFX_H_

// Contains modifications for Hemisphere Suite
#define HS_GFX_MOD

#include <stdint.h>
#include <string.h>

namespace weegfx {

using coord_t = int_fast16_t;
using font_glyph = const uint8_t *;

static constexpr coord_t kFixedFontW = 6;
static constexpr coord_t kFixedFontH = 8;

enum PIXEL_OP {
  PIXEL_OP_OR,    // DST | SRC
  PIXEL_OP_XOR,   // DST ^ SRC
  PIXEL_OP_NAND,  // DST &= ~SRC
  PIXEL_OP_SRC,   // DST = SRC
};

enum CLEAR_FRAME { CLEAR_FRAME_DISABLE, CLEAR_FRAME_ENABLE };

// Quick & dirty graphics for 128x64 framebuffer with vertical pixels.
// - Writes to provided framebuffer
// - Makes some assumptions based on fixed size and pixel orientation
class Graphics {
public:
  static constexpr uint8_t kWidth = 128;
  static constexpr uint8_t kHeight = 64;
  static constexpr size_t kFrameSize = kWidth * kHeight / 8;

  void Begin(uint8_t *frame, CLEAR_FRAME clear_frame);
  void End();

  inline void setPixel(coord_t x, coord_t y) __attribute__((always_inline));

  void drawRect(coord_t x, coord_t y, coord_t w, coord_t h);
  void clearRect(coord_t x, coord_t y, coord_t w, coord_t h);
  void invertRect(coord_t x, coord_t y, coord_t w, coord_t h);
  void drawFrame(coord_t x, coord_t y, coord_t w, coord_t h);

  void drawHLine(coord_t x, coord_t y, coord_t w);
  void drawVLine(coord_t x, coord_t y, coord_t h);
  void drawVLinePattern(coord_t x, coord_t y, coord_t h, uint8_t pattern);
  void drawHLinePattern(coord_t x, coord_t y, coord_t w, uint8_t skip);

  void drawLine(coord_t x1, coord_t y1, coord_t x2, coord_t y2, const uint8_t p = 1);

  void drawBitmap8(coord_t x, coord_t y, coord_t w, const uint8_t *data);
  void writeBitmap8(coord_t x, coord_t y, coord_t w, const uint8_t *data);

  // Beware: No clipping
  void drawCircle(coord_t center_x, coord_t center_y, coord_t r);

  void setPrintPos(coord_t x, coord_t y);
  void movePrintPos(coord_t dx, coord_t dy);
  coord_t getPrintPosX();
  coord_t getPrintPosY();

  void print(char);
  void print(int);
  void print(int, unsigned width);
  void write(int, unsigned width);
  void print(uint16_t, unsigned width);
  void print(uint32_t, unsigned width);
  void print(long);

  void pretty_print(int);
  void pretty_print(int, unsigned width);

  // Print right-aligned number at current print pos; print pos is unchanged
  void pretty_print_right(int);

  // Print string at current print pos and move print pos
  void print(const char *);
  void print(const char *, unsigned len);

  // Print right-aligned string at current print pos; print pos is unchanged
  void print_right(const char *);

  // Write right-aligned string at current print pos; print pos is unchanged
  void write_right(const char *);

  // Print string at absolute coords, doesn't move print pos
  void drawStr(coord_t x, coord_t y, const char *str);

  // Might be time-consuming
  void printf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

  inline void drawAlignedByte(coord_t x, coord_t y, uint8_t byte) __attribute__((always_inline));

private:
  uint8_t *frame_ = nullptr;

  coord_t text_x_ = 0;
  coord_t text_y_ = 0;

  inline uint8_t *get_frame_ptr(const coord_t x, const coord_t y) __attribute__((always_inline));

  // clang-format off
  template <PIXEL_OP pixel_op> void blit_char(char c, coord_t x, coord_t y);
  template <PIXEL_OP pixel_op> void print_impl(const char *s);
  // clang-format on
};

inline void Graphics::setPixel(coord_t x, coord_t y)
{
  *(get_frame_ptr(x, y)) |= (0x1 << (y & 0x7));
}

inline void Graphics::drawAlignedByte(coord_t x, coord_t y, uint8_t byte)
{
  *get_frame_ptr(x, y) = byte;
}

inline void Graphics::setPrintPos(coord_t x, coord_t y)
{
  text_x_ = x;
  text_y_ = y;
}

inline void Graphics::movePrintPos(coord_t dx, coord_t dy)
{
  text_x_ += dx;
  text_y_ += dy;
}

inline coord_t Graphics::getPrintPosX()
{
  return text_x_;
}

inline coord_t Graphics::getPrintPosY()
{
  return text_y_;
}

inline uint8_t *Graphics::get_frame_ptr(const coord_t x, const coord_t y)
{
  return frame_ + ((y >> 3) * kWidth) + x;
}

}  // namespace weegfx

#endif  // WEEGFX_H_
