// Copyright 2015-2019 Patrick Dowling, Max Stadler
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

#ifndef OC_SCALE_EDIT_H_
#define OC_SCALE_EDIT_H_

#include <stdint.h>
#include "OC_bitmaps.h"
#include "OC_menus.h"
#include "OC_strings.h"
#include "OC_ui.h"

namespace OC {

// Scale editor
// Edits both scale length and note note values, as well as a mask of "active"
// notes that the quantizer uses; some scales are read-only, in which case
// only the mask is editable.
// There's a specialized mode for Meta-Q that also allows editing of root/
// transpose, which might be useable for others as well with some tweaks (or,
// it might make sense to use a template parameter to distinguish between the
// two modes.
//
// The owner class needs to provide callbacks to get notification when values
// change: see ASR, A_SEQ, DQ, etc for details
//

enum ScaleEditorPage {
  EDIT_SCALE,
  EDIT_ROOT,
  EDIT_TRANSPOSE,
};

static constexpr int kDefaultScaleSlot = 0;

// Originally the Owner was a template parameter, which was okay(ish) until
// enough different variants were being used. Using an interface causes some
// other side effects, but since this is almost 100% UI it's not a time-
// critical path.
// A next step might be to specialize the event handler since there are some
// fairly significant differences between the different edit modes.
class ScaleEditorEventHandler {
public:
  virtual ~ScaleEditorEventHandler() = default;

  virtual int get_scale(int slot_index) const = 0;
  virtual uint16_t get_scale_mask(int slot_index) const = 0;

  virtual void update_scale_mask(uint16_t mask, int slot_index) = 0;
  virtual void scale_changed() = 0;

  // These only affect extended_mode_ == true so they get a default implementation
  virtual int get_root(int slot_index) const;
  virtual int get_transpose(int slot_index) const;
  virtual int get_slot_index() const;
  virtual void set_scale_at_slot(int scale, uint16_t mask, int root, int transpose, int slot_index);
};

// Pop-up UI for editing scales. Not pretty, but gets the job done.
class ScaleEditor {
public:
  ScaleEditor() { }
  ~ScaleEditor() { }

  void Init(bool extended_mode = false);

  void Edit(ScaleEditorEventHandler *owner, int scale);

  inline bool active() const {
    return nullptr != owner_;
  }

  void Close();
  void Draw() const;
  void HandleButtonEvent(const UI::Event &event);
  void HandleEncoderEvent(const UI::Event &event);
  static uint16_t RotateMask(uint16_t mask, int num_notes, int amount);

private:

  ScaleEditorEventHandler *owner_;
  const char * scale_name_;
  const braids::Scale *scale_;
  Scale *mutable_scale_;

  uint16_t mask_;
  size_t cursor_pos_;
  size_t num_notes_;
  int8_t slot_index_;
  bool extended_mode_;
  ScaleEditorPage edit_page_; 
  mutable uint32_t ticks_;

  void BeginEditing(int scale);
  void move_cursor(int offset);

  void invert_mask(); 

  void apply_mask(uint16_t mask) {
    if (mask_ != mask) {
      mask_ = mask;
      owner_->update_scale_mask(mask_, slot_index_);
    }
  }

  void set_scale(int scale);

  void reset_scale();
  void change_note(size_t pos, int delta, bool notify);
  void handleButtonLeft(const UI::Event &event);
  void handleButtonUp(const UI::Event &event);
  void handleButtonDown(const UI::Event &event);

  DISALLOW_COPY_AND_ASSIGN(ScaleEditor);
};

}; // namespace OC

#endif // OC_SCALE_EDIT_H_
