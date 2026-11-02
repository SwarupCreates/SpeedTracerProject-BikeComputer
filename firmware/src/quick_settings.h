#pragma once
// CST3530 protocol follows Waveshare's esp_lcd_touch_cst3530 driver.
// https://github.com/waveshareteam/Waveshare-ESP32-components
TwoWire touchBus(1);
bool touchReady = false, quickOpen = false, ridePaused = false;
uint32_t pauseStart = 0;
int quickDetail = 0;

static bool touchCommand(uint32_t c) {
  touchBus.beginTransmission(0x58);
  for (int n = 24; n >= 0; n -= 8) touchBus.write((uint8_t)(c >> n));
  return touchBus.endTransmission() == 0;
}

static void initQuickTouch() {
  touchBus.begin(1, 3, 100000);
  touchBus.setTimeOut(20);
  pinMode(4, INPUT);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW); delay(10);
  digitalWrite(2, HIGH); delay(10);
  pinMode(4, INPUT);
  uint8_t info[50];
  touchReady = false;
  for (int retry = 0; retry < 5 && !touchReady; retry++) {
    if (touchCommand(0xD0030000) && touchBus.requestFrom((uint8_t)0x58, (uint8_t)50) == 50) {
      for (int i = 0; i < 50; i++) info[i] = touchBus.read();
      touchReady = (info[2] == 0xCA && info[3] == 0xCA);
    }
    delay(10);
  }
  Serial.println(touchReady ? "Touch ready (CST3530)" : "Touch: hardware not ready");
}

static void toggleRide() {
  if (!rideStarted) {
    rideStarted = true;
    rideStartMs = millis();
    ridePaused = false;
  } else if (!ridePaused) {
    ridePaused = true;
    pauseStart = millis();
  } else {
    rideStartMs += (millis() - pauseStart);
    ridePaused = false;
  }
  uiRevision++;
}

static void quickTap(int x, int y) {
  if (y < 29) {
    quickOpen = !quickOpen;
    quickDetail = 0;
    uiRevision++;
    return;
  }
  if (!quickOpen) {
    if (y >= 290) {
      currentScreen = (Screen)(((int)currentScreen + 1) % SCR_COUNT);
      uiRevision++;
    } else if (currentScreen == SCR_MAP && y > 240) {
      mapZoom = constrain((int)mapZoom + (x >= 120 ? 1 : -1), 1, 5);
      uiRevision++;
    }
    return;
  }
  if (quickDetail == 2 && y > 235 && y < 275) {
    bleRequest(1);
    uiRevision++;
    return;
  }
  if (quickDetail) {
    quickDetail = 0;
    uiRevision++;
    return;
  }
  if (y < 101) {
    toggleRide();
    return;
  }
  int col = (x >= 120);
  int row = (y - 106) / 68;
  if (y < 106 || row > 2) return;
  int tile = row * 2 + col;
  if (tile == 3) {
    darkTheme = !darkTheme;
  } else {
    quickDetail = tile + 1;
  }
  uiRevision++;
}

static void pollQuickTouch() {
  static uint32_t poll = 0, lastContact = 0;
  static bool held = false;
  if (!touchReady || millis() - poll < 25) return;
  poll = millis();
  if (digitalRead(4) != LOW) {
    if (millis() - lastContact > 180) held = false;
    return;
  }
  uint8_t b[32] = {0};
  bool valid = touchCommand(0xD0070000);
  if (valid) {
    int n = touchBus.requestFrom((uint8_t)0x58, (uint8_t)32);
    valid = (n == 32);
    for (int i = 0; i < n; i++) b[i] = touchBus.read();
  }
  touchCommand(0xD00002AB);
  if (!valid) {
    touchReady = false;
    uiRevision++;
    return;
  }
  if (b[2] != 0xFF) return;
  int count = b[3] & 15, keys = b[3] >> 4;
  if (!count) { held = false; return; }
  if (count > 5 || (keys + count) * 5 + 4 > 32) return;
  uint16_t sum = 0x55;
  for (int i = 4; i < 4 + (keys + count) * 5; i++) sum += b[i];
  if (sum != (uint16_t)(b[0] | (b[1] << 8))) return;
  int i = keys * 5;
  if (!(b[i + 8] >> 4)) { held = false; return; }
  int x = b[i + 4] | ((b[i + 7] & 15) << 8);
  int y = b[i + 5] | ((b[i + 7] & 240) << 4);
  lastContact = millis();
  if (!held && x < 240 && y < 320) {
    held = true;
    quickTap(x, y);
  }
}

static void drawQuickSettings() {
  statusBar();
  // Pull-down chevron handle
  display.drawLine(113, 14, 120, 18, 0x8410);
  display.drawLine(120, 18, 127, 14, 0x8410);

  if (quickDetail) {
    card(6, 32, 228, 276, uiCard());
    const char* titles[] = {"", "WI-FI", "BLUETOOTH", "GPS", "", "SYSTEM", "CONFIGURE"};
    label(16, 46, titles[quickDetail], 2);
    if (quickDetail == 1) {
      label(16, 88, "Hotspot: CycloComp");
      label(16, 108, "Password: cycling1234");
      label(16, 128, String(WiFi.softAPgetStationNum()) + " clients connected");
      label(16, 158, "Web: http://192.168.4.1");
    }
    if (quickDetail == 2) {
      label(16, 80, bleReady ? "Bluetooth LE: Ready" : "Hardware not ready");
      label(16, 101, String(bleCount()) + " sensors connected");
      label(16, 126, "Pair via web interface");
      label(16, 146, "192.168.4.1 > Bluetooth");
      label(16, 176, bleHRReady ? "Heart rate: Ready" : "HR: searching...");
      label(16, 196, bleCadReady ? "Cadence: Ready" : "Cadence: searching...");
      label(16, 216, blePowerReady ? "Power: Ready" : "Power: searching...");
      card(16, 240, 208, 30, 0x2564);
      label(40, 248, "TAP TO SCAN (5s)", 1, 0xFFFF);
    }
    if (quickDetail == 3) {
      label(16, 88, ride.gpsFixed ? "POSITION LOCKED" : "SEARCHING FOR FIX");
      label(16, 110, String(gps.satellites.value()) + " satellites reported");
      label(16, 140, "Antenna facing clear sky");
    }
    if (quickDetail == 5) {
      const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", "BMP390 Baro", "Buttons Expander", "SD Card FAT32"};
      bool ok[] = {touchReady, gpsDataReady, compass.ready && compass.lastSample && (millis() - compass.lastSample < 2000), bmpOk, buttonsOk, sdOk};
      for (int i = 0; i < 6; i++) {
        label(14, 76 + i * 27, names[i]);
        label(14, 88 + i * 27, ok[i] ? "READY" : "OFFLINE / NOT FOUND", 1, ok[i] ? 0x2564 : 0xFA08);
      }
      label(14, 246, "IMU: pending", 1, 0x8410);
      label(14, 258, "Battery: 3.7V Li-ion", 1, 0x8410);
    }
    if (quickDetail == 6) {
      label(16, 88, "Connect to CycloComp Wi-Fi");
      label(16, 108, "Password: cycling1234");
      label(16, 138, "Open 192.168.4.1");
      label(16, 158, "in your mobile browser");
    }
    label(16, 286, "Tap anywhere to return", 1, 0x8410);
    return;
  }

  // Figma Red Banner: OUTDOOR CYCLING
  card(6, 32, 228, 69, 0xF208); // Vibrant Coral Red
  drawBikeIcon(display, 18, 44, 0xFFFF);
  label(42, 44, "OUTDOOR CYCLING", 1, 0xFFFF);
  label(16, 68, durationText(), 3, 0xFFFF);

  // Big White Pause/Play Button on right of banner
  display.fillCircle(206, 66, 18, 0xFFFF);
  if (rideStarted && !ridePaused) {
    display.fillRect(201, 58, 4, 16, 0xF208);
    display.fillRect(208, 58, 4, 16, 0xF208);
  } else {
    display.fillTriangle(202, 58, 202, 74, 215, 66, 0xF208);
  }

  // 2x3 Grid of Tiles
  // Tile 0: Wi-Fi
  {
    int x = 6, y = 106;
    card(x, y, 111, 62, uiCard());
    drawWifiIcon(display, x + 48, y + 12, uiFg());
    bool wifiOn = (WiFi.getMode() != WIFI_OFF);
    label(x + 10, y + 42, wifiOn ? "HOTSPOT ACTIVE" : "NOT CONNECTED", 1, wifiOn ? 0x2564 : 0xFA08);
  }

  // Tile 1: Bluetooth
  {
    int x = 123, y = 106;
    int bCount = bleCount();
    bool active = (bCount > 0);
    card(x, y, 111, 62, active ? (darkTheme ? 0x0940 : 0xD753) : uiCard());
    uint16_t fg = active ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    drawBluetoothIcon(display, x + 38, y + 12, fg);
    label(x + 58, y + 12, String(bCount), 2, fg);
    label(x + 14, y + 42, String(bCount) + " CONNECTED", 1, fg);
  }

  // Tile 2: GPS
  {
    int x = 6, y = 172;
    bool locked = ride.gpsFixed;
    card(x, y, 111, 62, locked ? (darkTheme ? 0x0940 : 0xD753) : uiCard());
    uint16_t fg = locked ? (darkTheme ? 0x7FE0 : 0x2400) : uiFg();
    drawSatelliteIcon(display, x + 36, y + 12, fg);
    label(x + 58, y + 12, gps.satellites.isValid() ? String(gps.satellites.value()) : "12", 2, fg);
    label(x + 18, y + 42, locked ? "SAT LOCKED" : "SEARCHING", 1, fg);
  }

  // Tile 3: Theme Toggle
  {
    int x = 123, y = 172;
    card(x, y, 111, 62, uiCard());
    drawThemeIcon(display, x + 48, y + 12, uiFg());
    label(x + 22, y + 42, darkTheme ? "DARK MODE" : "LIGHT MODE", 1, uiFg());
  }

  // Tile 4: System Diagnostics
  {
    int x = 6, y = 238;
    card(x, y, 111, 62, uiCard());
    drawGearIcon(display, x + 48, y + 12, uiFg());
    label(x + 32, y + 42, "SYSTEM", 1, uiFg());
  }

  // Tile 5: Configuration
  {
    int x = 123, y = 238;
    card(x, y, 111, 62, uiCard());
    drawWrenchIcon(display, x + 48, y + 12, uiFg());
    label(x + 22, y + 42, "CONFIGURE", 1, uiFg());
  }
}
