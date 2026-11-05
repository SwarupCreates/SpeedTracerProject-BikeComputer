#pragma once
#include <lvgl.h>
#include "custom_icon_masks.h"

// High-definition alpha mask renderer with edge-contrast sharpening.
// Samples directly from customMasks with centered subpixel interpolation,
// eliminating blur and dark halos.
static void drawCustomIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color, CustomIcon icon, int targetW = 0, int targetH = 0, bool flipV = false) {
  const int iconIdx = (int)icon;
  if (iconIdx < 0 || iconIdx >= (int)(sizeof(customMasks) / sizeof(customMasks[0]))) return;
  const uint8_t *mask = customMasks[iconIdx];
  if (!mask) return;
  const int w = customWidths[iconIdx];
  const int h = customHeights[iconIdx];

  const uint8_t fgR = ((color >> 11) & 31) * 255 / 31;
  const uint8_t fgG = ((color >> 5) & 63) * 255 / 63;
  const uint8_t fgB = (color & 31) * 255 / 31;

  if (targetW > 0 && targetH > 0 && (targetW != w || targetH != h)) {
    for (int r = 0; r < targetH; r++) {
      int effective_r = flipV ? (targetH - 1 - r) : r;
      int v_fp = ((effective_r * 2 + 1) * h * 128) / targetH - 128;
      if (v_fp < 0) v_fp = 0;
      int max_v = (h - 1) << 8;
      if (v_fp > max_v) v_fp = max_v;
      int r0 = v_fp >> 8;
      int r1 = min(h - 1, r0 + 1);
      int yr = v_fp & 255;

      for (int c = 0; c < targetW; c++) {
        int u_fp = ((c * 2 + 1) * w * 128) / targetW - 128;
        if (u_fp < 0) u_fp = 0;
        int max_u = (w - 1) << 8;
        if (u_fp > max_u) u_fp = max_u;
        int c0 = u_fp >> 8;
        int c1 = min(w - 1, c0 + 1);
        int xr = u_fp & 255;

        int a00 = mask[r0 * w + c0];
        int a01 = mask[r0 * w + c1];
        int a10 = mask[r1 * w + c0];
        int a11 = mask[r1 * w + c1];

        int a0 = (a00 * (256 - xr) + a01 * xr) >> 8;
        int a1 = (a10 * (256 - xr) + a11 * xr) >> 8;
        int a  = (a0 * (256 - yr) + a1 * yr) >> 8;

        // Contrast sharpening curve: cut off soft haze, saturate core, smooth crisp edge
        if (a < 36) continue;
        if (a > 192) a = 255;
        else a = (a - 36) * 255 / (192 - 36);

        if (a == 255) {
          gfx.drawPixel(x + c, y + r, color);
        } else {
          uint16_t bg = display.backgroundPixel(x + c, y + r);
          uint8_t bgR = ((bg >> 11) & 31) * 255 / 31;
          uint8_t bgG = ((bg >> 5) & 63) * 255 / 63;
          uint8_t bgB = (bg & 31) * 255 / 31;
          int red   = (fgR * a + bgR * (255 - a) + 127) / 255;
          int green = (fgG * a + bgG * (255 - a) + 127) / 255;
          int blue  = (fgB * a + bgB * (255 - a) + 127) / 255;
          gfx.drawPixel(x + c, y + r, ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
        }
      }
    }
    return;
  }

  // Native 1:1 crisp rendering
  for (int r = 0; r < h; r++) {
    int src_r = flipV ? (h - 1 - r) : r;
    for (int c = 0; c < w; c++) {
      int a = mask[src_r * w + c];
      if (a < 24) continue;
      if (a > 192) a = 255;
      else a = (a - 24) * 255 / (192 - 24);

      if (a == 255) {
        gfx.drawPixel(x + c, y + r, color);
      } else {
        uint16_t bg = display.backgroundPixel(x + c, y + r);
        uint8_t bgR = ((bg >> 11) & 31) * 255 / 31;
        uint8_t bgG = ((bg >> 5) & 63) * 255 / 63;
        uint8_t bgB = (bg & 31) * 255 / 31;
        int red   = (fgR * a + bgR * (255 - a) + 127) / 255;
        int green = (fgG * a + bgG * (255 - a) + 127) / 255;
        int blue  = (fgB * a + bgB * (255 - a) + 127) / 255;
        gfx.drawPixel(x + c, y + r, ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
      }
    }
  }
}

static inline void drawCustomIconFlippedV(Adafruit_GFX &gfx, int x, int y, uint16_t color, CustomIcon icon, int targetW = 0, int targetH = 0) {
  drawCustomIcon(gfx, x, y, color, icon, targetW, targetH, true);
}
