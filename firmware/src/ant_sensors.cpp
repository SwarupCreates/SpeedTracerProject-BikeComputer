#include "ant_sensors.h"
#include <esp_log.h>

volatile int antHR = 0;
volatile uint32_t antHRMs = 0;
volatile float antSpeed = 0.0f;
volatile uint32_t antSpeedMs = 0;
volatile int antCad = 0;
volatile uint32_t antCadMs = 0;
volatile int antPower = 0;
volatile uint32_t antPowerMs = 0;
volatile bool antReady = false;

volatile uint16_t antHrmDevId = 0;
volatile int8_t antHrmRssi = 0;
volatile uint16_t antSpdDevId = 0;
volatile int8_t antSpdRssi = 0;
volatile uint16_t antCadDevId = 0;
volatile int8_t antCadRssi = 0;
volatile uint16_t antPowerDevId = 0;
volatile int8_t antPowerRssi = 0;

static ant_node_t s_antNode;
static uint16_t s_prevCadTime = 0, s_prevCadRev = 0;
static uint16_t s_prevSpdTime = 0, s_prevSpdRev = 0;
static bool s_cadInit = false, s_spdInit = false;

static void on_ant_data(ant_node_t *node, const ant_node_rx_t *rx, const uint8_t page[8], void *user) {
    if (!rx || !page) return;
    uint32_t now = millis();

    if (rx->device_type == ANTPLUS_DEVTYPE_HRM) {
        antHrmDevId = rx->device_num;
        antHrmRssi = rx->rssi;
        antplus_hrm_data_t hr;
        if (antplus_hrm_decode(page, &hr) && hr.computed_heart_rate > 0 && hr.computed_heart_rate <= 250) {
            antHR = (int)hr.computed_heart_rate;
            antHRMs = now;
        }
    } else if (rx->device_type == ANTPLUS_DEVTYPE_BIKE_POWER) {
        antPowerDevId = rx->device_num;
        antPowerRssi = rx->rssi;
        antplus_power_data_t pwr;
        if (antplus_power_decode(page, &pwr)) {
            antPower = (int)pwr.instantaneous_power;
            antPowerMs = now;
            if (pwr.instantaneous_cadence != 0xFF) {
                antCad = (int)pwr.instantaneous_cadence;
                antCadMs = now;
            }
        }
    } else if (rx->device_type == ANTPLUS_DEVTYPE_BIKE_SPDCAD) {
        antSpdDevId = rx->device_num;
        antSpdRssi = rx->rssi;
        antCadDevId = rx->device_num;
        antCadRssi = rx->rssi;
        antplus_spdcad_data_t sc;
        if (antplus_spdcad_decode(page, &sc)) {
            if (s_cadInit) {
                uint16_t rpm = antplus_spdcad_rpm(s_prevCadTime, sc.cadence_event_time, s_prevCadRev, sc.cumulative_cadence);
                if (rpm <= 250) {
                    antCad = (int)rpm;
                    antCadMs = now;
                }
            }
            s_prevCadTime = sc.cadence_event_time;
            s_prevCadRev = sc.cumulative_cadence;
            s_cadInit = true;

            if (s_spdInit) {
                uint32_t speed_mm_s = antplus_spdcad_speed_mm_s(s_prevSpdTime, sc.speed_event_time, s_prevSpdRev, sc.cumulative_speed, 2096);
                float kmh = (float)speed_mm_s * 0.0036f;
                if (kmh <= 150.0f) {
                    antSpeed = kmh;
                    antSpeedMs = now;
                }
            }
            s_prevSpdTime = sc.speed_event_time;
            s_prevSpdRev = sc.cumulative_speed;
            s_spdInit = true;
        }
    } else if (rx->device_type == ANTPLUS_DEVTYPE_BIKE_SPEED) {
        antSpdDevId = rx->device_num;
        antSpdRssi = rx->rssi;
        uint16_t spdTime = (uint16_t)page[4] | ((uint16_t)page[5] << 8);
        uint16_t spdRev  = (uint16_t)page[6] | ((uint16_t)page[7] << 8);
        if (s_spdInit) {
            uint32_t speed_mm_s = antplus_spdcad_speed_mm_s(s_prevSpdTime, spdTime, s_prevSpdRev, spdRev, 2096);
            float kmh = (float)speed_mm_s * 0.0036f;
            if (kmh <= 150.0f) {
                antSpeed = kmh;
                antSpeedMs = now;
            }
        }
        s_prevSpdTime = spdTime;
        s_prevSpdRev = spdRev;
        s_spdInit = true;
    } else if (rx->device_type == ANTPLUS_DEVTYPE_BIKE_CADENCE) {
        antCadDevId = rx->device_num;
        antCadRssi = rx->rssi;
        uint16_t cadTime = (uint16_t)page[4] | ((uint16_t)page[5] << 8);
        uint16_t cadRev  = (uint16_t)page[6] | ((uint16_t)page[7] << 8);
        if (s_cadInit) {
            uint16_t rpm = antplus_spdcad_rpm(s_prevCadTime, cadTime, s_prevCadRev, cadRev);
            if (rpm <= 250) {
                antCad = (int)rpm;
                antCadMs = now;
            }
        }
        s_prevCadTime = cadTime;
        s_prevCadRev = cadRev;
        s_cadInit = true;
    }
}

void antSensorsBegin() {
    ant_node_config_t cfg = {};
    cfg.on_data = on_ant_data;
    cfg.coexist = true; // Radio coexistence with BLE phone connection
    cfg.task_core = 0;  // Pin ANT TDMA protocol engine to Core 0

    ant_espphy_status_t status = ant_node_start(&s_antNode, &cfg);
    if (status == ANT_ESPPHY_OK) {
        antReady = true;
        // Open slave wildcard channels for HRM, Power, Speed/Cadence, Speed-only, Cadence-only
        ant_node_open_antplus_slave(&s_antNode, 0, ANTPLUS_DEVTYPE_HRM, 0, ANTPLUS_PERIOD_HRM);
        ant_node_open_antplus_slave(&s_antNode, 1, ANTPLUS_DEVTYPE_BIKE_POWER, 0, ANTPLUS_PERIOD_BIKE_POWER);
        ant_node_open_antplus_slave(&s_antNode, 2, ANTPLUS_DEVTYPE_BIKE_SPDCAD, 0, ANTPLUS_PERIOD_BIKE_SPDCAD);
        ant_node_open_antplus_slave(&s_antNode, 3, ANTPLUS_DEVTYPE_BIKE_SPEED, 0, 8118);
        ant_node_open_antplus_slave(&s_antNode, 4, ANTPLUS_DEVTYPE_BIKE_CADENCE, 0, 8102);
        Serial.println("ANT+: Node started on Core 0 with 5 wildcard channels (HRM, Power, Spd/Cad, Spd, Cad)");
    } else {
        Serial.printf("ANT+: Node start deferred/failed with code %d\n", (int)status);
    }
}

void antSensorsUpdate() {
    // Timeout stale readings after 4 seconds
    uint32_t now = millis();
    if (antHRMs && (now - antHRMs > 4000)) { antHR = 0; antHRMs = 0; antHrmRssi = 0; }
    if (antSpeedMs && (now - antSpeedMs > 4000)) { antSpeed = 0.0f; antSpeedMs = 0; antSpdRssi = 0; }
    if (antCadMs && (now - antCadMs > 4000)) { antCad = 0; antCadMs = 0; antCadRssi = 0; }
    if (antPowerMs && (now - antPowerMs > 4000)) { antPower = 0; antPowerMs = 0; antPowerRssi = 0; }
}

void antSearchAll() {
    if (!antReady) return;
    ant_node_pair(&s_antNode, 0, ANTPLUS_DEVTYPE_HRM, ANTPLUS_PERIOD_HRM);
    ant_node_pair(&s_antNode, 1, ANTPLUS_DEVTYPE_BIKE_POWER, ANTPLUS_PERIOD_BIKE_POWER);
    ant_node_pair(&s_antNode, 2, ANTPLUS_DEVTYPE_BIKE_SPDCAD, ANTPLUS_PERIOD_BIKE_SPDCAD);
    ant_node_pair(&s_antNode, 3, ANTPLUS_DEVTYPE_BIKE_SPEED, 8118);
    ant_node_pair(&s_antNode, 4, ANTPLUS_DEVTYPE_BIKE_CADENCE, 8102);
    Serial.println("ANT+: Wildcard search re-triggered on all 5 channels");
}
