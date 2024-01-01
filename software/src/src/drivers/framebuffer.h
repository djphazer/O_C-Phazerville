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

  static const size_t kFrameSize = frame_size;

  FrameBuffer() { }

  void Init() {
    memset(frame_memory_, 0, sizeof(frame_memory_));
    for (size_t f = 0; f < frames; ++f)
      frame_buffers_[f] = frame_memory_ + kFrameSize * f;
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
    return frame_buffers_[read_ptr_ % frames];
  }

  // @return next writeable frame (assumes one exists)
  uint8_t *writeable_frame() {
    return frame_buffers_[write_ptr_ % frames];
  }

  void read() {
    ++read_ptr_;
  }

  void written() {
    if (capture_on_next_write) {
      capture_on_next_write = false;
      memcpy(capture_memory_, frame_buffers_[write_ptr_ % frames], kFrameSize);
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

private:

  uint8_t frame_memory_[kFrameSize * frames] __attribute__ ((aligned (4)));
  uint8_t capture_memory_[kFrameSize] __attribute__ ((aligned (4)));
  uint8_t *frame_buffers_[frames];

  volatile size_t write_ptr_;
  volatile size_t read_ptr_;
  volatile bool capture_on_next_write;
  volatile bool capture_is_valid;

  DISALLOW_COPY_AND_ASSIGN(FrameBuffer);
};

#endif // DRIVERS_FRAMEBUFFER_H_
