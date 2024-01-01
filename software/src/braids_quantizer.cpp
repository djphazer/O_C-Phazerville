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

#include "braids_quantizer.h"
#include "OC_options.h"
#include "util/util_misc.h"

#include <algorithm>
#include <cstdlib>

namespace braids {

const int32_t NEIGHBOR_WEIGHT = 10; // out of 16
constexpr int32_t CUR_WEIGHT = 16 - NEIGHBOR_WEIGHT;

void SortScale(Scale &scale) {
  std::sort(scale.notes, scale.notes + scale.num_notes);
}

void Quantizer::Init() {
  enabled_ = true;
  codeword_ = 0;
  transpose_ = 0;
  previous_boundary_ = 0;
  next_boundary_ = 0;
}

int32_t Quantizer::Process(int32_t pitch, int32_t root, int32_t transpose) {
  if (!enabled_) {
    return pitch;
  }

  pitch -= root;
  #ifdef BUCHLA_4U
    pitch -= ((12 << 7) << 2);
  #else
    pitch -= ((12 << 7) << 1);
  #endif

  if (!requantize_ && pitch >= previous_boundary_ && pitch <= next_boundary_ && transpose == transpose_) {
    // We're still in the voronoi cell for the active codeword.
    pitch = codeword_;
  } else {
    requantize_ = false;
    int16_t octave = pitch / span_ - (pitch < 0 ? 1 : 0);
    int16_t rel_pitch = pitch - span_ * octave;

    int16_t best_distance = 16384;
    int16_t q = -1;
    for (int16_t i = 0; i < num_notes_; i++) {
      int16_t distance = abs(rel_pitch - notes_[i]);
      if (distance < best_distance) {
        best_distance = distance;
        q = i;
      }
    }

    if (abs(pitch - (octave + 1) * span_ - notes_[0]) < best_distance) {
      octave++;
      q = 0;
    } else if (abs(pitch - (octave - 1) * span_ - notes_[num_notes_ - 1]) <= best_distance) {
      octave--;
      q = num_notes_ - 1;
    }

    // set boundaries for hysteresis
    codeword_ = notes_[q] + octave * span_;
    previous_boundary_ = q == 0
      ? notes_[num_notes_ - 1] + (octave - 1) * span_
      : notes_[q - 1] + octave * span_;
    previous_boundary_ =
        (NEIGHBOR_WEIGHT * previous_boundary_ + CUR_WEIGHT * codeword_) >> 4;

    next_boundary_ = q == num_notes_ - 1
      ? notes_[0] + (octave + 1) * span_
      : notes_[q + 1] + octave * span_;
    next_boundary_ =
        (NEIGHBOR_WEIGHT * next_boundary_ + CUR_WEIGHT * codeword_) >> 4;

    // apply transpose after setting boundaries
    q += transpose;
    octave += q / num_notes_;
    q %= num_notes_;
    if (q < 0) {
      q += num_notes_;
      octave--;
    }

    // set final values
    note_number_ = (octave + 2) * num_notes_ + q + 64; // 64 is C2
    codeword_ = notes_[q] + octave * span_;

    transpose_ = transpose;
    pitch = codeword_;
  }
  pitch += root;
  #ifdef BUCHLA_4U
    pitch += ((12 << 7) << 2);
  #else
    pitch += ((12 << 7) << 1);
  #endif
  return pitch;
}

int32_t Quantizer::Lookup(int32_t index) const {
  index -= 64;
  int16_t octave = index / num_notes_;
  int16_t rel_ix = index % num_notes_;
  if (rel_ix < 0) {
    octave--;
    rel_ix += num_notes_;
  }
  int32_t pitch = notes_[rel_ix] + octave * span_;
  return pitch;
}
}  // namespace braids
