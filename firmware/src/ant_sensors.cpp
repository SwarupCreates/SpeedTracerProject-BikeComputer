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

static ant_node_t s_antNode;
static uint16_t s_prevCadTime = 0, s_prevCadRev = 0;
static uint16_t s_prevSpdTime = 0, s_prevSpdRev = 0;
static bool s_cadInit = false, s_spdInit = false;

static void on_ant_data(ant_node_t *node, const ant_node_rx_t *rx, const uint8_t page[8], void *user) {
    if (!rx || !page) return;
    uint32_t now = millis();

    if (rx->device_type == ANTPLUS_DEVTYPE_HRM) {
        antplus_hrm_data_t hr;
        if (antplus_hrm_decode(page, &hr) && hr.computed_heart_rate > 0 && hr.computed_heart_rate <= 250) {
            antHR = (int)hr.computed_heart_rate;
            antHRMs = now;
        }
    } else if (rx->device_type == ANTPLUS_DEVTYPE_BIKE_POWER) {
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
                // Default circumference ~2096mm
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
        // Open slave wildcard channels for HRM, Power, Speed/Cadence
        ant_node_open_antplus_slave(&s_antNode, 0, ANTPLUS_DEVTYPE_HRM, 0, ANTPLUS_PERIOD_HRM);
        ant_node_open_antplus_slave(&s_antNode, 1, ANTPLUS_DEVTYPE_BIKE_POWER, 0, ANTPLUS_PERIOD_BIKE_POWER);
        ant_node_open_antplus_slave(&s_antNode, 2, ANTPLUS_DEVTYPE_BIKE_SPDCAD, 0, ANTPLUS_PERIOD_BIKE_SPDCAD);
        Serial.println("ANT+: Node started on Core 0 with channels for HRM (120), Power (11), Spd/Cad (121)");
    } else {
        Serial.printf("ANT+: Node start deferred/failed with code %d\n", (int)status);
    }
}

void antSensorsUpdate() {
    // Timeout stale readings after 4 seconds
    uint32_t now = millis();
    if (antHRMs && (now - antHRMs > 4000)) { antHR = 0; antHRMs = 0; }
    if (antSpeedMs && (now - antSpeedMs > 4000)) { antSpeed = 0.0f; antSpeedMs = 0; }
    if (antCadMs && (now - antCadMs > 4000)) { antCad = 0; antCadMs = 0; }
    if (antPowerMs && (now - antPowerMs > 4000)) { antPower = 0; antPowerMs = 0; }
}
