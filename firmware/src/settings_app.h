#pragma once
static void drawGlobalNav();
RTC_DATA_ATTR static uint32_t rtc_last_epoch = 0;
RTC_DATA_ATTR static uint32_t rtc_last_millis = 0;

static void persistSystemTime() {
  time_t now = time(nullptr);
  if (now >= 1704067200) {
    rtc_last_epoch = (uint32_t)now;
    rtc_last_millis = millis();
    Preferences p;
    if (p.begin("clock", false)) {
      p.putULong("epoch", (uint32_t)now);
      p.end();
    }
  }
}

static void restoreSystemTime() {
  time_t now = time(nullptr);
  if (now >= 1704067200) return; // Already valid

  if (rtc_last_epoch >= 1704067200) {
    uint32_t elapsedSec = (millis() - rtc_last_millis) / 1000UL;
    timeval tv = {(time_t)(rtc_last_epoch + elapsedSec), 0};
    settimeofday(&tv, nullptr);
    return;
  }

  Preferences p;
  if (p.begin("clock", true)) {
    uint32_t saved = p.getULong("epoch", 0);
    p.end();
    if (saved >= 1704067200) {
      timeval tv = {(time_t)saved, 0};
      settimeofday(&tv, nullptr);
    }
  }
}

#include "smooth_clock56.h"

#include <Preferences.h>
#include "smooth_digits48.h"

enum SettingsSubpage {
  SET_MAIN,
  SET_THEME,
  SET_FONT,
  SET_LANGUAGE,
  SET_TIME_REGION,
  SET_SECURITY,
  SET_RESET,
  SET_POWER
};

static SettingsSubpage settingsPage = SET_MAIN;

enum LockScreenMode {
  LOCK_HERO,
  LOCK_KEYPAD
};

static LockScreenMode lockScreenMode = LOCK_HERO;

// Security / Screen Lock State
static bool screenLockEnabled = false;
static String screenLockPin = "1234";
static bool screenLocked = false;
static String pinInput = "";
static bool pinError = false;
static uint32_t pinErrorMs = 0;
static bool settingChangePin = false;
static String secMessage = "";

// Timeout & Dark mode
static int screenTimeoutSec = 60; // Default 1 min
static const int TIMEOUT_PRESETS[] = {10, 30, 60, 180, 300, 0};
static const char* TIMEOUT_LABELS[] = {"10 sec", "30 sec", "1 min", "3 min", "5 min", "Never"};
static const int NUM_TIMEOUT_PRESETS = 6;
static bool lockscreenDarkOnly = true;
static bool screenBlanked = false;
static uint32_t lastTouchActivityMs = 0;

// Reset State
static bool settingFormatConfirm = false;
static String resetMessage = "";

// Forward declarations of core quick_settings functions
static void enterPowerOff();
static uint16_t blendText(uint16_t fg, uint16_t bg, int a);
static void setDarkTheme(bool value);
static void setAppsListView(bool value);
extern bool appsListView;
static void setActiveFont(uint8_t id);
extern uint8_t activeFontId;

static void initSecuritySettings() {
  static bool loaded = false;
  if (!loaded) {
    Preferences p;
    if (p.begin("security", true)) {
      screenLockEnabled = p.getBool("lock_en", false);
      screenLockPin = p.getString("lock_pin", "1234");
      screenTimeoutSec = p.getInt("timeout_s", 60);
      lockscreenDarkOnly = p.getBool("lock_dark", true);
      p.end();
    }
    if (screenLockPin.length() != 4) screenLockPin = "1234";
    if (screenLockEnabled) {
      screenLocked = true;
      lockScreenMode = LOCK_HERO;
    }
    loaded = true;
  }
}

static void saveSecuritySettings() {
  Preferences p;
  if (p.begin("security", false)) {
    p.putBool("lock_en", screenLockEnabled);
    p.putString("lock_pin", screenLockPin);
    p.putInt("timeout_s", screenTimeoutSec);
    p.putBool("lock_dark", lockscreenDarkOnly);
    p.end();
  }
}

static bool wipeSdCardDir(const char* dirPath) {
  if (!sdOk) return false;
  File root = SD_MMC.open(dirPath);
  if (!root || !root.isDirectory()) return false;
  File file = root.openNextFile();
  while (file) {
    String p = file.path();
    if (file.isDirectory()) {
      wipeSdCardDir(p.c_str());
      SD_MMC.rmdir(p.c_str());
    } else {
      SD_MMC.remove(p.c_str());
    }
    file = root.openNextFile();
  }
  return true;
}

static void executeSdFormat() {
  drawLoadingScreen("Formatting SD...", "Please wait!");
  if (!sdOk) {
    resetMessage = "SD Card not mounted!";
    return;
  }
  wipeSdCardDir("/sd");
  SD_MMC.mkdir("/maps");
  SD_MMC.mkdir("/activities");
  SD_MMC.mkdir("/routes");
  g_mapCache.begin();
  resetMessage = "SD wiped! Core folders reset.";
}

static void executeSoftReset() {
  drawLoadingScreen("Rebooting device...", "Please wait!");
  if (rideStarted || activityFile) saveActivity();
  delay(600);
  ESP.restart();
}

static void drawKeypad(int startY) {
  const char* labels[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {"CLR", "0", "<"}
  };
  for (int r = 0; r < 4; r++) {
    int ky = startY + r * 30;
    for (int c = 0; c < 3; c++) {
      int kx = 31 + c * 62;
      display.fillRoundRect(kx, ky, 54, 25, 6, uiCard());
      display.drawRoundRect(kx, ky, 54, 25, 6, darkTheme ? 0x2945 : 0xD6BA);
      uint16_t col = (r == 3 && c == 0) ? 0xF800 : (r == 3 && c == 2) ? (darkTheme ? 0x07FF : 0x0419) : uiFg();
      centeredLabel(kx, ky + 8, 54, labels[r][c], 1, col);
    }
  }
}

static void tapKeypad(int x, int y, int startY, bool isSettingNewPin) {
  if (x < 15 || x >= 225 || y < startY || y >= startY + 120) return;
  int c = (x < 93) ? 0 : (x < 147 ? 1 : 2);
  int r = (y - startY) / 30;
  if (r < 0 || r > 3) return;

  if (r < 3) {
    char digit = '1' + (r * 3 + c);
    if (pinInput.length() < 4) {
      pinInput += digit;
      pinError = false;
      uiRevision++;
    }
  } else if (r == 3) {
    if (c == 0) { // CLR
      pinInput = "";
      pinError = false;
      uiRevision++;
      return;
    } else if (c == 1) { // '0'
      if (pinInput.length() < 4) {
        pinInput += '0';
        pinError = false;
        uiRevision++;
      }
    } else if (c == 2) { // Backspace '<'
      if (pinInput.length() > 0) {
        pinInput.remove(pinInput.length() - 1);
        pinError = false;
        uiRevision++;
        return;
      }
    }
  }

  if (pinInput.length() == 4) {
    if (isSettingNewPin) {
      screenLockPin = pinInput;
      saveSecuritySettings();
      settingChangePin = false;
      pinInput = "";
      secMessage = "New PIN saved: " + screenLockPin;
      uiRevision++;
    } else {
      if (pinInput == screenLockPin) {
        screenLocked = false;
        pinInput = "";
        pinError = false;
        uiRevision++;
      } else {
        pinError = true;
        pinErrorMs = millis();
        pinInput = "";
        uiRevision++;
      }
    }
  }
}

static void drawSmoothClock48(int startY, int h12, int min, uint16_t fg) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", h12, min);

  const int colonAdvance = 14 * 16;
  int totalAdvance = 0;
  for (int i = 0; i < 5; i++) {
    char c = buf[i];
    if (c >= '0' && c <= '9') {
      totalAdvance += smoothDigitGlyphs48[c - '0'].advance;
    } else if (c == ':') {
      totalAdvance += colonAdvance;
    }
  }

  int pen = (120 * 16) - (totalAdvance / 2);
  for (int i = 0; i < 5; i++) {
    char c = buf[i];
    if (c >= '0' && c <= '9') {
      const auto& g = smoothDigitGlyphs48[c - '0'];
      int gx = (pen + 8) / 16 + g.x;
      int gy = startY + g.y;
      for (int row = 0; row < g.h; row++) {
        for (int col = 0; col < g.w; col++) {
          int idx = row * g.w + col;
          uint8_t bits = pgm_read_byte(&smoothDigitBits48[g.offset + idx / 2]);
          int a = (bits >> ((idx & 1) ? 0 : 4)) & 15;
          if (a) {
            uint16_t bg = display.backgroundPixel(gx + col, gy + row);
            display.drawPixel(gx + col, gy + row, blendText(fg, bg, a));
          }
        }
      }
      pen += g.advance;
    } else if (c == ':') {
      int cx = (pen + 8) / 16 + 7;
      display.fillCircle(cx, startY + 10, 3, fg);
      display.fillCircle(cx, startY + 24, 3, fg);
      pen += colonAdvance;
    }
  }
}

// Frame 14: Lock Screen Hero (Clock, Date, Up Chevron, Lock Button, "Swipe up to unlock")
static void drawSmoothClock56(int startY, int hour, int min, uint16_t fg) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", hour, min);
  int totalAdvance = 0;
  for (int i = 0; i < 5; i++) {
    char c = buf[i];
    int idx = (c >= '0' && c <= '9') ? (c - '0') : (c == ':' ? 10 : -1);
    if (idx >= 0) totalAdvance += pgm_read_word(&smoothClockGlyphs56[idx].advance);
  }
  int pen = (120 * 16) - (totalAdvance / 2);
  for (int i = 0; i < 5; i++) {
    char c = buf[i];
    int idx = (c >= '0' && c <= '9') ? (c - '0') : (c == ':' ? 10 : -1);
    if (idx >= 0) {
      uint16_t offset = pgm_read_word(&smoothClockGlyphs56[idx].offset);
      uint16_t advance = pgm_read_word(&smoothClockGlyphs56[idx].advance);
      uint8_t w = pgm_read_byte(&smoothClockGlyphs56[idx].w);
      uint8_t h = pgm_read_byte(&smoothClockGlyphs56[idx].h);
      int8_t gx_ofs = (int8_t)pgm_read_byte(&smoothClockGlyphs56[idx].x);
      int8_t gy_ofs = (int8_t)pgm_read_byte(&smoothClockGlyphs56[idx].y);

      int gx = (pen + 8) / 16 + gx_ofs;
      int gy = startY + (42 - h - gy_ofs);

      for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
          int bitIdx = row * w + col;
          uint8_t bits = pgm_read_byte(&smoothClockBits56[offset + bitIdx / 2]);
          int a = (bits >> ((bitIdx & 1) ? 0 : 4)) & 15;
          if (a) {
            uint16_t bg = display.backgroundPixel(gx + col, gy + row);
            display.drawPixel(gx + col, gy + row, blendText(fg, bg, a));
          }
        }
      }
      pen += advance;
    }
  }
}

// Frame 14: Lock Screen Hero (Clock, Date, Up Chevron, Lock Button, "Swipe up to unlock")
static void drawLockHeroScreen() {
  uint16_t bg = uiBg();
  uint16_t fg = uiFg();
  uint16_t muted = uiMuted();
  uint16_t cardBg = uiCard();

  statusBar(true, false);

  time_t now = time(nullptr);
  struct tm local;
  bool timeValid = (now >= 1704067200);
  if (timeValid) {
    localtime_r(&now, &local);
  } else {
    memset(&local, 0, sizeof(local));
    uint32_t sec = millis() / 1000UL;
    local.tm_hour = (sec / 3600UL) % 24;
    local.tm_min = (sec / 60UL) % 60;
    local.tm_sec = sec % 60;
    local.tm_mday = 1;
    local.tm_mon = 0;
    local.tm_year = 124;
  }

  const char* const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int h12 = local.tm_hour % 12;
  if (h12 == 0) h12 = 12;

  String dateStr;
  if (timeValid) {
    dateStr = String(days[local.tm_wday]) + " " + String(local.tm_mday) + " " + String(months[local.tm_mon]) + ", " + String(1900 + local.tm_year);
  } else {
    dateStr = "Waiting for time sync";
  }

  // Large 56px smooth digital clock centered at Y = 60
  drawSmoothClock56(60, h12, local.tm_min, fg);

  // Date subtitle centered at Y = 112 (size 0 = 12px smooth font, 8px below clock)
  centeredLabel(0, 112, 240, dateStr.c_str(), 0, fg);

  // Inverted downArrow chevron (up-arrow ^) above lock button centered at X = 108 (w = 24), Y = 200
  drawCustomIconFlippedV(display, 108, 200, muted, CustomIcon::downArrowIcon);

  // Circular lock button at X = 120, Y = 246, Radius = 26
  display.fillCircle(120, 246, 26, cardBg);
  display.drawCircle(120, 246, 26, darkTheme ? 0x2945 : 0xD6BA);
  // High-resolution unscaled 24x24 lock icon centered at (108, 234)
  drawCustomIcon(display, 108, 234, fg, CustomIcon::lockIconLarge);

  // Centered label at Y = 286 (increased font size: size 0 = 12px smooth anti-aliased font)
  centeredLabel(0, 286, 240, "Swipe up to unlock", 0, fg);
}

// Frame 15: Lock Screen Keypad (Status Bar, 4 PIN Dots, 3x4 Keypad, Universal Nav Bar)
static void drawLockKeypadScreen() {
  uint16_t bg = uiBg();
  uint16_t fg = uiFg();
  uint16_t muted = uiMuted();
  uint16_t cardBg = uiCard();

  statusBar(true, false);

  // 4 PIN Dots at Y = 48
  for (int i = 0; i < 4; i++) {
    int dx = 87 + i * 22;
    if (pinError) {
      display.fillCircle(dx, 48, 5, 0xF800);
    } else if (i < (int)pinInput.length()) {
      display.fillCircle(dx, 48, 5, fg);
    } else {
      display.fillCircle(dx, 48, 5, cardBg);
      display.drawCircle(dx, 48, 5, muted);
    }
  }

  // 3x3 Keypad buttons (radius = 25, diameter = 50, gap = 10)
  const char* digits[3][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"}
  };
  const int rowY[4] = {89, 144, 199, 254};
  const int colX[3] = {60, 120, 180};
  const int btnRadius = 25;
  const int btnDiameter = 50;
  const int btnGap = 10;

  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      int cx = colX[c];
      int cy = rowY[r];
      display.fillCircle(cx, cy, btnRadius, cardBg);
      display.drawCircle(cx, cy, btnRadius, darkTheme ? 0x2945 : 0xD6BA);
      centeredLabel(cx - btnRadius, cy - 7, btnDiameter, digits[r][c], 2, fg);
    }
  }

  // Row 3: Backspace Capsule (width = btnDiameter * 2 + btnGap = 110, height = 50, radius = 25)
  // Spans precisely from left of Col 0 (35) to right of Col 1 (145)
  int capsuleX = colX[0] - btnRadius; // 35
  int capsuleY = rowY[3] - btnRadius; // 229
  int capsuleW = btnDiameter * 2 + btnGap; // 110
  int capsuleH = btnDiameter; // 50
  display.fillRoundRect(capsuleX, capsuleY, capsuleW, capsuleH, btnRadius, cardBg);
  display.drawRoundRect(capsuleX, capsuleY, capsuleW, capsuleH, btnRadius, darkTheme ? 0x2945 : 0xD6BA);
  // Center our back icon (CustomIcon::GoBackIcon) inside the backspace capsule
  drawCustomIcon(display, 90 - 8, rowY[3] - 8, fg, CustomIcon::GoBackIcon);

  // '0' circular button at (180, 254), radius 25
  display.fillCircle(colX[2], rowY[3], btnRadius, cardBg);
  display.drawCircle(colX[2], rowY[3], btnRadius, darkTheme ? 0x2945 : 0xD6BA);
  centeredLabel(colX[2] - btnRadius, rowY[3] - 7, btnDiameter, "0", 2, fg);

  // Universal Bottom Navigation Bar (y = 286..319)
  display.fillRect(0, 286, 240, 34, uiBg());
  drawCustomIcon(display, 42, 297, fg, CustomIcon::GoBackIcon);
  drawCustomIcon(display, 112, 297, fg, CustomIcon::HomeIcon);
}

static void drawLockScreen() {
  bool prevDark = darkTheme;
  if (lockscreenDarkOnly) darkTheme = true;
  display.fillScreen(uiBg());

  if (lockScreenMode == LOCK_HERO) {
    drawLockHeroScreen();
  } else {
    drawLockKeypadScreen();
  }

  darkTheme = prevDark;
}

static void tapLockScreen(int x, int y) {
  // Universal Navigation Bar tap: y >= 282 (Back / Home returns to hero lock screen)
  if (y >= 282) {
    if (lockScreenMode == LOCK_KEYPAD) {
      lockScreenMode = LOCK_HERO;
      pinInput = "";
      pinError = false;
      uiRevision++;
    }
    return;
  }

  if (lockScreenMode == LOCK_HERO) {
    // Only taps on circular lock button (cx = 120, cy = 246, radius = 36)
    int dx = x - 120, dy = y - 246;
    if (dx * dx + dy * dy <= 36 * 36) {
      if (screenLockEnabled) {
        lockScreenMode = LOCK_KEYPAD;
        pinInput = "";
        pinError = false;
      } else {
        screenLocked = false;
      }
      uiRevision++;
    }
    return;
  }

  // LOCK_KEYPAD mode
  // Rows 0, 1, 2: digits '1' through '9' (y: 60 to 227)
  if (y >= 60 && y < 227) {
    int r = (y < 117) ? 0 : (y < 172 ? 1 : 2);
    int c = (x < 90) ? 0 : (x < 150 ? 1 : 2);
    char digit = '1' + (r * 3 + c);
    if (pinInput.length() < 4) {
      pinInput += digit;
      pinError = false;
      uiRevision++;
    }
  }
  // Row 3: Backspace capsule (x < 150) and '0' circle (x >= 150) (y: 227 to 282)
  else if (y >= 227 && y < 282) {
    if (x < 150) {
      // Backspace
      if (pinInput.length() > 0) {
        pinInput.remove(pinInput.length() - 1);
        pinError = false;
        uiRevision++;
        return;
      }
    } else {
      // '0'
      if (pinInput.length() < 4) {
        pinInput += '0';
        pinError = false;
        uiRevision++;
      }
    }
  }

  // Check PIN when 4 digits have been entered
  if (pinInput.length() == 4) {
    if (pinInput == screenLockPin) {
      screenLocked = false;
      lockScreenMode = LOCK_HERO;
      pinInput = "";
      pinError = false;
      uiRevision++;
    } else {
      pinError = true;
      pinErrorMs = millis();
      pinInput = "";
      uiRevision++;
    }
  }
}

static void drawSettingsApp() {
  initSecuritySettings();

  if (settingsPage == SET_MAIN) {
    const char* titles[] = {"Theme & Font", "Language", "Time & Region", "Security", "Reset", "Power"};
    CustomIcon icons[] = {
      CustomIcon::themeSwitcherIcon,
      CustomIcon::languageIcon,
      CustomIcon::historyIcon,
      CustomIcon::lockIcon,
      CustomIcon::refreshIcon,
      CustomIcon::powerIcon
    };

    display.fillRect(0, 26, 240, 264, uiBg());
    for (int i = 0; i < 6; i++) {
      int y = 38 - settingsScroll + i * 64;
      if (y + 54 <= 26 || y >= 286) continue;
      display.fillRoundRect(8, y, 224, 54, 27, uiCard());
      drawCustomIcon(display, 22, y + 15, uiFg(), icons[i], 24, 24);
      label(62, y + 19, titles[i], 2, uiFg());
      drawCustomIcon(display, 206, y + 19, uiMuted(), CustomIcon::goNextIcon, 16, 16);
    }
  }
  else if (settingsPage == SET_THEME) {
    // 1. Dark Mode Toggle Card (y = 38, h = 54, R = 27)
    display.fillRoundRect(8, 38, 224, 54, 27, uiCard());
    drawCustomIcon(display, 22, 38 + 15, uiFg(), CustomIcon::themeSwitcherIcon, 24, 24);
    label(62, 38 + 19, "Dark Mode", 2, uiFg());

    // Toggle pill
    int tx = 166, ty = 38 + 13, tw = 54, th = 28;
    uint16_t toggleBg = darkTheme ? 0x34C2 : 0xBDD7;
    display.fillRoundRect(tx, ty, tw, th, 14, toggleBg);
    int circleX = darkTheme ? (tx + tw - 14) : (tx + 14);
    display.fillCircle(circleX, ty + 14, 11, 0xFFFF);
    label(darkTheme ? (tx + 7) : (tx + 26), ty + 9, darkTheme ? "ON" : "OFF", 0, 0xFFFF);

    // 2. System Font Card (y = 102, h = 54, R = 27)
    display.fillRoundRect(8, 102, 224, 54, 27, uiCard());
    drawCustomIcon(display, 22, 102 + 15, uiFg(), CustomIcon::fontIcon, 24, 24);
    label(62, 102 + 19, "System Font", 2, uiFg());
    drawCustomIcon(display, 206, 102 + 19, uiMuted(), CustomIcon::goNextIcon, 16, 16);

    // 3. Apps Screen Layout Card (y = 166, h = 54, R = 27)
    display.fillRoundRect(8, 166, 224, 54, 27, uiCard());
    drawCustomIcon(display, 22, 166 + 15, uiFg(), CustomIcon::allAppsIcon, 24, 24);
    label(62, 166 + 19, "Apps Layout", 2, uiFg());

    // Toggle pill for Apps Layout (Grid / List)
    int tx2 = 166, ty2 = 166 + 13, tw2 = 54, th2 = 28;
    uint16_t toggleBg2 = appsListView ? 0x34C2 : 0xBDD7;
    display.fillRoundRect(tx2, ty2, tw2, th2, 14, toggleBg2);
    int circleX2 = appsListView ? (tx2 + tw2 - 14) : (tx2 + 14);
    display.fillCircle(circleX2, ty2 + 14, 11, 0xFFFF);
    label(appsListView ? (tx2 + 6) : (tx2 + 24), ty2 + 9, appsListView ? "LIST" : "GRID", 0, 0xFFFF);

    // 4. Info Card (y = 230, h = 54, R = 27)
    display.fillRoundRect(8, 230, 224, 54, 27, uiCard());
    drawCustomIcon(display, 22, 230 + 15, 0x34C2, CustomIcon::configureIcon, 24, 24);
    label(62, 230 + 14, "Active Font", 1, uiFg());
    label(62, 230 + 30, SYSTEM_FONT_NAMES[activeFontId], 0, 0x34C2);
  }
  else if (settingsPage == SET_FONT) {
    const char* fontDescs[] = {
      "Modern Geometric Sans",
      "Clean Balanced Sans",
      "Semi Bold Grotesque",
      "Semi Bold Dynamic Italic",
      "Modern Monospace Style",
      "Futuristic Tech Modular"
    };

    for (int i = 0; i < 6; i++) {
      int y = 34 + i * 38;
      bool sel = (activeFontId == i);
      display.fillRoundRect(6, y, 228, 34, 10, uiCard());
      if (sel) display.drawRoundRect(6, y, 228, 34, 10, 0x34C2);

      // Radio circle
      display.drawCircle(22, y + 17, 7, sel ? 0x34C2 : uiMuted());
      if (sel) display.fillCircle(22, y + 17, 3, 0x34C2);

      label(38, y + 4, SYSTEM_FONT_NAMES[i], 1, sel ? 0x34C2 : uiFg());
      label(38, y + 19, fontDescs[i], 0, uiMuted());
    }
  }
  else if (settingsPage == SET_LANGUAGE) {
    display.fillRoundRect(6, 34, 228, 54, 12, uiCard());

    // Selected radio circle
    display.drawCircle(24, 61, 8, 0x34C2);
    display.fillCircle(24, 61, 4, 0x34C2);

    label(44, 46, "English (United States)", 1, uiFg());
    label(44, 64, "Active system language", 1, uiMuted());

    display.fillRoundRect(6, 96, 228, 64, 12, uiCard());
    label(16, 144, "System Language", 1, uiFg());
    label(16, 160, "English (US) is the sole active", 1, uiMuted());
    label(16, 174, "language supported by firmware.", 1, uiMuted());
  }
  else if (settingsPage == SET_TIME_REGION) {
    // Live Clock Hero Card
    display.fillRoundRect(6, 34, 228, 66, 12, uiCard());

    time_t now = time(nullptr);
    char timeBuf[16] = "--:-- --";
    if (now >= 1704067200) {
      struct tm local;
      localtime_r(&now, &local);
      int h12 = local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12;
      snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", h12, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
    }
    label(18, 44, timeBuf, 3, darkTheme ? 0x07FF : 0x0419);
    label(18, 76, "India Standard Time (IST)", 1, uiMuted());

    // Details Card
    display.fillRoundRect(6, 108, 228, 80, 12, uiCard());
    label(16, 118, "Region: Asia / Kolkata", 1, uiFg());
    label(16, 134, "Timezone: UTC+05:30 (No DST)", 1, uiFg());
    label(16, 150, "Auto-Sync: Phone BLE (CTS) / GPS", 1, uiMuted());
    label(16, 166, now >= 1704067200 ? "Sync Status: Locked" : "Sync Status: Searching for fix", 1, now >= 1704067200 ? 0x2564 : 0xFA08);

    // Info Note
    display.fillRoundRect(6, 196, 228, 48, 12, uiCard());
    label(16, 236, "Clock syncs automatically via", 1, uiMuted());
    label(16, 250, "BLE phone link or GPS satellite.", 1, uiMuted());
  }
  else if (settingsPage == SET_SECURITY) {
    if (settingChangePin) {
      display.fillRoundRect(6, 34, 228, 66, 12, uiCard());
      label(16, 44, "SET NEW 4-DIGIT PIN", 1, uiFg());
      label(16, 58, "Enter 4 numbers below:", 1, uiMuted());

      int dotStartX = 87;
      for (int i = 0; i < 4; i++) {
        int dx = dotStartX + i * 22;
        if (i < (int)pinInput.length()) display.fillCircle(dx, 82, 5, uiFg());
        else display.drawCircle(dx, 82, 5, uiMuted());
      }
      drawKeypad(112);
    } else {
      // 1. Screen Lock Toggle Card (y = 34, h = 40)
      display.fillRoundRect(6, 34, 228, 40, 10, uiCard());
      label(16, 41, "Screen Lock", 1, uiFg());
      label(16, 56, screenLockEnabled ? "PIN required on wake" : "Direct access (no PIN)", 1, uiMuted());
      // Toggle Pill
      int tx = 168, ty = 41, tw = 54, th = 26;
      uint16_t toggleBg = screenLockEnabled ? 0x34C2 : (darkTheme ? 0x3A4B : 0xBDD7);
      display.fillRoundRect(tx, ty, tw, th, 13, toggleBg);
      int circleX = screenLockEnabled ? (tx + tw - 13) : (tx + 13);
      display.fillCircle(circleX, ty + 13, 10, 0xFFFF);
      label(screenLockEnabled ? (tx + 7) : (tx + 27), ty + 8, screenLockEnabled ? "ON" : "OFF", 1, 0xFFFF);

      // 2. Change PIN Card (y = 80, h = 40)
      display.fillRoundRect(6, 80, 228, 40, 10, uiCard());
      label(16, 87, "Change Password / PIN", 1, uiFg());
      label(16, 102, String("Current PIN: ") + (screenLockPin.length() ? "****" : "None"), 1, uiMuted());
      drawCustomIcon(display, 212, 92, uiMuted(), CustomIcon::goNextIcon, 16, 16);

      // 3. Screen Timeout Card (y = 126, h = 40)
      display.fillRoundRect(6, 126, 228, 40, 10, uiCard());
      label(16, 133, "Screen Timeout", 1, uiFg());
      const char* tLabel = "1 min";
      for (int i = 0; i < NUM_TIMEOUT_PRESETS; i++) {
        if (TIMEOUT_PRESETS[i] == screenTimeoutSec) { tLabel = TIMEOUT_LABELS[i]; break; }
      }
      label(16, 148, String("Auto-off: ") + tLabel + " (tap to cycle)", 1, uiMuted());
      label(170, 140, tLabel, 1, 0x34C2);

      // 4. Lockscreen Dark Mode Toggle (y = 172, h = 40)
      display.fillRoundRect(6, 172, 228, 40, 10, uiCard());
      label(16, 179, "Lockscreen Dark Mode", 1, uiFg());
      label(16, 194, lockscreenDarkOnly ? "Always AMOLED black" : "Follows active theme", 1, uiMuted());
      // Toggle Pill
      int tx2 = 168, ty2 = 179, tw2 = 54, th2 = 26;
      uint16_t toggleBg2 = lockscreenDarkOnly ? 0x34C2 : (darkTheme ? 0x3A4B : 0xBDD7);
      display.fillRoundRect(tx2, ty2, tw2, th2, 13, toggleBg2);
      int circleX2 = lockscreenDarkOnly ? (tx2 + tw2 - 13) : (tx2 + 13);
      display.fillCircle(circleX2, ty2 + 13, 10, 0xFFFF);
      label(lockscreenDarkOnly ? (tx2 + 7) : (tx2 + 27), ty2 + 8, lockscreenDarkOnly ? "ON" : "OFF", 1, 0xFFFF);

      // 5. Lock Device Now Button (y = 218, h = 38)
      display.fillRoundRect(6, 218, 228, 38, 12, uiCard());
      drawCustomIcon(display, 16, 226, uiFg(), CustomIcon::lockIcon, 22, 22);
      label(46, 230, "Lock Device Now", 1, uiFg());
      drawCustomIcon(display, 212, 229, uiFg(), CustomIcon::goNextIcon, 16, 16);

      // Status Note
      label(12, 264, secMessage.length() ? secMessage : "Protects settings & device with PIN lock.", 1, uiMuted());
    }
  }
  else if (settingsPage == SET_RESET) {
    if (settingFormatConfirm) {
      display.fillRoundRect(6, 38, 228, 178, 12, uiCard());
      display.drawRoundRect(6, 38, 228, 178, 12, 0xF800);

      drawCustomIcon(display, 110, 46, 0xF800, CustomIcon::warningIcon, 20, 20);
      centeredLabel(6, 70, 228, "FORMAT SD CARD?", 2, 0xF800);
      label(18, 94, "All saved GPX routes, FIT", 1, uiFg());
      label(18, 110, "activities, and offline map tiles", 1, uiFg());
      label(18, 126, "will be permanently erased!", 1, 0xF800);

      // Cancel button
      display.fillRoundRect(16, 152, 96, 42, 21, uiCard());
      centeredLabel(16, 168, 96, "CANCEL", 1, uiFg());

      // Confirm button
      display.fillRoundRect(124, 152, 96, 42, 21, 0xF800);
      centeredLabel(124, 168, 96, "WIPE SD", 1, 0xFFFF);
    } else {
      // 1. Soft Reset Card
      display.fillRoundRect(6, 34, 228, 76, 12, uiCard());
      label(16, 44, "Soft Reset", 2, uiFg());
      label(16, 64, "Reboot system (same as reset button)", 1, uiMuted());

      display.fillRoundRect(16, 80, 204, 24, 12, 0x2564);
      centeredLabel(16, 87, 204, "RESTART DEVICE", 1, 0xFFFF);

      // 2. Format SD Card
      display.fillRoundRect(6, 118, 228, 76, 12, uiCard());
      label(16, 128, "Format MicroSD", 2, uiFg());
      label(16, 148, "Wipes tracks, maps & resets folders", 1, uiMuted());

      display.fillRoundRect(16, 164, 204, 24, 12, 0xF800);
      centeredLabel(16, 171, 204, "FORMAT SD CARD", 1, 0xFFFF);

      // Status Note
      label(12, 206, resetMessage.length() ? resetMessage : (sdOk ? ("SD: " + sdStatus) : "SD Card unavailable"), 1, uiMuted());
    }
  }
  else if (settingsPage == SET_POWER) {
    // Battery / Hardware Details Card
    display.fillRoundRect(8, 38, 224, 82, 14, uiCard());
    label(18, 46, "Power System Details", 2, uiFg());
    label(18, 68, "Power Source: USB 5V / LiPo Battery", 1, uiFg());
    label(18, 84, "Power Latch: GPIO 7 (SYS_EN Active)", 1, 0x34C2);
    label(18, 98, "PMIC: ETA6003 | Key: GPIO 6", 0, uiMuted());

    // Restart Button (224x54, R=27)
    display.fillRoundRect(8, 130, 224, 54, 27, uiCard());
    drawCustomIcon(display, 22, 130 + 15, uiFg(), CustomIcon::refreshIcon, 24, 24);
    label(62, 130 + 19, "Restart Device", 2, uiFg());

    // Shut Down Button (224x54, R=27, Red)
    display.fillRoundRect(8, 194, 224, 54, 27, 0xF800);
    drawCustomIcon(display, 22, 194 + 15, 0xFFFF, CustomIcon::powerIcon, 24, 24);
    label(62, 194 + 19, "Shut Down", 2, 0xFFFF);

    label(12, 256, "Turn on: press power key or plug USB.", 0, uiMuted());
  }

  // Redraw fixed status bar and navigation bar for all settings pages
  statusBar(true, false);
  display.fillRect(0, 286, 240, 34, uiBg());
  drawGlobalNav();
}

static void tapSettings(int x, int y) {
  if (settingsPage == SET_MAIN) {
    if (x >= 8 && x <= 232 && y >= 26 && y < 286) {
      int yScrolled = y - 38 + settingsScroll;
      int item = yScrolled / 64;
      if (item >= 0 && item < 6 && (yScrolled % 64) < 54) {
        if (item == 0) { settingsPage = SET_THEME; uiRevision++; }
        else if (item == 1) { settingsPage = SET_LANGUAGE; uiRevision++; }
        else if (item == 2) { settingsPage = SET_TIME_REGION; uiRevision++; }
        else if (item == 3) { settingsPage = SET_SECURITY; secMessage = ""; uiRevision++; }
        else if (item == 4) { settingsPage = SET_RESET; resetMessage = ""; settingFormatConfirm = false; uiRevision++; }
        else if (item == 5) { settingsPage = SET_POWER; uiRevision++; }
      }
    }
  }
  else if (settingsPage == SET_THEME) {
    if (x >= 8 && x <= 232 && y >= 38 && y < 92) {
      setDarkTheme(!darkTheme);
      uiRevision++;
    } else if (x >= 8 && x <= 232 && y >= 102 && y < 156) {
      settingsPage = SET_FONT;
      uiRevision++;
    } else if (x >= 8 && x <= 232 && y >= 166 && y < 220) {
      setAppsListView(!appsListView);
      uiRevision++;
    } else if (x >= 8 && x <= 232 && y >= 230 && y < 284) {
      settingsPage = SET_FONT;
      uiRevision++;
    }
  }
  else if (settingsPage == SET_FONT) {
    if (x >= 6 && x < 234) {
      for (int i = 0; i < 6; i++) {
        int iy = 34 + i * 38;
        if (y >= iy && y < iy + 34) {
          setActiveFont(i);
          uiRevision++;
          break;
        }
      }
    }
  }
  else if (settingsPage == SET_SECURITY) {
    if (settingChangePin) {
      tapKeypad(x, y, 112, true);
    } else {
      // 1. Toggle Screen Lock (y: 34 to 74)
      if (x >= 6 && x < 234 && y >= 34 && y < 74) {
        screenLockEnabled = !screenLockEnabled;
        saveSecuritySettings();
        secMessage = screenLockEnabled ? "Screen Lock enabled!" : "Screen Lock disabled.";
        uiRevision++;
      }
      // 2. Change PIN (y: 80 to 120)
      else if (x >= 6 && x < 234 && y >= 80 && y < 120) {
        settingChangePin = true;
        pinInput = "";
        secMessage = "";
        uiRevision++;
      }
      // 3. Screen Timeout cycle (y: 126 to 166)
      else if (x >= 6 && x < 234 && y >= 126 && y < 166) {
        int curIdx = 2;
        for (int i = 0; i < NUM_TIMEOUT_PRESETS; i++) {
          if (TIMEOUT_PRESETS[i] == screenTimeoutSec) { curIdx = i; break; }
        }
        curIdx = (curIdx + 1) % NUM_TIMEOUT_PRESETS;
        screenTimeoutSec = TIMEOUT_PRESETS[curIdx];
        saveSecuritySettings();
        secMessage = String("Timeout set: ") + TIMEOUT_LABELS[curIdx];
        uiRevision++;
      }
      // 4. Lockscreen Dark Mode toggle (y: 172 to 212)
      else if (x >= 6 && x < 234 && y >= 172 && y < 212) {
        lockscreenDarkOnly = !lockscreenDarkOnly;
        saveSecuritySettings();
        secMessage = lockscreenDarkOnly ? "Lockscreen: Dark Mode ON" : "Lockscreen: Follows theme";
        uiRevision++;
      }
      // 5. Lock Now (y: 218 to 256)
      else if (x >= 6 && x < 234 && y >= 218 && y < 256) {
        screenLocked = true;
        lockScreenMode = LOCK_HERO;
        pinInput = "";
        pinError = false;
        uiRevision++;
      }
    }
  }
  else if (settingsPage == SET_RESET) {
    if (settingFormatConfirm) {
      if (y >= 152 && y < 194) {
        if (x >= 16 && x < 112) { // Cancel
          settingFormatConfirm = false;
          uiRevision++;
        } else if (x >= 124 && x < 220) { // Wipe SD
          settingFormatConfirm = false;
          executeSdFormat();
          uiRevision++;
        }
      }
    } else {
      // Soft reset
      if (x >= 16 && x < 220 && y >= 80 && y < 104) {
        executeSoftReset();
      }
      // Format SD button
      else if (x >= 16 && x < 220 && y >= 164 && y < 188) {
        settingFormatConfirm = true;
        uiRevision++;
      }
    }
  }
  else if (settingsPage == SET_POWER) {
    // Restart
    if (x >= 8 && x <= 232 && y >= 130 && y < 184) {
      executeSoftReset();
    }
    // Shut Down
    else if (x >= 8 && x <= 232 && y >= 194 && y < 248) {
      drawLoadingScreen("Shutting down...", "Saving data");
      delay(400);
      enterPowerOff();
    }
  }
}
