import os

def update_file(path):
    if not os.path.exists(path):
        print("Not found:", path)
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Compass needle smoothing
    old_compass = """  // Current heading with smooth low-pass interpolation
  static float s_smoothCompassH = 0.0f;
  static bool s_compassInit = false;
  float rawH = (compass.ready && isfinite(compass.heading)) ? compass.heading : (ride.gpsFixed ? (float)ride.heading : 0.0f);
  if (!s_compassInit) {
    s_smoothCompassH = rawH;
    s_compassInit = true;
  } else {
    float diff = rawH - s_smoothCompassH;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f) diff -= 360.0f;
    s_smoothCompassH += diff * 0.35f;
    while (s_smoothCompassH < 0.0f) s_smoothCompassH += 360.0f;
    while (s_smoothCompassH >= 360.0f) s_smoothCompassH -= 360.0f;
  }"""

    new_compass = """  // Current heading with smooth low-pass interpolation
  static float s_smoothCompassH = 0.0f;
  static bool s_compassInit = false;
  static uint32_t s_lastCompassFrameMs = 0;
  uint32_t nowCompassMs = millis();
  float dtCompass = (s_lastCompassFrameMs > 0) ? constrain((nowCompassMs - s_lastCompassFrameMs) * 0.001f, 0.001f, 0.05f) : 0.016f;
  s_lastCompassFrameMs = nowCompassMs;

  float rawH = (compass.ready && isfinite(compass.heading)) ? compass.heading : (ride.gpsFixed ? (float)ride.heading : 0.0f);
  if (!s_compassInit) {
    s_smoothCompassH = rawH;
    s_compassInit = true;
  } else {
    float diff = rawH - s_smoothCompassH;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f) diff -= 360.0f;
    if (fabsf(diff) > 0.35f) {
      float alpha = 1.0f - expf(-dtCompass * 12.0f);
      s_smoothCompassH += diff * alpha;
    }
    while (s_smoothCompassH < 0.0f) s_smoothCompassH += 360.0f;
    while (s_smoothCompassH >= 360.0f) s_smoothCompassH -= 360.0f;
  }"""

    # 2. Radar progress bar smooth 60fps animation
    old_bar = """    display.fillRoundRect(barX, barY, barW, barH, barR, trackColor);
    int fillW = constrain(sats * barW / maxSats, 0, barW);
    if (sats > 0 && fillW < barH) fillW = barH;
    if (fillW > 0) {
      display.fillRoundRect(barX, barY, fillW, barH, barR, fillColor);
    }"""

    new_bar = """    display.fillRoundRect(barX, barY, barW, barH, barR, trackColor);
    static float s_animBarW = 0.0f;
    static uint32_t s_lastRadarMs = 0;
    uint32_t nowRadarMs = millis();
    float dtBar = (s_lastRadarMs > 0) ? constrain((nowRadarMs - s_lastRadarMs) * 0.001f, 0.001f, 0.05f) : 0.016f;
    s_lastRadarMs = nowRadarMs;

    float targetW = (sats <= 0) ? 0.0f : constrain((float)sats * (float)barW / (float)maxSats, (float)barH, (float)barW);
    float alphaBar = 1.0f - expf(-dtBar * 10.0f);
    s_animBarW += (targetW - s_animBarW) * alphaBar;
    if (fabsf(targetW - s_animBarW) < 0.2f) s_animBarW = targetW;

    int fillW = (int)lroundf(s_animBarW);
    if (sats > 0 && fillW < barH) fillW = barH;
    if (fillW > 0) {
      display.fillRoundRect(barX, barY, fillW, barH, barR, fillColor);
    }"""

    # 3. 60 FPS radar state
    old_state = 'else if(gpsWaiting || gpsRadarInspector) state="gpsRadar:"+String(millis()/40)+'
    new_state = 'else if(gpsWaiting || gpsRadarInspector) state="gpsRadar:"+String(millis()/16)+'

    # Normalize CRLF to LF for matching
    content_lf = content.replace("\r\n", "\n")
    old_compass_lf = old_compass.replace("\r\n", "\n")
    new_compass_lf = new_compass.replace("\r\n", "\n")
    old_bar_lf = old_bar.replace("\r\n", "\n")
    new_bar_lf = new_bar.replace("\r\n", "\n")

    if old_compass_lf not in content_lf:
        print("ERROR: old_compass not found in", path)
    content_lf = content_lf.replace(old_compass_lf, new_compass_lf)

    if old_bar_lf not in content_lf:
        print("ERROR: old_bar not found in", path)
    content_lf = content_lf.replace(old_bar_lf, new_bar_lf)

    if old_state not in content_lf:
        print("ERROR: old_state not found in", path)
    content_lf = content_lf.replace(old_state, new_state)

    # Convert back to CRLF
    content_crlf = content_lf.replace("\n", "\r\n")
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content_crlf)
    print("SUCCESS: updated", path)

update_file("firmware/src/quick_settings.h")
update_file("schematikIO-Export/src/quick_settings.h")
