#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_GFX.h>
#include "config/pin_config.hpp"

#define COLOR_BLACK ST77XX_BLACK
#define COLOR_WHITE ST77XX_WHITE
#define COLOR_BLUE  ST77XX_BLUE
#define COLOR_RED   ST77XX_RED

class CycleDisplay : public Adafruit_ST7789 {
public:
    explicit CycleDisplay(SPIClass* spi) 
        : Adafruit_ST7789(spi, config::LCD_CS_PIN, config::LCD_DC_PIN, config::LCD_RST_PIN) {}
    void clearDisplay() { fillScreen(COLOR_WHITE); }
    void refresh() {}
};

class DisplayService {
public:
    DisplayService();
    ~DisplayService();

    void init();
    void setBacklight(bool on);
    void clear();
    void refresh();

    void drawArcFill(int cx, int cy, int r, int startDeg, int spanDeg, int thicc);
    void drawArcOutline(int cx, int cy, int r, int startDeg, int endDeg);
    void drawProgressBar(int x, int y, int w, int h, float frac);
    void drawDividerH(int y, int x0 = 0, int x1 = config::DISP_W - 1);

    CycleDisplay& getDisplay() { return display_; }

private:
    SPIClass lcdSPI_;
    CycleDisplay display_;
};
