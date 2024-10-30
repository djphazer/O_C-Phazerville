// Copyright 2015 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
// Re-implemented by Bryan Head to eliminate codebook
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//

// Note quantizer
// Heavily modified from the one included in Braids to remove the use a
// codebook. This significantly reduces the amount of memory required.
// Furthermore, it enables changing the configuration on the fly.

#ifndef BRAIDS_QUANTIZER_H_
#define BRAIDS_QUANTIZER_H_

#include "util/util_macros.h"

namespace braids {

struct Scale {
  int16_t span;
  size_t num_notes;
  int16_t notes[16];
};

void SortScale(Scale &);
class Quantizer {
 public:
  Quantizer() {}
  ~Quantizer() {}

  void Init();

  int32_t Process(int32_t pitch) {
    return Process(pitch, 0, 0);
  }

  int32_t Process(int32_t pitch, int32_t root, int32_t transpose);

  void Configure(const Scale& scale, uint16_t mask = 0xffff) {
    num_notes_ = 0;
    for (uint16_t i = 0; i < scale.num_notes; i++) {
      if (mask & 1) notes_[num_notes_++] = scale.notes[i];
      mask >>= 1;
    }
    span_ = scale.span;
    enabled_ = notes_ != NULL && num_notes_ != 0 && span_ != 0;
  }

  bool enabled() const {
    return enabled_;
  }

  int32_t Lookup(int32_t index) const;
  uint16_t GetLatestNoteNumber() { return note_number_; }

  // Force Process to process again (for after re-configuring)
  void Requantize() { requantize_ = true; }

 private:
  bool enabled_;
  int32_t codeword_;
  int32_t transpose_;
  int32_t previous_boundary_;
  int32_t next_boundary_;
  int32_t span_;
  int16_t notes_[16];
  uint8_t num_notes_;

  uint16_t note_number_;
  bool requantize_;

  DISALLOW_COPY_AND_ASSIGN(Quantizer);
};

}  // namespace braids

#endif
