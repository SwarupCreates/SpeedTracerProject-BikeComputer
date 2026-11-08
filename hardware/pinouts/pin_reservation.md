# Complete Project I/O Table

Verified and checked against the saved wiring and firmware for **Waveshare ESP32-S3-Touch-LCD-2.8 V2**.

> [!IMPORTANT]
> GPIO numbers are signal names—not physical connector positions. Always follow the board’s printed connector labels.

---

## 1. External Sensor Wiring
These are the wires you need to connect yourself.

| Module | Module Pin | Board Connection | Purpose |
| :--- | :--- | :--- | :--- |
| **GEPRC GEP-M1025Q GPS + Compass** | VCC | 3V3 | Power |
| | GND | GND | Ground |
| | TX | GPIO44 / board RXD | GPS sends data to the ESP32 |
| | RX | GPIO43 / board TXD | ESP32 sends configuration to GPS |
| | SDA | GPIO11 | Compass I²C data |
| | SCL | GPIO10 | Compass I²C clock |
| **7Semi BMP581 Altimeter** | V / 3V3 | 3V3 | Power (Do not bridge Vi to V) |
| | GND | GND | Ground |
| | SDI | GPIO11 | Pressure sensor I²C data (SDA) |
| | SCK | GPIO10 | Pressure sensor I²C clock (SCL) |
| | SDO | GND | Sets I²C address to `0x46` |
| | CS | 3V3 / Float | Selects I²C mode (**Must NOT connect to GND!**) |
| **Waveshare BMP390 Barometer (Legacy)** | VCC | 3V3 | Power |
| | GND | GND | Ground |
| | SDA | GPIO11 | Pressure sensor I²C data |
| | SCL | GPIO10 | Pressure sensor I²C clock |
| **Adafruit MCP23017 Button Expander** | VIN | 3V3 | Power |
| | GND | GND | Ground |
| | SDA | GPIO11 | Button expander I²C data |
| | SCL | GPIO10 | Button expander I²C clock |

> [!NOTE]
> **GPIO11 and GPIO10 are shared**: connect the compass, barometer, and expander modules to the same data and clock lines. This is intentional.
> For the carrier PCB assembly in the top cavity, see [Forehead PCB Guide](file:///c:/Users/srpcr/Desktop/Codespace/SpeedTracerProject/hardware/forehead_pcb/README.md).

### External Bus Settings

| Device | Interface | Current Setting |
| :--- | :--- | :--- |
| **GPS (GEPRC GEP-M1025Q)** | UART (Serial 1) | 115200 baud, 8N1 |
| **QMC5883L Compass** | I²C (Wire 0) | Address `0x0D` |
| **7Semi BMP581 Barometer** | I²C (Wire 0) | Address `0x46` (SDO to GND); fallback `0x47` |
| **Waveshare BMP390 (Legacy)**| I²C (Wire 0) | Address `0x77` default; firmware checks `0x76` |
| **MCP23017 Expander** | I²C (Wire 0) | Address `0x20` |

*For the BMP390, keep the module in its default I²C configuration. Its interrupt and SPI-only connections are unused.*

---

## 2. Optional Wired Directional Buttons
These connect to the MCP23017 expander, **not** directly to ESP32 GPIOs.

| Button | One Contact | Other Contact | Behaviour |
| :--- | :--- | :--- | :--- |
| **Up** | MCP23017 GPA0 | GND | Press connects input to ground |
| **Down** | MCP23017 GPA1 | GND | Same |
| **Left** | MCP23017 GPA2 | GND | Same |
| **Right** | MCP23017 GPA3 | GND | Same |

*Internal pull-ups are enabled in firmware.*

> [!WARNING]
> **GPA0–GPA3 are the expander’s GPIO pins.** Do not confuse them with its A0/A1/A2 address-selection pins. The project’s wiring entries abbreviate these button connections as A0–A3, but the intended physical connections are **GPA0–GPA3**.

---

## 3. Built-in Screen and Touch
These connections are already routed internally on the Waveshare board. Do not add external wires to them.

| Built-in Device | Signal | ESP32-S3 GPIO |
| :--- | :--- | :--- |
| **ST7789 LCD** | SPI clock (SCK) | 40 |
| | SPI data to display / MOSI | 45 |
| | Chip select (CS) | 42 |
| | Data/command (DC) | 41 |
| | Reset (RST) | 39 |
| | Backlight control (BL) | 5 |
| **CST3530 Touchscreen** | I²C data / SDA | 1 |
| | I²C clock / SCL | 3 |
| | Reset (RST) | 2 |
| | Interrupt (INT) | 4 |

> [!NOTE]
> Touch uses a **separate I²C bus (`TwoWire touchBus(1);`)** from the external sensors (`Wire(0)` on GPIO 11/10).

---

## 4. Built-in MicroSD Socket
Insert the 16 GB FAT32 microSD card directly into the onboard socket. No external SD module is needed.

| Signal | ESP32-S3 GPIO | Notes |
| :--- | :--- | :--- |
| **SPI clock (SCK)** | 14 | HSPI clock |
| **MOSI (data to card)** | 17 | HSPI data out |
| **MISO (data from card)**| 16 | HSPI data in |
| **Chip select (CS)** | 21 | HSPI chip select |

---

## 5. Power and Battery

| Connection / Control | Assignment | Notes |
| :--- | :--- | :--- |
| **USB Power & Deployment** | Onboard USB-C port | Recommended for initial testing and flashing |
| **Battery Positive (+)** | Main BAT connector positive | Protected single-cell 3.7 V Li-ion/LiPo (e.g. 1200mAh pouch) |
| **Battery Negative (-)** | Main BAT connector negative / GND | **Verify connector polarity before plugging in** |
| **Battery Power Hold** | GPIO 7 | Built-in; firmware drives HIGH to maintain battery power |
| **Power Key Input** | GPIO 6 | Built-in; active LOW, held > 3s triggers shutdown |
| **Battery Percentage Measurement** | Not implemented | No measurement GPIO is assigned in the current project |

> [!CAUTION]
> **Do not connect the battery to 3V3, VBUS or the RTC backup-battery connector.** Confirm the main battery connector’s polarity and the battery’s permitted charging current before attaching it.

---

## 6. Wireless Sensors and Other Onboard Hardware

| Item | External GPIO Required? | Current Status |
| :--- | :--- | :--- |
| **BLE Heart-Rate Sensor** | No | BLE support implemented; physical testing pending |
| **BLE Speed/Cadence Sensor** | No | Same |
| **BLE Cycling Power Meter** | No | Same |
| **Wi-Fi Phone Configuration** | No | Uses onboard 2.4 GHz radio (`CycloComp` AP) |
| **Experimental Software ANT+** | No additional pins proposed | Not integrated; no working ANT mode yet |
| **Built-in QMI8658 IMU** | No external wiring | Not implemented in current firmware; bus pins unrecorded |
| **Built-in PCF85063 RTC** | No external wiring | Bus pins unrecorded in current board file |

---

## Summary for Board-Only Testing

For initial board-only testing: connect USB-C and deploy. Touch, the display, and wireless configuration need no external sensor wiring. Add the external modules with power disconnected.
