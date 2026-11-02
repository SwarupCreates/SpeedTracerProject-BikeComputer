#pragma once
#include "ui/Screen.hpp"
#include "services/DisplayService.hpp"

class SplashScreen : public Screen {
public:
    explicit SplashScreen(DisplayService& display);
    ~SplashScreen() override = default;

    void init() override;
    void update() override;
    void render() override;

private:
    DisplayService& display_;
};
