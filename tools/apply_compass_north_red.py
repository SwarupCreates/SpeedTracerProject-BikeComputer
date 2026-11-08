import os

for path in ['firmware/src/quick_settings.h', 'schematikIO-Export/src/quick_settings.h']:
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    old_target = """  // Outer circular dial boundary (Frame 35)
  display.drawCircle(cx, cy, rOuter, uiFg());

  // Fixed solid red up-headed needle pointing to 12 o'clock
  // Vertex 1: Tip at (cx, 80)
  // Vertex 2: Base left at (cx - 10, 111)
  // Vertex 3: Base right at (cx + 10, 111)
  display.fillTriangle(cx, 80, cx - 10, 111, cx + 10, 111, 0xF800);

  // Reticle / Hub at center
  // 1. Center disc (radius 14): dark gray #292929 in dark mode, light gray #D6D6D6 in light mode
  uint16_t hubDiscColor = darkTheme ? 0x2945 : 0xD69A;
  display.fillCircle(cx, cy, 14, hubDiscColor);

  // 2. Inner ring outline (radius 23)
  display.drawCircle(cx, cy, 23, uiFg());

  // 3. Crosshair lines extending across the inner ring
  display.drawFastHLine(cx - 23, cy, 47, uiFg());
  display.drawFastVLine(cx, cy - 23, 47, uiFg());

  // Current heading with spring-damped physical animation
  static float s_smoothCompassH = 0.0f;
  static float s_compassVelocity = 0.0f;
  static bool s_compassInit = false;
  static uint32_t s_lastCompassFrameMs = 0;
  uint32_t nowCompassMs = millis();
  float dtCompass = (s_lastCompassFrameMs > 0) ? constrain((nowCompassMs - s_lastCompassFrameMs) * 0.001f, 0.001f, 0.05f) : 0.016f;
  s_lastCompassFrameMs = nowCompassMs;

  float rawH = (compass.ready && isfinite(compass.heading)) ? compass.heading : (ride.gpsFixed ? (float)ride.heading : 0.0f);
  if (!s_compassInit) {
    s_smoothCompassH = rawH;
    s_compassVelocity = 0.0f;
    s_compassInit = true;
  } else {
    float diff = rawH - s_smoothCompassH;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f) diff -= 360.0f;

    float absDiff = fabsf(diff);

    // Adaptive spring damping (eliminates fractional EMI micro-jitter while preserving 100% full turn sensitivity):
    // For fractional differences (< 1.5 deg), heavy viscous damping (zeta up to 1.75) dampens high-frequency twitching.
    // For intentional turns (> 2.0 deg), zeta relaxes to 1.05 (responsive, critically damped swing).
    float t = constrain((absDiff - 0.35f) / 1.65f, 0.0f, 1.0f);
    float zeta = 1.75f - 0.70f * t;
    const float omega = 7.5f;
    float kSpring = omega * omega;
    float kDamping = 2.0f * zeta * omega;

    float accel = kSpring * diff - kDamping * s_compassVelocity;
    s_compassVelocity += accel * dtCompass;
    s_smoothCompassH += s_compassVelocity * dtCompass;

    // Smooth settle cushion at the end of motion
    if (absDiff < 0.20f && fabsf(s_compassVelocity) < 0.40f) {
      s_compassVelocity *= 0.80f;
      if (fabsf(s_compassVelocity) < 0.05f) {
        s_compassVelocity = 0.0f;
        s_smoothCompassH = rawH;
      }
    }

    while (s_smoothCompassH < 0.0f) s_smoothCompassH += 360.0f;
    while (s_smoothCompassH >= 360.0f) s_smoothCompassH -= 360.0f;
  }
  float h = s_smoothCompassH;"""

    new_replacement = """  // Current heading with spring-damped physical animation
  static float s_smoothCompassH = 0.0f;
  static float s_compassVelocity = 0.0f;
  static bool s_compassInit = false;
  static uint32_t s_lastCompassFrameMs = 0;
  uint32_t nowCompassMs = millis();
  float dtCompass = (s_lastCompassFrameMs > 0) ? constrain((nowCompassMs - s_lastCompassFrameMs) * 0.001f, 0.001f, 0.05f) : 0.016f;
  s_lastCompassFrameMs = nowCompassMs;

  float rawH = (compass.ready && isfinite(compass.heading)) ? compass.heading : (ride.gpsFixed ? (float)ride.heading : 0.0f);
  if (!s_compassInit) {
    s_smoothCompassH = rawH;
    s_compassVelocity = 0.0f;
    s_compassInit = true;
  } else {
    float diff = rawH - s_smoothCompassH;
    while (diff < -180.0f) diff += 360.0f;
    while (diff > 180.0f) diff -= 360.0f;

    float absDiff = fabsf(diff);

    // Adaptive spring damping (eliminates fractional EMI micro-jitter while preserving 100% full turn sensitivity):
    // For fractional differences (< 1.5 deg), heavy viscous damping (zeta up to 1.75) dampens high-frequency twitching.
    // For intentional turns (> 2.0 deg), zeta relaxes to 1.05 (responsive, critically damped swing).
    float t = constrain((absDiff - 0.35f) / 1.65f, 0.0f, 1.0f);
    float zeta = 1.75f - 0.70f * t;
    const float omega = 7.5f;
    float kSpring = omega * omega;
    float kDamping = 2.0f * zeta * omega;

    float accel = kSpring * diff - kDamping * s_compassVelocity;
    s_compassVelocity += accel * dtCompass;
    s_smoothCompassH += s_compassVelocity * dtCompass;

    // Smooth settle cushion at the end of motion
    if (absDiff < 0.20f && fabsf(s_compassVelocity) < 0.40f) {
      s_compassVelocity *= 0.80f;
      if (fabsf(s_compassVelocity) < 0.05f) {
        s_compassVelocity = 0.0f;
        s_smoothCompassH = rawH;
      }
    }

    while (s_smoothCompassH < 0.0f) s_smoothCompassH += 360.0f;
    while (s_smoothCompassH >= 360.0f) s_smoothCompassH -= 360.0f;
  }
  float h = s_smoothCompassH;
  int deg = (int)lroundf(fmodf(h + 360.0f, 360.0f)) % 360;
  bool isNorth = (deg >= 338 || deg <= 22);

  // Outer circular dial boundary (Frame 35)
  display.drawCircle(cx, cy, rOuter, uiFg());

  // Fixed solid red up-headed needle pointing to 12 o'clock
  // Vertex 1: Tip at (cx, 80)
  // Vertex 2: Base left at (cx - 10, 111)
  // Vertex 3: Base right at (cx + 10, 111)
  display.fillTriangle(cx, 80, cx - 10, 111, cx + 10, 111, 0xF800);

  // Reticle / Hub at center
  // 1. Center disc (radius 14): red accented #2D0000 when headed North, or dark gray/light gray otherwise
  uint16_t hubDiscColor = isNorth ? (darkTheme ? 0x2800 : 0xFDB8) : (darkTheme ? 0x2945 : 0xD69A);
  display.fillCircle(cx, cy, 14, hubDiscColor);

  // 2. Inner ring outline (radius 23): red accented when headed North, uiFg() otherwise
  uint16_t reticleColor = isNorth ? 0xF800 : uiFg();
  display.drawCircle(cx, cy, 23, reticleColor);

  // 3. Crosshair lines extending across the inner ring: red accented when headed North, uiFg() otherwise
  display.drawFastHLine(cx - 23, cy, 47, reticleColor);
  display.drawFastVLine(cx, cy - 23, 47, reticleColor);"""

    content_lf = content.replace('\r\n', '\n')
    old_target_lf = old_target.replace('\r\n', '\n')
    new_replacement_lf = new_replacement.replace('\r\n', '\n')

    if old_target_lf in content_lf:
        content_lf = content_lf.replace(old_target_lf, new_replacement_lf)
        dup_deg = 'int deg = (int)lroundf(fmodf(h + 360.0f, 360.0f)) % 360;'
        idx1 = content_lf.find(dup_deg)
        if idx1 != -1:
            idx2 = content_lf.find(dup_deg, idx1 + len(dup_deg))
            if idx2 != -1:
                content_lf = content_lf[:idx2] + '// deg already computed above' + content_lf[idx2+len(dup_deg):]
        with open(path, 'w', encoding='utf-8', newline='\r\n') as f_out:
            f_out.write(content_lf)
        print(f'Successfully updated {path}')
    else:
        print(f'Target not found in {path}')
