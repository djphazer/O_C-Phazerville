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
#ifndef OC_APP_SWITCHER_H_
#define OC_APP_SWITCHER_H_

#include <array>
#include <tuple>
#include "OC_apps.h"
#include "OC_io.h"
#include "OC_storage.h"
#include "util/util_templates.h"

// lightweight type switcher
struct RuntimeSlot {
  void* instance = nullptr;
  void (*Process)(void* data, OC::IOFrame * ioframe);
  void (*InitDefaults)(void* data);
  const uint16_t id() const { if (instance) return static_cast<OC::AppBase *>(instance)->id(); return 0; }
  const char* name() const { if (instance) return static_cast<OC::AppBase *>(instance)->name(); return "?_?"; }
  const OC::IOSettings& io_settings() const { return static_cast<OC::AppBase *>(instance)->io_settings(); }
};

namespace OC { 

// Minimal app-switching helper/manager/general dogsbody.
// Use with care.
class AppSwitcher {
public:
  AppSwitcher() { }
  ~AppSwitcher() { }

  void Init(bool reset_settings);

  void set_current_app(size_t index);
  inline AppBase *current_app() const { return static_cast<AppBase *>(current_app_.instance); }
  inline const RuntimeSlot &current_slot() const { return current_app_; }

  inline void Process(IOFrame *ioframe) __attribute__((always_inline)) {
    if (current_app_.instance) {
      IO::Read(ioframe, &current_app_.io_settings());
      current_app_.Process(current_app_.instance, ioframe);
      IO::Write(ioframe, &current_app_.io_settings());
    }
  }

private:
  RuntimeSlot current_app_;

  DISALLOW_COPY_AND_ASSIGN(AppSwitcher);
};

extern AppSwitcher app_switcher;

// Helpers

// creates direct call binding for Process functions (avoids vtable)
template <typename AppType>
RuntimeSlot bind_to_slot(AppType *app) {
  if (!app) return {};

  RuntimeSlot slot;
  slot.instance = app;
  slot.Process = [](void* data, IOFrame * ioframe) {
    static_cast<AppType*>(data)->Process(ioframe);
  };
  slot.InitDefaults = [](void* data) {
    static_cast<AppType*>(data)->InitDefaults();
  };
  return slot;
}

// Quick helper variable template (Standard C++17 style) - to be used later
//template <typename T> inline constexpr bool has_override_v = has_override<T>::value;


template <typename Tuple, size_t... Is>
std::array<RuntimeSlot, std::tuple_size<Tuple>::value> tuple_to_array(Tuple &tuple, util::index_sequence<Is...>) {
    return std::array<RuntimeSlot, std::tuple_size<Tuple>::value>{ bind_to_slot(&std::get<Is>(tuple))... };
}

template <typename Tuple>
std::array<RuntimeSlot, std::tuple_size<Tuple>::value> tuple_to_array(Tuple &tuple) {
    return tuple_to_array(tuple, typename util::make_index_sequence<std::tuple_size<Tuple>::value>::type());
}

template <typename Empty, typename... Ts>
class AppContainer {
private:
  using TupleType = std::tuple<Ts...>;
  using ArrayType = std::array<RuntimeSlot, sizeof...(Ts)>;

  TupleType instances_;
  const ArrayType slots_ = tuple_to_array(instances_);

public:

  static_assert(sizeof...(Ts) >= 1, "At least one app type required");

  template <size_t N>
  static constexpr uint16_t GetAppIDAtIndex() {
    return std::tuple_element<N, TupleType>::type::kAppId;
  }

  static constexpr size_t TotalAppDataStorageSize() {
    return util::sum<size_t, AppStorageChunkSize<Ts>::value...>::value;
  }

  static constexpr size_t kNumApps = sizeof...(Ts);

  constexpr size_t num_apps() const {
    return kNumApps;
  }

  inline const RuntimeSlot& operator[](size_t i) const {
    return slots_[i < kNumApps ? i : 0];
  }

  template <typename T>
  void for_each(T op) {
    std::for_each(std::begin(slots_), std::end(slots_), op);
  }

  AppBase *FindAppByID(uint16_t id) const {
    auto result =
      std::find_if(std::begin(slots_), std::end(slots_),
                   [&id](RuntimeSlot app) { return app.id() == id; });
    return result != std::end(slots_) ? static_cast<AppBase*>(result->instance) : nullptr;
  }

  size_t IndexOfAppByID(uint16_t id) const {
    auto result =
      std::find_if(std::begin(slots_), std::end(slots_),
                   [&id](RuntimeSlot app) { return app.id() == id; });
    return result != std::end(slots_) ? std::distance(std::begin(slots_), result) : num_apps();
  }
};

} // OC

#endif // OC_APP_SWITCHER_H_
