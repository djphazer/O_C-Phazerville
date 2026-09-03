#ifndef DRIVERS_FRAMEBUFFER_H_
#define DRIVERS_FRAMEBUFFER_H_

#include "../../util/util_macros.h"

// - This could be specialized for frames == 2 (i.e. double-buffer)
// - Takes some short-cuts so assumes correct order of calls

// Initial version used a different ring-buffer implementation that used
// frames - 1 elements to be able to distinguish between empty/full, but
// which also meant that it wasn't properly double-buffering.
// This uses an alternate approach that relies on unsigned integer wrap,
// but allows a new frame to be written while the old one is being
// transferred.
// See https://gist.github.com/patrickdowling/0029f58fb20e63d7db9d

template <size_t frame_size, size_t frames>
class FrameBuffer {
public:

  static constexpr size_t kFrameSize = frame_size;
  static constexpr size_t kNumScreens = 4; // pow2

  FrameBuffer() { }

  void Init() {
    for (size_t f = 0; f < frames; ++f)
      memset(frame_memory_[f], 0, sizeof(frame_memory_[f]));
    write_ptr_ = read_ptr_ = 0;
    capture_on_next_write = false;
    capture_is_valid = false;
  }

  size_t writeable() const {
    return frames - readable();
  }

  size_t readable() const {
    return write_ptr_ - read_ptr_;
  }

  // @return readable frame (assumes one exists)
  const uint8_t *readable_frame() const {
    return frame_memory_[read_ptr_ % frames] + screen_ * kFrameSize;
  }

  // @return next writeable frame (assumes one exists)
  uint8_t *writeable_frame() {
    return frame_memory_[write_ptr_ % frames];
  }

  void read() {
    ++read_ptr_;
  }

  void written() {
    if (capture_on_next_write) {
      capture_on_next_write = false;
      memcpy(capture_memory_, frame_memory_[write_ptr_ % frames], kFrameSize * kNumScreens);
      capture_is_valid = true;
    }
    ++write_ptr_;
  }

  void capture_request() {
    capture_on_next_write = true;
  }
  const uint8_t * captured() {
    return (capture_is_valid) ? capture_memory_ : NULL;
  }
  void capture_retire() {
    capture_is_valid = false;
  }

  // select viewport offset within framebuffer
  // XXX: it's possible to do half-screen vertical offsets...
  void set_screen_view(size_t s) {
    screen_ = s & (kNumScreens - 1);
  }

private:
  uint8_t frame_memory_[frames][kFrameSize * kNumScreens]  __attribute__ ((aligned (4)));
  uint8_t capture_memory_[kFrameSize * kNumScreens] __attribute__ ((aligned (4)));

  volatile size_t write_ptr_;
  volatile size_t read_ptr_;
  volatile size_t screen_;
  volatile bool capture_on_next_write;
  volatile bool capture_is_valid;

  DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
};

#endif // DRIVERS_FRAMEBUFFER_H_
