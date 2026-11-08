# System Architecture

## Overview

SpeedTracer is built around an ESP32-S3 and follows a modular, decoupled architecture where drivers, hardware subsystems, business logic calculations, web connectivity, and UI rendering are separated into distinct modules.

```
+---------------------------------------------------------------------------------+
|                                     App                                         |
+---------------------------------------------------------------------------------+
         |                    |                  |                  |
         v                    v                  v                  v
+------------------+  +----------------+  +--------------+  +-----------------+
|  DisplayService  |  |   GPSService   |  | InputService |  | StorageService  |
| (ST7789V2 FSPI)  |  |  (GEPRC M1025Q |  |  (MCP23017   |  |   (MicroSD      |
|                  |  |  QMC5883L I2C  |  |   I2C GPIO   |  |    HSPI)        |
|                  |  |   BMP390 I2C)  |  |   Expander)  |  |                 |
+------------------+  +----------------+  +--------------+  +-----------------+
         ^                    |                                     ^
         |                    v                                     |
+-----------------------------------------------------------------------------+
|                                WebService                                   |
| (WiFi SoftAP "CycloComp" + WebServer REST API: /status, /config, /upload)    |
+-----------------------------------------------------------------------------+
         |
         v
+-----------------------------------------------------------------------------+
|                                    UI Layer                                 |
|            (SplashScreen  <-->  DashboardScreen  <-->  MapScreen)           |
+-----------------------------------------------------------------------------+
```

## Core Subsystems

### 1. Data Models (`models/`)
- `RideData`: Current speed, distance, elevation, grade %, cadence, power, VO2max estimate, heading, elapsed time, max/avg values, GPS fix.
- `Config`: Bike mass, rider mass, wheel circumference, gear ratio, power limits, manual VO2max, target distance.
- `NavState`: Route waypoints (`RoutePoint`), breadcrumbs (`TrackPoint`), target destination coordinates, remaining distance, bearing, route index.

### 2. Services (`services/`)
- **DisplayService**: Manages the ST7789V2 display on FSPI (pins 40, 45, 42, 41, 39, backlight 5) and drawing primitives (arcs, progress bars, dividers).
- **GPSService**: Handles the GEPRC M1025Q GNSS over UART (RX 44, TX 43), parses UBX NAV-PVT and NMEA fallback, reads the QMC5883L compass (0x0D), reads the 7Semi BMP581 / BMP390 altimeter (0x46 / 0x77 on the [Forehead PCB](file:///c:/Users/srpcr/Desktop/Codespace/SpeedTracerProject/hardware/forehead_pcb/README.md)), calculates Haversine distance, grade %, virtual cadence, and virtual power.
- **InputService**: Interfaces with the Adafruit MCP23017 I2C expander (0x20) with debounced handlers for remote buttons (UP, DOWN, LEFT, RIGHT) and checks the power key (GPIO 6) for long-press shutdown via SYS_EN (GPIO 7).
- **StorageService**: Interfaces with FAT32 MicroSD via HSPI (pins 14, 16, 17, 21), handles `/ride.csv` breadcrumb logging and `/route.json` route persistence.
- **WebService**: Manages the SoftAP WiFi access point and embedded HTTP WebServer on port 80 with endpoints for telemetry, navigation, route upload, config NVS management, GPX export, and compass calibration.

### 3. UI Layer (`ui/`)
- **SplashScreen**: Displayed on boot with WiFi credentials and IP address.
- **DashboardScreen**: Dominant speed arc dial, distance, runtime, cadence, power bar, VO2max, altitude, coordinates, and target distance progress bar.
- **MapScreen**: Breadcrumbs track, planned route line, heading triangle indicator, target destination marker, and 5-level geographic zoom.
