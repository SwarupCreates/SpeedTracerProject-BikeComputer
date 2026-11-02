#pragma once
#include <cstdint>

struct RideData {
  float    speedKmh     = 0.0f;
  float    distanceKm   = 0.0f;
  float    altM         = 0.0f;
  float    gradePercent = 0.0f;
  float    lat          = 0.0f;
  float    lon          = 0.0f;
  int      cadenceRpm   = 0;
  int      powerW       = 0;
  float    vo2est       = 0.0f;
  int      heading      = 0;
  uint32_t elapsedSec   = 0;
  float    avgSpeedKmh  = 0.0f;
  float    maxSpeedKmh  = 0.0f;
  int      maxPowerW    = 0;
  bool     gpsFixed     = false;
};
