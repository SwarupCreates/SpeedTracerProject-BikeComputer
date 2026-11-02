#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "models/RideData.hpp"
#include "models/Config.hpp"
#include "models/NavState.hpp"
#include "services/StorageService.hpp"
#include "services/GPSService.hpp"

class WebService {
public:
    WebService(RideData& ride, Config& cfg, SensorFlags& sensors,
               NavState& nav, StorageService& storage, GPSService& gpsService,
               RoutePoint* route, int& routeLen,
               TrackPoint* track, int& trackLen);
    ~WebService();

    void init();
    void update();

    WebServer& getServer() { return server_; }

private:
    void handleRoot();
    void handleStatus();
    void handleNavSet();
    void handleUploadRoute();
    void handleConfig();
    void handleSensor();
    void handleExportGPX();
    void handleLogDownload();
    void handleCompass();

    RideData& ride_;
    Config& cfg_;
    SensorFlags& sensors_;
    NavState& nav_;
    StorageService& storage_;
    GPSService& gpsService_;

    RoutePoint* route_;
    int& routeLen_;
    TrackPoint* track_;
    int& trackLen_;

    WebServer server_;
    Preferences prefs_;
};
