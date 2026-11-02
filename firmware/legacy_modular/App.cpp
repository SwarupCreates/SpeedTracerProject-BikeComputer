#include "App.hpp"
#include "Logger.hpp"
#include "models/RideData.hpp"
#include "models/Config.hpp"
#include "models/NavState.hpp"
#include "services/DisplayService.hpp"
#include "services/StorageService.hpp"
#include "services/GPSService.hpp"
#include "services/InputService.hpp"
#include "services/WebService.hpp"
#include "ui/SplashScreen.hpp"
#include "ui/DashboardScreen.hpp"
#include "ui/MapScreen.hpp"
#include "config/pin_config.hpp"
#include <Preferences.h>
#include <Wire.h>

enum AppScreen {
    SCR_SPLASH,
    SCR_DASH,
    SCR_MAP
};

struct App::Impl {
    RideData ride;
    Config cfg;
    SensorFlags sensors;
    NavState nav;

    RoutePoint route[MAX_ROUTE_POINTS];
    int routeLen = 0;

    TrackPoint track[MAX_TRACK_POINTS];
    int trackLen = 0;

    DisplayService display;
    StorageService storage;
    InputService input;
    GPSService gps;
    WebService web;

    SplashScreen splash;
    DashboardScreen dashboard;
    MapScreen map;

    AppScreen currentScreen = SCR_SPLASH;
    uint8_t mapZoom = 2;
    uint32_t lastDispMs = 0;
    String previousState;
    uint32_t splashStartMs = 0;

    // Smoothed visual metrics for responsive 30-60 fps animation
    float animatedSpeedKmh = 0.0f;

    Impl()
        : display(),
          storage(),
          input(),
          gps(ride, cfg, sensors, nav, storage, route, routeLen, track, trackLen),
          web(ride, cfg, sensors, nav, storage, gps, route, routeLen, track, trackLen),
          splash(display),
          dashboard(display, gps, ride, cfg, sensors),
          map(display, ride, nav, route, routeLen, track, trackLen, mapZoom) {}
};

App::App() : impl_(new Impl()) {}
App::~App() = default;

void App::init() {
    Logger::info("App", "Starting SpeedTracer firmware (Phase 1 Ready)");

    // Initialize display & show splash screen first
    impl_->display.init();
    impl_->splash.render();
    impl_->splashStartMs = millis();

    // Initialize shared I2C bus (STEMMA QT / Qwiic)
    Wire.begin(config::I2C_SDA_PIN, config::I2C_SCL_PIN);
    Wire.setTimeOut(30);

    // Initialize inputs (MCP23017 and onboard BOOT key)
    impl_->input.init(&Wire);

    // Initialize storage & load persisted routes if SD is present
    impl_->storage.init();
    impl_->storage.loadRoute(impl_->route, impl_->routeLen, MAX_ROUTE_POINTS, impl_->nav);

    // Load configuration from NVS Preferences
    Preferences prefs;
    prefs.begin("cyclo", true);
    impl_->cfg.gearRatio        = prefs.getFloat("gearRatio",  impl_->cfg.gearRatio);
    impl_->cfg.wheelCircumM     = prefs.getFloat("wheelCircum", impl_->cfg.wheelCircumM);
    impl_->cfg.riderMassKg      = prefs.getFloat("riderMass",  impl_->cfg.riderMassKg);
    impl_->cfg.maxSpeedKmh      = prefs.getFloat("maxSpeed",   impl_->cfg.maxSpeedKmh);
    impl_->cfg.maxPowerW        = prefs.getFloat("maxPower",   impl_->cfg.maxPowerW);
    impl_->cfg.maxCadRpm        = prefs.getFloat("maxCad",     impl_->cfg.maxCadRpm);
    impl_->cfg.vo2maxManual     = prefs.getFloat("vo2max",     impl_->cfg.vo2maxManual);
    impl_->cfg.targetDistanceKm = prefs.getFloat("targetDist", impl_->cfg.targetDistanceKm);
    prefs.end();

    // Initialize GPS, compass, and barometer
    impl_->gps.init(&Wire);

    // Initialize WiFi AP & WebServer
    impl_->web.init();
}

void App::update() {
    // Process input buttons & power management
    impl_->input.update();

    // Feed and update GPS, compass, altitude, calculations
    impl_->gps.update();

    // Process incoming HTTP requests
    impl_->web.update();

    // Handle screen switching via MCP23017 remote buttons (Phase 3)
    if (impl_->input.consumePressed(RemoteBtn::LEFT)) {
        impl_->currentScreen = SCR_DASH;
    }
    if (impl_->input.consumePressed(RemoteBtn::RIGHT)) {
        impl_->currentScreen = SCR_MAP;
    }
    if (impl_->input.consumePressed(RemoteBtn::UP)) {
        if (impl_->currentScreen == SCR_MAP) {
            impl_->map.zoomIn();
        }
    }
    if (impl_->input.consumePressed(RemoteBtn::DOWN)) {
        if (impl_->currentScreen == SCR_MAP) {
            impl_->map.zoomOut();
        }
    }

    // Phase 1 navigation: Onboard BOOT button (GPIO 0)
    // Short press toggles DASH <-> MAP
    if (impl_->input.consumeBootPressed()) {
        if (impl_->currentScreen == SCR_DASH) {
            impl_->currentScreen = SCR_MAP;
        } else {
            impl_->currentScreen = SCR_DASH;
        }
        impl_->previousState = ""; // Force redraw on switch
    }
    // Long press cycles map zoom (1 -> 2 -> 3 -> 4 -> 5 -> 1)
    if (impl_->input.consumeBootHeld()) {
        if (impl_->currentScreen == SCR_MAP) {
            impl_->mapZoom = (impl_->mapZoom % 5) + 1;
            impl_->previousState = "";
        }
    }

    // Splash screen duration: switch to dashboard after 2.5 seconds
    if (impl_->currentScreen == SCR_SPLASH && (millis() - impl_->splashStartMs > 2500)) {
        impl_->currentScreen = SCR_DASH;
    }

    // Smooth speed animation filter for gauge sweeps
    impl_->animatedSpeedKmh += (impl_->ride.speedKmh - impl_->animatedSpeedKmh) * 0.35f;
    if (fabs(impl_->ride.speedKmh - impl_->animatedSpeedKmh) < 0.05f) {
        impl_->animatedSpeedKmh = impl_->ride.speedKmh;
    }

    // Redraw screen at responsive 10 Hz (100ms) with state-change check
    uint32_t now = millis();
    if (now - impl_->lastDispMs > 100) {
        impl_->lastDispMs = now;

        if (impl_->currentScreen == SCR_SPLASH) {
            return;
        }

        String state = String(impl_->currentScreen) + ":" +
                       String(impl_->mapZoom) + ":" +
                       String(impl_->ride.speedKmh, 1) + ":" +
                       String(impl_->ride.distanceKm, 2) + ":" +
                       String(impl_->ride.elapsedSec / 60) + ":" +
                       String(impl_->ride.lat, 5) + ":" +
                       String(impl_->ride.lon, 5) + ":" +
                       String(impl_->ride.altM, 1) + ":" +
                       String(impl_->ride.cadenceRpm) + ":" +
                       String(impl_->ride.powerW) + ":" +
                       String(impl_->ride.gpsFixed) + ":" +
                       String(impl_->routeLen) + ":" +
                       String(impl_->trackLen) + ":" +
                       String(impl_->nav.destLat, 5) + ":" +
                       String(impl_->nav.destLon, 5) + ":" +
                       String(impl_->cfg.vo2maxManual) + ":" +
                       String(impl_->cfg.targetDistanceKm);

        if (state == impl_->previousState) {
            return;
        }
        impl_->previousState = state;

        switch (impl_->currentScreen) {
            case SCR_DASH:
                impl_->dashboard.render();
                break;
            case SCR_MAP:
                impl_->map.render();
                break;
            default:
                break;
        }
    }
}
