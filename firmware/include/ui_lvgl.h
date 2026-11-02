#pragma once
/**
 * @file ui_lvgl.h
 * SpeedTracer Cycling Computer - LVGL 9 Figma-Matched Component Architecture
 * Implements Dashboard, Metrics, Map, and QuickSettings views using native LVGL widgets
 */

#include <lvgl.h>

// ── Color Theme Definitions ──────────────────────────────────────────────────
typedef struct {
  lv_color_t bg;
  lv_color_t card;
  lv_color_t card_hr;
  lv_color_t card_pwr;
  lv_color_t text_primary;
  lv_color_t text_secondary;
  lv_color_t accent_teal;
  lv_color_t accent_orange;
  lv_color_t accent_red;
} UiTheme;

static inline UiTheme get_theme(bool dark) {
  UiTheme t;
  if (dark) {
    t.bg             = lv_color_hex(0x000000);
    t.card           = lv_color_hex(0x181818);
    t.card_hr        = lv_color_hex(0x0C260C);
    t.card_pwr       = lv_color_hex(0x282300);
    t.text_primary   = lv_color_hex(0xFFFFFF);
    t.text_secondary = lv_color_hex(0x9E9E9E);
    t.accent_teal    = lv_color_hex(0x26C6DA);
    t.accent_orange  = lv_color_hex(0xFF7043);
    t.accent_red     = lv_color_hex(0xEF5350);
  } else {
    t.bg             = lv_color_hex(0xFFFFFF);
    t.card           = lv_color_hex(0xF2F2F4);
    t.card_hr        = lv_color_hex(0xDDF4DD);
    t.card_pwr       = lv_color_hex(0xFFFDE6);
    t.text_primary   = lv_color_hex(0x1C1C1E);
    t.text_secondary = lv_color_hex(0x6E6E73);
    t.accent_teal    = lv_color_hex(0x00897B);
    t.accent_orange  = lv_color_hex(0xF4511E);
    t.accent_red     = lv_color_hex(0xE53935);
  }
  return t;
}

// ── UI View Objects ──────────────────────────────────────────────────────────
typedef struct {
  lv_obj_t *root;
  lv_obj_t *label_time;
  lv_obj_t *label_battery;

  // Heart Rate Card
  lv_obj_t *hr_card;
  lv_obj_t *label_hr_avg;
  lv_obj_t *label_hr_val;
  lv_obj_t *label_hr_max;
  lv_obj_t *hr_zone_bars[5];
  lv_obj_t *hr_zone_pointer;

  // Hero Speed Card
  lv_obj_t *speed_card;
  lv_obj_t *speed_arc;
  lv_obj_t *label_speed_avg;
  lv_obj_t *label_speed_val;
  lv_obj_t *label_speed_max;
  lv_obj_t *label_distance;
  lv_obj_t *label_elapsed;
  lv_obj_t *label_gradient;

  // Calories & Satellites Card
  lv_obj_t *cal_card;
  lv_obj_t *cal_arc;
  lv_obj_t *label_calories;
  lv_obj_t *label_satellites;

  // QuickSettings Drawer
  lv_obj_t *drawer;
  bool drawer_open;
} UiDashboard;

static UiDashboard uiDash;

// ── Create Figma Heart Rate Card ─────────────────────────────────────────────
static void build_heart_card(lv_obj_t *parent, UiTheme theme) {
  uiDash.hr_card = lv_obj_create(parent);
  lv_obj_set_size(uiDash.hr_card, 228, 68);
  lv_obj_set_style_radius(uiDash.hr_card, 10, 0);
  lv_obj_set_style_bg_color(uiDash.hr_card, theme.card_hr, 0);
  lv_obj_set_style_border_width(uiDash.hr_card, 0, 0);
  lv_obj_set_style_pad_all(uiDash.hr_card, 6, 0);

  // AVG Label
  uiDash.label_hr_avg = lv_label_create(uiDash.hr_card);
  lv_label_set_text(uiDash.label_hr_avg, "AVG\n140");
  lv_obj_set_style_text_color(uiDash.label_hr_avg, theme.text_secondary, 0);
  lv_obj_align(uiDash.label_hr_avg, LV_ALIGN_TOP_LEFT, 6, 2);

  // Huge HR Center Readout
  uiDash.label_hr_val = lv_label_create(uiDash.hr_card);
  lv_label_set_text(uiDash.label_hr_val, "149 BPM");
  lv_obj_set_style_text_font(uiDash.label_hr_val, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(uiDash.label_hr_val, theme.text_primary, 0);
  lv_obj_align(uiDash.label_hr_val, LV_ALIGN_TOP_MID, 0, 0);

  // MAX Label
  uiDash.label_hr_max = lv_label_create(uiDash.hr_card);
  lv_label_set_text(uiDash.label_hr_max, "MAX\n180");
  lv_obj_set_style_text_color(uiDash.label_hr_max, theme.text_secondary, 0);
  lv_obj_align(uiDash.label_hr_max, LV_ALIGN_TOP_RIGHT, -6, 2);

  // 5-Zone Colored Bar
  const uint32_t zone_colors[5] = {0x9E9E9E, 0x00ACC1, 0x43A047, 0xFDD835, 0xE53935};
  for (int i = 0; i < 5; i++) {
    uiDash.hr_zone_bars[i] = lv_obj_create(uiDash.hr_card);
    lv_obj_set_size(uiDash.hr_zone_bars[i], 38, 5);
    lv_obj_set_style_radius(uiDash.hr_zone_bars[i], 2, 0);
    lv_obj_set_style_bg_color(uiDash.hr_zone_bars[i], lv_color_hex(zone_colors[i]), 0);
    lv_obj_set_style_border_width(uiDash.hr_zone_bars[i], 0, 0);
    lv_obj_align(uiDash.hr_zone_bars[i], LV_ALIGN_BOTTOM_LEFT, 4 + i * 42, -2);
  }
}

// ── Create Figma Hero Speed Card ─────────────────────────────────────────────
static void build_speed_card(lv_obj_t *parent, UiTheme theme) {
  uiDash.speed_card = lv_obj_create(parent);
  lv_obj_set_size(uiDash.speed_card, 228, 170);
  lv_obj_set_style_radius(uiDash.speed_card, 10, 0);
  lv_obj_set_style_bg_color(uiDash.speed_card, theme.card, 0);
  lv_obj_set_style_border_width(uiDash.speed_card, 0, 0);
  lv_obj_set_style_pad_all(uiDash.speed_card, 8, 0);

  // AVG Speed
  uiDash.label_speed_avg = lv_label_create(uiDash.speed_card);
  lv_label_set_text(uiDash.label_speed_avg, "AVG\n30.5");
  lv_obj_set_style_text_color(uiDash.label_speed_avg, theme.text_secondary, 0);
  lv_obj_align(uiDash.label_speed_avg, LV_ALIGN_TOP_LEFT, 4, 2);

  // MAX Speed
  uiDash.label_speed_max = lv_label_create(uiDash.speed_card);
  lv_label_set_text(uiDash.label_speed_max, "MAX\n44.7");
  lv_obj_set_style_text_color(uiDash.label_speed_max, theme.text_secondary, 0);
  lv_obj_align(uiDash.label_speed_max, LV_ALIGN_TOP_RIGHT, -4, 2);

  // Hero Speed Arc Dial (270 degrees)
  uiDash.speed_arc = lv_arc_create(uiDash.speed_card);
  lv_obj_set_size(uiDash.speed_arc, 114, 114);
  lv_arc_set_rotation(uiDash.speed_arc, 135);
  lv_arc_set_bg_angles(uiDash.speed_arc, 0, 270);
  lv_arc_set_value(uiDash.speed_arc, 47); // 47% of max speed
  lv_obj_set_style_arc_width(uiDash.speed_arc, 6, LV_PART_MAIN);
  lv_obj_set_style_arc_width(uiDash.speed_arc, 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(uiDash.speed_arc, theme.accent_teal, LV_PART_INDICATOR);
  lv_obj_remove_style(uiDash.speed_arc, nullptr, LV_PART_KNOB); // hide knob
  lv_obj_align(uiDash.speed_arc, LV_ALIGN_CENTER, 0, -14);

  // Live Speed Label
  uiDash.label_speed_val = lv_label_create(uiDash.speed_card);
  lv_label_set_text(uiDash.label_speed_val, "28.2\nKM/H");
  lv_obj_set_style_text_font(uiDash.label_speed_val, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_align(uiDash.label_speed_val, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(uiDash.label_speed_val, theme.text_primary, 0);
  lv_obj_align(uiDash.label_speed_val, LV_ALIGN_CENTER, 0, -10);

  // Bottom Complications Row
  uiDash.label_distance = lv_label_create(uiDash.speed_card);
  lv_label_set_text(uiDash.label_distance, "50.25\nDIST (KM)");
  lv_obj_align(uiDash.label_distance, LV_ALIGN_BOTTOM_LEFT, 4, -2);

  uiDash.label_elapsed = lv_label_create(uiDash.speed_card);
  lv_label_set_text(uiDash.label_elapsed, "01:30:25\nTOTAL TIME");
  lv_obj_align(uiDash.label_elapsed, LV_ALIGN_BOTTOM_MID, 0, -2);

  uiDash.label_gradient = lv_label_create(uiDash.speed_card);
  lv_label_set_text(uiDash.label_gradient, "5%\nGRADIENT");
  lv_obj_align(uiDash.label_gradient, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
}

// ── Create Entire Dashboard Screen ───────────────────────────────────────────
inline void create_lvgl_dashboard(bool darkTheme) {
  UiTheme theme = get_theme(darkTheme);

  uiDash.root = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(uiDash.root, theme.bg, 0);
  lv_obj_set_style_pad_all(uiDash.root, 6, 0);

  // 1. Status Bar
  uiDash.label_time = lv_label_create(uiDash.root);
  lv_label_set_text(uiDash.label_time, "05:15 AM");
  lv_obj_set_style_text_color(uiDash.label_time, theme.text_primary, 0);
  lv_obj_align(uiDash.label_time, LV_ALIGN_TOP_LEFT, 2, 2);

  uiDash.label_battery = lv_label_create(uiDash.root);
  lv_label_set_text(uiDash.label_battery, "76%");
  lv_obj_set_style_text_color(uiDash.label_battery, theme.text_primary, 0);
  lv_obj_align(uiDash.label_battery, LV_ALIGN_TOP_RIGHT, -2, 2);

  // 2. Heart Rate Card
  build_heart_card(uiDash.root, theme);
  lv_obj_align(uiDash.hr_card, LV_ALIGN_TOP_MID, 0, 24);

  // 3. Hero Speed Card
  build_speed_card(uiDash.root, theme);
  lv_obj_align(uiDash.speed_card, LV_ALIGN_TOP_MID, 0, 96);

  // 4. Load onto active display
  lv_screen_load(uiDash.root);
}
