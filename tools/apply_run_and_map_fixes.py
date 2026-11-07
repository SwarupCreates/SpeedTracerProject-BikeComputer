#!/usr/bin/env python3
import sys, re

def update_firmware_cpp(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        c = f.read()

    # 1. Add elevationGainM to struct RideData
    if "float    elevationGainM = 0;" not in c:
        c = c.replace(
            "struct RideData {\n  float    speedKmh     = 0;\n  float    distanceKm   = 0;\n  float    altM         = 0;",
            "struct RideData {\n  float    speedKmh     = 0;\n  float    distanceKm   = 0;\n  float    altM         = 0;\n  float    elevationGainM = 0;"
        )

    # 2. MAX_TRACK 8192 and TrackPoint* track = nullptr;
    c = c.replace(
        "#define MAX_TRACK 1024\nTrackPoint track[MAX_TRACK];",
        "#define MAX_TRACK 8192\nTrackPoint* track = nullptr;"
    )

    # 3. Allocate track in setup()
    if "track = (TrackPoint*)ps_malloc(sizeof(TrackPoint) * MAX_TRACK);" not in c:
        setup_target = "  display.fillScreen(uiBg());\n  uiRevision++;"
        setup_replacement = """  display.fillScreen(uiBg());
  uiRevision++;

  if (psramFound()) {
    track = (TrackPoint*)ps_malloc(sizeof(TrackPoint) * MAX_TRACK);
  }
  if (!track) {
    track = (TrackPoint*)malloc(sizeof(TrackPoint) * 1024);
  }"""
        c = c.replace(setup_target, setup_replacement)

    # 4. Update paceText
    old_pace = """static String paceText(float kmh){
  if(kmh < 1.8f) return "0:00";
  unsigned seconds = lroundf(3600.0f / kmh);
  if(seconds > 1500) return "0:00";
  return String(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + String(seconds % 60);
}"""
    new_pace = """static String paceText(float kmh){
  if(kmh < 0.5f) return "--:--";
  unsigned seconds = lroundf(3600.0f / kmh);
  if(seconds > 3599) return "59:59";
  unsigned mm = seconds / 60;
  unsigned ss = seconds % 60;
  return String(mm) + ":" + (ss < 10 ? "0" : "") + String(ss);
}"""
    c = c.replace(old_pace, new_pace)

    # 5. Update updateMaxSpeed filter
    old_max_spd = """static void updateMaxSpeed(float currentSpeed) {
  // Physical acceleration limit: human motion cannot jump more than 10 km/h in a single second from standstill
  if (histCount > 0 && abs(currentSpeed - speedHist[0]) > 10.0f) {
    return; // Discard erratic single-second GPS jump
  }"""
    new_max_spd = """static void updateMaxSpeed(float currentSpeed) {
  float maxAllowable = runningMode ? 25.0f : 90.0f;
  float maxJump = runningMode ? 3.0f : 8.0f;
  if (currentSpeed > maxAllowable) return;
  // Physical acceleration limit: reject erratic single-second GPS jump
  if (histCount > 0 && abs(currentSpeed - speedHist[0]) > maxJump) {
    return;
  }"""
    c = c.replace(old_max_spd, new_max_spd)

    # 6. Smooth Speed Arc & HR Tile in drawDashScreen()
    old_dash_arc = """  // Center Circular Arc Gauge (cx = 120, cy = 103, radius = 62, 270 deg sweep)
  const int cx = 120, cy = 103;
  uint16_t arcBg = darkTheme ? 0x2945 : 0xCE79;
  for (int d = 135; d <= 405; d++) {
    float a = radians((float)d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 59; r <= 64; r++) {
      display.drawPixel(cx + (int)lroundf(r * ca), cy + (int)lroundf(r * sa), arcBg);
    }
  }

  // Active Progress Arc
  float spVal = ride.speedKmh;
  if (spVal < 1.5f) spVal = 0.0f;
  float gaugeMax = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  int fillSpan = (int)(270.0f * constrain(spVal / gaugeMax, 0.0f, 1.0f));
  if (fillSpan > 0) {
    for (int d = 135; d <= 135 + fillSpan; d++) {
      float a = radians((float)d);
      float ca = cosf(a), sa = sinf(a);
      for (int r = 59; r <= 64; r++) {
        display.drawPixel(cx + (int)lroundf(r * ca), cy + (int)lroundf(r * sa), TEAL);
      }
    }
  }"""
    new_dash_arc = """  // Center Circular Arc Gauge (cx = 120, cy = 103, radius = 62, 270 deg sweep)
  const int cx = 120, cy = 103;
  uint16_t arcBg = darkTheme ? 0x2945 : 0xCE79;
  for (float d = 135.0f; d <= 405.0f; d += 0.5f) {
    float a = radians(d);
    float ca = cosf(a), sa = sinf(a);
    for (int r = 59; r <= 64; r++) {
      display.drawPixel(cx + (int)lroundf(r * ca), cy + (int)lroundf(r * sa), arcBg);
    }
  }

  // Active Progress Arc (zero out stationary idle jitter)
  float spVal = (ride.speedKmh >= 1.0f && activityRecording()) ? ride.speedKmh : 0.0f;
  float gaugeMax = runningMode ? cfg.maxSpeedRun : cfg.maxSpeedRide;
  int fillSpan = (int)(270.0f * constrain(spVal / gaugeMax, 0.0f, 1.0f));
  if (fillSpan > 0) {
    for (float d = 135.0f; d <= 135.0f + (float)fillSpan; d += 0.5f) {
      float a = radians(d);
      float ca = cosf(a), sa = sinf(a);
      for (int r = 59; r <= 64; r++) {
        display.drawPixel(cx + (int)lroundf(r * ca), cy + (int)lroundf(r * sa), TEAL);
      }
    }
  }"""
    c = c.replace(old_dash_arc, new_dash_arc)

    # 7. HR Pill & Zone in drawDashScreen()
    old_hr_dash = """  // Center HR Pill (w = 116, h = 44, R = 22)
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg = darkTheme ? 0x0A20 : 0xDFF0;
  uint16_t hrPillFg = darkTheme ? 0x7FE0 : 0x0540;
  display.fillRoundRect(62, 201, 116, 44, 22, hrPillBg);
  String hrStr = bleHRReady ? String(hrVal) : "--";
  int hrTextW = textWidth(hrStr, 4);
  int hrBlockW = 30 + 6 + hrTextW;
  int hrStartX = 62 + (116 - hrBlockW) / 2;
  drawCustomIcon(display, hrStartX, 208, hrPillFg, CustomIcon::heartRateIcon, 30, 30);
  label(hrStartX + 36, 209, hrStr, 4, hrPillFg);

  label(192, 208, "MAX", 1, mutedCol);
  label(188, 222, (hrSamples ? String(hrMax) : String("--")), 2, fgCol);

  // Zone text and pointer
  uint16_t zoneCol = darkTheme ? zc.textDark : zc.textLight;
  int segCenters[5] = {35, 78, 120, 162, 205};
  int ptrX = segCenters[constrain(zone - 1, 0, 4)];
  display.fillTriangle(ptrX - 4, 267, ptrX + 4, 267, ptrX, 272, zoneCol);

  // Indicator text sits directly on top of the down arrow marker
  String zoneStr = "ZONE " + String(zone);
  int zw = textWidth(zoneStr, 1);
  int zx = constrain(ptrX - zw / 2, 6, 234 - zw);
  label(zx, 255, zoneStr, 1, zoneCol);"""

    new_hr_dash = """  // Center HR Pill (w = 116, h = 44, R = 22)
  int hrVal = bleHRReady ? bleHR : 0;
  int zone = (bleHRReady && hrVal > 0) ? calcHrZone(hrVal) : 1;
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg, hrPillFg;
  if (bleHRReady && hrVal > 0) {
    hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
    hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  } else {
    hrPillBg = darkTheme ? 0x18C3 : 0xEF5D;
    hrPillFg = mutedCol;
  }
  display.fillRoundRect(62, 201, 116, 44, 22, hrPillBg);
  String hrStr = (bleHRReady && hrVal > 0) ? String(hrVal) : "--";
  int hrTextW = textWidth(hrStr, 4);
  int hrBlockW = 30 + 6 + hrTextW;
  int hrStartX = 62 + (116 - hrBlockW) / 2;
  drawCustomIcon(display, hrStartX, 208, hrPillFg, CustomIcon::heartRateIcon, 30, 30);
  label(hrStartX + 36, 209, hrStr, 4, hrPillFg);

  label(192, 208, "MAX", 1, mutedCol);
  label(188, 222, (hrSamples ? String(hrMax) : String("--")), 2, fgCol);

  // Zone text and pointer (neutral muted grey if no HR reading)
  uint16_t zoneCol = (bleHRReady && hrVal > 0) ? (darkTheme ? zc.textDark : zc.textLight) : mutedCol;
  int segCenters[5] = {35, 78, 120, 162, 205};
  int ptrX = segCenters[constrain(zone - 1, 0, 4)];
  display.fillTriangle(ptrX - 4, 267, ptrX + 4, 267, ptrX, 272, zoneCol);

  // Indicator text sits directly on top of the down arrow marker
  String zoneStr = "ZONE " + String(zone);
  int zw = textWidth(zoneStr, 1);
  int zx = constrain(ptrX - zw / 2, 6, 234 - zw);
  label(zx, 255, zoneStr, 1, zoneCol);"""
    c = c.replace(old_hr_dash, new_hr_dash)

    # 8. HR Pill in drawMetricsScreen()
    old_hr_metrics = """  int hrVal = bleHRReady ? bleHR : 0;
  int zone = calcHrZone(hrVal);
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
  uint16_t hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  display.fillRoundRect(124, 38, 108, 54, 27, hrPillBg);
  String hrStr = bleHRReady ? String(hrVal) : "--";"""
    new_hr_metrics = """  int hrVal = bleHRReady ? bleHR : 0;
  int zone = (bleHRReady && hrVal > 0) ? calcHrZone(hrVal) : 1;
  const HrZoneColors &zc = HR_ZONE_PALETTES[zone - 1];
  uint16_t hrPillBg, hrPillFg;
  if (bleHRReady && hrVal > 0) {
    hrPillBg = darkTheme ? zc.bgDark : zc.bgLight;
    hrPillFg = darkTheme ? zc.textDark : zc.textLight;
  } else {
    hrPillBg = darkTheme ? 0x18C3 : 0xEF5D;
    hrPillFg = mutedCol;
  }
  display.fillRoundRect(124, 38, 108, 54, 27, hrPillBg);
  String hrStr = (bleHRReady && hrVal > 0) ? String(hrVal) : "--";"""
    c = c.replace(old_hr_metrics, new_hr_metrics)

    # 9. Map Trail Drawing Optimization & Anti-cap
    old_map_trail = """  } else {
    // If NOT navigating: livestream recorded breadcrumbs behind rider in Strava Orange (#FC5200)
    if (trackLen > 0) {
      int tx0, ty0;
      projectLatLon(track[0].lat, track[0].lon, tx0, ty0);
      for (int i = 1; i < trackLen; i++) {
        int tx1, ty1;
        projectLatLon(track[i].lat, track[i].lon, tx1, ty1);
        clipAndDrawLine(tx0, ty0, tx1, ty1, stravaOrange, 3);
        tx0 = tx1;
        ty0 = ty1;
      }
      if (ride.gpsFixed && isfinite(ride.lat) && ride.lat != 0.0) {
        int cx1, cy1;
        projectLatLon(ride.lat, ride.lon, cx1, cy1);
        clipAndDrawLine(tx0, ty0, cx1, cy1, stravaOrange, 3);
      }
    }
  }"""
    new_map_trail = """  } else {
    // If NOT navigating: livestream recorded breadcrumbs behind rider in Strava Orange (#FC5200)
    if (trackLen > 0 && track) {
      int tx0, ty0;
      projectLatLon(track[0].lat, track[0].lon, tx0, ty0);
      for (int i = 1; i < trackLen; i++) {
        int tx1, ty1;
        projectLatLon(track[i].lat, track[i].lon, tx1, ty1);
        if (abs(tx1 - tx0) >= 2 || abs(ty1 - ty0) >= 2 || i == trackLen - 1) {
          clipAndDrawLine(tx0, ty0, tx1, ty1, stravaOrange, 3);
          tx0 = tx1;
          ty0 = ty1;
        }
      }
      if (ride.gpsFixed && isfinite(ride.lat) && ride.lat != 0.0) {
        int cx1, cy1;
        projectLatLon(ride.lat, ride.lon, cx1, cy1);
        clipAndDrawLine(tx0, ty0, cx1, cy1, stravaOrange, 3);
      }
    }
  }"""
    c = c.replace(old_map_trail, new_map_trail)

    # 10. Distance accumulation thresholds
    old_dist_accum = """        // Movement conditions:
        // Accumulate once displaced at least 1.5 meters (0.0015 km) from anchor,
        // capturing walking steps (~0.5 to 1.5 m/s) and runs/rides cleanly.
        bool hasMoved = (delta >= 0.0015f && delta < 0.200f && impliedSpeedKmh < 130.0f);
        float minMotionSpd = runningMode ? 0.5f : 0.8f;
        bool isMotion = (curSpd >= minMotionSpd) || (impliedSpeedKmh >= minMotionSpd);"""
    new_dist_accum = """        // Movement conditions:
        // Accumulate once displaced at least 1.0 meter (0.0010 km) from anchor,
        // capturing walking steps (~0.5 to 1.5 m/s) and runs/rides cleanly.
        bool hasMoved = (delta >= 0.0010f && delta < 0.200f && impliedSpeedKmh < 130.0f);
        float minMotionSpd = runningMode ? 0.3f : 0.6f;
        bool isMotion = (curSpd >= minMotionSpd) || (impliedSpeedKmh >= minMotionSpd);"""
    c = c.replace(old_dist_accum, new_dist_accum)

    # 11. Breadcrumb recording with decimation when buffer is full
    old_track_record = """    // Track breadcrumb every 3s while moving
    if (rideStarted && !ridePaused && !autoPaused && millis() - lastTrackMs > 3000) {
      lastTrackMs = millis();
      if(activityFile){fitPoint(nLat,nLon);archivePoint(nLat,nLon);}
      if (trackLen < MAX_TRACK)
        track[trackLen++] = {nLat, nLon, ride.altM, ride.elapsedSec};
    }"""
    new_track_record = """    // Track breadcrumb every 3s while moving
    if (rideStarted && !ridePaused && !autoPaused && millis() - lastTrackMs > 3000) {
      lastTrackMs = millis();
      if(activityFile){fitPoint(nLat,nLon);archivePoint(nLat,nLon);}
      if (track) {
        if (trackLen >= MAX_TRACK) {
          // Decimate older half: discard every 2nd point to compress and make room
          int half = MAX_TRACK / 2;
          int newHalf = half / 2;
          for (int i = 1; i < newHalf; i++) {
            track[i] = track[i * 2];
          }
          int remaining = MAX_TRACK - half;
          memmove(&track[newHalf], &track[half], remaining * sizeof(TrackPoint));
          trackLen = newHalf + remaining;
        }
        track[trackLen++] = {nLat, nLon, ride.altM, ride.elapsedSec};
      }
    }"""
    c = c.replace(old_track_record, new_track_record)

    # 12. Speed deadzone & Auto-Pause / Auto-Resume
    old_deadzone = """  // GPS noise deadzones:
  // In outdoor ride: stationary jitter up to ~1.0 km/h
  // Stationary drift deadzone
  float speedDeadzone = runningMode ? 1.4f : 1.8f;
  if (rawGpsSpeed < speedDeadzone) {
    rawGpsSpeed = 0.0f;
  }"""
    new_deadzone = """  // GPS noise deadzones:
  // In outdoor ride / walk: stationary jitter up to ~0.6 km/h (running) or 1.2 km/h (cycling)
  float speedDeadzone = runningMode ? 0.6f : 1.2f;
  if (rawGpsSpeed < speedDeadzone) {
    rawGpsSpeed = 0.0f;
  }"""
    c = c.replace(old_deadzone, new_deadzone)

    old_autopause = """  // Auto-Pause & Auto-Resume Detection (3-tier filter with speed hysteresis, temporal & spatial confirmation)
  if (rideStarted && !ridePaused && autoPauseEnabled) {
    float pauseSpeedThreshold = runningMode ? 1.0f : 1.5f;
    float resumeSpeedThreshold = runningMode ? 2.8f : 4.0f;
    const float highSpeedThreshold = 7.0f;
    float curSpd = speedAvailable() ? ride.speedKmh : 0.0f;

    if (!autoPaused) {
      if (curSpd < pauseSpeedThreshold && millis() > autoPauseOverrideGraceMs) {
        if (nearZeroSpeedStartMs == 0) {
          nearZeroSpeedStartMs = millis();
        } else if (millis() - nearZeroSpeedStartMs >= 4000) { // 4 seconds idle buffer
          autoPaused = true;
          autoPauseStartMs = millis();
          nearZeroSpeedStartMs = 0;
          autoResumeMovementStartMs = 0;
          autoPauseAnchorLat = (ride.gpsFixed && gps.location.isValid()) ? (float)gps.location.lat() : 0.0f;
          autoPauseAnchorLon = (ride.gpsFixed && gps.location.isValid()) ? (float)gps.location.lng() : 0.0f;
          fitEvent(4); // Pause FIT event
          uiRevision++;
        }
      } else {
        nearZeroSpeedStartMs = 0;
      }
    } else {
      // Auto-resume check:
      // Require HDOP to be reasonable to prevent indoor multipath false triggers
      bool hdopAcceptable = (!gps.hdop.isValid() || gps.hdop.hdop() <= 4.5f);
      bool movingNow = (curSpd >= resumeSpeedThreshold) && hdopAcceptable;

      if (!movingNow) {
        autoResumeMovementStartMs = 0;
      } else {
        if (autoResumeMovementStartMs == 0) {
          autoResumeMovementStartMs = millis();
        } else {
          uint32_t moveDuration = millis() - autoResumeMovementStartMs;

          // Check physical displacement from the pause location (eliminates stationary chair drift)
          float dispMeters = 0.0f;
          if (autoPauseAnchorLat != 0.0f && autoPauseAnchorLon != 0.0f && ride.gpsFixed && gps.location.isValid()) {
            dispMeters = haversineKm(autoPauseAnchorLat, autoPauseAnchorLon, (float)gps.location.lat(), (float)gps.location.lng()) * 1000.0f;
          }
          bool displacementOk = (dispMeters >= 8.0f) || (autoPauseAnchorLat == 0.0f);

          // Fast resume if riding fast (>= 7 km/h for 1.5s), or standard resume (>= 2.5s sustained with > 8m displacement)
          bool highSpeedConfirm = (curSpd >= highSpeedThreshold && moveDuration >= 1500);
          bool normalConfirm = (moveDuration >= 2500 && displacementOk);

          if (highSpeedConfirm || normalConfirm) {"""

    new_autopause = """  // Auto-Pause & Auto-Resume Detection (3-tier filter with speed hysteresis, temporal & spatial confirmation)
  if (rideStarted && !ridePaused && autoPauseEnabled) {
    float pauseSpeedThreshold = runningMode ? 0.7f : 1.2f;
    float resumeSpeedThreshold = runningMode ? 1.2f : 2.0f;
    const float highSpeedThreshold = runningMode ? 4.0f : 6.0f;
    float curSpd = speedAvailable() ? ride.speedKmh : 0.0f;

    if (!autoPaused) {
      if (curSpd < pauseSpeedThreshold && millis() > autoPauseOverrideGraceMs) {
        if (nearZeroSpeedStartMs == 0) {
          nearZeroSpeedStartMs = millis();
        } else if (millis() - nearZeroSpeedStartMs >= (runningMode ? 5000 : 4000)) { // 5s buffer in run/walk
          autoPaused = true;
          autoPauseStartMs = millis();
          nearZeroSpeedStartMs = 0;
          autoResumeMovementStartMs = 0;
          autoPauseAnchorLat = (ride.gpsFixed && gps.location.isValid()) ? (float)gps.location.lat() : 0.0f;
          autoPauseAnchorLon = (ride.gpsFixed && gps.location.isValid()) ? (float)gps.location.lng() : 0.0f;
          fitEvent(4); // Pause FIT event
          uiRevision++;
        }
      } else {
        nearZeroSpeedStartMs = 0;
      }
    } else {
      // Auto-resume check:
      // Require HDOP to be reasonable to prevent indoor multipath false triggers
      bool hdopAcceptable = (!gps.hdop.isValid() || gps.hdop.hdop() <= 5.5f);
      bool movingNow = (curSpd >= resumeSpeedThreshold) && hdopAcceptable;

      if (!movingNow) {
        autoResumeMovementStartMs = 0;
      } else {
        if (autoResumeMovementStartMs == 0) {
          autoResumeMovementStartMs = millis();
        } else {
          uint32_t moveDuration = millis() - autoResumeMovementStartMs;

          // Check physical displacement from the pause location (eliminates stationary chair drift)
          float dispMeters = 0.0f;
          if (autoPauseAnchorLat != 0.0f && autoPauseAnchorLon != 0.0f && ride.gpsFixed && gps.location.isValid()) {
            dispMeters = haversineKm(autoPauseAnchorLat, autoPauseAnchorLon, (float)gps.location.lat(), (float)gps.location.lng()) * 1000.0f;
          }
          bool displacementOk = (dispMeters >= 3.5f) || (autoPauseAnchorLat == 0.0f);

          // Fast resume if moving fast (>= highSpeedThreshold for 800ms), or standard resume (>= 1.2s sustained with > 3.5m displacement)
          bool highSpeedConfirm = (curSpd >= highSpeedThreshold && moveDuration >= 800);
          bool normalConfirm = (moveDuration >= 1200 && displacementOk);

          if (highSpeedConfirm || normalConfirm) {"""
    c = c.replace(old_autopause, new_autopause)

    # 13. Elevation gain hysteresis tracking in loop()
    old_alt_update = """      ride.altM = filteredGpsAlt;
    }
  }

  // Zero out slope jitter when stationary or moving under 2.0 km/h"""
    new_alt_update = """      ride.altM = filteredGpsAlt;
    }
  }

  // 2.5m climb hysteresis filter on ride.altM to eliminate noise accumulation
  if (rideStarted && !ridePaused && !autoPaused && isfinite(ride.altM) && ride.altM > -100.0f) {
    static float s_elevMin = 0.0f, s_elevMax = 0.0f;
    static bool s_elevInit = false;
    if (!s_elevInit) {
      s_elevMin = s_elevMax = ride.altM;
      s_elevInit = true;
    } else {
      if (ride.altM < s_elevMin) {
        s_elevMin = ride.altM;
        s_elevMax = ride.altM;
      } else if (ride.altM > s_elevMax) {
        if (ride.altM - s_elevMin >= 2.5f) {
          ride.elevationGainM += (ride.altM - s_elevMin);
          s_elevMin = ride.altM;
          s_elevMax = ride.altM;
        } else {
          s_elevMax = ride.altM;
        }
      } else if (s_elevMax - ride.altM >= 2.0f) {
        s_elevMin = ride.altM;
        s_elevMax = ride.altM;
      }
    }
  }

  // Zero out slope jitter when stationary or moving under 2.0 km/h"""
    c = c.replace(old_alt_update, new_alt_update)

    # 14. 60 FPS display unlock & Compass App redraw trigger
    old_fps = """  // Target 50 FPS display refresh (20ms period, zero-allocation scalar comparison)
  if (millis() - lastDispMs >= 20) {"""
    new_fps = """  // Target 60 FPS display refresh (16ms period, zero-allocation scalar comparison)
  if (millis() - lastDispMs >= 16) {"""
    c = c.replace(old_fps, new_fps)

    old_radar_tick = """      (uint32_t)((gpsWaiting || gpsRadarInspector || countdownStart || countdownGoStart || (welcomeOpen && homePage == HOME_COMPASS) || autoPaused) ? (millis() / 25) : 0),"""
    new_radar_tick = """      (uint32_t)((gpsWaiting || gpsRadarInspector || countdownStart || countdownGoStart || ((welcomeOpen || quickOpen) && homePage == HOME_COMPASS) || autoPaused) ? (millis() / 16) : 0),"""
    c = c.replace(old_radar_tick, new_radar_tick)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(c)
    print(f"Updated {filepath}")

def update_activity_archive_h(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        c = f.read()

    # Log filtered ride.altM instead of raw GPS altitude
    old_ele = 'if(gps.altitude.isValid()&&gps.altitude.age()<3000)gpxFile.printf("<ele>%.2f</ele>",gps.altitude.meters());'
    new_ele = 'if(isfinite(ride.altM))gpxFile.printf("<ele>%.2f</ele>",ride.altM);'
    c = c.replace(old_ele, new_ele)

    # Save elevationGainM in .summary.json
    old_sum = 'd["completedEpoch"]=(uint32_t)time(nullptr);d["distanceKm"]=ride.distanceKm;d["seconds"]=ride.elapsedSec;'
    new_sum = 'd["completedEpoch"]=(uint32_t)time(nullptr);d["distanceKm"]=ride.distanceKm;d["seconds"]=ride.elapsedSec;d["elevationGainM"]=ride.elevationGainM;'
    c = c.replace(old_sum, new_sum)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(c)
    print(f"Updated {filepath}")

def update_route_archive_h(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        c = f.read()

    # In readGPX(), add climb hysteresis variables next to lastE
    c = c.replace(
        "double lastA=0,lastB=0;float lastE=NAN;uint32_t lastTSec=0;",
        "double lastA=0,lastB=0;float lastE=NAN;uint32_t lastTSec=0;float gpxElevMin=0,gpxElevMax=0;"
    )

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(c)
    print(f"Updated {filepath}")

    # Replace raw elevation summing with 2.5m climb hysteresis
    old_ele_sum = """                if(isfinite(ele)){
                  previewElevation=true;
                  if(isfinite(lastE)&&ele>lastE) previewGain+=(ele-lastE);
                }"""
    new_ele_sum = """                if(isfinite(ele)){
                  previewElevation=true;
                  if(!isfinite(lastE)){
                    lastE=ele; gpxElevMin=ele; gpxElevMax=ele;
                  }else{
                    if(ele < gpxElevMin){
                      gpxElevMin=ele; gpxElevMax=ele;
                    }else if(ele > gpxElevMax){
                      if(ele - gpxElevMin >= 2.5f){
                        previewGain += (ele - gpxElevMin);
                        gpxElevMin = ele; gpxElevMax = ele;
                      }else{
                        gpxElevMax = ele;
                      }
                    }else if(gpxElevMax - ele >= 2.0f){
                      gpxElevMin = ele; gpxElevMax = ele;
                    }
                  }
                }"""
    c = c.replace(old_ele_sum, new_ele_sum)

    # In readGPX summary assignment: do not overwrite distanceKm or elevationGainM if summary already has them
    old_assign = """    if(!archiveSummary["seconds"].is<uint32_t>() || archiveSummary["seconds"].as<uint32_t>()==0){
      archiveSummary["seconds"]=totalDuration;
      archiveSummary["distanceKm"]=previewKm;
      archiveSummary["elevationGainM"]=previewGain;"""
    new_assign = """    if(!archiveSummary["seconds"].is<uint32_t>() || archiveSummary["seconds"].as<uint32_t>()==0){
      archiveSummary["seconds"]=totalDuration;
      if(!archiveSummary["distanceKm"].is<float>() || archiveSummary["distanceKm"].as<float>() <= 0.001f){
        archiveSummary["distanceKm"]=previewKm;
      }
      if(!archiveSummary["elevationGainM"].is<float>() || archiveSummary["elevationGainM"].as<float>() <= 0.001f){
        archiveSummary["elevationGainM"]=previewGain;
      }"""
    c = c.replace(old_assign, new_assign)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(c)
    print(f"Updated {filepath}")

def update_geprc_h(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        c = f.read()

    # Reduce compass sampling interval to 25ms
    old_sample = "void update(){if(!ready||millis()-sampleMs<40)return;"
    new_sample = "void update(){if(!ready||millis()-sampleMs<25)return;"
    c = c.replace(old_sample, new_sample)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(c)
    print(f"Updated {filepath}")

def update_quick_settings_h(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        c = f.read()

    # 1. Faster compass needle interpolation in drawCompassApp()
    old_comp_interp = "s_smoothCompassH += diff * 0.20f;"
    new_comp_interp = "s_smoothCompassH += diff * 0.35f;"
    c = c.replace(old_comp_interp, new_comp_interp)

    # 2. In drawLifecycleScreen(), compass app animation check
    old_life_comp = 'else if(welcomeOpen && !quickOpen && homePage == HOME_COMPASS) state="compass:"+String(millis()/25)+":"+String((int)(compass.heading*10))+":"+String(darkTheme);'
    new_life_comp = 'else if((welcomeOpen || quickOpen) && homePage == HOME_COMPASS) state="compass:"+String(millis()/16)+":"+String((int)(compass.heading*10))+":"+String(darkTheme);'
    c = c.replace(old_life_comp, new_life_comp)

    # 3. In activity card details: use archiveSummary distance & elevation if available
    old_dist_str = 'String distStr = (previewKm > 0.001f) ? (String(previewKm, 2) + "KM") : "--KM";'
    new_dist_str = """float showDist = (archiveSummary["distanceKm"].is<float>() && archiveSummary["distanceKm"].as<float>() > 0.001f)
                     ? archiveSummary["distanceKm"].as<float>()
                     : previewKm;
      String distStr = (showDist > 0.001f) ? (String(showDist, 2) + "KM") : "--KM";"""
    c = c.replace(old_dist_str, new_dist_str)

    old_elev_str = 'float grad = (previewKm > 0.01f && previewElevation) ? ((previewGain / (previewKm * 1000.0f)) * 100.0f) : 0.0f;'
    new_elev_str = """float showGain = (archiveSummary["elevationGainM"].is<float>() && archiveSummary["elevationGainM"].as<float>() > 0.0f)
                     ? archiveSummary["elevationGainM"].as<float>()
                     : previewGain;
        float grad = (previewKm > 0.01f && previewElevation) ? ((showGain / (previewKm * 1000.0f)) * 100.0f) : 0.0f;"""
    c = c.replace(old_elev_str, new_elev_str)

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(c)
    print(f"Updated {filepath}")

if __name__ == '__main__':
    for prefix in ['firmware/src', 'schematikIO-Export/src']:
        update_firmware_cpp(f"{prefix}/firmware.cpp")
        update_activity_archive_h(f"{prefix}/activity_archive.h")
        update_route_archive_h(f"{prefix}/route_archive.h")
        update_geprc_h(f"{prefix}/geprc.h")
        update_quick_settings_h(f"{prefix}/quick_settings.h")
    print("All files updated successfully.")
