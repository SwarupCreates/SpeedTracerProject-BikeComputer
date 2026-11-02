#include "ui/MapScreen.hpp"
#include <cmath>

static const float zoomRadiusKm[] = {0, 4.0f, 1.5f, 0.5f, 0.2f, 0.08f};

MapScreen::MapScreen(DisplayService& display, const RideData& ride,
                     const NavState& nav, const RoutePoint* route, const int& routeLen,
                     const TrackPoint* track, const int& trackLen, uint8_t& mapZoom)
    : display_(display), ride_(ride), nav_(nav),
      route_(route), routeLen_(routeLen),
      track_(track), trackLen_(trackLen), mapZoom_(mapZoom) {}

void MapScreen::init() {
    display_.clear();
}

void MapScreen::update() {}

void MapScreen::zoomIn() {
    mapZoom_ = min(static_cast<int>(mapZoom_) + 1, 5);
}

void MapScreen::zoomOut() {
    mapZoom_ = max(static_cast<int>(mapZoom_) - 1, 1);
}

void MapScreen::render() {
    display_.clear();
    auto& d = display_.getDisplay();
    d.setTextColor(COLOR_BLACK);
    d.setTextSize(1);

    d.setCursor(8, 8);
    d.print("ROUTE / BREADCRUMBS   N ^");

    double lat = ride_.gpsFixed ? ride_.lat : (routeLen_ ? route_[0].lat : 0.0);
    double lon = ride_.gpsFixed ? ride_.lon : (routeLen_ ? route_[0].lon : 0.0);
    float scale = 100.0f / zoomRadiusKm[mapZoom_];

    auto px = [&](float x) {
        return constrain(static_cast<int>(120 + (x - lon) * 111.0f * cos(radians(lat)) * scale), 0, 239);
    };
    auto py = [&](float y) {
        return constrain(static_cast<int>(132 - (y - lat) * 111.0f * scale), 28, 235);
    };

    // Draw planned route
    for (int i = 1; i < routeLen_; i++) {
        d.drawLine(px(route_[i - 1].lon), py(route_[i - 1].lat),
                   px(route_[i].lon), py(route_[i].lat), COLOR_BLACK);
    }

    // Draw recorded breadcrumbs
    for (int i = 0; i < trackLen_; i++) {
        d.drawPixel(px(track_[i].lon), py(track_[i].lat), COLOR_BLUE);
    }

    // Draw heading indicator triangle
    if (ride_.gpsFixed) {
        float h = radians(ride_.heading);
        d.fillTriangle(
            120 + 9 * sin(h), 132 - 9 * cos(h),
            120 - 5 * cos(h), 132 - 5 * sin(h),
            120 + 5 * cos(h), 132 + 5 * sin(h),
            COLOR_RED
        );
    }

    // Draw destination circle
    if (nav_.hasDestination) {
        d.drawCircle(px(nav_.destLon), py(nav_.destLat), 5, COLOR_RED);
    }

    // Telemetry bar
    d.setCursor(8, 244);
    d.printf("%.1f km/h  %d rpm  %dW est", ride_.speedKmh, ride_.cadenceRpm, ride_.powerW);

    // Nav distance and bearing
    d.setCursor(8, 263);
    if (nav_.hasDestination) {
        d.printf("Direct %.2f km  bearing %d", nav_.remainDistKm, nav_.bearingDeg);
    } else {
        d.print("Set destination in web page");
    }

    // Nav ETA
    d.setCursor(8, 282);
    if (nav_.hasDestination && ride_.speedKmh > 3.0f) {
        d.printf("Direct ETA %.0f min", (nav_.remainDistKm / ride_.speedKmh) * 60.0f);
    } else {
        d.print("ETA --");
    }

    // Zoom level info
    d.setCursor(8, 303);
    d.print("No road routing yet | Z");
    d.print(mapZoom_);
}
