#pragma once
/**
 * @file lvgl_hal.h
 * SpeedTracer Cycling Computer - LVGL 9 Hardware Abstraction Layer (HAL)
 * Connects LVGL 9 to Waveshare ST7789 IPS Display + CST3530 Capacitive Touchpad
 */

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <lvgl.h>
#include <Adafruit_ST7789.h>

#define DISP_HOR_RES 240
#define DISP_VER_RES 320

// ST7789 Display Pins (FSPI)
#define LCD_MOSI 45
#define LCD_SCLK 40
#define LCD_CS   42
#define LCD_DC   41
#define LCD_RST  39
#define LCD_BL   5

// CST3530 Touchpad Pins (Bus 1)
#define TOUCH_SDA 1
#define TOUCH_SCL 3
#define TOUCH_RST 2
#define TOUCH_INT 4
#define TOUCH_ADDR 0x58

extern SPIClass lcdSPI;
extern Adafruit_ST7789 *gfxDisplay;
extern TwoWire touchBus;

// LVGL HAL Objects
static lv_display_t *lvglDisplay = nullptr;
static lv_indev_t   *lvglTouch   = nullptr;
static lv_color_t   *dispBuf1    = nullptr;
static lv_color_t   *dispBuf2    = nullptr;

// ── Display Flush Callback ───────────────────────────────────────────────────
static void lvgl_disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  if (gfxDisplay) {
    gfxDisplay->startWrite();
    gfxDisplay->setAddrWindow(area->x1, area->y1, w, h);
    gfxDisplay->writePixels((uint16_t *)px_map, w * h);
    gfxDisplay->endWrite();
  }

  // Tell LVGL that flushing is complete
  lv_display_flush_ready(disp);
}

// ── Touch Read Callback ──────────────────────────────────────────────────────
static bool cst3530_cmd(uint32_t c) {
  touchBus.beginTransmission(TOUCH_ADDR);
  for (int n = 24; n >= 0; n -= 8) touchBus.write((uint8_t)(c >> n));
  return touchBus.endTransmission() == 0;
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  // If INT pin is HIGH, touchpad is idle (no finger detected)
  if (digitalRead(TOUCH_INT) != LOW) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  uint8_t buf[32] = {0};
  bool valid = cst3530_cmd(0xD0070000);
  if (valid) {
    int n = touchBus.requestFrom((uint8_t)TOUCH_ADDR, (uint8_t)32);
    valid = (n == 32);
    for (int i = 0; i < n; i++) buf[i] = touchBus.read();
  }
  cst3530_cmd(0xD00002AB);

  if (!valid || buf[2] != 0xFF) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int count = buf[3] & 0x0F;
  int keys  = buf[3] >> 4;
  if (count == 0 || (keys + count) * 5 + 4 > 32) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int i = keys * 5;
  if (!(buf[i + 8] >> 4)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  int x = buf[i + 4] | ((buf[i + 7] & 0x0F) << 8);
  int y = buf[i + 5] | ((buf[i + 7] & 0xF0) << 4);

  if (x < DISP_HOR_RES && y < DISP_VER_RES) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ── LVGL Core Task (Core 1) ──────────────────────────────────────────────────
static void lvglTask(void *pvParameters) {
  for (;;) {
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(16)); // ~60 FPS update loop
  }
}

// ── Hardware Initialization ──────────────────────────────────────────────────
inline void init_lvgl_hal() {
  // 1. Initialize LVGL Core
  lv_init();

  // 2. Allocate Dual Framebuffers in PSRAM (or internal SRAM fallback)
  size_t buf_size = DISP_HOR_RES * 40 * sizeof(lv_color_t); // 40-line chunk = 19.2 KB
  dispBuf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  dispBuf2 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  if (!dispBuf1) {
    // Fallback to PSRAM
    dispBuf1 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    dispBuf2 = (lv_color_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  // 3. Register Display
  lvglDisplay = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
  lv_display_set_buffers(lvglDisplay, dispBuf1, dispBuf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lvglDisplay, lvgl_disp_flush_cb);

  // 4. Initialize Touchscreen CST3530
  pinMode(TOUCH_INT, INPUT);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, LOW);  delay(10);
  digitalWrite(TOUCH_RST, HIGH); delay(10);

  // 5. Register Touch Input Device
  lvglTouch = lv_indev_create();
  lv_indev_set_type(lvglTouch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lvglTouch, lvgl_touch_read_cb);

  // 6. Launch LVGL Task on Core 1
  xTaskCreatePinnedToCore(
      lvglTask,
      "LVGL_Task",
      8192,
      nullptr,
      2,
      nullptr,
      1 // Pin to Core 1 (Core 0 handles GPS & BLE)
  );

  Serial.println("LVGL 9 HAL initialized successfully (Dual DMA Buffers + CST3530 Touch).");
}
