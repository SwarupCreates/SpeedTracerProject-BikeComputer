#include "ui/MenuScreen.hpp"
#include <algorithm>

MenuScreen::MenuScreen(DisplayService &display, InputService &input)
    : display_(display), input_(input) {}

void MenuScreen::init() {
    selection_ = 0;
}

void MenuScreen::update() {
    if (input_.consumePressed(RemoteBtn::UP)) {
        selection_ = std::max(0, selection_ - 1);
    }
    if (input_.consumePressed(RemoteBtn::DOWN)) {
        selection_ = selection_ + 1;
    }
}

void MenuScreen::render() {
    display_.clear();
    auto& d = display_.getDisplay();
    d.setTextColor(COLOR_BLACK);
    d.setTextSize(1);
    d.setCursor(8, 10);
    d.print("Menu (stub)");
}
