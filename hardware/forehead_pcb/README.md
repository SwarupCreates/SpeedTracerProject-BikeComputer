# Forehead PCB & Sensor Array Assembly Guide

This document specifies the technical design, pinout, mechanical fit, assembly instructions, and schematic review for the **Forehead PCB Sensor Assembly** of the SpeedTracer Bike Computer.

---

## 1. Overview & Architecture

The **Forehead PCB** is an upper expansion carrier board designed to house and interconnect the two primary external sensors:
1. **GEPRC GEP-M1025Q**: Combined GNSS Positioning module (u-blox M10) and 3-axis Magnetometer / Compass (QMC5883L).
2. **7Semi BMP581 Breakout**: Ultra-high precision barometric altimeter core (Bosch BMP581) providing $\sim 3.3\text{ cm}$ vertical elevation resolution.

The Forehead PCB aggregates all sensor signals into a single unified 6-wire bus (`G V T R L A`), which routes down the back of the enclosure to connect directly to the two 4-pin JST SH1.0 ports on the main **Waveshare ESP32-S3-Touch-LCD-2.8 V2** display unit.

```
+-------------------------------------------------------------+
|                      FOREHEAD COMPARTMENT                   |
|                                                             |
|  +---------------------------+   +-----------------------+  |
|  |     GEPRC GEP-M1025Q      |   |   7Semi BMP581 Board  |  |
|  |   GNSS + Compass Module   |   |   Barometric Altimeter|  |
|  |       (29 x 29 mm)        |   |      (17 x 22 mm)     |  |
|  +-------------+-------------+   +-----------+-----------+  |
|                | [6 pads: G V T R L A]       | [8 pads: Vi V G SCK SDO SDI CS INT]
|                \--------------+--------------/              |
|                               |                             |
|               +---------------+---------------+             |
|               |          FOREHEAD PCB         |             |
|               |          (49 x 43 mm)         |             |
|               +---------------+---------------+             |
+-------------------------------|-----------------------------+
                                | 6-Wire Harness (G V T R L A)
                                v
+-------------------------------------------------------------+
|                   MAIN UNIT (WAVESHARE ESP32-S3)            |
|                                                             |
|   +-----------------------+       +---------------------+   |
|   | Left 4-Pin JST (UART) |       | Right 4-Pin JST (I2C)|  |
|   | Pins: GND, 3V3, TX, RX|       | Pins: GND, 3V3,SDA,SCL|  |
|   +-----------------------+       +---------------------+   |
+-------------------------------------------------------------+
```

---

## 2. Component Dimensions & Enclosure Clearance

Refer to `01_component_dimensions.jpg` and `02_encasing_dimensions_theory.jpg`:

| Component | Width ($X$) | Height ($Y$) | Depth ($Z$) | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Enclosure Outer** | 55.0 mm | 116.0 mm | $\sim 14.0\text{ mm}$ | Machined or 3D printed housing |
| **Forehead Cavity** | 49.0 mm | 43.0 mm | $\sim 10.0\text{ mm}$ | Upper sensor compartment |
| **Main Unit Display** | 50.54 mm | 73.06 mm | 11.40 mm | Waveshare ESP32-S3-Touch-LCD-2.8 |
| **Forehead PCB Carrier** | 49.0 mm | 43.0 mm | 1.6 mm | 2-layer FR4 PCB with mounting notch |
| **GEPRC M1025Q GPS** | 29.0 mm | 29.0 mm | 8.50 mm | Ceramic patch antenna facing skyward |
| **7Semi BMP581 Breakout** | 17.0 mm | 22.0 mm | 3.00 mm | Pressure port facing ambient chamber |

### Fit Verification
- **Combined Width**: $29.0\text{ mm (GPS)} + 17.0\text{ mm (BMP581)} = 46.0\text{ mm}$.
- **Cavity Width**: $49.0\text{ mm}$.
- **Clearance Margin**: $3.0\text{ mm}$ total ($1.5\text{ mm}$ inter-module spacing, $0.75\text{ mm}$ perimeter margin).
- **Total Height**: $73.06\text{ mm (Screen)} + 43.0\text{ mm (Forehead)} = 116.06\text{ mm}$, aligning with the 116.0 mm enclosure design.
- **Z-Axis Depth**: The GPS module is 8.5 mm thick, while the display unit is 11.4 mm thick. This leaves 2.9 mm behind the Forehead PCB to route the wire harness cleanly.

---

## 3. Forehead PCB Pinout & Connections

Refer to `03_forehead_pcb_design_theory.jpg` and `05_forehead_pcb_layout.jpg`:

### 3.1 6-Pin Output Bus (Bottom Edge)

| Wire Color | Label | Function | ESP32-S3 Assignment | Destination Port |
| :--- | :--- | :--- | :--- | :--- |
| **Black** | **G** | Ground | Common GND | UART & I²C JST Pin 2 |
| **Red** | **V** | Power (+3.3V) | 3V3 Power Rail | UART & I²C JST Pin 1 |
| **White** | **T** | GPS TX (Output) | GPIO 44 (Board RXD) | Left JST Pin 4 |
| **Orange** | **R** | GPS RX (Input) | GPIO 43 (Board TXD) | Left JST Pin 3 |
| **Green** | **L** | SCL (I²C Clock) | GPIO 10 (Wire SCL) | Right JST Pin 4 |
| **Blue** | **A** | SDA (I²C Data) | GPIO 11 (Wire SDA) | Right JST Pin 3 |

---

### 3.2 GPS Module (GEPRC M1025Q) Header Pinout

The GEPRC module has 6 solder pads aligned with the bottom bus:

| Pad | Label | Connected To | Signal Description |
| :---: | :---: | :--- | :--- |
| **1** | **G** | `BOT_G` (GND) | Ground |
| **2** | **V** | `BOT_V` (3V3) | Power supply (3.3V) |
| **3** | **T** | `BOT_T` (TX) | NMEA/UBX data stream $\rightarrow$ ESP32 GPIO 44 |
| **4** | **R** | `BOT_R` (RX) | Configuration commands $\leftarrow$ ESP32 GPIO 43 |
| **5** | **L** | `BOT_L` (SCL) | QMC5883L Compass clock (GPIO 10) |
| **6** | **A** | `BOT_A` (SDA) | QMC5883L Compass data (GPIO 11) |

---

### 3.3 7Semi BMP581 Breakout Pinout & Correct Configuration

The 7Semi breakout has 8 pads. The table below compares the theoretical layout with the **electrically required connections**:

| Pin | Function | Original Theory Routing | **Corrected Requirement** | Reason / Impact |
| :---: | :--- | :--- | :--- | :--- |
| **Vi** | Voltage In (3.3-5V) | Tied to `V` and 3V3 rail | **Leave Unconnected OR use as 3.3V in** | **Do NOT bridge `Vi` and `V` together**. Bridging bypasses LDO regulation and introduces back-feed risk. |
| **V** | 3.3V Output / Supply | Tied to `Vi` and 3V3 rail | **Connect to 3.3V Rail (`BOT_V`)** | Feeds clean 3.3V directly to the BMP581 chip. |
| **G** | Ground | Connected to `BOT_G` | **Connected to `BOT_G`** | Correct. Ground reference. |
| **SCK**| I²C Clock (SCL) | Connected to `BOT_L` (SCL) | **Connected to `BOT_L` (SCL)** | Correct. I²C clock line (GPIO 10). |
| **SDO**| I²C Address Select | Connected to Ground (`GND`) | **Connected to Ground (`GND`)** | Correct. Sets I²C address to `0x46` (`BMP581_I2C_ADDRESS_SECONDARY`). |
| **SDI**| I²C Data (SDA) | Connected to `BOT_A` (SDA) | **Connected to `BOT_A` (SDA)** | Correct. I²C data line (GPIO 11). |
| **CS** | Chip Select | Connected to Ground (`GND`) | **CRITICAL: Connect to 3.3V (or leave float)** | **Tying CS to GND forces SPI mode!** In Bosch sensors, `CS = HIGH` is required for I²C mode. |
| **INT**| Interrupt Output | Left floating | **Leave floating (Unconnected)** | Correct. Firmware uses polled I2C mode. |

---

## 4. Critical Engineering Reviews & Improvements

### 1. [CRITICAL BUG] CS (Chip Select) Must NOT Connect to Ground
> [!CAUTION]
> In Bosch Sensortec architecture (BMP388, BMP390, BMP581), the `CS` (Chip Select / CSB) pin acts as the **protocol selector**:
> - **`CS = HIGH (VDD / 3.3V)`**: Selects **I²C Mode**.
> - **`CS = LOW (GND)`**: Selects **SPI Mode**.
>
> In the theoretical layout diagram (`05_forehead_pcb_layout.jpg`), the `CS` pad has a trace running to the right and joining the common ground line alongside `G` and `SDO`.
> **If fabricated this way, the BMP581 will be permanently locked in SPI mode and will NEVER respond on the I²C bus.**
>
> **Action Required**: Cut the ground link to `CS`. Either pull `CS` to `3V3` via a trace, or leave it unconnected (the 7Semi breakout has an onboard pull-up resistor to 3.3V on the CS pin to default to I²C mode).

---

### 2. [CRITICAL WARNING] Do NOT Short `Vi` to `V`
> [!WARNING]
> On breakout boards equipped with an onboard LDO regulator:
> - `Vi` is the input to the regulator.
> - `V` (3.3V) is the regulated output connected to the BMP581 VDD rail.
>
> Bridging `Vi` directly to `V` shorts the regulator input to its output. While benign if the input is strictly 3.3V, if 5V is ever inadvertently applied to the bus, 5V will flow directly into the BMP581 chip via `V`, immediately destroying the silicon (absolute max rating is 3.6V).
>
> **Action Required**: Connect the 3.3V supply from `BOT_V` **only to `V`** (or only to `Vi`), never short both pads together.

---

### 3. Solid Ground Plane for RF & Barometric Stability
- **The GPS Problem**: The u-blox M10 GPS and ceramic antenna receive weak satellite signals ($\sim -130\text{ dBm}$ to $-160\text{ dBm}$). Running thin, serpentine ground and data traces directly beside the antenna creates ground bounce and EMI.
- **The Baro Problem**: The BMP581 capacitive sensing bridge measures sub-millibar pressure variations. Fluctuations in ground reference cause noisy altitude readings.
- **Recommendation**: Design the Forehead PCB as a **2-layer board**:
  - **Top Layer**: Signal routing (`SDA`, `SCL`, `TX`, `RX`, `3V3`).
  - **Bottom Layer**: **Continuous Solid Ground Pour (GND plane)**. This provides low-impedance return paths and acts as an EMI shield between the noisy ESP32/display below and the GPS antenna above.

---

### 4. Enclosure Barometer Vent Hole (Acoustic/Barometric Membrane)
> [!IMPORTANT]
> The BMP581 measures external air pressure through the vent hole on its metal package.
> If the 3D-printed enclosure is sealed or airtight:
> 1. Heat generated by the ESP32-S3 and LCD backlight will heat trapped air, creating artificial pressure increases that register as false descents.
> 2. Rapid elevation changes (descending or climbing a steep hill) will have delayed pressure response.
>
> **Action Required**: Add a **1.0 mm – 1.5 mm vent hole** in the enclosure wall adjacent to the BMP581 chamber. Cover the hole internally with an **IP67 waterproof breathable acoustic/barometric membrane** (e.g. Gore-Tex or adhesive PTFE vent sticker). This allows ambient air pressure to equalize instantaneously while preventing moisture and road dust ingress.

---

### 5. Fastener Material (Non-Magnetic Screws)
- The GEPRC module integrates a **QMC5883L 3-axis magnetometer** to provide real-time compass heading.
- Standard zinc-plated steel screws produce localized magnetic bias fields that distort heading calculations and cause compass jitter.
- **Action Required**: For all enclosure assembly screws (especially the 4 corner screws near the forehead), use **non-magnetic A2/A4 (304/316) stainless steel** or **brass/nylon** screws.

---

### 6. Wire Harness Routing & Splitting
Refer to `04_assembly_with_forehead_pcb.jpg`:
- The 6 wires (`G V T R L A`) originate at the bottom of the Forehead PCB and travel down 73 mm along the rear of the display to reach the Waveshare board's two 4-pin JST SH1.0 connectors:
  - **UART Connector (Left)**: Pins 1=3V3 (`Red`), 2=GND (`Black`), 3=TX43 (`White`), 4=RX44 (`Orange`).
  - **I²C Connector (Right)**: Pins 1=3V3, 2=GND, 3=SDA11 (`Blue`), 4=SCL10 (`Green`).
- **Tip**: To eliminate messy loose wires inside the case, use a flat 6-conductor silicone ribbon cable adhering neatly against the rear housing wall.
