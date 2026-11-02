#pragma once
#include "ui/Screen.hpp"
#include "services/DisplayService.hpp"
#include "models/RideData.hpp"
#include "models/NavState.hpp"

class MapScreen : public Screen {
public:
    MapScreen(DisplayService& display, const RideData& ride,
              const NavState& nav, const RoutePoint* route, const int& routeLen,
              const TrackPoint* track, const int& trackLen, uint8_t& mapZoom);
    ~MapScreen() override = default;

    void init() override;
    void update() override;
    void render() override;

    void zoomIn();
    void zoomOut();

private:
    DisplayService& display_;
    const RideData& ride_;
    const NavState& nav_;
    const RoutePoint* route_;
    const int& routeLen_;
    const TrackPoint* track_;
    const int& trackLen_;
    uint8_t& mapZoom_;
};
