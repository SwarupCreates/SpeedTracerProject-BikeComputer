#pragma once
#include "ui/Screen.hpp"
#include "services/DisplayService.hpp"
#include "services/GPSService.hpp"
#include "models/RideData.hpp"
#include "models/Config.hpp"
#include "models/NavState.hpp"

class DashboardScreen : public Screen {
public:
    DashboardScreen(DisplayService& display, GPSService& gpsService,
                    const RideData& ride, const Config& cfg,
                    const SensorFlags& sensors);
    ~DashboardScreen() override = default;

    void init() override;
    void update() override;
    void render() override;

private:
    DisplayService& display_;
    GPSService& gpsService_;
    const RideData& ride_;
    const Config& cfg_;
    const SensorFlags& sensors_;
};
