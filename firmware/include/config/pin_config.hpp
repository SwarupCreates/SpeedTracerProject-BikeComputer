#pragma once
#include <cstdint>

namespace config {
    // -------------------------------------------------------------------------
    // Power & Battery Management
    // -------------------------------------------------------------------------
    constexpr int PWR_HOLD_PIN = 7;     // SYS_EN: active HIGH to maintain battery power
    constexpr int PWR_KEY_PIN = 6;      // Power key: active LOW, held > 3s triggers shutdown

    // -------------------------------------------------------------------------
    // Built-in ST7789V2 LCD (FSPI)
    // -------------------------------------------------------------------------
    constexpr int LCD_SCK_PIN = 40;
    constexpr int LCD_MOSI_PIN = 45;
    constexpr int LCD_MISO_PIN = -1;    // Not connected
    constexpr int LCD_CS_PIN = 42;
    constexpr int LCD_DC_PIN = 41;
    constexpr int LCD_RST_PIN = 39;
    constexpr int LCD_BL_PIN = 5;       // Backlight: HIGH = ON, LOW = OFF
    constexpr int DISP_W = 240;
    constexpr int DISP_H = 320;

    // -------------------------------------------------------------------------
    // Built-in CST3530 Capacitive Touchscreen (Separate I2C Bus: TwoWire 1)
    // -------------------------------------------------------------------------
    constexpr int TOUCH_I2C_BUS = 1;
    constexpr int TOUCH_SDA_PIN = 1;
    constexpr int TOUCH_SCL_PIN = 3;
    constexpr int TOUCH_RST_PIN = 2;
    constexpr int TOUCH_INT_PIN = 4;
    constexpr uint8_t TOUCH_I2C_ADDR = 0x58;

    // -------------------------------------------------------------------------
    // Shared External Sensor I2C Bus (TwoWire 0: GPIO 11 / GPIO 10)
    // -------------------------------------------------------------------------
    constexpr int I2C_SDA_PIN = 11;     // Shared by GPS compass, BMP390, MCP23017
    constexpr int I2C_SCL_PIN = 10;
    constexpr uint8_t MCP23017_ADDR = 0x20;
    constexpr uint8_t BMP390_ADDR = 0x77;       // Firmware also auto-checks 0x76
    constexpr uint8_t BMP390_ALT_ADDR = 0x76;
    constexpr uint8_t QMC5883L_ADDR = 0x0D;

    // -------------------------------------------------------------------------
    // Optional Wired Directional Buttons (on MCP23017 GPA0-GPA3, INPUT_PULLUP)
    // -------------------------------------------------------------------------
    constexpr uint8_t RMT_UP_PIN = 0;   // MCP23017 GPA0 (Pin 21) <-> GND
    constexpr uint8_t RMT_DN_PIN = 1;   // MCP23017 GPA1 (Pin 22) <-> GND
    constexpr uint8_t RMT_LT_PIN = 2;   // MCP23017 GPA2 (Pin 23) <-> GND
    constexpr uint8_t RMT_RT_PIN = 3;   // MCP23017 GPA3 (Pin 24) <-> GND

    // -------------------------------------------------------------------------
    // GPS UART (GEPRC GEP-M1025Q u-blox M10) via HardwareSerial 1
    // -------------------------------------------------------------------------
    constexpr int GPS_UART_NUM = 1;
    constexpr int GPS_RX_PIN = 44;      // ESP32 RX from GPS TX
    constexpr int GPS_TX_PIN = 43;      // ESP32 TX to GPS RX
    constexpr int GPS_UART_BAUD = 115200;

    // -------------------------------------------------------------------------
    // Built-in MicroSD Socket (HSPI)
    // -------------------------------------------------------------------------
    constexpr int SD_SCK_PIN = 14;
    constexpr int SD_MISO_PIN = 16;     // Data from card
    constexpr int SD_MOSI_PIN = 17;     // Data to card
    constexpr int SD_CS_PIN = 21;
}
