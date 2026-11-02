#include <Arduino.h>
#include "App.hpp"
#include "Logger.hpp"

static App app;

void setup() {
    Serial.begin(115200);
    Logger::init();
    Logger::info("Main", "Starting SpeedTracer v1.0");

    app.init();
}

void loop() {
    app.update();
}
