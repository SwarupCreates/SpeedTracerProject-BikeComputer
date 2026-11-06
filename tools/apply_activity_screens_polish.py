import os

def update_firmware():
    with open('firmware/src/firmware.cpp', 'r', encoding='utf-8') as f:
        fc = f.read()

    # 1. Update statusBar activity blinking icon
    old_status_act = """  if (recording) {
    drawBikeIcon(display, x, 5, blinkOn ? RED : uiBg());
    x += iconSize + gap;
  }"""
    new_status_act = """  if (recording) {
    const uint16_t actCol = darkTheme ? 0x3CE0 : 0x05E0;
    const CustomIcon actIcon = runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon;
    drawCustomIcon(display, x, 5, blinkOn ? actCol : uiBg(), actIcon, 16, 16);
    x += iconSize + gap;
  }"""
    assert old_status_act in fc, 'old_status_act not found'
    fc = fc.replace(old_status_act, new_status_act, 1)

    # 2. Update drawDashScreen
    old_dash = """  // Bike / Run Icon inside Arc
  if (runningMode) drawCustomIcon(display, 107, 58, TEAL, CustomIcon::runIcon, 26, 18);
  else drawBikeIcon(display, 107, 58, TEAL);

  // Large Speed Digits & Unit inside Arc
  String sp = speedAvailable() ? (runningMode ? paceText(ride.speedKmh) : String(ride.speedKmh, 1)) : (runningMode ? String("0:00") : String("0.0"));
  centeredLabel(40, 84, 160, sp, 3, fgCol);
  centeredLabel(40, 122, 160, (ride.gpsFixed || (!runningMode && (bleSpeedReady || (sensors.speedPhysical && millis() - externalSensorMs < 5000)))) ? (runningMode ? "MIN/KM" : "KM/H") : "NO DATA", 1, mutedCol);

  // Complications Row (Frame 27: 3 columns at y = 158..180)
  // Col 1: Distance
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("--"));
  centeredLabel(10, 158, 64, distStr, 2, fgCol);
  centeredLabel(10, 175, 64, "DISTANCE (KM)", 0, mutedCol);

  // Col 2: Total Time
  centeredLabel(78, 158, 84, durationText(), 2, fgCol);
  centeredLabel(78, 175, 84, "TOTAL TIME", 0, mutedCol);

  // Col 3: Gradient
  String gradStr = (ride.gpsFixed && trackLen > 1 ? String(ride.gradePercent, 0) + "%" : String("--"));
  centeredLabel(166, 158, 64, gradStr, 2, fgCol);
  centeredLabel(166, 175, 64, "GRADIENT", 0, mutedCol);

  // Heart Rate Row (Frame 27: y = 201..245)
  label(18, 208, "AVG", 1, mutedCol);
  label(18, 222, (hrSamples ? String(hrSum / hrSamples) : String("--")), 2, fgCol);

  // Center HR Pill (w = 116, h = 44, R = 22)
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg = darkTheme ? 0x0A20 : 0xDFF0;
  uint16_t hrPillFg = darkTheme ? 0x7FE0 : 0x0540;
  display.fillRoundRect(62, 201, 116, 44, 22, hrPillBg);
  drawHeartIcon(display, 76, 214, hrPillFg);
  label(104, 212, bleHRReady ? String(hrVal) : "--", 3, hrPillFg);

  label(192, 208, "MAX", 1, mutedCol);
  label(188, 222, (hrSamples ? String(hrMax) : String("--")), 2, fgCol);"""

    new_dash = """  // Bike / Run Icon inside Arc (Frame 27: 30x30 square icon at x=105, y=54)
  drawCustomIcon(display, 105, 54, TEAL, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 30, 30);

  // Large Speed Digits & Unit inside Arc
  String sp = speedAvailable() ? (runningMode ? paceText(ride.speedKmh) : String(ride.speedKmh, 1)) : (runningMode ? String("0:00") : String("0.0"));
  centeredLabel(40, 88, 160, sp, 3, fgCol);
  centeredLabel(40, 122, 160, (ride.gpsFixed || (!runningMode && (bleSpeedReady || (sensors.speedPhysical && millis() - externalSensorMs < 5000)))) ? (runningMode ? "MIN/KM" : "KM/H") : "NO DATA", 1, mutedCol);

  // Complications Row (Frame 27: 3 columns at y = 158..180)
  // Col 1: Distance
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("--"));
  centeredLabel(10, 158, 64, distStr, 2, fgCol);
  centeredLabel(10, 175, 64, "DISTANCE (KM)", 0, mutedCol);

  // Col 2: Total Time
  centeredLabel(78, 158, 84, durationText(), 2, fgCol);
  centeredLabel(78, 175, 84, "TOTAL TIME", 0, mutedCol);

  // Col 3: Gradient
  String gradStr = (ride.gpsFixed && trackLen > 1 ? String(ride.gradePercent, 0) + "%" : String("--"));
  centeredLabel(166, 158, 64, gradStr, 2, fgCol);
  centeredLabel(166, 175, 64, "GRADIENT", 0, mutedCol);

  // Heart Rate Row (Frame 27: y = 201..245)
  label(18, 208, "AVG", 1, mutedCol);
  label(18, 222, (hrSamples ? String(hrSum / hrSamples) : String("--")), 2, fgCol);

  // Center HR Pill (w = 116, h = 44, R = 22)
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg = darkTheme ? 0x0A20 : 0xDFF0;
  uint16_t hrPillFg = darkTheme ? 0x7FE0 : 0x0540;
  display.fillRoundRect(62, 201, 116, 44, 22, hrPillBg);
  String hrStr = bleHRReady ? String(hrVal) : "--";
  int hrTextW = textWidth(hrStr, 3);
  int hrBlockW = 30 + 6 + hrTextW;
  int hrStartX = 62 + (116 - hrBlockW) / 2;
  drawCustomIcon(display, hrStartX, 208, hrPillFg, CustomIcon::heartRateIcon, 30, 30);
  label(hrStartX + 36, 213, hrStr, 3, hrPillFg);

  label(192, 208, "MAX", 1, mutedCol);
  label(188, 222, (hrSamples ? String(hrMax) : String("--")), 2, fgCol);"""
    assert old_dash in fc, 'old_dash not found'
    fc = fc.replace(old_dash, new_dash, 1)

    # 3. Update drawMetricsScreen
    old_metrics = """static void drawMetricsScreen() {
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Row 1 (y = 38, h = 54): Two 108x54 pills
  uint16_t spdBg = darkTheme ? 0x09A5 : 0xCFF7;
  uint16_t spdFg = darkTheme ? 0x475F : 0x0395;
  display.fillRoundRect(8, 38, 108, 54, 27, spdBg);
  String sp = speedAvailable() ? (runningMode ? paceText(ride.speedKmh) : String(ride.speedKmh, 1)) : (runningMode ? String("0:00") : String("0.0"));
  centeredLabel(8, 50, 108, sp, 3, spdFg);

  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
  uint16_t hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  display.fillRoundRect(124, 38, 108, 54, 27, hrPillBg);
  drawHeartIcon(display, 138, 54, hrPillFg);
  label(162, 50, bleHRReady ? String(hrVal) : "--", 3, hrPillFg);

  // Row 2 (y = 102, h = 54): Calories Card (224x54)
  display.fillRoundRect(8, 102, 224, 54, 27, cardBg);
  const int calCx = 38, calCy = 129;
  uint16_t ringBg = darkTheme ? 0x31A6 : 0xD69A;
  for (int d = 0; d < 360; d += 6) {
    float a = radians((float)d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 13; r <= 15; r++) display.drawPixel(calCx + (int)(r * ca), calCy + (int)(r * sa), ringBg);
  }
  float calRatio = constrain(calorieEstimate / 1200.0f, 0.0f, 1.0f);
  int calSweep = (int)(360.0f * calRatio);
  for (int d = -90; d <= -90 + calSweep; d += 3) {
    float a = radians((float)d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 13; r <= 15; r++) display.drawPixel(calCx + (int)(r * ca), calCy + (int)(r * sa), ORANGE);
  }
  drawFlameIcon(display, calCx - 6, calCy - 8, ORANGE);
  label(68, 115, String((int)calorieEstimate) + "/1200", 3, fgCol);
  label(186, 122, "KCAL", 1, mutedCol);

  // Row 3 (y = 166, h = 54): Two 108x54 pills
  display.fillRoundRect(8, 166, 108, 54, 27, cardBg);
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("0.00"));
  centeredLabel(8, 178, 108, distStr, 3, fgCol);

  display.fillRoundRect(124, 166, 108, 54, 27, cardBg);
  drawLightningIcon(display, 138, 178, 0xFEE0);
  String pwrStr = runningMode ? paceText(ride.speedKmh) : (blePowerReady ? String(ride.powerW) : "104");
  label(160, 178, pwrStr, 3, fgCol);

  // Row 4 (y = 230, h = 54): 224x54 pill with Bike icon + Duration
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  drawCustomIcon(display, 28, 244, TEAL, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 26, 18);
  centeredLabel(50, 242, 170, durationText(), 3, fgCol);
}"""

    new_metrics = """static void drawMetricsScreen() {
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Row 1 (y = 38, h = 54): Two 108x54 pills
  uint16_t spdBg = darkTheme ? 0x09A5 : 0xCFF7;
  uint16_t spdFg = darkTheme ? 0x475F : 0x0395;
  display.fillRoundRect(8, 38, 108, 54, 27, spdBg);
  String sp = speedAvailable() ? (runningMode ? paceText(ride.speedKmh) : String(ride.speedKmh, 1)) : (runningMode ? String("0:00") : String("0.0"));
  centeredLabel(8, 55, 108, sp, 3, spdFg);

  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
  uint16_t hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  display.fillRoundRect(124, 38, 108, 54, 27, hrPillBg);
  String hrStr = bleHRReady ? String(hrVal) : "--";
  int hrTextW = textWidth(hrStr, 3);
  int hrBlockW = 30 + 6 + hrTextW;
  int hrStartX = 124 + (108 - hrBlockW) / 2;
  drawCustomIcon(display, hrStartX, 50, hrPillFg, CustomIcon::heartRateIcon, 30, 30);
  label(hrStartX + 36, 55, hrStr, 3, hrPillFg);

  // Row 2 (y = 102, h = 54): Calories Card (224x54) with 42x42 Dial & 24x24 Icon
  display.fillRoundRect(8, 102, 224, 54, 27, cardBg);
  const int calCx = 36, calCy = 129;
  uint16_t ringBg = darkTheme ? 0x31A6 : 0xD69A;
  for (int d = 0; d < 360; d += 4) {
    float a = radians((float)d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 18; r <= 21; r++) display.drawPixel(calCx + (int)lroundf(r * ca), calCy + (int)lroundf(r * sa), ringBg);
  }
  int tgtCal = max(100, cfg.targetCalories);
  float calRatio = constrain(calorieEstimate / (float)tgtCal, 0.0f, 1.0f);
  int calSweep = (int)(360.0f * calRatio);
  for (int d = -90; d <= -90 + calSweep; d += 2) {
    float a = radians((float)d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 18; r <= 21; r++) display.drawPixel(calCx + (int)lroundf(r * ca), calCy + (int)lroundf(r * sa), ORANGE);
  }
  drawCustomIcon(display, 24, 117, ORANGE, CustomIcon::caloriesIcon, 24, 24);
  String calStr = String((int)calorieEstimate) + "/" + String(tgtCal);
  label(64, 119, calStr, 3, fgCol);
  label(222 - textWidth("KCAL", 1), 125, "KCAL", 1, mutedCol);

  // Row 3 (y = 166, h = 54): Two 108x54 pills
  display.fillRoundRect(8, 166, 108, 54, 27, cardBg);
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("0.00"));
  centeredLabel(8, 183, 108, distStr, 3, fgCol);

  display.fillRoundRect(124, 166, 108, 54, 27, cardBg);
  String pwrStr = runningMode ? paceText(ride.speedKmh) : (blePowerReady ? String(ride.powerW) : "104");
  int pwrTextW = textWidth(pwrStr, 3);
  int pwrBlockW = 30 + 6 + pwrTextW;
  int pwrStartX = 124 + (108 - pwrBlockW) / 2;
  drawCustomIcon(display, pwrStartX, 178, 0xFEE0, CustomIcon::powerIcon, 30, 30);
  label(pwrStartX + 36, 183, pwrStr, 3, fgCol);

  // Row 4 (y = 230, h = 54): 224x54 pill with Bike icon + Duration
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  String durStr = durationText();
  int durTextW = textWidth(durStr, 3);
  int durBlockW = 30 + 10 + durTextW;
  int durStartX = 8 + (224 - durBlockW) / 2;
  drawCustomIcon(display, durStartX, 242, TEAL, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 30, 30);
  label(durStartX + 40, 247, durStr, 3, fgCol);
}"""
    assert old_metrics in fc, 'old_metrics not found'
    fc = fc.replace(old_metrics, new_metrics, 1)

    # 4. Update drawMapScreen bottom HUD
    old_map_hud = """  // Bottom Split HUD (Frame 30: SPEED & DISTANCE dual pills at y = 230..284)
  // Left Pill: SPEED (Dark cyan background, cyan text)
  uint16_t spdBg = darkTheme ? 0x09A5 : 0xCFF7;
  uint16_t spdFg = darkTheme ? 0x475F : 0x0395;
  display.fillRoundRect(8, 230, 108, 54, 27, spdBg);
  float curSpeed = speedAvailable() ? ride.speedKmh : 0.0f;
  if (curSpeed < 0.1f) curSpeed = 0.0f;
  String sp = runningMode ? paceText(curSpeed) : String(curSpeed, 1);
  centeredLabel(8, 242, 108, sp, 3, spdFg);

  // Right Pill: DISTANCE (Dark card background, white text)
  display.fillRoundRect(124, 230, 108, 54, 27, darkTheme ? 0x18C3 : 0xEF5D);
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("0.00"));
  centeredLabel(124, 242, 108, distStr, 3, uiFg());"""

    new_map_hud = """  // Bottom Split HUD (Frame 30: SPEED & DISTANCE dual pills at y = 230..284)
  // Left Pill: SPEED (Dark cyan background, cyan text)
  uint16_t spdBg = darkTheme ? 0x09A5 : 0xCFF7;
  uint16_t spdFg = darkTheme ? 0x475F : 0x0395;
  display.fillRoundRect(8, 230, 108, 54, 27, spdBg);
  float curSpeed = speedAvailable() ? ride.speedKmh : 0.0f;
  if (curSpeed < 0.1f) curSpeed = 0.0f;
  String sp = runningMode ? paceText(curSpeed) : String(curSpeed, 1);
  centeredLabel(8, 247, 108, sp, 3, spdFg);

  // Right Pill: DISTANCE (Dark card background, white text)
  display.fillRoundRect(124, 230, 108, 54, 27, darkTheme ? 0x18C3 : 0xEF5D);
  String distStr = (rideStarted || activityPoints ? String(ride.distanceKm, 2) : String("0.00"));
  centeredLabel(124, 247, 108, distStr, 3, uiFg());"""
    assert old_map_hud in fc, 'old_map_hud not found'
    fc = fc.replace(old_map_hud, new_map_hud, 1)

    # 5. Update drawActivityControlsScreen
    old_controls = """static void drawActivityControlsScreen() {
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Row 1 (y = 38, h = 54): 224x54 pill with Bike icon + Duration
  display.fillRoundRect(8, 38, 224, 54, 27, cardBg);
  drawCustomIcon(display, 28, 52, TEAL, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 26, 18);
  centeredLabel(50, 50, 170, durationText(), 3, fgCol);

  // Row 2 (y = 102, h = 54): Auto Pause Activity toggle
  display.fillRoundRect(8, 102, 224, 54, 27, cardBg);
  label(24, 120, "Auto Pause Activity", 1, fgCol);
  uint16_t swBg = autoPauseEnabled ? (darkTheme ? 0x3CE0 : 0x2CE0) : (darkTheme ? 0x31A6 : 0xD69A);
  display.fillRoundRect(168, 114, 48, 30, 15, swBg);
  display.fillCircle(autoPauseEnabled ? 201 : 183, 129, 11, 0xFFFF);

  // Row 3 (y = 166, h = 54): Pause Ride warm amber card
  uint16_t amberCardBg = darkTheme ? 0x4220 : 0xFFE0;
  uint16_t amberText = darkTheme ? 0xFEE0 : 0x8C00;
  display.fillRoundRect(8, 166, 224, 54, 27, amberCardBg);
  label(24, 184, ridePaused ? "Resume Ride" : "Pause Ride", 2, amberText);
  display.fillCircle(198, 193, 17, 0xFFFF);
  if (!ridePaused) {
    display.fillRect(193, 186, 3, 14, 0xF800);
    display.fillRect(199, 186, 3, 14, 0xF800);
  } else {
    display.fillTriangle(195, 186, 195, 200, 204, 193, 0x05E0);
  }

  // Row 4 (y = 230, h = 54): REVERSE SLIDER "Swipe to end"
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  label(32, 251, "Swipe to end", 1, mutedCol);

  int pillX = 124 + controlSliderOffset;
  display.fillRoundRect(pillX, 230, 108, 54, 27, 0xE800);
  drawCustomIconFlippedH(display, pillX + 22, 248, 0xFFFF, CustomIcon::goNextIcon, 16, 16);
  drawCustomIcon(display, pillX + 58, 245, 0xFFFF, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 26, 18);"""

    new_controls = """static void drawActivityControlsScreen() {
  statusBar();
  uint16_t fgCol = uiFg();
  uint16_t mutedCol = uiMuted();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Row 1 (y = 38, h = 54): 224x54 pill with Bike icon + Duration
  display.fillRoundRect(8, 38, 224, 54, 27, cardBg);
  String durStr = durationText();
  int durTextW = textWidth(durStr, 3);
  int durBlockW = 30 + 10 + durTextW;
  int durStartX = 8 + (224 - durBlockW) / 2;
  drawCustomIcon(display, durStartX, 50, TEAL, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 30, 30);
  label(durStartX + 40, 55, durStr, 3, fgCol);

  // Row 2 (y = 102, h = 54): Auto Pause Activity toggle
  display.fillRoundRect(8, 102, 224, 54, 27, cardBg);
  label(24, 125, "Auto Pause Activity", 1, fgCol);
  uint16_t swBg = autoPauseEnabled ? (darkTheme ? 0x3CE0 : 0x2CE0) : (darkTheme ? 0x31A6 : 0xD69A);
  display.fillRoundRect(168, 114, 48, 30, 15, swBg);
  display.fillCircle(autoPauseEnabled ? 201 : 183, 129, 11, 0xFFFF);

  // Row 3 (y = 166, h = 54): Pause Ride warm amber card
  uint16_t amberCardBg = darkTheme ? 0x4220 : 0xFFE0;
  uint16_t amberText = darkTheme ? 0xFEE0 : 0x8C00;
  display.fillRoundRect(8, 166, 224, 54, 27, amberCardBg);
  label(24, 186, ridePaused ? "Resume Ride" : "Pause Ride", 2, amberText);
  display.fillCircle(198, 193, 17, 0xFFFF);
  if (!ridePaused) {
    display.fillRect(193, 186, 3, 14, 0xF800);
    display.fillRect(199, 186, 3, 14, 0xF800);
  } else {
    display.fillTriangle(195, 186, 195, 200, 204, 193, 0x05E0);
  }

  // Row 4 (y = 230, h = 54): REVERSE SLIDER "Swipe to end"
  display.fillRoundRect(8, 230, 224, 54, 27, cardBg);
  label(32, 253, "Swipe to end", 1, mutedCol);

  int pillX = 124 + controlSliderOffset;
  display.fillRoundRect(pillX, 230, 108, 54, 27, 0xE800);
  drawCustomIconFlippedH(display, pillX + 18, 249, 0xFFFF, CustomIcon::goNextIcon, 16, 16);
  drawCustomIcon(display, pillX + 54, 242, 0xFFFF, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 30, 30);"""
    assert old_controls in fc, 'old_controls not found'
    fc = fc.replace(old_controls, new_controls, 1)

    # 6. Update drawAutoPauseScreen
    old_pause = """static void drawAutoPauseScreen() {
  statusBar();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Upper Card: 224x182, R = 24 (y = 34..216)
  display.fillRoundRect(8, 34, 224, 182, 24, cardBg);

  // Big White Circle Disc: center (120, 106), r = 30
  display.fillCircle(120, 106, 30, 0xFFFF);
  display.fillRect(111, 94, 6, 24, 0xF800);
  display.fillRect(123, 94, 6, 24, 0xF800);

  // Centered label "Auto paused"
  centeredLabel(8, 162, 224, "Auto paused", 3, uiFg());

  // Lower Green Pill: 224x54, R = 27 (y = 228..282)
  uint16_t greenBg = darkTheme ? 0x3CE0 : 0x2CE0;
  display.fillRoundRect(8, 228, 224, 54, 27, greenBg);
  drawCustomIcon(display, 42, 242, 0xFFFF, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 30, 22);
  label(88, 243, "Override", 3, 0xFFFF);
}"""

    new_pause = """static void drawAutoPauseScreen() {
  statusBar();
  uint16_t cardBg = darkTheme ? 0x18C3 : 0xEF5D;

  // Upper Card: 224x182, R = 24 (y = 34..216)
  display.fillRoundRect(8, 34, 224, 182, 24, cardBg);

  // Big White Circle Disc: center (120, 106), r = 30
  display.fillCircle(120, 106, 30, 0xFFFF);
  display.fillRect(111, 94, 6, 24, 0xF800);
  display.fillRect(123, 94, 6, 24, 0xF800);

  // Centered label "Auto paused"
  centeredLabel(8, 160, 224, "Auto paused", 3, uiFg());

  // Lower Green Pill: 224x54, R = 27 (y = 228..282)
  uint16_t greenBg = darkTheme ? 0x3CE0 : 0x2CE0;
  display.fillRoundRect(8, 228, 224, 54, 27, greenBg);
  String ovrStr = "Override";
  int ovrTextW = textWidth(ovrStr, 3);
  int ovrBlockW = 30 + 10 + ovrTextW;
  int ovrStartX = 8 + (224 - ovrBlockW) / 2;
  drawCustomIcon(display, ovrStartX, 240, 0xFFFF, runningMode ? CustomIcon::runIcon : CustomIcon::bikeIcon, 30, 30);
  label(ovrStartX + 40, 245, ovrStr, 3, 0xFFFF);
}"""
    assert old_pause in fc, 'old_pause not found'
    fc = fc.replace(old_pause, new_pause, 1)

    with open('firmware/src/firmware.cpp', 'w', encoding='utf-8') as f:
        f.write(fc)
    print('Updated firmware/src/firmware.cpp successfully!')

def update_quick_settings():
    with open('firmware/src/quick_settings.h', 'r', encoding='utf-8') as f:
        qs = f.read()

    # 1. saveActivityConfig
    old_save = """static void saveActivityConfig() {
  cfg.maxSpeedKmh = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  Preferences p;
  if (p.begin("cyclo", false)) {
    p.putFloat("maxSpdRide", cfg.maxSpeedRide);
    p.putFloat("maxSpdRun", cfg.maxSpeedRun);
    p.putInt("cntdownSec", cfg.countdownSec);
    p.end();
  }
}"""
    new_save = """static void saveActivityConfig() {
  cfg.maxSpeedKmh = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  Preferences p;
  if (p.begin("cyclo", false)) {
    p.putFloat("maxSpdRide", cfg.maxSpeedRide);
    p.putFloat("maxSpdRun", cfg.maxSpeedRun);
    p.putInt("cntdownSec", cfg.countdownSec);
    p.putInt("tgtCalories", cfg.targetCalories);
    p.end();
  }
}"""
    assert old_save in qs, 'old_save not found'
    qs = qs.replace(old_save, new_save, 1)

    # 2. Tile 5 in drawActivitySettingsApp
    old_tile5 = """  // Tile 5: Speed Deadzone info
  y = 226;
  card(6, y, 228, 38, uiCard());
  drawSatelliteIcon(display, 16, y + 10, uiFg());
  label(44, y + 6, "GPS SPEED DEADZONE", 1, uiFg());
  label(44, y + 20, runningMode ? "Run: < 0.8 km/h clamped to 0" : "Ride: < 1.0 km/h clamped to 0", 1, uiMuted());"""

    new_tile5 = """  // Tile 5: Target Calories
  y = 226;
  card(6, y, 228, 42, uiCard());
  drawCustomIcon(display, 16, y + 9, ORANGE, CustomIcon::caloriesIcon, 24, 24);
  label(46, y + 6, "TARGET CALORIES", 1, uiFg());
  label(46, y + 22, String(cfg.targetCalories) + " KCAL (Target)", 1, uiMuted());
  display.fillRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(152, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(152, y + 13, 30, "-", 2, uiFg());
  display.fillRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2124 : 0xCE79);
  display.drawRoundRect(190, y + 7, 30, 28, 6, darkTheme ? 0x2945 : 0xB596);
  centeredLabel(190, y + 13, 30, "+", 2, uiFg());"""
    assert old_tile5 in qs, 'old_tile5 not found'
    qs = qs.replace(old_tile5, new_tile5, 1)

    # 3. tapActivitySettings
    old_tap = """  // Tile 4: Start Countdown Toggle (y=178..222)
  if (y >= 178 && y < 222) {
    if (cfg.countdownSec == 3) cfg.countdownSec = 5;
    else if (cfg.countdownSec == 5) cfg.countdownSec = 10;
    else cfg.countdownSec = 3;
    saveActivityConfig();
    uiRevision++;
    return;
  }
}"""
    new_tap = """  // Tile 4: Start Countdown Toggle (y=178..222)
  if (y >= 178 && y < 222) {
    if (cfg.countdownSec == 3) cfg.countdownSec = 5;
    else if (cfg.countdownSec == 5) cfg.countdownSec = 10;
    else cfg.countdownSec = 3;
    saveActivityConfig();
    uiRevision++;
    return;
  }

  // Tile 5: Target Calories (y=226..270)
  if (y >= 226 && y < 270) {
    if (x >= 140 && x < 184) {
      cfg.targetCalories = max(200, cfg.targetCalories - 100);
      saveActivityConfig();
      uiRevision++;
    } else if (x >= 184 && x < 232) {
      cfg.targetCalories = min(5000, cfg.targetCalories + 100);
      saveActivityConfig();
      uiRevision++;
    }
    return;
  }
}"""
    assert old_tap in qs, 'old_tap not found'
    qs = qs.replace(old_tap, new_tap, 1)

    with open('firmware/src/quick_settings.h', 'w', encoding='utf-8') as f:
        f.write(qs)
    print('Updated firmware/src/quick_settings.h successfully!')

if __name__ == '__main__':
    update_firmware()
    update_quick_settings()
