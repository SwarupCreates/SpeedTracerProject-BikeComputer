#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

// ═══════════════════════════════════════════════════════════════════════════════
//  SpeedTracer Custom Vector Icons & UI Complications
//  Crafted to match the Figma cycling computer mockups
// ═══════════════════════════════════════════════════════════════════════════════

// 1. Bicycle Icon (rider / bike glyph)
static inline void drawBikeIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // x, y = top-left bounding box (~18x12)
  // Wheels
  gfx.drawCircle(x + 3, y + 8, 3, color);
  gfx.drawCircle(x + 14, y + 8, 3, color);
  // Center hubs
  gfx.drawPixel(x + 3, y + 8, color);
  gfx.drawPixel(x + 14, y + 8, color);
  // Bottom bracket
  int bbX = x + 8, bbY = y + 8;
  // Frame tubes
  gfx.drawLine(x + 3, y + 8, bbX, bbY, color);          // chainstay
  gfx.drawLine(bbX, bbY, x + 7, y + 3, color);          // seat tube
  gfx.drawLine(x + 7, y + 3, x + 12, y + 3, color);     // top tube
  gfx.drawLine(bbX, bbY, x + 12, y + 3, color);         // down tube
  gfx.drawLine(x + 12, y + 3, x + 14, y + 8, color);    // fork
  // Seat & Handlebars
  gfx.drawLine(x + 5, y + 2, x + 9, y + 2, color);      // saddle
  gfx.drawLine(x + 12, y + 3, x + 13, y + 1, color);    // stem
  gfx.drawLine(x + 11, y + 1, x + 15, y + 1, color);    // handlebar
}

// 2. Heart Pulse Icon (BPM / Zone)
static inline void drawHeartIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color, bool pulseLine = true) {
  // x, y = center top (~14x12)
  gfx.fillCircle(x + 3, y + 3, 3, color);
  gfx.fillCircle(x + 9, y + 3, 3, color);
  gfx.fillTriangle(x, y + 4, x + 12, y + 4, x + 6, y + 11, color);
  if (pulseLine) {
    // Small electrocardiogram pulse wave beside or inside
    uint16_t ecgCol = (color == 0xFFFF) ? 0x0000 : 0xFFFF;
    gfx.drawLine(x + 2, y + 5, x + 4, y + 5, ecgCol);
    gfx.drawLine(x + 4, y + 5, x + 6, y + 2, ecgCol);
    gfx.drawLine(x + 6, y + 2, x + 8, y + 8, ecgCol);
    gfx.drawLine(x + 8, y + 8, x + 10, y + 5, ecgCol);
  }
}

// 3. GNSS Satellite Icon
static inline void drawSatelliteIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // Central body (angled rectangle)
  gfx.drawRect(x + 5, y + 5, 5, 5, color);
  gfx.fillRect(x + 6, y + 6, 3, 3, color);
  // Solar panels (top-left & bottom-right wings)
  gfx.drawRect(x, y + 1, 4, 3, color);
  gfx.drawRect(x + 11, y + 11, 4, 3, color);
  // Solar panel grid lines
  gfx.drawLine(x + 2, y + 1, x + 2, y + 3, color);
  gfx.drawLine(x + 13, y + 11, x + 13, y + 13, color);
  // Connecting struts
  gfx.drawLine(x + 4, y + 3, x + 5, y + 5, color);
  gfx.drawLine(x + 10, y + 10, x + 11, y + 11, color);
  // Broadcast signal waves
  gfx.drawCircle(x + 12, y + 3, 3, color);
  gfx.drawCircle(x + 12, y + 3, 5, color);
}

// 4. Bluetooth Icon (Standard Nordic Rune B)
static inline void drawBluetoothIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // x, y = top-left (~10x15)
  int cx = x + 4;
  // Center spine
  gfx.drawLine(cx, y, cx, y + 14, color);
  gfx.drawLine(cx + 1, y, cx + 1, y + 14, color);
  // Top right loop
  gfx.drawLine(cx, y + 7, cx + 5, y + 3, color);
  gfx.drawLine(cx + 5, y + 3, cx, y, color);
  // Bottom right loop
  gfx.drawLine(cx, y + 7, cx + 5, y + 11, color);
  gfx.drawLine(cx + 5, y + 11, cx, y + 14, color);
  // Wings / cross
  gfx.drawLine(cx - 3, y + 4, cx + 5, y + 11, color);
  gfx.drawLine(cx - 3, y + 10, cx + 5, y + 3, color);
}

// 5. Wi-Fi Icon (Radio Arcs)
static inline void drawWifiIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // Center broadcast point at (x+6, y+11)
  int cx = x + 6, cy = y + 11;
  gfx.fillCircle(cx, cy, 1, color);
  // Inner arc (radius 4)
  for (int a = 220; a <= 320; a += 8) {
    float r = radians((float)a);
    gfx.drawPixel(cx + (int)(4 * cosf(r)), cy + (int)(4 * sinf(r)), color);
  }
  // Outer arc (radius 7)
  for (int a = 220; a <= 320; a += 6) {
    float r = radians((float)a);
    gfx.drawPixel(cx + (int)(7 * cosf(r)), cy + (int)(7 * sinf(r)), color);
  }
  // Max arc (radius 10)
  for (int a = 220; a <= 320; a += 5) {
    float r = radians((float)a);
    gfx.drawPixel(cx + (int)(10 * cosf(r)), cy + (int)(10 * sinf(r)), color);
  }
}

// 6. Battery Gauge Icon
static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct, uint16_t fgColor) {
  // Shell
  gfx.drawRoundRect(x, y, 22, 11, 2, fgColor);
  // Positive terminal nub
  gfx.fillRect(x + 22, y + 3, 2, 5, fgColor);
  // Inner level fill
  pct = constrain(pct, 0, 100);
  int w = (pct * 18) / 100;
  if (w > 0) {
    uint16_t fillCol = (pct > 20) ? 0x2564 : 0xF9A7; // Green or warning salmon
    gfx.fillRect(x + 2, y + 2, w, 7, fillCol);
  }
}

// 7. Flame / Calories Icon
static inline void drawFlameIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // x, y = top-left (~12x15)
  // Outer flame teardrop
  gfx.fillCircle(x + 6, y + 9, 5, color);
  gfx.fillTriangle(x + 1, y + 8, x + 11, y + 8, x + 6, y, color);
  // Inner flicker tongue (carved out)
  uint16_t bg = 0xFFFF; // cutout
  gfx.fillCircle(x + 6, y + 10, 2, bg);
  gfx.fillTriangle(x + 4, y + 10, x + 8, y + 10, x + 6, y + 5, bg);
}

// 8. Lightning Bolt / Power Icon
static inline void drawLightningIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // Top slant
  gfx.fillTriangle(x + 6, y, x + 1, y + 7, x + 6, y + 7, color);
  // Bottom slant
  gfx.fillTriangle(x + 4, y + 6, x + 9, y + 6, x + 3, y + 14, color);
}

// 9. Temperature / Cloud Icon
static inline void drawTempCloudIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // Cloud puffs
  gfx.drawCircle(x + 5, y + 6, 4, color);
  gfx.drawCircle(x + 10, y + 4, 4, color);
  gfx.drawCircle(x + 14, y + 6, 3, color);
  gfx.drawLine(x + 2, y + 9, x + 16, y + 9, color);
  // Thermometer or mist lines below
  gfx.drawLine(x + 4, y + 12, x + 14, y + 12, color);
  gfx.drawLine(x + 6, y + 14, x + 12, y + 14, color);
}

// 10. Turn Cue Arrow (Directions e.g. ↰ Left 90 deg)
static inline void drawTurnLeftArrow(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // Arrow shaft coming from bottom, turning left
  gfx.drawLine(x + 10, y + 14, x + 10, y + 4, color);
  gfx.drawLine(x + 11, y + 14, x + 11, y + 4, color);
  gfx.drawLine(x + 10, y + 4, x + 3, y + 4, color);
  gfx.drawLine(x + 10, y + 5, x + 3, y + 5, color);
  // Arrowhead pointing left
  gfx.fillTriangle(x + 4, y + 1, x + 4, y + 8, x, y + 4, color);
}

// 11. System Gear Icon
static inline void drawGearIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  int cx = x + 7, cy = y + 7;
  gfx.drawCircle(cx, cy, 5, color);
  gfx.drawCircle(cx, cy, 2, color);
  // 4 orthogonal teeth
  gfx.fillRect(cx - 1, cy - 7, 3, 2, color);
  gfx.fillRect(cx - 1, cy + 6, 3, 2, color);
  gfx.fillRect(cx - 7, cy - 1, 2, 3, color);
  gfx.fillRect(cx + 6, cy - 1, 2, 3, color);
  // 4 diagonal teeth
  gfx.drawPixel(cx - 4, cy - 4, color);
  gfx.drawPixel(cx + 4, cy - 4, color);
  gfx.drawPixel(cx - 4, cy + 4, color);
  gfx.drawPixel(cx + 4, cy + 4, color);
}

// 12. Configure / Wrench Icon
static inline void drawWrenchIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  // Angled handle
  gfx.drawLine(x + 12, y + 12, x + 5, y + 5, color);
  gfx.drawLine(x + 13, y + 12, x + 6, y + 5, color);
  // Wrench head
  gfx.drawCircle(x + 4, y + 4, 4, color);
  gfx.fillCircle(x + 3, y + 3, 2, (color == 0xFFFF) ? 0x0000 : 0xFFFF);
}

// 13. Theme Half-Moon Icon
static inline void drawThemeIcon(Adafruit_GFX &gfx, int x, int y, uint16_t color) {
  int cx = x + 7, cy = y + 7, r = 6;
  gfx.drawCircle(cx, cy, r, color);
  // Fill left half
  for (int dy = -r; dy <= r; dy++) {
    int dxMax = (int)sqrtf(max(0, r * r - dy * dy));
    gfx.drawLine(cx - dxMax, cy + dy, cx, cy + dy, color);
  }
}

// 14. 5-Zone Heart Rate Color Scheme & Palettes
// Dynamic card backgrounds and high-contrast text shades for each training zone
struct HrZoneColors {
  uint16_t bgLight;   // Lighter shade of the zone color for Light Mode card background
  uint16_t bgDark;    // Deep/dark shade of the zone color for Dark Mode card background
  uint16_t textLight; // Primary high-contrast text color for Light Mode
  uint16_t textDark;  // Primary high-contrast text color for Dark Mode
  uint16_t subLight;  // Muted label color for Light Mode
  uint16_t subDark;   // Muted label color for Dark Mode
  uint16_t barColor;  // Vibrant saturated bar color
};

static const HrZoneColors HR_ZONE_PALETTES[5] = {
  // Zone 1: Active Recovery (<115 BPM) - Soft Gray
  { 0xEF7D, 0x2124, 0x4208, 0xDEFB, 0x7BEF, 0x8C71, 0xAD55 },
  // Zone 2: Endurance (115-134 BPM) - Ice Cyan
  { 0xD7FB, 0x0A2C, 0x034E, 0x5EFB, 0x2BCE, 0x23EF, 0x3DF7 },
  // Zone 3: Tempo (135-154 BPM) - Mint Green (Figma Frame 1 & 2)
  { 0xD753, 0x0940, 0x2400, 0x7FE0, 0x4B60, 0x5D00, 0x2564 },
  // Zone 4: Threshold (155-174 BPM) - Buttercup Yellow
  { 0xFFF1, 0x2920, 0x8380, 0xFFE0, 0x9CE0, 0x73E0, 0xFE60 },
  // Zone 5: Anaerobic (>174 BPM) - Pastel Coral / Crimson
  { 0xFDE7, 0x3000, 0x9800, 0xFA6A, 0xAB65, 0x7884, 0xFA08 }
};

static inline int calcHrZone(int hrBpm) {
  if (hrBpm < 115) return 1;
  if (hrBpm < 135) return 2;
  if (hrBpm < 155) return 3;
  if (hrBpm < 175) return 4;
  return 5;
}

// 15. 5-Zone Heart Rate Color Bar with Pointer Triangle
static inline void drawHRZoneBar(Adafruit_GFX &gfx, int x, int y, int totalW, int activeZone) {
  activeZone = constrain(activeZone, 1, 5);
  int barW = (totalW - 16) / 5; // e.g. (200 - 16)/5 = ~36px
  int barH = 5;

  for (int z = 0; z < 5; z++) {
    int bx = x + z * (barW + 4);
    gfx.fillRoundRect(bx, y, barW, barH, 2, HR_ZONE_PALETTES[z].barColor);
    // If this is the active zone, draw inverted pointer triangle below it
    if (z + 1 == activeZone) {
      int tx = bx + barW / 2;
      gfx.fillTriangle(tx - 4, y - 5, tx + 4, y - 5, tx, y - 1, HR_ZONE_PALETTES[z].barColor);
    }
  }
}

// 16. Compass Coin Badge (for Map Screen)
static inline void drawCompassCoin(Adafruit_GFX &gfx, int cx, int cy, float headingDeg) {
  // Circular badge
  gfx.fillCircle(cx, cy, 14, 0xFFFF);
  gfx.drawCircle(cx, cy, 14, 0xCE79);
  gfx.drawCircle(cx, cy, 13, 0xEF7D);

  // Rotating needle: Red North pointer, Light Gray South pointer
  float rad = radians(headingDeg);
  float cosA = cosf(rad);
  float sinA = sinf(rad);

  // North tip (red)
  int ntx = cx + (int)(10 * sinA);
  int nty = cy - (int)(10 * cosA);
  // Base corners
  int bx1 = cx - (int)(3 * cosA);
  int by1 = cy - (int)(3 * sinA);
  int bx2 = cx + (int)(3 * cosA);
  int by2 = cy + (int)(3 * sinA);
  // South tip (gray)
  int stx = cx - (int)(10 * sinA);
  int sty = cy + (int)(10 * cosA);

  gfx.fillTriangle(ntx, nty, bx1, by1, bx2, by2, 0xF9A7); // Red North
  gfx.fillTriangle(stx, sty, bx1, by1, bx2, by2, 0xCE79); // Gray South
  gfx.fillCircle(cx, cy, 2, 0x3186); // Center brass pivot
}
