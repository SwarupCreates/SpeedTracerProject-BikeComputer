#pragma once
#include <Arduino.h>
#include "ant_node.h"
#include "antplus_profiles.h"

// Volatile sensor values updated atomically from ANT TDMA task on Core 0
extern volatile int antHR;
extern volatile uint32_t antHRMs;
extern volatile float antSpeed;
extern volatile uint32_t antSpeedMs;
extern volatile int antCad;
extern volatile uint32_t antCadMs;
extern volatile int antPower;
extern volatile uint32_t antPowerMs;
extern volatile bool antReady;

// Learned / Paired device telemetry
extern volatile uint16_t antHrmDevId;
extern volatile int8_t antHrmRssi;
extern volatile uint16_t antSpdDevId;
extern volatile int8_t antSpdRssi;
extern volatile uint16_t antCadDevId;
extern volatile int8_t antCadRssi;
extern volatile uint16_t antPowerDevId;
extern volatile int8_t antPowerRssi;

void antSensorsBegin();
void antSensorsUpdate();
void antSearchAll();
