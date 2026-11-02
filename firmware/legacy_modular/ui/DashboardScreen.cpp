#include "ui/DashboardScreen.hpp"

DashboardScreen::DashboardScreen(DisplayService& display, GPSService& gpsService,
                                 const RideData& ride, const Config& cfg,
                                 const SensorFlags& sensors)
    : display_(display), gpsService_(gpsService), ride_(ride), cfg_(cfg), sensors_(sensors) {}

void DashboardScreen::init() {
    display_.clear();
}

void DashboardScreen::update() {}

void DashboardScreen::render() {
    display_.clear();
    auto& d = display_.getDisplay();
    d.setTextColor(COLOR_BLACK);

    d.setTextSize(1);
    d.setCursor(8, 8);
    d.print("CYCLOCOMP | V2");

    d.setCursor(8, 23);
    d.print(ride_.gpsFixed ? "GPS LIVE" : "WAITING FOR GPS");

    // Speed arc gauge
    display_.drawArcOutline(120, 105, 66, 225, 495);
    float speedRatio = constrain(ride_.speedKmh / cfg_.maxSpeedKmh, 0.0f, 1.0f);
    display_.drawArcFill(120, 105, 62, 225, static_cast<int>(270.0f * speedRatio), 5);

    // Speed number
    d.setTextSize(3);
    d.setCursor(64, 85);
    d.print(ride_.speedKmh, 1);

    d.setTextSize(1);
    d.setCursor(100, 125);
    d.print("km/h");

    // Distance and runtime
    d.setCursor(8, 174);
    d.printf("DIST %.2f km   %lu:%02lu",
             ride_.distanceKm,
             ride_.elapsedSec / 3600,
             (ride_.elapsedSec / 60) % 60);

    // Cadence
    d.setCursor(8, 194);
    d.printf("CAD %d %s",
             ride_.cadenceRpm,
             sensors_.cadencePhysical ? "external" : "est (fixed gear)");

    // Power
    d.setCursor(8, 213);
    d.printf("POWER %d W est", ride_.powerW);
    display_.drawProgressBar(8, 226, 224, 8, ride_.powerW / cfg_.maxPowerW);

    // VO2max
    d.setCursor(8, 244);
    if (cfg_.vo2maxManual > 0.0f) {
        d.printf("VO2max %.1f imported/manual", cfg_.vo2maxManual);
    } else {
        d.print("VO2max -- (not measured)");
    }

    // Altitude
    d.setCursor(8, 261);
    d.printf("ALT %.1fm %s", ride_.altM, gpsService_.isBmpOk() ? "baro" : "GPS");

    // GPS coordinates
    d.setCursor(8, 277);
    if (ride_.gpsFixed) {
        d.printf("%.5f, %.5f", ride_.lat, ride_.lon);
    } else {
        d.print("Coordinates --");
    }

    // Target distance progress bar
    float targetFrac = (cfg_.targetDistanceKm > 0.0f) ? (ride_.distanceKm / cfg_.targetDistanceKm) : 0.0f;
    display_.drawProgressBar(8, 294, 224, 8, targetFrac);

    // Navigation hint
    d.setCursor(8, 309);
    d.print("LEFT dash  RIGHT map  UP/DN zoom");
}
