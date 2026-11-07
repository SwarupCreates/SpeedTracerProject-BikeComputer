def update_icons_h(path):
    with open(path, 'r', encoding='utf-8') as f:
        c = f.read()

    old_func = """static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct, bool isCharging, bool alert = false, uint16_t overrideColor = 0) {
  uint16_t col;
  CustomIcon icon;

  if (isCharging) {
    col = overrideColor ? overrideColor : 0x3CE0; // Bright charging green
    // Smooth 5-phase animation: 25% -> 50% -> 75% -> 100% -> Bolt
    int phase = (millis() / 350) % 5;
    if (phase == 0) icon = CustomIcon::battery25Icon;
    else if (phase == 1) icon = CustomIcon::battery50Icon;
    else if (phase == 2) icon = CustomIcon::battery75Icon;
    else if (phase == 3) icon = CustomIcon::batteryFullIcon;
    else icon = CustomIcon::batteryChargeIcon;
  } else if (alert || pct < 10) {
    bool blink = (millis() / 500) % 2;
    col = overrideColor ? overrideColor : (blink ? 0xF800 : 0x8000);
    icon = CustomIcon::batteryAlertIcon;
  } else {
    // Normal discharge: switch design based on level
    if (pct >= 80) {
      icon = CustomIcon::batteryFullIcon;
      col = overrideColor ? overrideColor : 0xFFFF;
    } else if (pct >= 50) {
      icon = CustomIcon::battery75Icon;
      col = overrideColor ? overrideColor : 0xFFFF;
    } else if (pct >= 25) {
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
  drawBatteryIcon(gfx, x, y, pct, false, (pct < 10), fgColor);
}

static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct = 76) {
  drawBatteryIcon(gfx, x, y, pct, false, (pct < 10), 0);
}"""

    new_func = """static inline void drawBatteryIcon(Adafruit_GFX &gfx, int x, int y, int pct, bool isCharging, bool alert = false, uint16_t overrideColor = 0, bool connected = true) {
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
}"""

    if old_func in c:
        c = c.replace(old_func, new_func)
    else:
        old_crlf = old_func.replace('\n', '\r\n')
        new_crlf = new_func.replace('\n', '\r\n')
        if old_crlf in c:
            c = c.replace(old_crlf, new_crlf)
        else:
            print("Warning: old_func not found in", path)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(c)
    print("Updated", path)


def update_firmware_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        c = f.read()

    # 1. State definitions
    old_state = """// Live Battery State (Frame 27 Parity with Level & Charging Animations)
static int batteryPercent = 76;
static bool batteryCharging = false;
static bool batteryAlert = false;"""

    new_state = """// Live Battery State (Frame 27 Parity with Level & Charging Animations)
static bool batteryConnected = false; // Default: no physical battery connected
static int batteryPercent = 76;
static bool batteryCharging = false;
static bool batteryAlert = false;"""

    if old_state in c:
        c = c.replace(old_state, new_state)
    else:
        old_crlf = old_state.replace('\n', '\r\n')
        new_crlf = new_state.replace('\n', '\r\n')
        if old_crlf in c:
            c = c.replace(old_crlf, new_crlf)
        else:
            print("Warning: old_state not found in", path)

    # 2. statusBar battery rendering
    old_status = """  // Battery Status (Frame 27 Parity with Animated & Level-Switched States)
  String batteryText;
  uint16_t batCol;
  if (batteryCharging) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0x3CE0; // Charging green
  } else if (batteryAlert || batteryPercent < 10) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xF800; // Red alert
  } else if (batteryPercent < 25) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xF800; // Red low
  } else if (batteryPercent < 50) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xFA60; // Amber
  } else {
    batteryText = String(batteryPercent) + "%";
    batCol = uiFg(); // Crisp white in dark theme
  }

  int batX = 208;
  int batY = 8;
  label(batX - 4 - textWidth(batteryText, 0), 10, batteryText, 0, batCol);
  drawBatteryIcon(display, batX, batY, batteryPercent, batteryCharging, batteryAlert);"""

    new_status = """  // Battery Status (Frame 27 Parity with User Spec Ranges & NO BATTERY Detection)
  String batteryText;
  uint16_t batCol;
  if (!batteryConnected) {
    batteryText = "N/A";
    batCol = uiMuted();
  } else if (batteryCharging) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0x3CE0; // Charging green
  } else if (batteryAlert || batteryPercent <= 10) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xF800; // Red alert (0-10)
  } else if (batteryPercent <= 40) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xF800; // Red low (11-40)
  } else if (batteryPercent <= 60) {
    batteryText = String(batteryPercent) + "%";
    batCol = 0xFA60; // Amber (41-60)
  } else {
    batteryText = String(batteryPercent) + "%";
    batCol = uiFg(); // Crisp white in dark theme (61-100)
  }

  int batX = 208;
  int batY = 8;
  label(batX - 4 - textWidth(batteryText, 0), 10, batteryText, 0, batCol);
  drawBatteryIcon(display, batX, batY, batteryPercent, batteryCharging, batteryAlert, batCol, batteryConnected);"""

    if old_status in c:
        c = c.replace(old_status, new_status)
    else:
        old_crlf = old_status.replace('\n', '\r\n')
        new_crlf = new_status.replace('\n', '\r\n')
        if old_crlf in c:
            c = c.replace(old_crlf, new_crlf)
        else:
            print("Warning: old_status not found in", path)

    # 3. Serial command handler
    old_cmd = """    if (arg == "CHG" || arg == "CHARGE") {
      batteryCharging = true;
      batteryAlert = false;
      Serial.println("OK:BAT_CHARGING");
    } else if (arg == "ALERT") {
      batteryCharging = false;
      batteryAlert = true;
      Serial.println("OK:BAT_ALERT");
    } else {
      int val = arg.toInt();
      batteryPercent = constrain(val, 0, 100);
      batteryCharging = false;
      batteryAlert = (batteryPercent < 10);
      Serial.printf("OK:BAT_SET %d\\n", batteryPercent);
    }"""

    new_cmd = """    if (arg == "NONE" || arg == "DISCONNECT" || arg == "NO_BAT") {
      batteryConnected = false;
      batteryCharging = false;
      batteryAlert = false;
      Serial.println("OK:BAT_NONE");
    } else if (arg == "CHG" || arg == "CHARGE") {
      batteryConnected = true;
      batteryCharging = true;
      batteryAlert = false;
      Serial.println("OK:BAT_CHARGING");
    } else if (arg == "ALERT") {
      batteryConnected = true;
      batteryCharging = false;
      batteryAlert = true;
      Serial.println("OK:BAT_ALERT");
    } else {
      int val = arg.toInt();
      batteryConnected = true;
      batteryPercent = constrain(val, 0, 100);
      batteryCharging = false;
      batteryAlert = (batteryPercent <= 10);
      Serial.printf("OK:BAT_SET %d\\n", batteryPercent);
    }"""

    if old_cmd in c:
        c = c.replace(old_cmd, new_cmd)
    else:
        old_crlf = old_cmd.replace('\n', '\r\n')
        new_crlf = new_cmd.replace('\n', '\r\n')
        if old_crlf in c:
            c = c.replace(old_crlf, new_crlf)
        else:
            print("Warning: old_cmd not found in", path)

    # 4. JSON status report in activity archive
    old_doc = """  doc["batteryStatus"]="unavailable";
  doc["batteryPercent"]=nullptr; doc["batteryConnected"]=nullptr; doc["batteryCharging"]=nullptr;"""

    new_doc = """  if (!batteryConnected) {
    doc["batteryStatus"]="unavailable";
    doc["batteryPercent"]=nullptr; doc["batteryConnected"]=false; doc["batteryCharging"]=false;
  } else {
    doc["batteryStatus"]=batteryCharging ? "charging" : (batteryPercent <= 10 ? "low" : "normal");
    doc["batteryPercent"]=batteryPercent; doc["batteryConnected"]=true; doc["batteryCharging"]=batteryCharging;
  }"""

    if old_doc in c:
        c = c.replace(old_doc, new_doc)
    else:
        old_crlf = old_doc.replace('\n', '\r\n')
        new_crlf = new_doc.replace('\n', '\r\n')
        if old_crlf in c:
            c = c.replace(old_crlf, new_crlf)
        else:
            print("Warning: old_doc not found in", path)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(c)
    print("Updated", path)

if __name__ == "__main__":
    update_icons_h("firmware/src/icons.h")
    update_icons_h("schematikIO-Export/src/icons.h")
    update_firmware_cpp("firmware/src/firmware.cpp")
    update_firmware_cpp("schematikIO-Export/src/firmware.cpp")
