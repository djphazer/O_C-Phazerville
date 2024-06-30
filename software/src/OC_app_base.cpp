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

#include "OC_apps.h"
#include "OC_global_settings.h"
#include "OC_io_settings_menu.h"

namespace OC {

// This isn't necessarily ideal place for this to live, but it's simultaneously
// global (there can be only one) and app-specific. Really we kind of want some
// kind of popup/overlay/menu stack.
static IOSettingsMenu io_settings_menu;

void AppBase::InitDefaults()
{
  io_settings_.InitDefaults();
  Init();
}

size_t AppBase::Save(util::StreamBufferWriter &stream_buffer) const
{
  io_settings_.Save(stream_buffer);
  SaveAppData(stream_buffer);
  return stream_buffer.written();
}

size_t AppBase::Restore(util::StreamBufferReader &stream_buffer)
{
  io_settings_.Restore(stream_buffer);
  RestoreAppData(stream_buffer);
  return stream_buffer.read();
}

void AppBase::Draw(UiMode ui_mode) const
{
  if (UI_MODE_MENU == ui_mode) {
    if (!io_settings_menu.active())
      DrawMenu();
    else
      io_settings_menu.Draw();
  } else {
    DrawScreensaver();
  }
}

UiMode AppBase::DispatchEvent(const UI::Event &event)
{
  UiMode mode = UI_MODE_MENU;
  if (!io_settings_menu.active()) {
// TODO: VOR/Vbias button gestures
#if 0
      case UI::EVENT_BUTTON_PRESS:
#ifdef VOR
        if (OC::CONTROL_BUTTON_M == event.control) {
            VBiasManager *vbias_m = vbias_m->get();
            vbias_m->AdvanceBias();
        } else
#endif
        app->HandleButtonEvent(event);
        break;
      case UI::EVENT_BUTTON_DOWN:
#ifdef VOR
        // dual encoder press
        if ( ((OC::CONTROL_BUTTON_L | OC::CONTROL_BUTTON_R) == event.mask) )
        {
            VBiasManager *vbias_m = vbias_m->get();
            vbias_m->AdvanceBias();
            SetButtonIgnoreMask(); // ignore release and long-press
        }
        else
#endif
            app->HandleButtonEvent(event);
        break;
#endif

    if (UI::EVENT_BUTTON_PRESS == event.type)
      HandleButtonEvent(event);
    else if (UI::EVENT_ENCODER == event.type)
      HandleEncoderEvent(event);
  } else {
    mode = io_settings_menu.DispatchEvent(event);
  }
  return mode;
}

void AppBase::DispatchAppEvent(AppEvent app_event)
{
  switch(app_event) {
    case APP_EVENT_RESUME:
    io_settings_menu.Close();
    default:
    break;
  }

  // NOTE it might make sense/simplify things further to split this into
  // dedicated functions for the derived classes (OnSuspend/OnResume etc) since
  // a lot of the apps have very similar, mostly empty implementations.
  HandleAppEvent(app_event);
}

void AppBase::EditIOSettings()
{
  if (io_settings_allowed()) {
    APPS_SERIAL_PRINTLN("EditIOSettings(%s)", name());
    io_settings_menu.Edit(this);
  }
}

void AppBase::DispatchLoop()
{
  if (io_settings_menu.active())
    io_settings_menu.Update();

  Loop();
}

uint32_t AppBase::io_settings_status_mask() const
{
  return io_settings_.status_mask() &
         global_settings.autotune_calibration_data.valid_mask();
}

} // OC
