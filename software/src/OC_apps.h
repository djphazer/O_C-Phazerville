// Copyright (c) 2016-2019 Patrick Dowling
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

#ifndef OC_APP_H_
#define OC_APP_H_

#include "OC_options.h"
#include "OC_io.h"
#include "OC_ui.h"
#include "src/UI/ui_events.h"
#include "util/util_turing.h"
#include "util/util_misc.h"

#ifdef OC_DEBUG_APPS
# define APPS_SERIAL_PRINTLN(msg, ...) serial_printf(msg "\n", ##__VA_ARGS__)
#else
# define APPS_SERIAL_PRINTLN(...)
#endif

namespace OC {

void draw_save_message(uint8_t c);
void SaveAppData();

static inline void save_app_data() { SaveAppData(); }

enum AppEvent {
  APP_EVENT_SUSPEND,
  APP_EVENT_RESUME,
  APP_EVENT_SCREENSAVER_ON,
  APP_EVENT_SCREENSAVER_OFF
};

// The original "app" interface was built around structs filled with function
// pointers, i.e. without any form of "this" pointer. Mainly because that's
// how it evolved out of the original (single-app) firmware and protoapps.
//
// Adding the IO settings to each app, and having them be able to access it
// from within meant either:
// a) adding it as a parameter to all functions, or
// b) biting the refactoring bullet.
//
// Obviously, the choice was c).
//
// Using virtual functions make it (slightly) less trivial to instantiate an
// app (i.e. it can't just be an array of app) but that's the price to pay...
//
// It's possible there might be optimized ways of calling the virtual functions
// but only the ISR (Process) is in the critical path.
//
class AppBase {
public:
  virtual ~AppBase() { }
  
  inline uint16_t id() const { return id_; }
  inline const char *name() const { return name_; }

  IOSettings &mutable_io_settings() { return io_settings_; }
  const IOSettings &io_settings() const { return io_settings_; }

  void InitDefaults();
  size_t storage_size() const { return IOSettings::storageSize() + appdata_storage_size(); }
  size_t Save(util::StreamBufferWriter &) const;
  size_t Restore(util::StreamBufferReader &);
  void Draw(UiMode ui_mode) const;
  UiMode DispatchEvent(const UI::Event &);
  void DispatchAppEvent(AppEvent);
  void EditIOSettings();
  void DispatchLoop();

  // Main implementation interface for derived classes
  virtual void Init() = 0;
  virtual size_t appdata_storage_size() const = 0;
  virtual size_t SaveAppData(util::StreamBufferWriter &) const = 0;
  virtual size_t RestoreAppData(util::StreamBufferReader &) = 0;
  virtual void HandleAppEvent(AppEvent) = 0;
  virtual void Loop() = 0;
  virtual void DrawMenu() const = 0;
  virtual void DrawScreensaver() const = 0;
  virtual void HandleButtonEvent(const UI::Event &) = 0;
  virtual void HandleEncoderEvent(const UI::Event &) = 0;
  virtual void Process(IOFrame *ioframe) = 0;
  virtual void GetIOConfig(IOConfig &) const = 0;
  virtual void DrawDebugInfo() const = 0;

private:
  const uint16_t id_;
  const char * const name_;
  const char * const boring_name_;

protected:
  IOSettings io_settings_;

  AppBase(uint16_t appid, const char *const appname, const char *const boring_name)
  : id_(appid), name_(appname), boring_name_(boring_name) { }

  virtual bool io_settings_allowed() const {
    return true;
  }

  uint32_t io_settings_status_mask() const;

  DISALLOW_COPY_AND_ASSIGN(AppBase);
};

void draw_save_message(uint8_t c);
void save_app_data();
void start_calibration();

template <typename T, typename Traits> class AppBaseImpl : public AppBase {
public:
  static constexpr uint16_t kAppId = Traits::id;
  static constexpr const char *const kAppName = Traits::app_name;
  static constexpr const char *const kBoringAppName = Traits::boring_app_name;
protected:
  AppBaseImpl() : AppBase(kAppId, kAppName, kBoringAppName) { }
};

#define OC_APP_TRAITS(clazz, app_id, name, boring_name) \
struct MACRO_CONCAT(clazz, Traits) { \
  static constexpr uint16_t id = app_id; \
  static constexpr const char *const app_name = name; \
  static constexpr const char *const boring_app_name = boring_name; \
}

#define OC_APP_CLASS(clazz) \
clazz : public OC::AppBaseImpl<clazz, MACRO_CONCAT(clazz, Traits)>

#define OC_APP_INTERFACE_DECLARE(clazz) \
public: \
  clazz() : OC::AppBaseImpl<clazz, MACRO_CONCAT(clazz, Traits)>() { } \
  virtual void Init() final; \
  virtual size_t appdata_storage_size() const final { return kAppDataStorageSize; } \
  virtual size_t SaveAppData(util::StreamBufferWriter &) const final; \
  virtual size_t RestoreAppData(util::StreamBufferReader &) final; \
  virtual void HandleAppEvent(OC::AppEvent) final; \
  virtual void Loop() final; \
  virtual void DrawMenu() const final; \
  virtual void DrawScreensaver() const final; \
  virtual void HandleButtonEvent(const UI::Event &) final; \
  virtual void HandleEncoderEvent(const UI::Event &) final; \
  virtual void Process(OC::IOFrame *ioframe) final; \
  virtual void GetIOConfig(OC::IOConfig &) const final; \
  virtual void DrawDebugInfo() const final

#define OC_APP_STORAGE_SIZE(s) \
  static constexpr size_t kAppDataStorageSize = s;

}; // namespace OC

#endif // OC_APP_H_
