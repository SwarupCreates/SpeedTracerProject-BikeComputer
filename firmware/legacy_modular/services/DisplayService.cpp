#include "services/DisplayService.hpp"
#include "Logger.hpp"
#include <cmath>

DisplayService::DisplayService()
    : lcdSPI_(FSPI), display_(&lcdSPI_) {}

DisplayService::~DisplayService() = default;

void DisplayService::init() {
    Logger::info("Display", "Initializing ST7789 display (240x320)");
    pinMode(config::LCD_BL_PIN, OUTPUT);
    digitalWrite(config::LCD_BL_PIN, LOW);

    lcdSPI_.begin(config::LCD_SCK_PIN, config::LCD_MISO_PIN, config::LCD_MOSI_PIN, config::LCD_CS_PIN);
    display_.init(config::DISP_W, config::DISP_H);
    display_.setRotation(0);
    display_.invertDisplay(true);

    setBacklight(true);
}

void DisplayService::setBacklight(bool on) {
    digitalWrite(config::LCD_BL_PIN, on ? HIGH : LOW);
}

void DisplayService::clear() {
    display_.clearDisplay();
}

void DisplayService::refresh() {
    display_.refresh();
}

void DisplayService::drawArcFill(int cx, int cy, int r, int startDeg, int spanDeg, int thicc) {
    spanDeg = constrain(spanDeg, 0, 360);
    for (int t = startDeg; t <= startDeg + spanDeg; t++) {
        float rad = radians(t - 90);
        for (int d = 0; d < thicc; d++) {
            int x = cx + static_cast<int>((r - d) * cosf(rad));
            int y = cy + static_cast<int>((r - d) * sinf(rad));
            if (x >= 0 && x < config::DISP_W && y >= 0 && y < config::DISP_H) {
                display_.drawPixel(x, y, COLOR_BLACK);
            }
        }
    }
}

void DisplayService::drawArcOutline(int cx, int cy, int r, int startDeg, int endDeg) {
    for (int t = startDeg; t <= endDeg; t++) {
        float rad = radians(t - 90);
        int x = cx + static_cast<int>(r * cosf(rad));
        int y = cy + static_cast<int>(r * sinf(rad));
        if (x >= 0 && x < config::DISP_W && y >= 0 && y < config::DISP_H) {
            display_.drawPixel(x, y, COLOR_BLACK);
        }
    }
}

void DisplayService::drawProgressBar(int x, int y, int w, int h, float frac) {
    frac = constrain(frac, 0.0f, 1.0f);
    display_.drawRect(x, y, w, h, COLOR_BLACK);
    int fill = static_cast<int>(frac * (w - 4));
    if (fill > 0) {
        display_.fillRect(x + 2, y + 2, fill, h - 4, COLOR_BLACK);
    }
}

void DisplayService::drawDividerH(int y, int x0, int x1) {
    display_.drawLine(x0, y, x1, y, COLOR_BLACK);
}
