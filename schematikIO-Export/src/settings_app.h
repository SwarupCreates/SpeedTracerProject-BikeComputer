#pragma once
#include <Preferences.h>

enum SettingsSubpage {
  SET_MAIN,
  SET_LANGUAGE,
  SET_TIME_REGION,
  SET_SECURITY,
  SET_RESET,
  SET_POWER
};

static SettingsSubpage settingsPage = SET_MAIN;

// Security / Screen Lock State
static bool screenLockEnabled = false;
static String screenLockPin = "1234";
static bool screenLocked = false;
static String pinInput = "";
static bool pinError = false;
static uint32_t pinErrorMs = 0;
static bool settingChangePin = false;
static String secMessage = "";

// Reset State
static bool settingFormatConfirm = false;
static String resetMessage = "";

// Forward declarations of core quick_settings functions
static void enterPowerOff();

static void initSecuritySettings() {
  static bool loaded = false;
  if (!loaded) {
    Preferences p;
    if (p.begin("security", true)) {
      screenLockEnabled = p.getBool("lock_en", false);
      screenLockPin = p.getString("lock_pin", "1234");
      p.end();
    }
    if (screenLockPin.length() != 4) screenLockPin = "1234";
    if (screenLockEnabled) screenLocked = true;
    loaded = true;
  }
}

static void saveSecuritySettings() {
  Preferences p;
  if (p.begin("security", false)) {
    p.putBool("lock_en", screenLockEnabled);
    p.putString("lock_pin", screenLockPin);
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

static void drawLockScreen() {
  display.fillScreen(uiBg());
  statusBar(false, false);

  if (pinError && millis() - pinErrorMs > 2500) {
    pinError = false;
  }

  drawCustomIcon(display, 110, 48, uiFg(), CustomIcon::configureIcon, 20, 20);
  centeredLabel(0, 74, 240, "DEVICE LOCKED", 2, uiFg());
  centeredLabel(0, 96, 240, pinError ? "Incorrect PIN! Try again." : "Enter 4-digit PIN to unlock", 1, pinError ? 0xF800 : uiMuted());

  // 4 PIN Dots
  int dotStartX = 87;
  for (int i = 0; i < 4; i++) {
    int dx = dotStartX + i * 22;
    if (i < (int)pinInput.length()) {
      display.fillCircle(dx, 122, 5, uiFg());
    } else {
      display.drawCircle(dx, 122, 5, uiMuted());
    }
  }

  drawKeypad(146);
}

static void tapLockScreen(int x, int y) {
  tapKeypad(x, y, 146, false);
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
      CustomIcon::systemIcon,
      CustomIcon::historyIcon,
      CustomIcon::configureIcon,
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
      // 1. Screen Lock Toggle Card
      display.fillRoundRect(6, 68, 228, 52, 10, uiCard());
      display.drawRoundRect(6, 68, 228, 52, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 78, "Screen Lock", 2, uiFg());
      label(16, 98, screenLockEnabled ? "Requires PIN on wake" : "No lock (direct access)", 1, uiMuted());

      // Toggle Pill
      int tx = 162, ty = 78, tw = 58, th = 30;
      uint16_t toggleBg = screenLockEnabled ? 0x34C2 : (darkTheme ? 0x3A4B : 0xBDD7);
      display.fillRoundRect(tx, ty, tw, th, 15, toggleBg);
      int circleX = screenLockEnabled ? (tx + tw - 15) : (tx + 15);
      display.fillCircle(circleX, ty + 15, 11, 0xFFFF);
      label(screenLockEnabled ? (tx + 8) : (tx + 30), ty + 10, screenLockEnabled ? "ON" : "OFF", 1, 0xFFFF);

      // 2. Change PIN Card
      display.fillRoundRect(6, 126, 228, 46, 10, uiCard());
      display.drawRoundRect(6, 126, 228, 46, 10, darkTheme ? 0x2945 : 0xD6BA);
      label(16, 136, "Change Password / PIN", 1, uiFg());
      label(16, 152, String("Current PIN: ") + (screenLockPin.length() ? "****" : "None"), 1, uiMuted());
      drawCustomIcon(display, 214, 141, uiMuted(), CustomIcon::goNextIcon, 16, 16);

      // 3. Lock Device Now Button
      display.fillRoundRect(6, 178, 228, 44, 10, uiCard());
      display.drawRoundRect(6, 178, 228, 44, 10, screenLockEnabled ? (darkTheme ? 0x07FF : 0x0419) : (darkTheme ? 0x2945 : 0xD6BA));
      label(16, 192, screenLockEnabled ? "Lock Screen Now" : "Enable lock above to set PIN", 1, screenLockEnabled ? uiFg() : uiMuted());
      if (screenLockEnabled) drawCustomIcon(display, 214, 192, uiFg(), CustomIcon::goNextIcon, 16, 16);

      // Status Note
      label(12, 234, secMessage.length() ? secMessage : "Protects settings & ride history with 4-digit PIN.", 1, uiMuted());
    }
  }
  else if (settingsPage == SET_RESET) {
    label(34, 37, "RESET", 2);

    if (settingFormatConfirm) {
      display.fillRoundRect(6, 72, 228, 178, 12, uiCard());
      display.drawRoundRect(6, 72, 228, 178, 12, 0xF800);

      centeredLabel(6, 88, 228, "FORMAT SD CARD?", 2, 0xF800);
      label(18, 118, "All saved GPX routes, FIT", 1, uiFg());
      label(18, 134, "activities, and offline map tiles", 1, uiFg());
      label(18, 150, "will be permanently erased!", 1, 0xF800);

      // Cancel button
      display.fillRoundRect(16, 182, 96, 42, 21, uiCard());
      display.drawRoundRect(16, 182, 96, 42, 21, darkTheme ? 0x2945 : 0xD6BA);
      centeredLabel(16, 198, 96, "CANCEL", 1, uiFg());

      // Confirm button
      display.fillRoundRect(124, 182, 96, 42, 21, 0xF800);
      centeredLabel(124, 198, 96, "WIPE SD", 1, 0xFFFF);
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
      // Toggle Screen Lock
      if (x >= 150 && x < 230 && y >= 68 && y < 120) {
        screenLockEnabled = !screenLockEnabled;
        saveSecuritySettings();
        secMessage = screenLockEnabled ? "Screen Lock enabled!" : "Screen Lock disabled.";
        uiRevision++;
      }
      // Change PIN
      else if (x >= 6 && x < 234 && y >= 126 && y < 172) {
        settingChangePin = true;
        pinInput = "";
        secMessage = "";
        uiRevision++;
      }
      // Lock Now
      else if (x >= 6 && x < 234 && y >= 178 && y < 222) {
        if (screenLockEnabled) {
          screenLocked = true;
          pinInput = "";
          pinError = false;
          uiRevision++;
        }
      }
    }
  }
  else if (settingsPage == SET_RESET) {
    if (settingFormatConfirm) {
      if (y >= 182 && y < 224) {
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
