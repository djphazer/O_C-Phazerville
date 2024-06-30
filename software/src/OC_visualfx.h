// Copyright (c) 2016 Patrick Dowling
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
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

#ifndef OC_VISUALFX_H_
#define OC_VISUALFX_H_

#include <stdint.h>
#include "util/util_history.h"
#include "OC_bitmaps.h"

namespace OC {

namespace vfx {

// Ultra simple history that maintains a scroll position. The history depth is
// allowed to be non pow2 to allow keeping an additonal item to scroll in.
//
// Exercises for the reader:
// - Optional start delay after Push
// - Non-linear scrolling, e.g. using easing LUTs from Frames
// - Variable scroll rate depending on time between Push calls
template <typename T, size_t depth>
class ScrollingHistory {
public:
  ScrollingHistory() { }

  static constexpr size_t kDepth = depth;
  static constexpr uint32_t kScrollRate = (1ULL << 32) / (OC_CORE_ISR_FREQ / 8);

  void Init(T initial_value = 0) {
    scroll_pos_ = 0;
    history_.Init(initial_value);
  }

  void Push(T value) {
    history_.Push(value);
    scroll_pos_ = 0xffffffff;
  }

  void Update() {
    if (scroll_pos_ >= kScrollRate)
      scroll_pos_ -= kScrollRate;
    else
      scroll_pos_ = 0;
  }

  uint32_t get_scroll_pos() const {
    return scroll_pos_ >> 24;
  }

  void Read(T *dest) const {
    return history_.Read(dest);
  }

private:

  uint32_t scroll_pos_;
  util::History<T, kDepth> history_;
};

// Very simple, single-line text marquee scrolly thing.
// May or may not be safe :)
template <int max_text_width, bool clear_background = true, bool ellipsis = true>
class Marquee {
public:
  Marquee() { }
  ~Marquee() { }

  static constexpr int kMaxTextWidth = max_text_width;
  static constexpr weegfx::coord_t kFixedFontW = weegfx::Graphics::kFixedFontW;
  static constexpr weegfx::coord_t kWidth = max_text_width * kFixedFontW;
  static constexpr weegfx::coord_t kHeight = weegfx::Graphics::kFixedFontH;
  static constexpr uint32_t kScrollRate = (1ULL << 32) / (OC_CORE_ISR_FREQ) / 64;

  static constexpr uint32_t kShift = 22; // 1024 pixels - delay
  static constexpr uint32_t kDelayTime = 0x3f;

  static_assert(max_text_width * kFixedFontW + kDelayTime < (0x1UL << (32 - kShift)), "Text too large for amount of pixels");

  void Init(const char *text) {
    text_ = text;
    len_ = strlen(text);
    phase_ = 0;
    end_phase_ = (len_ > kMaxTextWidth)
      ? (kDelayTime + len_ * kFixedFontW) << kShift
      : 0;
    ticks_.Reset();
  }

  void Reset() {
    phase_ = 0;
    ticks_.Reset();
  }

  void Update() {
    auto phase = phase_ + ticks_.Update() * kScrollRate;
    if (phase > end_phase_)
      phase = 0;
    phase_ = phase;
  }

  void Draw(weegfx::coord_t x, weegfx::coord_t y) const {
    if (clear_background)
      graphics.clearRect(x, y, kWidth, kHeight);

    auto phase = phase_ >> kShift;
    int start;
    if (phase >= kDelayTime) {
      phase -= kDelayTime;
      auto pos = static_cast<weegfx::coord_t>(phase);
      start = pos / kFixedFontW;
      if (start < len_)
        graphics.drawStrClipX(x - (pos - start * kFixedFontW), y, text_ + start, x, kWidth);
    } else {   
      graphics.drawStrClipX(x, y, text_, x, kWidth);
      start = 0;
    }
    if (ellipsis) {
      if (start + max_text_width < len_)
        graphics.drawBitmap8(x + kMaxTextWidth * kFixedFontW, y, 5, bitmap_ellipsis_8);
    }
  }

private:
  const char *text_;
  int len_;
  uint32_t phase_, end_phase_;
  TickCount ticks_;

  DISALLOW_COPY_AND_ASSIGN(Marquee);
};

} // namespace vfx
} // namespace OC

#endif // OC_VISUALFX_H_
