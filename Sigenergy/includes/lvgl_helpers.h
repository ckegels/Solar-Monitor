#pragma once
// Small LVGL performance helpers, included from solar-display.yaml.
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esphome/core/log.h"
#include "lvgl.h"

// Skip LVGL label updates when the text has not changed.
// lv_label_set_text() always re-measures and redraws a label, even when the new
// text is identical, and this firmware refreshes many labels several times a
// second. Those needless redraws compete with scrolling and animations.
static inline void lvx_label_set_text_if_changed(lv_obj_t *obj, const char *text) {
  if (obj != nullptr && text != nullptr) {
    const char *cur = lv_label_get_text(obj);
    if (cur != nullptr && strcmp(cur, text) == 0)
      return;
  }
  lv_label_set_text(obj, text);
}

// Let LVGL render into a small strip buffer in fast internal RAM instead of the
// full-screen buffer ESPHome puts in (much slower) PSRAM. LVGL then draws the
// screen in strips of `lines` rows. Falls back to the PSRAM buffer if there is
// not enough internal RAM.
static inline void lvx_use_internal_draw_buffer(uint32_t lines) {
  lv_display_t *disp = lv_display_get_default();
  if (disp == nullptr)
    return;
  uint32_t stride = lv_draw_buf_width_to_stride(lv_display_get_horizontal_resolution(disp), LV_COLOR_FORMAT_RGB565);
  size_t bytes = (size_t) stride * lines;
  void *buf = heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (buf != nullptr)
    lv_display_set_buffers(disp, buf, nullptr, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

// Log which draw buffer LVGL is using (call it once the log connection is up).
static inline void lvx_log_draw_buffer() {
  lv_display_t *disp = lv_display_get_default();
  lv_draw_buf_t *db = disp ? lv_display_get_buf_active(disp) : nullptr;
  if (db == nullptr)
    return;
  bool internal = esp_ptr_internal(db->data);
  ESP_LOGI("lvgl", "Draw buffer: %u bytes in %s RAM; internal RAM free %u bytes (largest block %u, lowest %u)",
           (unsigned) db->data_size, internal ? "internal" : "PSRAM",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned) heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}

// Route every lv_label_set_text() in the YAML lambdas through the check above.
// (Only affects code after this header: the generated main.cpp, not LVGL itself.)
#define lv_label_set_text(obj, text) lvx_label_set_text_if_changed((obj), (text))
