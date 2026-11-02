# Project Roadmap

## Phase 1: Core Telemetry & GPS Mapping (Current Focus)
- [x] Waveshare ESP32-S3-Touch-LCD-2.8 board support (ST7789V2 240x320 IPS display over FSPI).
- [x] GEPRC GEP-M1025Q GPS module integration (u-blox M10 GNSS over UART 115200 with UBX NAV-PVT parsing).
- [x] QMC5883L 3-axis magnetometer integration over shared I2C with calibration routine and NVS persistence.
- [x] Animated hero speed arc gauge, trip distance, elapsed time, average/max speed, virtual cadence, and virtual power.
- [x] Dynamic map screen with user location pinned at screen center, heading indicator triangle, and 5 zoom levels.
- [x] SoftAP WiFi access point (`CycloComp`) with embedded web interface for configuration and navigation.
- [x] Onboard BOOT button (GPIO 0) screen toggling and map zooming for button-less testing.
- [x] Precompiled binaries and one-click flashers (`flash.bat`, `flash.sh`) for rapid flashing.

## Phase 2: MicroSD Offline Maps & GPX Navigation
- [ ] FAT32 MicroSD card support via board's onboard TF slot (HSPI).
- [ ] Continuous CSV breadcrumb logging (`/ride.csv`) to SD card.
- [ ] Offline road network vector tile storage and rendering on the map screen.
- [ ] GPX route loading and turn-by-turn navigation guidance with target destination coordinates.

## Phase 3: Physical Remote Buttons & Barometer
- [ ] Adafruit MCP23017 16-channel I2C GPIO expander support.
- [ ] 4 handlebar momentary pushbuttons (UP, DOWN, LEFT, RIGHT).
- [ ] Adafruit BMP390 precision barometric pressure sensor (STEMMA QT) for barometric altitude and slope % calculation.
- [ ] 3.7V 1200mAh protected Li-ion pouch battery integration with hardware power latch (`SYS_EN` on GPIO 7) and long-press power off on GPIO 6.
- [ ] Low-power sleep modes and battery percentage monitoring.

## Future Horizons
- [ ] BLE sensor support (heart rate monitors, power meters, speed/cadence sensors).
- [ ] 3D-printable weatherproof enclosure with quarter-turn Garmin/Wahoo style handlebar mount.
- [ ] Strava / Komoot automated ride sync over WiFi.
