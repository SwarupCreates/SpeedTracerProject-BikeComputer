import sys

def update_firmware_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    content_lf = content.replace('\r\n', '\n')

    old_vars = (
        'static float    lastLat      = 0, lastLon = 0, lastAltM = 0;\n'
        'static float    distAccum    = 0;'
    )
    new_vars = (
        'static float    lastLat      = 0, lastLon = 0, lastAltM = 0;\n'
        'static float    distAccum    = 0;\n'
        'float           distAnchorLat = 0.0f, distAnchorLon = 0.0f;'
    )

    old_dist_block = (
        '  if (gps.location.isValid() && gps.location.isUpdated()) {\n'
        '    float nLat = gps.location.lat(), nLon = gps.location.lng();\n'
        '\n'
        '    if (lastLat != 0.0f) {\n'
        '      float delta = haversineKm(lastLat, lastLon, nLat, nLon);\n'
        '      if (rideStarted && !ridePaused && delta > 0.001f && delta<0.1f && gps.speed.kmph()>2 && ((geprcGps.lastPvt && millis()-geprcGps.lastPvt<3000)?geprcGps.pdop<3:(gps.hdop.isValid() && gps.hdop.hdop()<3))) {   // ignore sub-metre jitter\n'
        '        distAccum += delta;\n'
        '        ride.distanceKm = distAccum;\n'
        '        // Grade: alt change / horizontal distance\n'
        '        float altDelta = ride.altM - lastAltM;\n'
        '        float rawGrade = (altDelta / (delta * 1000.0f)) * 100.0f;\n'
        '        ride.gradePercent = constrain((0.65f * ride.gradePercent) + (0.35f * rawGrade), -30.0f, 30.0f);\n'
        '        lastAltM = ride.altM;\n'
        '        // Advance route waypoint\n'
        '        if (routeLen > 0 && nav.routeIndex < routeLen - 1) {\n'
        '          float wd = haversineKm(nLat, nLon, route[nav.routeIndex+1].lat, route[nav.routeIndex+1].lon);\n'
        '          if (wd < 0.025f) nav.routeIndex++;\n'
        '          // Remaining distance follows the selected polyline, not a straight line.\n'
        '          int next=min(nav.routeIndex+1,routeLen-1);\n'
        '          nav.remainDistKm=haversineKm(nLat,nLon,route[next].lat,route[next].lon);\n'
        '          for(int k=next;k<routeLen-1;k++)nav.remainDistKm+=haversineKm(route[k].lat,route[k].lon,route[k+1].lat,route[k+1].lon);\n'
        '          nav.bearingDeg=bearingTo(nLat,nLon,route[next].lat,route[next].lon);\n'
        '        }\n'
        '      }\n'
        '    }\n'
        '    lastLat = ride.lat = nLat;\n'
        '    lastLon = ride.lon = nLon;\n'
        '    ride.gpsFixed = gps.location.age()<3000;'
    )

    new_dist_block = (
        '  if (gps.location.isValid() && gps.location.isUpdated()) {\n'
        '    float nLat = (float)gps.location.lat(), nLon = (float)gps.location.lng();\n'
        '\n'
        '    // Check GPS fix quality: accept fixes with PDOP/HDOP < 6.0f, or valid 3D fixes when DOP is unavailable\n'
        '    bool goodFixQuality = true;\n'
        '    if (geprcGps.lastPvt && (millis() - geprcGps.lastPvt < 3000)) {\n'
        '      goodFixQuality = (geprcGps.pdop < 6.0f);\n'
        '    } else if (gps.hdop.isValid() && (gps.hdop.age() < 5000)) {\n'
        '      goodFixQuality = (gps.hdop.hdop() < 6.0f);\n'
        '    }\n'
        '\n'
        '    if (rideStarted && !ridePaused) {\n'
        '      static uint32_t lastDistCommitMs = 0;\n'
        '\n'
        '      // Anchor holds its position until distance exceeds displacement threshold,\n'
        '      // preventing fractional sub-metre walking movement from being wiped on each sample.\n'
        '      if (distAnchorLat == 0.0f || distAnchorLon == 0.0f) {\n'
        '        distAnchorLat = nLat;\n'
        '        distAnchorLon = nLon;\n'
        '        lastDistCommitMs = millis();\n'
        '        lastAltM = ride.altM;\n'
        '      } else {\n'
        '        float delta = haversineKm(distAnchorLat, distAnchorLon, nLat, nLon);\n'
        '        uint32_t dtMs = millis() - lastDistCommitMs;\n'
        '        float dtSec = dtMs > 0 ? (dtMs / 1000.0f) : 1.0f;\n'
        '        float impliedSpeedKmh = (dtSec > 0.1f) ? ((delta / dtSec) * 3600.0f) : 0.0f;\n'
        '\n'
        '        // Current speed check (GPS or BLE sensor)\n'
        '        float curSpd = ride.speedKmh > 0.0f ? ride.speedKmh : (gps.speed.isValid() && gps.speed.age() < 3000 ? (float)gps.speed.kmph() : 0.0f);\n'
        '        if (!runningMode && bleSpeedReady) curSpd = max(curSpd, bleSpeed);\n'
        '\n'
        '        // Movement conditions:\n'
        '        // Accumulate once displaced at least 2.5 meters (0.0025 km) from anchor,\n'
        '        // which filters stationary GPS noise while seamlessly capturing walking steps (~0.8 to 1.5 m/s).\n'
        '        // Implied speed cap (< 130 km/h) and step distance cap (< 150m) reject GPS teleport jumps.\n'
        '        bool hasMoved = (delta >= 0.0025f && delta < 0.150f && impliedSpeedKmh < 130.0f);\n'
        '        bool isMotion = (curSpd >= 0.8f) || (impliedSpeedKmh >= 0.8f);\n'
        '\n'
        '        if (goodFixQuality && hasMoved && isMotion) {\n'
        '          distAccum += delta;\n'
        '          ride.distanceKm = distAccum;\n'
        '          distAnchorLat = nLat;\n'
        '          distAnchorLon = nLon;\n'
        '          lastDistCommitMs = millis();\n'
        '\n'
        '          // Grade: alt change / horizontal distance (only over >= 5m to avoid division noise)\n'
        '          if (delta >= 0.005f) {\n'
        '            float altDelta = ride.altM - lastAltM;\n'
        '            float rawGrade = (altDelta / (delta * 1000.0f)) * 100.0f;\n'
        '            ride.gradePercent = constrain((0.65f * ride.gradePercent) + (0.35f * rawGrade), -30.0f, 30.0f);\n'
        '            lastAltM = ride.altM;\n'
        '          }\n'
        '\n'
        '          // Advance route waypoint\n'
        '          if (routeLen > 0 && nav.routeIndex < routeLen - 1) {\n'
        '            float wd = haversineKm(nLat, nLon, route[nav.routeIndex+1].lat, route[nav.routeIndex+1].lon);\n'
        '            if (wd < 0.025f) nav.routeIndex++;\n'
        '            int next = min(nav.routeIndex + 1, routeLen - 1);\n'
        '            nav.remainDistKm = haversineKm(nLat, nLon, route[next].lat, route[next].lon);\n'
        '            for (int k = next; k < routeLen - 1; k++) nav.remainDistKm += haversineKm(route[k].lat, route[k].lon, route[k+1].lat, route[k+1].lon);\n'
        '            nav.bearingDeg = bearingTo(nLat, nLon, route[next].lat, route[next].lon);\n'
        '          }\n'
        '        } else if (impliedSpeedKmh > 130.0f || delta >= 0.150f) {\n'
        '          // Reject multipath teleport jump: reset anchor to current point without accumulating\n'
        '          distAnchorLat = nLat;\n'
        '          distAnchorLon = nLon;\n'
        '          lastDistCommitMs = millis();\n'
        '        }\n'
        '      }\n'
        '    }\n'
        '    lastLat = ride.lat = nLat;\n'
        '    lastLon = ride.lon = nLon;\n'
        '    ride.gpsFixed = gps.location.age() < 3000;'
    )

    old_deadzone = (
        '  // GPS noise deadzones:\n'
        '  // In outdoor ride: variance 0.1-0.6 km/h, stationary jitter up to ~1.2 km/h\n'
        '  // In outdoor run: walking / stationary drift up to 1.8 km/h\n'
        '  float speedDeadzone = runningMode ? 1.8f : 1.2f;\n'
        '  if (rawGpsSpeed < speedDeadzone) {\n'
        '    rawGpsSpeed = 0.0f;\n'
        '  }'
    )

    new_deadzone = (
        '  // GPS noise deadzones:\n'
        '  // In outdoor ride: stationary jitter up to ~1.0 km/h\n'
        '  // In outdoor run/walk: stationary drift up to 0.8 km/h\n'
        '  float speedDeadzone = runningMode ? 0.8f : 1.0f;\n'
        '  if (rawGpsSpeed < speedDeadzone) {\n'
        '    rawGpsSpeed = 0.0f;\n'
        '  }'
    )

    old_gps_lost = 'if(!ride.gpsFixed) {ride.lat=0;ride.lon=0;ride.gradePercent=0;nav.remainDistKm=0;lastLat=0;lastLon=0;}'
    new_gps_lost = 'if(!ride.gpsFixed) {ride.lat=0;ride.lon=0;ride.gradePercent=0;nav.remainDistKm=0;lastLat=0;lastLon=0;distAnchorLat=0;distAnchorLon=0;}'

    assert old_vars in content_lf, f'old_vars not found in {path}'
    content_lf = content_lf.replace(old_vars, new_vars)

    assert old_dist_block in content_lf, f'old_dist_block not found in {path}'
    content_lf = content_lf.replace(old_dist_block, new_dist_block)

    assert old_deadzone in content_lf, f'old_deadzone not found in {path}'
    content_lf = content_lf.replace(old_deadzone, new_deadzone)

    assert old_gps_lost in content_lf, f'old_gps_lost not found in {path}'
    content_lf = content_lf.replace(old_gps_lost, new_gps_lost)

    content_crlf = content_lf.replace('\n', '\r\n')
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(content_crlf)
    print(f'Successfully updated {path}')

def update_quick_settings_h(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    content_lf = content.replace('\r\n', '\n')

    old_decl = 'extern bool appsListView;'
    new_decl = 'extern bool appsListView;\nextern float distAnchorLat, distAnchorLon;'

    old_start_reset = (
        '    distAccum=0; trackLen=0; ride.distanceKm=0;ride.elapsedSec=0;\n'
        '    ride.avgSpeedKmh=0;ride.maxSpeedKmh=0;ride.maxPowerW=0;\n'
        '    resetMaxSpeedTracker();\n'
        '    hrSum=powerSum=0;hrSamples=powerSamples=0;hrMax=0;lastLat=lastLon=0;'
    )
    new_start_reset = (
        '    distAccum=0; trackLen=0; ride.distanceKm=0;ride.elapsedSec=0;\n'
        '    ride.avgSpeedKmh=0;ride.maxSpeedKmh=0;ride.maxPowerW=0;\n'
        '    resetMaxSpeedTracker();\n'
        '    hrSum=powerSum=0;hrSamples=powerSamples=0;hrMax=0;lastLat=lastLon=0;distAnchorLat=distAnchorLon=0;'
    )

    old_resume_reset = (
        '    fitEvent(0);\n'
        '    rideStartMs += (millis() - pauseStart);\n'
        '    ridePaused = false;'
    )
    new_resume_reset = (
        '    fitEvent(0);\n'
        '    rideStartMs += (millis() - pauseStart);\n'
        '    ridePaused = false;\n'
        '    distAnchorLat = distAnchorLon = 0;'
    )

    old_deadzone_label = 'label(44, y + 20, runningMode ? "Run: < 1.8 km/h clamped to 0" : "Ride: < 1.2 km/h clamped to 0", 1, uiMuted());'
    new_deadzone_label = 'label(44, y + 20, runningMode ? "Run: < 0.8 km/h clamped to 0" : "Ride: < 1.0 km/h clamped to 0", 1, uiMuted());'

    assert old_decl in content_lf, f'old_decl not found in {path}'
    content_lf = content_lf.replace(old_decl, new_decl)

    assert old_start_reset in content_lf, f'old_start_reset not found in {path}'
    content_lf = content_lf.replace(old_start_reset, new_start_reset)

    assert old_resume_reset in content_lf, f'old_resume_reset not found in {path}'
    content_lf = content_lf.replace(old_resume_reset, new_resume_reset)

    assert old_deadzone_label in content_lf, f'old_deadzone_label not found in {path}'
    content_lf = content_lf.replace(old_deadzone_label, new_deadzone_label)

    content_crlf = content_lf.replace('\n', '\r\n')
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(content_crlf)
    print(f'Successfully updated {path}')

if __name__ == '__main__':
    update_firmware_cpp('firmware/src/firmware.cpp')
    update_firmware_cpp('schematikIO-Export/src/firmware.cpp')
    update_quick_settings_h('firmware/src/quick_settings.h')
    update_quick_settings_h('schematikIO-Export/src/quick_settings.h')
