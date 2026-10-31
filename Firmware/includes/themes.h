#pragma once
// Colour themes for the LVGL UI, switchable at runtime.
//
// The YAML is written in theme 0 ("Graphite", the original look). Every colour in
// THEMES[0] is a "role"; switching theme walks all LVGL objects and replaces each
// colour that matches a role of the current theme with the same role of the new
// theme. Colours that are not roles (solar/grid/battery data colours, warnings)
// are never touched. Lambdas that set role colours at runtime go through
// lvx_theme::color(), which maps a theme-0 colour to the current theme.
//
// Rules for new themes: no colour may appear twice in one theme, and none may be
// one of the fixed data colours (checked by the generator that wrote this table).
#include "lvgl.h"
#include "lvgl_private.h"  // lv_display_t::screens, to reach every page
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace lvx_theme {

enum Role { PAGE, CARD, BUTTON, BUTTON2, BORDER, BORDER2, DIVIDER, PILL, INACTIVE, SHADOW, TEXT, TEXT2, TEXT3, TEXT4, TEXT5, TINT, LINE, ACCENT, CONTROL, ROLE_COUNT };

struct Theme {
  const char *name;
  const char *desc;
  uint32_t c[ROLE_COUNT];
};

static const Theme THEMES[] = {
    {"Graphite", "Default", {0x141B21, 0x1C2229, 0x23282F, 0x2E3A46, 0x333333, 0x2A323A, 0x3A3F46, 0x424242, 0x444444, 0x222222, 0xFFFFFF, 0x9E9E9E, 0xDDDDDD, 0xE0E0E0, 0x9AA4AE, 0xB4DCFF, 0x334455, 0x2E7D32, 0x2196F3}},
    {"Midnight", "Deep navy", {0x0B1120, 0x111A2E, 0x17223A, 0x1E2B47, 0x22304D, 0x1C2840, 0x2A3A5C, 0x2A3550, 0x2F3B57, 0x070B14, 0xF1F5F9, 0x94A3B8, 0xCBD5E1, 0xE2E8F0, 0x8B9BB4, 0x93C5FD, 0x2B3F66, 0x3B82F6, 0x60A5FA}},
    {"Nord", "Arctic frost", {0x2E3440, 0x3B4252, 0x434C5E, 0x4C566A, 0x4E5870, 0x465066, 0x56617A, 0x4A5468, 0x505A70, 0x242933, 0xECEFF4, 0xA3ABB9, 0xD8DEE9, 0xE5E9F0, 0x9AA5B8, 0x8FBCBB, 0x5E81AC, 0x88C0D0, 0x81A1C1}},
    {"Dracula", "Purple night", {0x1E1F29, 0x282A36, 0x343746, 0x3B3E50, 0x44475A, 0x383A4C, 0x4D5066, 0x464A5E, 0x4A4D61, 0x16171F, 0xF8F8F2, 0x9EA1BD, 0xE2E2DC, 0xEDEDE7, 0x8F93B3, 0x8BE9FD, 0x6272A4, 0xBD93F9, 0xFF79C6}},
    {"Forest", "Deep green", {0x0D1712, 0x14231B, 0x1A2C22, 0x21372A, 0x284232, 0x20342A, 0x2F4D3B, 0x2A3D32, 0x304536, 0x08100C, 0xF0FDF4, 0x9DB8A9, 0xD1E7DA, 0xE3F2E9, 0x8FAA9B, 0xA7F3D0, 0x2F5B45, 0x22C55E, 0x4ADE80}},
    {"Ember", "Warm amber", {0x17110D, 0x221913, 0x2B2019, 0x35281F, 0x3D2F25, 0x33271F, 0x47372B, 0x3F3128, 0x45362C, 0x0F0B08, 0xFFF7ED, 0xBBA898, 0xF0E2D3, 0xF7EBDD, 0xA89585, 0xFDBA74, 0x5A4332, 0xF59E0B, 0xFBBF24}},
    {"OLED", "True black", {0x020202, 0x0D0D0D, 0x141414, 0x1A1A1A, 0x262626, 0x1F1F1F, 0x2E2E2E, 0x2A2A2A, 0x303030, 0x010101, 0xFAFAFA, 0x8C8C8C, 0xD4D4D4, 0xE5E5E5, 0x7F7F7F, 0x99F6E4, 0x333B3A, 0x14B8A6, 0x2DD4BF}},
    {"Plum", "Rose & plum", {0x1A1218, 0x251A22, 0x2E2029, 0x382731, 0x42303B, 0x372830, 0x4C3845, 0x45333E, 0x4A3843, 0x110B10, 0xFDF2F8, 0xB89AAB, 0xEED9E4, 0xF6E6EE, 0xA88B9B, 0xF9A8D4, 0x5C3F51, 0xEC4899, 0xF472B6}},
};
static constexpr int COUNT = sizeof(THEMES) / sizeof(THEMES[0]);

inline int &current() {
  static int cur = 0;  // the YAML is drawn in theme 0
  return cur;
}
// Bumped on every switch, so caches of "last applied style" know to re-apply.
inline uint32_t &epoch() {
  static uint32_t e = 0;
  return e;
}

// The current theme's version of a colour that the YAML writes in theme 0.
inline uint32_t color(uint32_t base) {
  int cur = current();
  if (cur == 0)
    return base;
  for (int r = 0; r < ROLE_COUNT; r++) {
    if (THEMES[0].c[r] == base)
      return THEMES[cur].c[r];
  }
  return base;
}

static const lv_style_prop_t COLOR_PROPS[] = {
    LV_STYLE_BG_COLOR,     LV_STYLE_BG_GRAD_COLOR, LV_STYLE_BORDER_COLOR, LV_STYLE_OUTLINE_COLOR, LV_STYLE_SHADOW_COLOR,
    LV_STYLE_TEXT_COLOR,   LV_STYLE_LINE_COLOR,    LV_STYLE_ARC_COLOR,    LV_STYLE_IMAGE_RECOLOR,
};
static const lv_style_selector_t PARTS[] = {
    LV_PART_MAIN, LV_PART_INDICATOR, LV_PART_KNOB, LV_PART_ITEMS, LV_PART_SELECTED, LV_PART_CURSOR,
};

inline void remap_(lv_obj_t *obj, const Theme &from, const Theme &to, lv_obj_t *skip) {
  if (obj == nullptr || obj == skip)
    return;
  for (lv_style_selector_t part : PARTS) {
    for (lv_style_prop_t prop : COLOR_PROPS) {
      lv_style_value_t v;
      if (lv_obj_get_local_style_prop(obj, prop, &v, part) != LV_STYLE_RES_FOUND)
        continue;
      uint32_t rgb = lv_color_to_u32(v.color) & 0xFFFFFF;
      for (int r = 0; r < ROLE_COUNT; r++) {
        if (from.c[r] != rgb)
          continue;
        v.color = lv_color_hex(to.c[r]);
        lv_obj_set_local_style_prop(obj, prop, v, part);
        break;
      }
    }
  }
  uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; i++)
    remap_(lv_obj_get_child(obj, i), from, to, skip);
}

// Switch every page (and the top layer) to theme `idx`. `skip` is left as it is,
// e.g. the preview tiles that always show their own theme's colours.
inline void apply(int idx, lv_obj_t *skip = nullptr) {
  if (idx < 0 || idx >= COUNT)
    idx = 0;
  int cur = current();
  lv_display_t *disp = lv_display_get_default();
  if (idx == cur || disp == nullptr)
    return;
  const Theme &from = THEMES[cur];
  const Theme &to = THEMES[idx];
  uint32_t start = esphome::millis();
  for (uint32_t i = 0; i < disp->screen_cnt; i++)
    remap_(disp->screens[i], from, to, skip);
  remap_(lv_display_get_layer_top(disp), from, to, skip);
  current() = idx;
  epoch()++;
  ESP_LOGI("theme", "Theme '%s' applied in %u ms", to.name, (unsigned) (esphome::millis() - start));
}

}  // namespace lvx_theme
