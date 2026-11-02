#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BMP3XX.h>
#include "Drivers/geprc.h"
#include "models/RideData.hpp"
#include "models/Config.hpp"
#include "models/NavState.hpp"
#include "services/StorageService.hpp"
#include "config/pin_config.hpp"

class GPSService {
public:
    GPSService(RideData& ride, Config& cfg, SensorFlags& sensors,
               NavState& nav, StorageService& storage,
               RoutePoint* route, int& routeLen,
               TrackPoint* track, int& trackLen);
    ~GPSService();

    void init(TwoWire* wire = &Wire);
    void update();

    static float haversineKm(float la1, float lo1, float la2, float lo2);
    static int bearingTo(float la1, float lo1, float la2, float lo2);
    int calcCadence(float speedKmh) const;
    int calcVirtualPower(float speedKmh, float gradePercent) const;

    GeprcCompass& getCompass() { return compass_; }
    GeprcGps& getGeprcGps() { return geprcGps_; }
    TinyGPSPlus& getTinyGps() { return gps_; }
    bool isBmpOk() const { return bmpOk_; }

private:
    RideData& ride_;
    Config& cfg_;
    SensorFlags& sensors_;
    NavState& nav_;
    StorageService& storage_;

    RoutePoint* route_;
    int& routeLen_;
    TrackPoint* track_;
    int& trackLen_;

    HardwareSerial gpsSerial_;
    TinyGPSPlus gps_;
    GeprcGps geprcGps_;
    GeprcCompass compass_;
    Adafruit_BMP3XX bmp_;

    bool bmpOk_ = false;
    uint32_t sensorMs_ = 0;
    uint32_t lastTrackMs_ = 0;
    uint32_t rideStartMs_ = 0;
    bool rideStarted_ = false;
    float lastLat_ = 0.0f;
    float lastLon_ = 0.0f;
    float lastAltM_ = 0.0f;
    float distAccum_ = 0.0f;
    uint32_t externalSensorMs_ = 0;
};
