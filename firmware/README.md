# Firmware

This folder contains the firmware for SpeedTracer, targeted at the **Waveshare ESP32-S3-Touch-LCD-2.8 V2** development board.

## Supported Hardware & Peripherals

- **SoC & Display**: Waveshare ESP32-S3 2.8" IPS LCD (240×320 ST7789V2 via FSPI: SCK 40, MOSI 45, CS 42, DC 41, RST 39, BL 5).
- **Power Control**: SYS_EN battery latch on GPIO 7 (active HIGH), Power key on GPIO 6 (hold > 3s cuts power).
- **Sensors (Shared I2C on SDA 11 / SCL 10)**:
  - **Adafruit MCP23017** (0x20): 4 remote momentary buttons (UP 0, DOWN 1, LEFT 2, RIGHT 3) with internal pull-ups.
  - **Adafruit BMP390** (0x77): Barometric altitude and grade % calculation.
  - **QMC5883L Compass** (0x0D on GPS board): Magnetic heading with EEPROM calibration and alignment offsets.
- **GPS**: GEPRC GEP-M1025Q (u-blox M10) via HardwareSerial 1 (RX 44, TX 43 @ 115200 baud) supporting UBX NAV-PVT and NMEA fallback.
- **Storage**: FAT32 MicroSD via HSPI (SCK 14, MISO 16, MOSI 17, CS 21) for `/ride.csv` logging and `/route.json` route persistence.
- **Connectivity**: SoftAP `CycloComp` with embedded HTTP WebServer (live telemetry, route upload, config NVS editor, GPX preview export, SD log download, compass calibration).

## Architecture

The firmware is structured into modular services and UI screens:
- `App`: System initialization, lifecycle, and screen state machine.
- `services/`:
  - `DisplayService`: Display driver, backlight control, and custom geometry primitives (arcs, progress bars, dividers).
  - `GPSService`: GPS UART reader, UBX/NMEA parser, QMC5883L compass driver, BMP390 altitude, distance/grade/virtual power/cadence calculations.
  - `InputService`: MCP23017 remote buttons debounce and power hold detection.
  - `StorageService`: MicroSD HSPI driver, route loading/saving, and ride CSV logging.
  - `WebService`: SoftAP, WebServer HTTP endpoints, and HTML telemetry/config interface.
- `ui/`:
  - `SplashScreen`: Boot splash with WiFi credentials and IP address.
  - `DashboardScreen`: Speed dial gauge, distance, time, cadence, power, VO2max, altitude, coordinates, and target progress bar.
  - `MapScreen`: Route line visualization, GPS breadcrumbs, heading triangle, destination marker, and zoom levels.
- `standalone/`: Contains `firmware.cpp` as a single-file reference implementation.

## Build and Upload (PlatformIO)

Build:
```bash
pio run -e esp32-s3-devkitc-1
```

Upload:
```bash
pio run -e esp32-s3-devkitc-1 -t upload
```

Serial Monitor:
```bash
pio device monitor -e esp32-s3-devkitc-1
```
