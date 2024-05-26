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
#include "src/UI/ui_event_queue.h"

namespace OC {

struct App;

class IOSettingsMenu {
public:

  void Init();

  void Edit(App *app);

  inline bool active() const {
    return (io_settings_ != nullptr);
  }

  void Draw() const;
  void DispatchEvent(const UI::Event &event);

private:
  //bool enabled_ = false;

  menu::ScreenCursor<menu::kScreenLines> cursor_;
  int selected_channel_;

  IOSettings *io_settings_;
  IOConfig io_config_;

  void DrawInputSettingsPage() const;
  void DrawOutputPage() const;
  void DrawTodoPage() const;
};

} // namespace OC

#endif // OC_IO_SETTINGS_MENU_H_
