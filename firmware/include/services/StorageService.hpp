#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include "models/NavState.hpp"
#include "config/pin_config.hpp"

class StorageService {
public:
    StorageService();
    ~StorageService();

    bool init();
    bool isOk() const { return sdOk_; }

    bool loadRoute(RoutePoint* route, int& routeLen, int maxRoute, NavState& nav);
    bool saveRoute(const DynamicJsonDocument& doc);
    void logBreadcrumb(const char* utc, float lat, float lon, float alt, float speedKmh, int cadenceRpm, int powerW);

    File openFile(const char* path, const char* mode = FILE_READ);
    bool exists(const char* path);

private:
    SPIClass sdSPI_;
    bool sdOk_ = false;
};
