#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include "config/pin_config.hpp"

enum class RemoteBtn {
    UP = config::RMT_UP_PIN,
    DOWN = config::RMT_DN_PIN,
    LEFT = config::RMT_LT_PIN,
    RIGHT = config::RMT_RT_PIN,
    BOOT = 0,
    NONE = 255
};

struct DebouncedBtn {
    uint8_t  pin = 0;
    bool     last = HIGH;
    uint32_t lastMs = 0;
    bool     pressed = false;

    void update(bool curState, uint32_t now) {
        if (curState != last && (now - lastMs) > 40) {
            lastMs = now;
            last = curState;
            if (curState == LOW) pressed = true;
        }
    }

    bool consume() {
        if (pressed) {
            pressed = false;
            return true;
        }
        return false;
    }
};

class InputService {
public:
    InputService();
    ~InputService();

    void init(TwoWire* wire = &Wire);
    void update();

    bool isOk() const { return buttonsOk_; }
    bool consumePressed(RemoteBtn btn);

    // Onboard BOOT button (GPIO 0) helpers for Phase 1 testing
    bool consumeBootPressed();
    bool consumeBootHeld();

    // Power key handling
    void checkPowerOff();

private:
    Adafruit_MCP23X17 buttons_;
    bool buttonsOk_ = false;

    DebouncedBtn btnUp_{config::RMT_UP_PIN};
    DebouncedBtn btnDn_{config::RMT_DN_PIN};
    DebouncedBtn btnLt_{config::RMT_LT_PIN};
    DebouncedBtn btnRt_{config::RMT_RT_PIN};

    // Onboard BOOT button on GPIO 0
    DebouncedBtn btnBoot_{0};
    uint32_t bootPressMs_ = 0;
    bool bootHeldFired_ = false;

    uint32_t pwrKeyHeldMs_ = 0;
};
