import os

def update_ble_sensors(path):
    if not os.path.exists(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    old_text = """static int bleHR=0;static bool bleHRReady=false,bleCadReady=false,bleSpeedReady=false,blePowerReady=false;
static float bleSpeed=0;static int bleCad=0,blePower=0;
static void bleReadings(){bleHRReady=bleCadReady=bleSpeedReady=blePowerReady=false;if(!bleMutex)return;xSemaphoreTake(bleMutex,portMAX_DELAY);bleCircum=cfg.wheelCircumM;
 for(auto&s:bleSlots)if(s.connected){if(bleFresh(s.hrMs)){bleHR=s.hr;bleHRReady=true;}if(bleFresh(s.cadMs)){bleCad=s.cad;bleCadReady=true;}if(bleFresh(s.speedMs)){bleSpeed=s.speed;bleSpeedReady=true;}if(bleFresh(s.powerMs)){blePower=s.power;blePowerReady=true;}}
 xSemaphoreGive(bleMutex);
}"""

    new_text = """#include "ant_sensors.h"

static int bleHR=0;static bool bleHRReady=false,bleCadReady=false,bleSpeedReady=false,blePowerReady=false;
static float bleSpeed=0;static int bleCad=0,blePower=0;
static void bleReadings(){
 bleHRReady=bleCadReady=bleSpeedReady=blePowerReady=false;
 if(bleMutex){
  xSemaphoreTake(bleMutex,portMAX_DELAY);bleCircum=cfg.wheelCircumM;
  for(auto&s:bleSlots)if(s.connected){if(bleFresh(s.hrMs)){bleHR=s.hr;bleHRReady=true;}if(bleFresh(s.cadMs)){bleCad=s.cad;bleCadReady=true;}if(bleFresh(s.speedMs)){bleSpeed=s.speed;bleSpeedReady=true;}if(bleFresh(s.powerMs)){blePower=s.power;blePowerReady=true;}}
  xSemaphoreGive(bleMutex);
 }
 antSensorsUpdate();
 if (antHRMs && (millis() - antHRMs < 5000) && antHR > 0) {
  bleHR = antHR;
  bleHRReady = true;
 }
 if (antSpeedMs && (millis() - antSpeedMs < 5000)) {
  bleSpeed = antSpeed;
  bleSpeedReady = true;
 }
 if (antCadMs && (millis() - antCadMs < 5000)) {
  bleCad = antCad;
  bleCadReady = true;
 }
 if (antPowerMs && (millis() - antPowerMs < 5000)) {
  blePower = antPower;
  blePowerReady = true;
 }
}"""

    content_lf = content.replace("\r\n", "\n")
    old_text_lf = old_text.replace("\r\n", "\n")
    new_text_lf = new_text.replace("\r\n", "\n")

    if old_text_lf in content_lf:
        content_lf = content_lf.replace(old_text_lf, new_text_lf)
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(content_lf.replace("\n", "\r\n"))
        print("Updated", path)
    else:
        print("old_text not found in", path)

def update_firmware_cpp(path):
    if not os.path.exists(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # 1. Add antSensorsBegin() in setup()
    old_setup = """  // 8. Start Bluetooth LE AFTER Wi-Fi & Display are up and running!
  bleBegin();
  bleHttp();
}"""

    new_setup = """  // 8. Start Bluetooth LE AFTER Wi-Fi & Display are up and running!
  bleBegin();
  bleHttp();
  antSensorsBegin();
}"""

    # 2. Compass rate limit update in readSensors()
    old_compass = """  geprcGps.poll(gpsSerial);
  compass.update();"""

    new_compass = """  geprcGps.poll(gpsSerial);
  bool compassScreenActive = (welcomeOpen || quickOpen) && (homePage == HOME_COMPASS);
  compass.update(compassScreenActive);"""

    content_lf = content.replace("\r\n", "\n")
    old_setup_lf = old_setup.replace("\r\n", "\n")
    new_setup_lf = new_setup.replace("\r\n", "\n")
    old_compass_lf = old_compass.replace("\r\n", "\n")
    new_compass_lf = new_compass.replace("\r\n", "\n")

    if old_setup_lf in content_lf:
        content_lf = content_lf.replace(old_setup_lf, new_setup_lf)
    else:
        print("old_setup not found in", path)

    if old_compass_lf in content_lf:
        content_lf = content_lf.replace(old_compass_lf, new_compass_lf)
    else:
        print("old_compass not found in", path)

    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(content_lf.replace("\n", "\r\n"))
    print("Updated", path)

update_ble_sensors("firmware/src/ble_sensors.h")
update_ble_sensors("schematikIO-Export/src/ble_sensors.h")
update_firmware_cpp("firmware/src/firmware.cpp")
update_firmware_cpp("schematikIO-Export/src/firmware.cpp")
