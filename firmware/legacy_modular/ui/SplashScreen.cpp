#include "ui/SplashScreen.hpp"

SplashScreen::SplashScreen(DisplayService& display)
    : display_(display) {}

void SplashScreen::init() {}

void SplashScreen::update() {}

void SplashScreen::render() {
    auto& d = display_.getDisplay();
    display_.clear();
    d.setTextColor(COLOR_BLACK);

    d.setTextSize(2);
    d.setCursor(12, 50);
    d.print("CycloComp V2");

    d.setTextSize(1);
    d.setCursor(12, 90);
    d.print("CycloComp / cycling1234");
    d.setCursor(12, 110);
    d.print("http://192.168.4.1");
}
