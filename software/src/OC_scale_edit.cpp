// Copyright (c) 2015-2019 Patrick Dowling, Max Stadler
//
// Author: Patrick Dowling (pld@gurkenkiste.com)
// Lots of additions to original code by Max Stadler (mxmlnstdlr@gmail.com)
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

#include <Arduino.h>
#include "OC_scale_edit.h"

namespace OC {

/*virtual*/ int ScaleEditorEventHandler::get_root(int /*scale_index*/) const {
  return 0;
}

/*virtual*/ int ScaleEditorEventHandler::get_transpose(int /*scale_index*/) const {
  return 0;
}

/*virtual*/ int ScaleEditorEventHandler::get_slot_index() const {
  return 0;
}

/*virtual*/ void ScaleEditorEventHandler::set_scale_at_slot(int /*scale*/, uint16_t /*mask*/, int /*root*/, int /*transpose*/, int /*scale_slot*/) {
}


void ScaleEditor::Init(bool extended_mode/* = false*/) {
  owner_ = nullptr;
  scale_name_ = "?!";
  scale_ = mutable_scale_ = &dummy_scale;
  mask_ = 0;
  cursor_pos_ = 0;
  num_notes_ = 0;
  slot_index_ = 0;
  extended_mode_ = extended_mode; // mode: true = Meta-Q; false = scale editor
  edit_page_ = EDIT_SCALE;
  ticks_ = 0;
}

void ScaleEditor::Edit(ScaleEditorEventHandler *owner, int scale) {
  if (Scales::SCALE_NONE != scale) {
    owner_ = owner;
    BeginEditing(scale);
  }
}

void ScaleEditor::Draw() const {

  size_t num_notes = num_notes_;
  static constexpr weegfx::coord_t kMinWidth = 64;
  weegfx::coord_t w =
  mutable_scale_ ? ((num_notes + 1) * 7) : (num_notes * 7);

  if (w < kMinWidth || (edit_page_ == EDIT_ROOT) || (edit_page_ == EDIT_TRANSPOSE)) w = kMinWidth;

  weegfx::coord_t x = 64 - (w + 1) / 2;
  weegfx::coord_t y = 16 - 3;
  weegfx::coord_t h = 36;

  graphics.clearRect(x, y, w + 4, h);
  graphics.drawFrame(x, y, w + 5, h);

  x += 2;
  y += 3;

  graphics.setPrintPos(x, y);
  graphics.print(scale_name_);
  
  if (extended_mode_) {
    ticks_++;
    uint8_t id = slot_index_;
    if (slot_index_ == owner_->get_slot_index())
      id += 4;
    graphics.print(Strings::scale_id[id]);
  }

  if (extended_mode_ && edit_page_) {

    switch (edit_page_) {

      case EDIT_ROOT:
      case EDIT_TRANSPOSE: {
        x += w / 2 - (25 + 3); y += 10;
        graphics.drawFrame(x, y, 25, 20);
        graphics.drawFrame(x + 32, y, 25, 20);
        // print root:
        int root = owner_->get_root(slot_index_);
        if (root == 1 || root == 3 || root == 6 || root == 8 || root == 10)
          graphics.setPrintPos(x + 7, y + 7);
        else 
          graphics.setPrintPos(x + 9, y + 7);
        graphics.print(Strings::note_names_unpadded[root]);
        // print transpose:
        int transpose = owner_->get_transpose(slot_index_);
        graphics.setPrintPos(x + 32 + 5, y + 7);
        if (transpose >= 0)
          graphics.print("+");
        graphics.print(transpose);
        // draw cursor
        if (edit_page_ == EDIT_ROOT)
          graphics.invertRect(x - 1, y - 1, 27, 22); 
        else
          graphics.invertRect(x + 32 - 1, y - 1, 27, 22);
      }
      break;
      default:
      break;
    } 
  }
  else {
    // edit scale    
    graphics.setPrintPos(x, y + 24);
    if (cursor_pos_ != num_notes) {
      graphics.movePrintPos(weegfx::Graphics::kFixedFontW, 0);
      if (mutable_scale_ && ui.read_immediate(CONTROL_BUTTON_L))
        graphics.drawBitmap8(x + 1, y + 23, kBitmapEditIndicatorW, bitmap_edit_indicators_8);
      else if (mutable_scale_)
        graphics.drawBitmap8(x + 1, y + 23, 4, bitmap_indicator_4x8);

      uint32_t note_value = scale_->notes[cursor_pos_];
      uint32_t cents = (note_value * 100) >> 7;
      uint32_t frac_cents = ((note_value * 100000) >> 7) - cents * 1000;
      // move print position, so that things look somewhat nicer
      if (cents < 10)
        graphics.movePrintPos(weegfx::Graphics::kFixedFontW * -3, 0);
      else if (cents < 100)
        graphics.movePrintPos(weegfx::Graphics::kFixedFontW * -2, 0);
      else if (cents < 1000)
        graphics.movePrintPos(weegfx::Graphics::kFixedFontW * -1, 0);
      // justify left ...  
      if (! mutable_scale_)
        graphics.movePrintPos(weegfx::Graphics::kFixedFontW * -1, 0);
      graphics.printf("%4u.%02uc", cents, (frac_cents + 5) / 10);

    } else {
      graphics.print((int)num_notes);
    }

    x += mutable_scale_ ? (w >> 0x1) - (((num_notes) * 7 + 4) >> 0x1) : (w >> 0x1) - (((num_notes - 1) * 7 + 4) >> 0x1); 
    y += 11;
    uint16_t mask = mask_;
    for (size_t i = 0; i < num_notes; ++i, x += 7, mask >>= 1) {
      if (mask & 0x1)
        graphics.drawRect(x, y, 4, 8);
      else
        graphics.drawBitmap8(x, y, 4, bitmap_empty_frame4x8);

      if (i == cursor_pos_)
        graphics.drawFrame(x - 2, y - 2, 8, 12);
    }
    if (mutable_scale_) {
      graphics.drawBitmap8(x, y, 4, bitmap_end_marker4x8);
      if (cursor_pos_ == num_notes)
        graphics.drawFrame(x - 2, y - 2, 8, 12);
    }
  }
}

void ScaleEditor::HandleButtonEvent(const UI::Event &event) {
  if (UI::EVENT_BUTTON_PRESS == event.type) {
    switch (event.control) {
    case CONTROL_BUTTON_UP:
      handleButtonUp(event);
      break;
    case CONTROL_BUTTON_DOWN:
      handleButtonDown(event);
      break;
    case CONTROL_BUTTON_L:
      handleButtonLeft(event);
      break;
    case CONTROL_BUTTON_R:
      Close();
      break;
    }
  } else if (UI::EVENT_BUTTON_LONG_PRESS == event.type) {
    switch (event.control) {
    default:
      break;
    }
  }
}

void ScaleEditor::HandleEncoderEvent(const UI::Event &event) {
  bool scale_changed = false;
  uint16_t mask = mask_;

  if (CONTROL_ENCODER_L == event.control) {
    
    if (extended_mode_ && ui.read_immediate(CONTROL_BUTTON_UP)) {
          ui.IgnoreButton(CONTROL_BUTTON_UP);
      
          // we're in Meta-Q, so we can change the scale:
         
          int _scale = owner_->get_scale(slot_index_) + event.value;
          CONSTRAIN(_scale, Scales::SCALE_USER_0, Scales::NUM_SCALES-1);
          
          if (_scale == Scales::SCALE_NONE) {
             // just skip this here ...
             if (event.value > 0) 
                _scale++;
             else 
                _scale--;
          }
          // update active scale with mask/root/transpose settings, and set flag:
          owner_->set_scale_at_slot(_scale, mask_, owner_->get_root(slot_index_), owner_->get_transpose(slot_index_), slot_index_); 
          scale_changed = true; 
          set_scale(_scale);
          ticks_ = 0;
    }
    else {
      move_cursor(event.value);
    }
  } else if (CONTROL_ENCODER_R == event.control) {

      switch (edit_page_) {

        case EDIT_SCALE: 
        {
          bool handled = false;
          if (mutable_scale_) {
            if (cursor_pos_ < num_notes_) {
              if (event.mask & CONTROL_BUTTON_L) {
                ui.IgnoreButton(CONTROL_BUTTON_L);
                change_note(cursor_pos_, event.value, false);
                scale_changed = true;
                handled = true;
              }
            } else {
              if (cursor_pos_ == num_notes_) {
                int num_notes = num_notes_;
                num_notes += event.value;
                CONSTRAIN(num_notes, kMinScaleLength, kMaxScaleLength);
      
                num_notes_ = num_notes;
                if (event.value > 0) {
                  for (size_t pos = cursor_pos_; pos < num_notes_; ++pos)
                    change_note(pos, 0, false);
      
                  // Enable new notes by default
                  mask |= ~(0xffff << (num_notes_ - cursor_pos_)) << cursor_pos_;
                } else {
                  // scale might be shortened to where no notes are active in mask
                  if (0 == (mask & ~(0xffff < num_notes_)))
                    mask |= 0x1;
                }
      
                mutable_scale_->num_notes = num_notes_;
                cursor_pos_ = num_notes_;
                handled = scale_changed = true;
              }
            }
          }
          if (!handled) {
              mask = RotateMask(mask_, num_notes_, event.value);
          }
        }
        break;
        case EDIT_ROOT:
        {
         int _root = owner_->get_root(slot_index_) + event.value;
         CONSTRAIN(_root, 0, 11);
         owner_->set_scale_at_slot(owner_->get_scale(slot_index_), mask_, _root, owner_->get_transpose(slot_index_), slot_index_); 
        }
        break;
        case EDIT_TRANSPOSE: 
        {
         int _transpose = owner_->get_transpose(slot_index_) + event.value;
         CONSTRAIN(_transpose, -12, 12);
         owner_->set_scale_at_slot(owner_->get_scale(slot_index_), mask_, owner_->get_root(slot_index_), _transpose, slot_index_); 
        }
        break;
        default:
        break;
      }
  }
  // This isn't entirely atomic
  apply_mask(mask);
  if (scale_changed)
    owner_->scale_changed();
}

void ScaleEditor::move_cursor(int offset) {

  if (EDIT_ROOT == edit_page_ || EDIT_TRANSPOSE == edit_page_) {
    auto page = edit_page_ + offset;
    CONSTRAIN(page, EDIT_ROOT, EDIT_TRANSPOSE);
    edit_page_ = static_cast<ScaleEditorPage>(page);
  } else {
    int cursor_pos = cursor_pos_ + offset;
    const int max = mutable_scale_ ? num_notes_ : num_notes_ - 1;
    CONSTRAIN(cursor_pos, 0, max);
    cursor_pos_ = cursor_pos;
  }
}

void ScaleEditor::handleButtonUp(const UI::Event &event) {

  if (event.mask & CONTROL_BUTTON_L) {
    ui.IgnoreButton(CONTROL_BUTTON_L);
    if (cursor_pos_ == num_notes_)
      reset_scale();
    else
      change_note(cursor_pos_, 128, true);
  } else {
    if (!extended_mode_)
      invert_mask();
    else {
      
      if (ticks_ > 250) {
        slot_index_++;  
        if (slot_index_ > 0x3) 
          slot_index_ = 0;

        set_scale(owner_->get_scale(slot_index_));
      }
    }
  }
}

void ScaleEditor::handleButtonDown(const UI::Event &event) {
  if (event.mask & CONTROL_BUTTON_L) {
    ui.IgnoreButton(CONTROL_BUTTON_L);
    change_note(cursor_pos_, -128, true);
  } else {
    if (!extended_mode_)
      invert_mask();
    else {
      
      if (ticks_ > 250) {
        slot_index_--;  
        if (slot_index_ < 0) 
          slot_index_ = 3; 

        set_scale(owner_->get_scale(slot_index_));
      }
    }
  }
}

void ScaleEditor::handleButtonLeft(const UI::Event &) {

  if (extended_mode_ && ui.read_immediate(CONTROL_BUTTON_UP)) {
    ui.IgnoreButton(CONTROL_BUTTON_UP);

    if (edit_page_ == EDIT_SCALE) 
      edit_page_ = EDIT_ROOT; // switch to root
    // and don't accidentally change scale slot:  
    ticks_ = 0x0;
  }
  else { 
    // edit scale mask
    if (edit_page_ > EDIT_SCALE)
      edit_page_ = EDIT_SCALE;
    else {
      uint16_t m = 0x1 << cursor_pos_;
      uint16_t mask = mask_;
    
      if (cursor_pos_ < num_notes_) {
        // toggle note active state; avoid 0 mask
        if (mask & m) {
          if ((mask & ~(0xffff << num_notes_)) != m)
            mask &= ~m;
        } else {
          mask |= m;
        }
        apply_mask(mask);
      } 
    }
  }
}

void ScaleEditor::invert_mask() {
  uint16_t m = ~(0xffffU << num_notes_);
  uint16_t mask = mask_;
  // Don't invert to zero
  if ((mask & m) != m)
    mask ^= m;
  apply_mask(mask);
}

/*static*/ uint16_t ScaleEditor::RotateMask(uint16_t mask, int num_notes, int amount) {
  uint16_t used_bits = ~(0xffffU << num_notes);
  mask &= used_bits;

  if (amount < 0) {
    amount = -amount % num_notes;
    mask = (mask >> amount) | (mask << (num_notes - amount));
  } else {
    amount = amount % num_notes;
    mask = (mask << amount) | (mask >> (num_notes - amount));
  }
  return mask | ~used_bits; // fill upper bits
}

void ScaleEditor::set_scale(int scale) {
  // SERIAL_PRINTLN("scaled=%d", scale);
  if (scale < Scales::SCALE_USER_LAST) {
    scale_ = mutable_scale_ = &user_scales[scale];
    scale_name_ = scale_names_short[scale];
    // SERIAL_PRINTLN("Editing mutable scale '%s'", scale_name_);
  } else {
    scale_ = &Scales::GetScale(scale);
    mutable_scale_ = nullptr;
    scale_name_ = scale_names_short[scale];
    // SERIAL_PRINTLN("Editing const scale '%s'", scale_name_);
  }
  cursor_pos_ = 0;
  num_notes_ = scale_->num_notes;
  mask_ = owner_->get_scale_mask(slot_index_);
}

void ScaleEditor::reset_scale() {
  // Serial.println("Resetting scale to SEMI");

  *mutable_scale_ = Scales::GetScale(Scales::SCALE_SEMI);
  num_notes_ = mutable_scale_->num_notes;
  cursor_pos_ = num_notes_;
  mask_ = ~(0xfff << num_notes_);
  apply_mask(mask_);
}

void ScaleEditor::change_note(size_t pos, int delta, bool notify) {
  if (mutable_scale_ && pos < num_notes_) {
    int32_t note = mutable_scale_->notes[pos] + delta;

    const int32_t min = pos > 0 ? mutable_scale_->notes[pos - 1] : 0;
    const int32_t max = pos < num_notes_ - 1 ? mutable_scale_->notes[pos + 1] : mutable_scale_->span;

    // TODO It's probably possible to construct a pothological scale,
    // maybe factor cursor_pos into it somehow?
    if (note <= min) note = pos > 0 ? min + 1 : 0;
    if (note >= max) note = max - 1;
    mutable_scale_->notes[pos] = note;
//    braids::SortScale(*mutable_scale_); // TODO side effects?

    if (notify)
      owner_->scale_changed();
  }
}

void ScaleEditor::BeginEditing(int scale) {
  if (extended_mode_) { // == meta-Q
    slot_index_ = owner_->get_slot_index();
    ui.preempt_screensaver(true);
  } else {
    slot_index_ = kDefaultScaleSlot;
  }
  set_scale(scale);
}

void ScaleEditor::Close() {
  ui.SetButtonIgnoreMask();
  ui.preempt_screensaver(false);
  owner_ = nullptr;
  slot_index_ = kDefaultScaleSlot;
  edit_page_ = EDIT_SCALE;
}

} // OC
