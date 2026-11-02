#pragma once
#include "ui/Screen.hpp"
#include "services/InputService.hpp"
#include "services/DisplayService.hpp"

class MenuScreen : public Screen {
public:
    MenuScreen(DisplayService &display, InputService &input);
    ~MenuScreen() override = default;

    void init() override;
    void update() override;
    void render() override;

private:
    DisplayService &display_;
    InputService &input_;
    int selection_ = 0;
};
