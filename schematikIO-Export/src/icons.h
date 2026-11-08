#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <math.h>

#include "lvgl_icons.h"
// SVG artwork rendered by LVGL; dynamic battery and other complications retained.
static inline void drawBikeIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::bikeIcon);
}
static inline void drawHeartIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color,bool pulseLine=true) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::heartRateIcon);
}
static inline void drawSatelliteIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color, int targetW = 0, int targetH = 0) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::satelliteIcon, targetW, targetH);
}
static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct, bool isCharging, bool alert = false, uint16_t overrideColor = 0, bool connected = true) {
  uint16_t col;
  CustomIcon icon;

  if (!connected) {
    icon = CustomIcon::batteryAlertIcon; // alert.svg (NO BATTERY)
    col = overrideColor ? overrideColor : 0x8410; // uiMuted()
    drawCustomIcon(gfx, x, y, col, icon, 24, 12);
    return;
  }

  if (isCharging) {
    col = overrideColor ? overrideColor : 0x3CE0; // Bright charging green
    // Smooth 5-phase animation: 25% -> 50% -> 75% -> 100% -> Bolt
    int phase = (millis() / 350) % 5;
    if (phase == 0) icon = CustomIcon::battery25Icon;
    else if (phase == 1) icon = CustomIcon::battery50Icon;
    else if (phase == 2) icon = CustomIcon::battery75Icon;
    else if (phase == 3) icon = CustomIcon::batteryFullIcon;
    else icon = CustomIcon::batteryChargeIcon;
  } else if (alert || pct <= 10) {
    bool blink = (millis() / 500) % 2;
    col = overrideColor ? overrideColor : (blink ? 0xF800 : 0x8000);
    icon = CustomIcon::batteryAlertIcon;
  } else {
    // Normal discharge ranges from user specification:
    // 91-100: full
    // 61-90: 75%
    // 41-60: 50%
    // 11-40: 25%
    if (pct >= 91) {
      icon = CustomIcon::batteryFullIcon;
      col = overrideColor ? overrideColor : 0xFFFF;
    } else if (pct >= 61) {
      icon = CustomIcon::battery75Icon;
      col = overrideColor ? overrideColor : 0xFFFF;
    } else if (pct >= 41) {
      icon = CustomIcon::battery50Icon;
      col = overrideColor ? overrideColor : 0xFA60; // Amber
    } else {
      icon = CustomIcon::battery25Icon;
      col = overrideColor ? overrideColor : 0xF800; // Red
    }
  }

  drawCustomIcon(gfx, x, y, col, icon, 24, 12);
}

static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct, uint16_t fgColor) {
  drawBatteryIcon(gfx, x, y, pct, false, (pct <= 10), fgColor, true);
}

static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct = 76) {
  drawBatteryIcon(gfx, x, y, pct, false, (pct <= 10), 0, true);
}
static inline void drawFlameIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::caloriesIcon);
}
static inline void drawLightningIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::powerIcon);
}
static inline void drawTempCloudIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  gfx.drawCircle(x+5,y+6,4,color); gfx.drawCircle(x+10,y+4,4,color); gfx.drawCircle(x+14,y+6,3,color);
  gfx.drawLine(x+2,y+9,x+16,y+9,color); gfx.drawLine(x+4,y+12,x+14,y+12,color); gfx.drawLine(x+6,y+14,x+12,y+14,color);
}
static inline void drawGearIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::systemIcon);
}
static inline void drawWrenchIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::configureIcon);
}
static inline void drawThemeIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color) {
  drawCustomIcon(gfx,x,y,color,CustomIcon::themeSwitcherIcon);
}
struct HrZoneColors { uint16_t bgLight,bgDark,textLight,textDark,subLight,subDark,barColor; };
static const HrZoneColors HR_ZONE_PALETTES[5]={
 {0xEF7D,0x2124,0x4208,0xDEFB,0x7BEF,0x8C71,0xAD55},
 {0xD7FB,0x0A2C,0x034E,0x5EFB,0x2BCE,0x23EF,0x3DF7},
 {0xD753,0x0940,0x2400,0x7FE0,0x4B60,0x5D00,0x2564},
 {0xFFF1,0x2920,0x8380,0xFFE0,0x9CE0,0x73E0,0xFE60},
 {0xFDE7,0x3000,0x9800,0xFA6A,0xAB65,0x7884,0xFA08}
};
static inline int calcHrZone(int hrBpm) {
  if(hrBpm<115) return 1; if(hrBpm<135) return 2; if(hrBpm<155) return 3; if(hrBpm<175) return 4; return 5;
}
static inline void drawHRZoneBar(Adafruit_GFX &gfx,int x,int y,int totalW,int activeZone) {
  activeZone=constrain(activeZone,1,5); int barW=(totalW-16)/5,barH=5;
  for(int z=0;z<5;z++) { int bx=x+z*(barW+4); gfx.fillRoundRect(bx,y,barW,barH,2,HR_ZONE_PALETTES[z].barColor);
    if(z+1==activeZone) { int tx=bx+barW/2; gfx.fillTriangle(tx-4,y-5,tx+4,y-5,tx,y-1,HR_ZONE_PALETTES[z].barColor); }
  }
}
static inline void drawCompassCoin(Adafruit_GFX &gfx,int cx,int cy,float headingDeg) {
  gfx.fillCircle(cx,cy,14,0xFFFF); gfx.drawCircle(cx,cy,14,0xCE79); gfx.drawCircle(cx,cy,13,0xEF7D);
  float rad=radians(headingDeg),cosA=cosf(rad),sinA=sinf(rad);
  int ntx=cx+(int)(10*sinA),nty=cy-(int)(10*cosA);
  int bx1=cx-(int)(3*cosA),by1=cy-(int)(3*sinA),bx2=cx+(int)(3*cosA),by2=cy+(int)(3*sinA);
  int stx=cx-(int)(10*sinA),sty=cy+(int)(10*cosA);
  gfx.fillTriangle(ntx,nty,bx1,by1,bx2,by2,0xF9A7); gfx.fillTriangle(stx,sty,bx1,by1,bx2,by2,0xCE79); gfx.fillCircle(cx,cy,2,0x3186);
}

// 28x35 Destination Pin Mask from destPinIcon.svg
static const uint8_t destPinMask28x35[] PROGMEM = {
  0x00, 0x7F, 0xE0, 0x00, 0x01, 0xFF, 0xF8, 0x00, 0x03, 0xFF, 0xFC, 0x00,
  0x0F, 0xFF, 0xFF, 0x00, 0x0F, 0xFF, 0xFF, 0x00, 0x1F, 0xFF, 0xFF, 0x80,
  0x3F, 0xFF, 0xFF, 0xC0, 0x7F, 0xFF, 0xFF, 0xE0, 0x7F, 0xFF, 0xFF, 0xE0,
  0x7F, 0xFF, 0xFF, 0xE0, 0xFF, 0xFF, 0xFF, 0xF0, 0xFF, 0xF0, 0xFF, 0xF0,
  0xFF, 0xE0, 0x7F, 0xF0, 0xFF, 0xE0, 0x7F, 0xF0, 0xFF, 0xE0, 0x7F, 0xF0,
  0xFF, 0xE0, 0x7F, 0xF0, 0xFF, 0xF0, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF, 0xF0,
  0x7F, 0xFF, 0xFF, 0xE0, 0x7F, 0xFF, 0xFF, 0xE0, 0x7F, 0xFF, 0xFF, 0xE0,
  0x3F, 0xFF, 0xFF, 0xC0, 0x3F, 0xFF, 0xFF, 0x80, 0x1F, 0xFF, 0xFF, 0x80,
  0x0F, 0xFF, 0xFF, 0x00, 0x0F, 0xFF, 0xFF, 0x00, 0x07, 0xFF, 0xFE, 0x00,
  0x03, 0xFF, 0xFC, 0x00, 0x01, 0xFF, 0xF8, 0x00, 0x00, 0xFF, 0xF0, 0x00,
  0x00, 0x7F, 0xE0, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x1F, 0x80, 0x00,
  0x00, 0x0F, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
};

// 18x18 Snap To Self Location Reticle Mask from snapToSelfLocIcon.svg
static const uint8_t snapReticleMask18x18[] PROGMEM = {
  0x00, 0xC0, 0x00, 0x03, 0xE0, 0x00, 0x0F, 0xF8, 0x00, 0x1C, 0xDC, 0x00,
  0x38, 0x0E, 0x00, 0x30, 0x06, 0x00, 0x61, 0xC3, 0x00, 0x63, 0xE3, 0x00,
  0xF3, 0xE3, 0xC0, 0xF3, 0xE3, 0xC0, 0x61, 0xC3, 0x00, 0x30, 0x06, 0x00,
  0x38, 0x0E, 0x00, 0x1C, 0x1C, 0x00, 0x0F, 0xF8, 0x00, 0x03, 0xE0, 0x00,
  0x00, 0xC0, 0x00, 0x00, 0xC0, 0x00,
};

// Crisp vector reticle for Snap-to-location button (anti-aliased geometry, mathematically centered)
static inline void drawVectorReticle(Adafruit_GFX &gfx, int cx, int cy, uint16_t color = 0xFFFF) {
  gfx.fillCircle(cx, cy, 2, color);
  gfx.drawCircle(cx, cy, 6, color);
  gfx.drawCircle(cx, cy, 7, color);
  gfx.drawFastVLine(cx, cy - 11, 4, color);
  gfx.drawFastVLine(cx, cy + 8,  4, color);
  gfx.drawFastHLine(cx - 11, cy, 4, color);
  gfx.drawFastHLine(cx + 8,  cy, 4, color);
}


// 16x16 Search Button Magnifying Glass Mask from searchButtonIcon.svg
static const uint8_t searchIconMask16x16[] PROGMEM = {
  0x00, 0x00, 0x0F, 0x00, 0x1F, 0x80, 0x30, 0xC0, 0x60, 0x60, 0x60, 0x60,
  0x60, 0x60, 0x60, 0x60, 0x30, 0xC0, 0x1F, 0xA0, 0x0F, 0x70, 0x00, 0x38,
  0x00, 0x1C, 0x00, 0x0E, 0x00, 0x07, 0x00, 0x02,
};

// 16x12 Keypad Backspace Arrow Mask
static const uint8_t bkspArrowMask16x12[] PROGMEM = {
  0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x07, 0x00, 0x0E, 0x00, 0x1C, 0x00,
  0x3F, 0xFC, 0x1F, 0xFC, 0x0E, 0x00, 0x07, 0x00, 0x03, 0x80, 0x01, 0xC0,
};

// Rotating Self-Location Puck from selfLocIcon.svg:
// Center of rotation inside the blue disc, with white outer ring and rotating blue heading arrowhead.
static inline void drawMapHeadingMarker(Adafruit_GFX &gfx, int cx, int cy, float headingDeg, float scale = 0.75f) {
  float rad = radians(headingDeg);
  float fx = sinf(rad), fy = -cosf(rad);
  float rx = cosf(rad), ry = sinf(rad);

  float tipDist = 21.0f * scale;
  float baseDist = 6.0f * scale;
  float baseW = 10.0f * scale;

  int tipX = cx + (int)lroundf(tipDist * fx);
  int tipY = cy + (int)lroundf(tipDist * fy);
  int leftX = cx + (int)lroundf(baseDist * fx - baseW * rx);
  int leftY = cy + (int)lroundf(baseDist * fy - baseW * ry);
  int rightX = cx + (int)lroundf(baseDist * fx + baseW * rx);
  int rightY = cy + (int)lroundf(baseDist * fy + baseW * ry);

  // 1. Blue arrowhead triangle pointing along heading
  gfx.fillTriangle(tipX, tipY, leftX, leftY, rightX, rightY, 0x335F);

  // 2. Pure white outer ring
  int rOuter = (int)lroundf(13.0f * scale);
  gfx.fillCircle(cx, cy, rOuter, 0xFFFF);

  // 3. Royal blue inner disc
  int rInner = (int)lroundf(9.0f * scale);
  gfx.fillCircle(cx, cy, rInner, 0x335F);
}

static inline void drawSelfLocPuck(Adafruit_GFX &gfx, int cx, int cy, float headingDeg, float scale = 0.75f) {
  drawMapHeadingMarker(gfx, cx, cy, headingDeg, scale);
}

static inline void drawBluetoothStateIcon(Adafruit_GFX &g,int x,int y,uint16_t c,bool connected, int targetW = 0, int targetH = 0){drawCustomIcon(g,x,y,c,connected?CustomIcon::bluetoothConnectedIcon:CustomIcon::bluetoothDisconnectedIcon, targetW, targetH);}
static inline void drawWifiStateIcon(Adafruit_GFX &g,int x,int y,uint16_t c,bool connected){drawCustomIcon(g,x,y,c,connected?CustomIcon::wifiConnectedIcon:CustomIcon::wifiDisconnectedIcon);}
static inline void drawHistoryIcon(Adafruit_GFX &gfx,int x,int y,uint16_t color){drawCustomIcon(gfx,x,y,color,CustomIcon::historyIcon);}

// startIcon20 20x20: Solid vibrant green circle (#34C200) with pure white play/start marker (|>)
static const uint8_t s_startIcon20[400] = {
  0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0,
  0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,
  0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,
  0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,
  0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
  0,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,2,2,2,0,
  0,2,2,2,2,2,1,1,2,2,1,1,2,2,2,2,2,2,2,0,
  2,2,2,2,2,2,1,1,2,2,1,1,1,1,2,2,2,2,2,2,
  2,2,2,2,2,2,1,1,2,2,1,1,1,1,1,2,2,2,2,2,
  2,2,2,2,2,2,1,1,2,2,1,1,1,1,1,2,2,2,2,2,
  2,2,2,2,2,2,1,1,2,2,1,1,1,1,2,2,2,2,2,2,
  0,2,2,2,2,2,1,1,2,2,1,1,2,2,2,2,2,2,2,0,
  0,2,2,2,2,2,1,1,2,2,2,2,2,2,2,2,2,2,2,0,
  0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
  0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,
  0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,
  0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,
  0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,0,0,0,0,0,
  0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0,
};

static inline void drawStartNodeIcon(Adafruit_GFX &gfx, int x, int y) {
  const uint16_t greenCol = 0x3600; // #34C200 (Solid vibrant green circle)
  const uint16_t whiteCol = 0xFFFF; // #FFFFFF (Solid pure white play symbol)
  for (int r = 0; r < 20; r++) {
    for (int c = 0; c < 20; c++) {
      uint8_t v = s_startIcon20[r * 20 + c];
      if (v == 1) gfx.drawPixel(x + c, y + r, whiteCol);
      else if (v == 2) gfx.drawPixel(x + c, y + r, greenCol);
    }
  }
}

// finishIcon20 20x20: Black outer ring & quadrants with white cross arms and black center square
static const uint8_t s_finishIcon20[400] = {
  0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0,
  0,0,0,0,0,2,2,2,1,1,1,1,2,2,2,0,0,0,0,0,
  0,0,0,0,2,2,2,1,1,1,1,1,1,2,2,2,0,0,0,0,
  0,0,0,2,2,2,2,1,1,1,1,1,1,2,2,2,2,0,0,0,
  0,0,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,0,0,
  0,2,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,2,0,
  0,2,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,2,0,
  0,2,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,2,0,
  2,1,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,1,2,
  2,1,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,1,2,
  2,1,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,1,2,
  2,1,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,1,2,
  0,2,1,1,1,1,1,2,2,2,2,2,2,1,1,1,1,1,2,0,
  0,2,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,2,0,
  0,2,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,2,0,
  0,0,2,2,2,2,2,1,1,1,1,1,1,2,2,2,2,2,0,0,
  0,0,0,2,2,2,2,1,1,1,1,1,1,2,2,2,2,0,0,0,
  0,0,0,0,2,2,2,1,1,1,1,1,1,2,2,2,0,0,0,0,
  0,0,0,0,0,2,2,2,1,1,1,1,2,2,2,0,0,0,0,0,
  0,0,0,0,0,0,0,0,2,2,2,2,0,0,0,0,0,0,0,0,
};

static inline void drawFinishNodeIcon(Adafruit_GFX &gfx, int x, int y) {
  const uint16_t blackCol = 0x0000; // #000000 (Black outer ring, quadrants, and center target square)
  const uint16_t whiteCol = 0xFFFF; // #FFFFFF (White cross arms)
  for (int r = 0; r < 20; r++) {
    for (int c = 0; c < 20; c++) {
      uint8_t v = s_finishIcon20[r * 20 + c];
      if (v == 1) gfx.drawPixel(x + c, y + r, whiteCol);
      else if (v == 2) gfx.drawPixel(x + c, y + r, blackCol);
    }
  }
}
