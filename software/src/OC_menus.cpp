#include <Arduino.h>
#include <array>
#include "OC_config.h"
#include "OC_core.h"
#include "OC_bitmaps.h"
#include "OC_menus.h"
#include "OC_DAC.h"
#include "OC_options.h"
#include "util/util_templates.h"

namespace OC {

struct coords {
  weegfx::coord_t x, y;
};

static constexpr float note_circle_r = 28.f;
static constexpr float pi = 3.14159265358979323846f;
static constexpr float semitone_radians = (2.f * pi / 12.f);
constexpr float index_to_rads(size_t index) { return ((index + 12 - 3) % 12) * semitone_radians; }

template <size_t index> constexpr coords generate_circle_coords() {
  return {
    static_cast<weegfx::coord_t>(note_circle_r * cosf(index_to_rads(index))),
    static_cast<weegfx::coord_t>(note_circle_r * sinf(index_to_rads(index)))
  };
}

template <size_t ...Is>
constexpr std::array<coords, sizeof...(Is)> generate_circle_pos_lut(util::index_sequence<Is...>) {
  return { generate_circle_coords<Is>()... };
}

static constexpr std::array<coords, 12> circle_pos_lut = generate_circle_pos_lut(util::make_index_sequence<12>::type());

namespace menu {
void Init()
{
};

void DrawEditIcon(weegfx::coord_t x, weegfx::coord_t y, int value, int min_value, int max_value) {
  const uint8_t *src = OC::bitmap_edit_indicators_8;
  if (value == max_value)
    src += OC::kBitmapEditIndicatorW * 2;
  else if (value == min_value)
    src += OC::kBitmapEditIndicatorW;

  graphics.drawBitmap8(x - 5, y + 1, OC::kBitmapEditIndicatorW, src);
}

void DrawEditIcon(weegfx::coord_t x, weegfx::coord_t y, int value, const settings::value_attr &attr) {
  const uint8_t *src = OC::bitmap_edit_indicators_8;
  if (value == attr.max_)
    src += OC::kBitmapEditIndicatorW * 2;
  else if (value == attr.min_)
    src += OC::kBitmapEditIndicatorW;

  graphics.drawBitmap8(x - 5, y + 1, OC::kBitmapEditIndicatorW, src);
}

}; // namespace menu


void visualize_pitch_classes(uint8_t *normalized, weegfx::coord_t centerx, weegfx::coord_t centery) {
  graphics.drawCircle(centerx, centery, note_circle_r);

  coords last_pos = circle_pos_lut[normalized[0]];
  last_pos.x += centerx;
  last_pos.y += centery;
  for (size_t i = 1; i < 3; ++i) {
    graphics.drawBitmap8(last_pos.x - 3, last_pos.y - 3, 8, OC::circle_disk_bitmap_8x8);
    coords current_pos = circle_pos_lut[normalized[i]];
    current_pos.x += centerx;
    current_pos.y += centery;
    graphics.drawLine(last_pos.x, last_pos.y, current_pos.x, current_pos.y);
    last_pos = current_pos;
  }
  graphics.drawLine(last_pos.x, last_pos.y, circle_pos_lut[normalized[0]].x + centerx, circle_pos_lut[normalized[0]].y + centery);
  graphics.drawBitmap8(last_pos.x - 3, last_pos.y - 3, 8, OC::circle_disk_bitmap_8x8);
}

/* ----------- screensaver ----------------- */
void screensaver() {
  weegfx::coord_t x = 8;
  uint8_t y, width = 8;
  for(int i = 0; i < 4; i++, x += 32 ) { 
    y = DAC::value(i) >> 10; 
    y++; 
    graphics.drawRect(x, 64-y, width, width); // replace second 'width' with y for bars.
  }
}

static const size_t kScopeDepth = 64;

uint16_t scope_history[DAC::kHistoryDepth];
uint16_t averaged_scope_history[DAC_CHANNEL_LAST][kScopeDepth];
size_t averaged_scope_tail = 0;
DAC_CHANNEL scope_update_channel = DAC_CHANNEL_A;

template <size_t size>
inline uint16_t calc_average(const uint16_t *data) {
  uint32_t sum = 0;
  size_t n = size;
  while (n--)
    sum += *data++;
  return sum / size;
}

template <unsigned rshift, uint16_t bitmask>
void scope_averaging() {
    switch (scope_update_channel) {
    case DAC_CHANNEL_A:
      DAC::getHistory<DAC_CHANNEL_A>(scope_history);
      averaged_scope_history[DAC_CHANNEL_A][averaged_scope_tail] = ((65535U - calc_average<DAC::kHistoryDepth>(scope_history)) >> rshift) & bitmask;
      scope_update_channel = DAC_CHANNEL_B;
      break;
    case DAC_CHANNEL_B:
      DAC::getHistory<DAC_CHANNEL_B>(scope_history);
      averaged_scope_history[DAC_CHANNEL_B][averaged_scope_tail] = ((65535U - calc_average<DAC::kHistoryDepth>(scope_history)) >> rshift) & bitmask;
      scope_update_channel = DAC_CHANNEL_C;
      break;
    case DAC_CHANNEL_C:
      DAC::getHistory<DAC_CHANNEL_C>(scope_history);
      averaged_scope_history[DAC_CHANNEL_C][averaged_scope_tail] = ((65535U - calc_average<DAC::kHistoryDepth>(scope_history)) >> rshift) & bitmask;
      scope_update_channel = DAC_CHANNEL_D;
      break;
    case DAC_CHANNEL_D:
      DAC::getHistory<DAC_CHANNEL_D>(scope_history);
      averaged_scope_history[DAC_CHANNEL_D][averaged_scope_tail] = ((65535U - calc_average<DAC::kHistoryDepth>(scope_history)) >> rshift) & bitmask;
      scope_update_channel = DAC_CHANNEL_A;
      averaged_scope_tail = (averaged_scope_tail + 1) % kScopeDepth;
      break;
    default: break;
  }
}

void scope_render() {
  scope_averaging<11, 0x1f>();

  for (weegfx::coord_t x = 0; x < (weegfx::coord_t)kScopeDepth - 1; ++x) {
    size_t index = (x + averaged_scope_tail + 1) % kScopeDepth;
    #ifdef BUCHLA_4U
      graphics.setPixel(x, 0 + averaged_scope_history[DAC_CHANNEL_C][index]);
      graphics.setPixel(64 + x, 0 + averaged_scope_history[DAC_CHANNEL_D][index]);
      graphics.setPixel(x, 32 + averaged_scope_history[DAC_CHANNEL_A][index]);
      graphics.setPixel(64 + x, 32 + averaged_scope_history[DAC_CHANNEL_B][index]);  
    #else
      graphics.setPixel(x, 0 + averaged_scope_history[DAC_CHANNEL_A][index]);
      graphics.setPixel(64 + x, 0 + averaged_scope_history[DAC_CHANNEL_B][index]);
      graphics.setPixel(x, 32 + averaged_scope_history[DAC_CHANNEL_C][index]);
      graphics.setPixel(64 + x, 32 + averaged_scope_history[DAC_CHANNEL_D][index]);
    #endif
  }
}

void vectorscope_render() {
  scope_averaging<10, 0x3f>();

  for (weegfx::coord_t x = 0; x < (weegfx::coord_t)kScopeDepth - 1; ++x) {
    size_t index = (x + averaged_scope_tail + 1) % kScopeDepth;
    graphics.setPixel(averaged_scope_history[DAC_CHANNEL_A][index], averaged_scope_history[DAC_CHANNEL_B][index]);
    graphics.setPixel(64 + averaged_scope_history[DAC_CHANNEL_C][index], averaged_scope_history[DAC_CHANNEL_D][index]);
  }
}

}; // namespace OC
