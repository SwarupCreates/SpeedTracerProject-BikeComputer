#pragma once

struct Config {
  float gearRatio        = 50.0f / 17.0f;
  float wheelCircumM     = 2.096f;
  float riderMassKg      = 75.0f;
  float bikeMassKg       = 9.0f;
  float maxSpeedKmh      = 60.0f;
  float maxPowerW        = 400.0f;
  float maxCadRpm        = 120.0f;
  float vo2maxManual     = 0.0f;
  float targetDistanceKm = 0.0f;   // 0 = no target set
};
