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

void antSensorsBegin();
void antSensorsUpdate();
