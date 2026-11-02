#pragma once
#include <cstdint>

constexpr int MAX_ROUTE_POINTS = 512;
constexpr int MAX_TRACK_POINTS = 1024;

struct RoutePoint {
  float lat = 0.0f;
  float lon = 0.0f;
};

struct TrackPoint {
  float lat = 0.0f;
  float lon = 0.0f;
  float alt = 0.0f;
  uint32_t sec = 0;
};

struct NavState {
  float destLat        = 0.0f;
  float destLon        = 0.0f;
  bool  hasDestination = false;
  float remainDistKm   = 0.0f;
  int   bearingDeg     = 0;
  int   routeIndex     = 0;
};

struct SensorFlags {
  bool  cadencePhysical = false;
  bool  speedPhysical   = false;
  int   physCadenceRpm  = 0;
  float physSpeedKmh    = 0.0f;
};
