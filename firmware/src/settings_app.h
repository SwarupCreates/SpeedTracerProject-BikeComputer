#pragma once
#include <Preferences.h>
#include "smooth_digits48.h"

enum SettingsSubpage {
  SET_MAIN,
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
  if (x < 31 || x >= 209 || y < startY || y >= startY + 120) return;
  int c = (x - 31) / 62;
  if ((x - 31) % 62 >= 54) return;
  int r = (y - startY) / 30;
  if ((y - startY) % 30 >= 25) return;

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
static void drawLockHeroScreen() {
  uint16_t bg = uiBg();
  uint16_t fg = uiFg();
  uint16_t muted = uiMuted();
  uint16_t cardBg = uiCard();

  display.fillScreen(bg);
  statusBar(true, false);

  time_t now = time(nullptr);
  struct tm local;
  bool timeValid = (now >= 1704067200);
  if (timeValid) {
    localtime_r(&now, &local);
  } else {
    memset(&local, 0, sizeof(local));
    local.tm_hour = 5;
    local.tm_min = 15;
    local.tm_wday = 5; // Friday
    local.tm_mday = 6;
    local.tm_mon = 10; // November
    local.tm_year = 126; // 2026
  }

  const char* const days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
  const char* const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  int h12 = local.tm_hour % 12;
  if (h12 == 0) h12 = 12;

  String dateStr = String(days[local.tm_wday]) + " " + String(local.tm_mday) + " " + String(months[local.tm_mon]) + ", " + String(1900 + local.tm_year);

  // Big digital clock centered at Y = 74
  drawSmoothClock48(74, h12, local.tm_min, fg);

  // Date subtitle centered at Y = 126
  centeredLabel(0, 126, 240, dateStr.c_str(), 1, fg);

  // Inverted downArrow chevron (up-arrow ^) above lock button at Y = 184
  drawCustomIconFlippedV(display, 112, 184, muted, CustomIcon::downArrowIcon, 16, 16);

  // Circular lock button at X = 120, Y = 232, Radius = 24
  display.fillCircle(120, 232, 24, cardBg);
  display.drawCircle(120, 232, 24, darkTheme ? 0x2945 : 0xD6BA);
  drawCustomIcon(display, 112, 224, fg, CustomIcon::lockIcon, 16, 16);

  // Centered label at Y = 276
  centeredLabel(0, 276, 240, "Swipe up to unlock", 1, fg);
}

// Frame 15: Lock Screen Keypad (Status Bar, 4 PIN Dots, 3x4 Circular Keypad, Nav Bar)
static void drawLockKeypadScreen() {
  uint16_t bg = uiBg();
  uint16_t fg = uiFg();
  uint16_t muted = uiMuted();
  uint16_t cardBg = uiCard();

  display.fillScreen(bg);
  statusBar(true, false);

  if (pinError && millis() - pinErrorMs > 2000) {
    pinError = false;
  }

  // 4 PIN Dots at Y = 56
  for (int i = 0; i < 4; i++) {
    int dx = 87 + i * 22;
    if (pinError) {
      display.fillCircle(dx, 56, 5, 0xF800);
    } else if (i < (int)pinInput.length()) {
      display.fillCircle(dx, 56, 5, fg);
    } else {
      display.fillCircle(dx, 56, 5, cardBg);
      display.drawCircle(dx, 56, 5, muted);
    }
  }

  // 3x4 Keypad buttons
  const char* digits[3][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"}
  };
  const int rowY[3] = {104, 152, 200};
  const int colX[3] = {60, 120, 180};
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      int cx = colX[c];
      int cy = rowY[r];
      display.fillCircle(cx, cy, 22, cardBg);
      display.drawCircle(cx, cy, 22, darkTheme ? 0x2945 : 0xD6BA);
      centeredLabel(cx - 22, cy - 7, 44, digits[r][c], 2, fg);
    }
  }

  // Row 3: Backspace Capsule (Cols 0 & 1: x = 38 to 142, y = 226, w = 104, h = 44, r = 22)
  display.fillRoundRect(38, 226, 104, 44, 22, cardBg);
  display.drawRoundRect(38, 226, 104, 44, 22, darkTheme ? 0x2945 : 0xD6BA);
  // Crisp Backspace Arrow in center (cx = 90, cy = 248)
  display.fillRect(80, 247, 20, 2, fg);
  display.drawLine(80, 248, 87, 241, fg);
  display.drawLine(80, 249, 87, 242, fg);
  display.drawLine(80, 248, 87, 255, fg);
  display.drawLine(80, 247, 87, 254, fg);

  // '0' circular button at (180, 248)
  display.fillCircle(180, 248, 22, cardBg);
  display.drawCircle(180, 248, 22, darkTheme ? 0x2945 : 0xD6BA);
  centeredLabel(180 - 22, 248 - 7, 44, "0", 2, fg);

  // Bottom Navigation Bar at Y = 298
  // Back arrow on left (cx = 60)
  drawCustomIcon(display, 60 - 8, 298 - 8, fg, CustomIcon::GoBackIcon, 16, 16);
  // Home circle in center (cx = 120)
  display.drawCircle(120, 298, 8, fg);
  display.drawCircle(120, 298, 7, fg);
}

static void drawLockScreen() {
  bool prevDark = darkTheme;
  if (lockscreenDarkOnly) darkTheme = true;

  if (lockScreenMode == LOCK_HERO) {
    drawLockHeroScreen();
  } else {
    drawLockKeypadScreen();
  }

  darkTheme = prevDark;
}

static void tapLockScreen(int x, int y) {
  if (lockScreenMode == LOCK_HERO) {
    int dx = x - 120, dy = y - 232;
    bool lockBtnTapped = (dx * dx + dy * dy <= 30 * 30);
    bool unlockAreaTapped = (y >= 170 && y < 300);

    if (lockBtnTapped || unlockAreaTapped) {
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
  // Bottom navigation bar tap
  if (y >= 280) {
    lockScreenMode = LOCK_HERO;
    pinInput = "";
    pinError = false;
    uiRevision++;
    return;
  }

  // Keypad row 0: 1, 2, 3 (y: 82 to 126)
  // Keypad row 1: 4, 5, 6 (y: 130 to 174)
  // Keypad row 2: 7, 8, 9 (y: 178 to 222)
  if (y >= 82 && y < 222) {
    int r = (y - 82) / 48;
    if (r >= 0 && r < 3) {
      int c = -1;
      if (x >= 38 && x < 82) c = 0;
      else if (x >= 98 && x < 142) c = 1;
      else if (x >= 158 && x < 202) c = 2;

      if (c >= 0) {
        char digit = '1' + (r * 3 + c);
        if (pinInput.length() < 4) {
          pinInput += digit;
          pinError = false;
          uiRevision++;
        }
      }
    }
  } else if (y >= 226 && y < 270) {
    // Backspace capsule: x = 38 to 142
    if (x >= 38 && x < 142) {
      if (pinInput.length() > 0) {
        pinInput.remove(pinInput.length() - 1);
        pinError = false;
        uiRevision++;
        return;
      }
    }
    // '0': x = 158 to 202
    else if (x >= 158 && x < 202) {
      if (pinInput.length() < 4) {
        pinInput += '0';
        pinError = false;
        uiRevision++;
      }
    }
  }

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

  // Header Back Button
  drawCustomIcon(display, 8, 33, uiFg(), CustomIcon::GoBackIcon, 20, 20);

  if (settingsPage == SET_MAIN) {
    label(34, 37, "SETTINGS", 2);

    const char* titles[] = {"Language", "Set Time and Region", "Security", "Reset", "Power"};
    String subtitles[] = {
      "English (US)",
      "UTC+05:30 (IST)",
      screenLockEnabled ? "Screen Lock: ON" : "Screen Lock: OFF",
      "Format SD & Soft Reset",
      "Battery & Power Options"
    };
    CustomIcon icons[] = {
      CustomIcon::languageIcon,
      CustomIcon::historyIcon,
      CustomIcon::lockIcon,
      CustomIcon::refreshIcon,
      CustomIcon::powerIcon
    };

    for (int i = 0; i < 5; i++) {
      int y = 68 + i * 43;
      display.fillRoundRect(6, y, 228, 38, 10, uiCard());
      display.drawRoundRect(6, y, 228, 38, 10, darkTheme ? 0x2945 : 0xD6BA);
      drawCustomIcon(display, 16, y + 11, uiFg(), icons[i], 16, 16);
      label(40, y + 7, titles[i], 1, uiFg());
      label(40, y + 21, subtitles[i], 1, uiMuted());
      drawCustomIcon(display, 214, y + 11, uiMuted(), CustomIcon::goNextIcon, 16, 16);
    }
  }
  else if (settingsPage == SET_LANGUAGE) {
    label(34, 37, "LANGUAGE", 2);

    display.fillRoundRect(6, 72, 228, 52, 10, uiCard());
    display.drawRoundRect(6, 72, 228, 52, 10, darkTheme ? 0x2945 : 0xD6BA);

    // Selected radio circle
    display.drawCircle(22, 98, 8, 0x34C2);
    display.fillCircle(22, 98, 4, 0x34C2);

    label(40, 84, "English (United States)", 1, uiFg());
    label(40, 100, "Active system language", 1, uiMuted());

    display.fillRoundRect(6, 134, 228, 64, 10, uiCard());
    display.drawRoundRect(6, 134, 228, 64, 10, darkTheme ? 0x2945 : 0xD6BA);
    label(16, 148, "System Language", 1, uiFg());
    label(16, 164, "English (US) is the sole active", 1, uiMuted());
    label(16, 178, "language supported by firmware.", 1, uiMuted());
  }
  else if (settingsPage == SET_TIME_REGION) {
    label(34, 37, "TIME & REGION", 2);

    // Live Clock Hero Card
    display.fillRoundRect(6, 68, 228, 64, 10, uiCard());
    display.drawRoundRect(6, 68, 228, 64, 10, darkTheme ? 0x2945 : 0xD6BA);

    time_t now = time(nullptr);
    char timeBuf[16] = "--:-- --";
    if (now >= 1704067200) {
      struct tm local;
      localtime_r(&now, &local);
      int h12 = local.tm_hour % 12 == 0 ? 12 : local.tm_hour % 12;
      snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", h12, local.tm_min, local.tm_hour < 12 ? "AM" : "PM");
    }
    label(18, 78, timeBuf, 3, darkTheme ? 0x07FF : 0x0419);
    label(18, 110, "India Standard Time (IST)", 1, uiMuted());

    // Details Card
    display.fillRoundRect(6, 140, 228, 80, 10, uiCard());
    display.drawRoundRect(6, 140, 228, 80, 10, darkTheme ? 0x2945 : 0xD6BA);
    label(16, 150, "Region: Asia / Kolkata", 1, uiFg());
    label(16, 166, "Timezone: UTC+05:30 (No DST)", 1, uiFg());
    label(16, 182, "Auto-Sync: Phone BLE (CTS) / GPS", 1, uiMuted());
    label(16, 198, now >= 1704067200 ? "Sync Status: Locked" : "Sync Status: Searching for fix", 1, now >= 1704067200 ? 0x2564 : 0xFA08);

    // Info Note
    display.fillRoundRect(6, 228, 228, 48, 10, uiCard());
    display.drawRoundRect(6, 228, 228, 48, 10, darkTheme ? 0x2945 : 0xD6BA);
    label(16, 238, "Clock syncs automatically via", 1, uiMuted());
    label(16, 252, "BLE phone link or GPS satellite.", 1, uiMuted());
  }
  else if (settingsPage == SET_SECURITY) {
    label(34, 37, "SECURITY", 2);

    if (settingChangePin) {
      display.fillRoundRect(6, 68, 228, 66, 10, uiCard());
      display.drawRoundRect(6, 68, 228, 66, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 78, "SET NEW 4-DIGIT PIN", 1, uiFg());
      label(16, 92, "Enter 4 numbers below:", 1, uiMuted());

      int dotStartX = 87;
      for (int i = 0; i < 4; i++) {
        int dx = dotStartX + i * 22;
        if (i < (int)pinInput.length()) display.fillCircle(dx, 116, 5, uiFg());
        else display.drawCircle(dx, 116, 5, uiMuted());
      }
      drawKeypad(146);
    } else {
      // 1. Screen Lock Toggle Card (y = 66, h = 38)
      display.fillRoundRect(6, 66, 228, 38, 10, uiCard());
      display.drawRoundRect(6, 66, 228, 38, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 73, "Screen Lock", 1, uiFg());
      label(16, 88, screenLockEnabled ? "PIN required on wake" : "Direct access (no PIN)", 1, uiMuted());
      // Toggle Pill
      int tx = 168, ty = 71, tw = 54, th = 26;
      uint16_t toggleBg = screenLockEnabled ? 0x34C2 : (darkTheme ? 0x3A4B : 0xBDD7);
      display.fillRoundRect(tx, ty, tw, th, 13, toggleBg);
      int circleX = screenLockEnabled ? (tx + tw - 13) : (tx + 13);
      display.fillCircle(circleX, ty + 13, 10, 0xFFFF);
      label(screenLockEnabled ? (tx + 7) : (tx + 27), ty + 8, screenLockEnabled ? "ON" : "OFF", 1, 0xFFFF);

      // 2. Change PIN Card (y = 108, h = 38)
      display.fillRoundRect(6, 108, 228, 38, 10, uiCard());
      display.drawRoundRect(6, 108, 228, 38, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 115, "Change Password / PIN", 1, uiFg());
      label(16, 130, String("Current PIN: ") + (screenLockPin.length() ? "****" : "None"), 1, uiMuted());
      drawCustomIcon(display, 214, 119, uiMuted(), CustomIcon::goNextIcon, 16, 16);

      // 3. Screen Timeout Card (y = 150, h = 38)
      display.fillRoundRect(6, 150, 228, 38, 10, uiCard());
      display.drawRoundRect(6, 150, 228, 38, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 157, "Screen Timeout", 1, uiFg());
      const char* tLabel = "1 min";
      for (int i = 0; i < NUM_TIMEOUT_PRESETS; i++) {
        if (TIMEOUT_PRESETS[i] == screenTimeoutSec) { tLabel = TIMEOUT_LABELS[i]; break; }
      }
      label(16, 172, String("Auto-off: ") + tLabel + " (tap to cycle)", 1, uiMuted());
      label(170, 164, tLabel, 1, 0x34C2);

      // 4. Lockscreen Dark Mode Toggle (y = 192, h = 38)
      display.fillRoundRect(6, 192, 228, 38, 10, uiCard());
      display.drawRoundRect(6, 192, 228, 38, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 199, "Lockscreen Dark Mode", 1, uiFg());
      label(16, 214, lockscreenDarkOnly ? "Always AMOLED black" : "Follows active theme", 1, uiMuted());
      // Toggle Pill
      int tx2 = 168, ty2 = 197, tw2 = 54, th2 = 26;
      uint16_t toggleBg2 = lockscreenDarkOnly ? 0x34C2 : (darkTheme ? 0x3A4B : 0xBDD7);
      display.fillRoundRect(tx2, ty2, tw2, th2, 13, toggleBg2);
      int circleX2 = lockscreenDarkOnly ? (tx2 + tw2 - 13) : (tx2 + 13);
      display.fillCircle(circleX2, ty2 + 13, 10, 0xFFFF);
      label(lockscreenDarkOnly ? (tx2 + 7) : (tx2 + 27), ty2 + 8, lockscreenDarkOnly ? "ON" : "OFF", 1, 0xFFFF);

      // 5. Lock Device Now Button (y = 234, h = 36)
      display.fillRoundRect(6, 234, 228, 36, 10, uiCard());
      display.drawRoundRect(6, 234, 228, 36, 10, darkTheme ? 0x07FF : 0x0419);
      drawCustomIcon(display, 16, 244, uiFg(), CustomIcon::lockIcon, 16, 16);
      label(40, 246, "Lock Device Now", 1, uiFg());
      drawCustomIcon(display, 214, 244, uiFg(), CustomIcon::goNextIcon, 16, 16);

      // Status Note
      label(12, 276, secMessage.length() ? secMessage : "Protects settings & device with PIN lock.", 1, uiMuted());
    }
  }
  else if (settingsPage == SET_RESET) {
    label(34, 37, "RESET", 2);

    if (settingFormatConfirm) {
      display.fillRoundRect(6, 72, 228, 178, 12, uiCard());
      display.drawRoundRect(6, 72, 228, 178, 12, 0xF800);

      drawCustomIcon(display, 110, 80, 0xF800, CustomIcon::warningIcon, 20, 20);
      centeredLabel(6, 104, 228, "FORMAT SD CARD?", 2, 0xF800);
      label(18, 128, "All saved GPX routes, FIT", 1, uiFg());
      label(18, 144, "activities, and offline map tiles", 1, uiFg());
      label(18, 160, "will be permanently erased!", 1, 0xF800);

      // Cancel button
      display.fillRoundRect(16, 186, 96, 42, 21, uiCard());
      display.drawRoundRect(16, 186, 96, 42, 21, darkTheme ? 0x2945 : 0xD6BA);
      centeredLabel(16, 202, 96, "CANCEL", 1, uiFg());

      // Confirm button
      display.fillRoundRect(124, 186, 96, 42, 21, 0xF800);
      centeredLabel(124, 202, 96, "WIPE SD", 1, 0xFFFF);
    } else {
      // 1. Soft Reset Card
      display.fillRoundRect(6, 68, 228, 76, 10, uiCard());
      display.drawRoundRect(6, 68, 228, 76, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 78, "Soft Reset", 2, uiFg());
      label(16, 98, "Reboot system (same as reset button)", 1, uiMuted());

      display.fillRoundRect(16, 114, 204, 24, 12, 0x2564);
      centeredLabel(16, 121, 204, "RESTART DEVICE", 1, 0xFFFF);

      // 2. Format SD Card
      display.fillRoundRect(6, 152, 228, 76, 10, uiCard());
      display.drawRoundRect(6, 152, 228, 76, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 162, "Format MicroSD", 2, uiFg());
      label(16, 182, "Wipes tracks, maps & resets folders", 1, uiMuted());

      display.fillRoundRect(16, 198, 204, 24, 12, 0xF800);
      centeredLabel(16, 205, 204, "FORMAT SD CARD", 1, 0xFFFF);

      // Status Note
      label(12, 240, resetMessage.length() ? resetMessage : (sdOk ? ("SD: " + sdStatus) : "SD Card unavailable"), 1, uiMuted());
    }
  }
  else if (settingsPage == SET_POWER) {
    label(34, 37, "POWER", 2);

    // Battery / Hardware Details Card
    display.fillRoundRect(6, 68, 228, 88, 10, uiCard());
    display.drawRoundRect(6, 68, 228, 88, 10, darkTheme ? 0x2945 : 0xD6BA);
    label(16, 78, "Power System Details", 2, uiFg());
    label(16, 100, "Power Source: USB 5V / LiPo Battery", 1, uiFg());
    label(16, 116, "Power Latch: GPIO 7 (SYS_EN Active)", 1, 0x34C2);
    label(16, 130, "Onboard PMIC: ETA6003 Power-Path", 1, uiMuted());
    label(16, 144, "Hardware Key: GPIO 6 (Active Low)", 1, uiMuted());

    // Restart Button
    display.fillRoundRect(6, 164, 228, 42, 21, uiCard());
    display.drawRoundRect(6, 164, 228, 42, 21, darkTheme ? 0x2945 : 0xD6BA);
    drawCustomIcon(display, 20, 177, uiFg(), CustomIcon::refreshIcon, 16, 16);
    label(46, 178, "Restart Device", 2, uiFg());

    // Shut Down Button
    display.fillRoundRect(6, 214, 228, 42, 21, 0xF800);
    drawCustomIcon(display, 48, 227, 0xFFFF, CustomIcon::powerIcon, 16, 16);
    label(74, 228, "Shut Down", 2, 0xFFFF);

    label(12, 266, "To turn back on: press power key or plug USB.", 1, uiMuted());
  }
}

static void tapSettings(int x, int y) {
  // Back button header tap
  if (x < 50 && y >= 28 && y < 60) {
    if (settingsPage != SET_MAIN) {
      if (settingChangePin) { settingChangePin = false; pinInput = ""; }
      else if (settingFormatConfirm) { settingFormatConfirm = false; }
      else settingsPage = SET_MAIN;
    } else {
      homePage = HOME_APPS;
    }
    uiRevision++;
    return;
  }

  if (settingsPage == SET_MAIN) {
    if (x >= 6 && x < 234) {
      int item = -1;
      for (int i = 0; i < 5; i++) {
        int iy = 68 + i * 43;
        if (y >= iy && y < iy + 38) { item = i; break; }
      }
      if (item == 0) { settingsPage = SET_LANGUAGE; uiRevision++; }
      else if (item == 1) { settingsPage = SET_TIME_REGION; uiRevision++; }
      else if (item == 2) { settingsPage = SET_SECURITY; secMessage = ""; uiRevision++; }
      else if (item == 3) { settingsPage = SET_RESET; resetMessage = ""; settingFormatConfirm = false; uiRevision++; }
      else if (item == 4) { settingsPage = SET_POWER; uiRevision++; }
    }
  }
  else if (settingsPage == SET_SECURITY) {
    if (settingChangePin) {
      tapKeypad(x, y, 146, true);
    } else {
      // 1. Toggle Screen Lock (y: 66 to 104)
      if (x >= 6 && x < 234 && y >= 66 && y < 104) {
        screenLockEnabled = !screenLockEnabled;
        saveSecuritySettings();
        secMessage = screenLockEnabled ? "Screen Lock enabled!" : "Screen Lock disabled.";
        uiRevision++;
      }
      // 2. Change PIN (y: 108 to 146)
      else if (x >= 6 && x < 234 && y >= 108 && y < 146) {
        settingChangePin = true;
        pinInput = "";
        secMessage = "";
        uiRevision++;
      }
      // 3. Screen Timeout cycle (y: 150 to 188)
      else if (x >= 6 && x < 234 && y >= 150 && y < 188) {
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
      // 4. Lockscreen Dark Mode toggle (y: 192 to 230)
      else if (x >= 6 && x < 234 && y >= 192 && y < 230) {
        lockscreenDarkOnly = !lockscreenDarkOnly;
        saveSecuritySettings();
        secMessage = lockscreenDarkOnly ? "Lockscreen: Dark Mode ON" : "Lockscreen: Follows theme";
        uiRevision++;
      }
      // 5. Lock Now (y: 234 to 272)
      else if (x >= 6 && x < 234 && y >= 234 && y < 272) {
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
      if (y >= 186 && y < 228) {
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
      if (x >= 16 && x < 220 && y >= 114 && y < 138) {
        executeSoftReset();
      }
      // Format SD button
      else if (x >= 16 && x < 220 && y >= 198 && y < 222) {
        settingFormatConfirm = true;
        uiRevision++;
      }
    }
  }
  else if (settingsPage == SET_POWER) {
    // Restart
    if (x >= 6 && x < 234 && y >= 164 && y < 206) {
      executeSoftReset();
    }
    // Shut Down
    else if (x >= 6 && x < 234 && y >= 214 && y < 256) {
      drawLoadingScreen("Shutting down...", "Saving data");
      delay(400);
      enterPowerOff();
    }
  }
}
