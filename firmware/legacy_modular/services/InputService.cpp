#include "services/InputService.hpp"
#include "Logger.hpp"

InputService::InputService() = default;
InputService::~InputService() = default;

void InputService::init(TwoWire* wire) {
    Logger::info("Input", "Initializing inputs: Power control, BOOT key, and MCP23017");

    // Power latch setup
    pinMode(config::PWR_HOLD_PIN, OUTPUT);
    digitalWrite(config::PWR_HOLD_PIN, HIGH); // Maintain battery power

    pinMode(config::PWR_KEY_PIN, INPUT);

    // Onboard BOOT button (GPIO 0) for standalone testing without expander
    pinMode(0, INPUT_PULLUP);

    if (wire != nullptr) {
        buttonsOk_ = buttons_.begin_I2C(config::MCP23017_ADDR, wire);
        if (buttonsOk_) {
            for (int i = 0; i < 4; i++) {
                buttons_.pinMode(i, INPUT_PULLUP);
            }
            Logger::info("Input", "MCP23017 initialized successfully at 0x20");
        } else {
            Logger::info("Input", "MCP23017 not detected (using onboard BOOT key on GPIO 0)");
        }
    }
}

void InputService::update() {
    uint32_t now = millis();

    // Check power button
    checkPowerOff();

    // Debounce onboard BOOT button (GPIO 0)
    btnBoot_.update(digitalRead(0), now);

    if (digitalRead(0) == LOW) {
        if (bootPressMs_ == 0) {
            bootPressMs_ = now;
            bootHeldFired_ = false;
        }
    } else {
        bootPressMs_ = 0;
        bootHeldFired_ = false;
    }

    if (!buttonsOk_) return;

    btnUp_.update(buttons_.digitalRead(btnUp_.pin), now);
    btnDn_.update(buttons_.digitalRead(btnDn_.pin), now);
    btnLt_.update(buttons_.digitalRead(btnLt_.pin), now);
    btnRt_.update(buttons_.digitalRead(btnRt_.pin), now);
}

bool InputService::consumeBootPressed() {
    return btnBoot_.consume();
}

bool InputService::consumeBootHeld() {
    if (bootPressMs_ > 0 && !bootHeldFired_ && (millis() - bootPressMs_ > 700)) {
        bootHeldFired_ = true;
        return true;
    }
    return false;
}

bool InputService::consumePressed(RemoteBtn btn) {
    switch (btn) {
        case RemoteBtn::UP:    return btnUp_.consume();
        case RemoteBtn::DOWN:  return btnDn_.consume();
        case RemoteBtn::LEFT:  return btnLt_.consume();
        case RemoteBtn::RIGHT: return btnRt_.consume();
        case RemoteBtn::BOOT:  return btnBoot_.consume();
        default: return false;
    }
}

void InputService::checkPowerOff() {
    if (digitalRead(config::PWR_KEY_PIN) == LOW) {
        if (!pwrKeyHeldMs_) {
            pwrKeyHeldMs_ = millis();
        }
        if (millis() - pwrKeyHeldMs_ > 3000) {
            Logger::info("Input", "Power button held > 3s, shutting down...");
            digitalWrite(config::LCD_BL_PIN, LOW);
            digitalWrite(config::PWR_HOLD_PIN, LOW);
            delay(100);
        }
    } else {
        pwrKeyHeldMs_ = 0;
    }
}
