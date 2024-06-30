// Copyright 2019 Patrick Dowling
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
#ifndef OC_IO_SETTINGS_MENU_H_
#define OC_IO_SETTINGS_MENU_H_

#include <array>
#include "OC_io.h"
#include "OC_menus.h"
#include "OC_ui.h"
#include "OC_visualfx.h"

namespace OC {

class AppBase;

class IOSettingsMenu {
public:

  void Edit(AppBase *app);
  void Close();

  inline bool active() const {
    return io_settings_ != nullptr;
  }

  void Update();

  void Draw() const;
  UiMode DispatchEvent(const UI::Event &event);

private:
  menu::ScreenCursor<menu::kScreenLines> cursor_;
  int selected_channel_ = 0;

  IOSettings *io_settings_ = nullptr;
  IOConfig io_config_;

  vfx::Marquee<11> marquee_;

  // Cached labels
  char labels_[IOSettings::kSettingsPerChannel][kMaxIOLabelLength + 1];

  bool autotune_available() const;
  void SetChannel(int channel);
};

} // namespace OC

#endif // OC_IO_SETTINGS_MENU_H_
