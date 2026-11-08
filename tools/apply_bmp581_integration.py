import os

def update_firmware_cpp(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Add #include <SparkFun_BMP581_Arduino_Library.h>
    target_inc = '#include "map_tile_cache.h"\nMapTileCache g_mapCache;\nAdafruit_MCP23X17 buttons;\nAdafruit_BMP3XX bmp;\n'
    replacement_inc = '#include "map_tile_cache.h"\n#include <SparkFun_BMP581_Arduino_Library.h>\nMapTileCache g_mapCache;\nAdafruit_MCP23X17 buttons;\nAdafruit_BMP3XX bmp;\nBMP581 bmp581;\nbool isBaroAvailable = false;\nbool isBmp581 = false;\n'

    if target_inc in content:
        content = content.replace(target_inc, replacement_inc)
        print(f'{path}: added BMP581 header & instance')
    else:
        print(f'{path}: target_inc not found')

    # 2. Update beginBarometer()
    target_begin = '''static bool beginBarometer(){
  bool ok = false;
  if(externalPresent(0x77)) ok = bmp.begin_I2C(0x77, &Wire);
  else if(externalPresent(0x76)) ok = bmp.begin_I2C(0x76, &Wire);
  if(ok){
    bmp.setTemperatureOversampling(BMP3_NO_OVERSAMPLING);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    Serial.println("BMP3XX Barometer initialized successfully!");
    return true;
  }
  return false;
}'''

    replacement_begin = '''static bool beginBarometer(){
  bool ok = false;

  // 1. Scan for 7Semi BMP581 at 0x46 (SDO->GND) or 0x47 (SDO->3V3/float)
  if (externalPresent(BMP581_I2C_ADDRESS_SECONDARY)) { // 0x46
    if (bmp581.beginI2C(BMP581_I2C_ADDRESS_SECONDARY, Wire) == BMP5_OK) {
      isBmp581 = true;
      ok = true;
    }
  } else if (externalPresent(BMP581_I2C_ADDRESS_DEFAULT)) { // 0x47
    if (bmp581.beginI2C(BMP581_I2C_ADDRESS_DEFAULT, Wire) == BMP5_OK) {
      isBmp581 = true;
      ok = true;
    }
  }

  if (ok && isBmp581) {
    // 7Semi BMP581 Precision Cycling Configuration:
    // 1. Set Oversampling to 8x pressure, 2x temperature (Balances high accuracy with fast response)
    bmp5_osr_odr_press_config osrConfig = {
      .osr_t = BMP5_OVERSAMPLING_2X,
      .osr_p = BMP5_OVERSAMPLING_8X,
      0, 0
    };
    bmp581.setOSRMultipliers(&osrConfig);

    // 2. Set Output Data Rate (ODR) to 50Hz
    bmp581.setODRFrequency(BMP5_ODR_50_HZ);

    // 3. Set IIR Filter Coefficient to 3 (Dampens slight air pressure vibrations)
    bmp5_iir_config iirConfig = {
      .set_iir_t = BMP5_IIR_FILTER_COEFF_3,
      .set_iir_p = BMP5_IIR_FILTER_COEFF_3,
      .shdw_set_iir_t = BMP5_ENABLE,
      .shdw_set_iir_p = BMP5_ENABLE,
      .iir_flush_forced_en = BMP5_DISABLE
    };
    bmp581.setFilterConfig(&iirConfig);

    Serial.println("7Semi BMP581 Barometer initialized successfully (8x OSR, IIR 3, 50Hz)!");
    isBaroAvailable = true;
    bmpOk = true;
    return true;
  }

  // 2. Fallback to BMP3XX (0x77 / 0x76) if present
  if(externalPresent(0x77)) ok = bmp.begin_I2C(0x77, &Wire);
  else if(externalPresent(0x76)) ok = bmp.begin_I2C(0x76, &Wire);
  if(ok){
    bmp.setTemperatureOversampling(BMP3_NO_OVERSAMPLING);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    Serial.println("BMP3XX Barometer initialized successfully!");
    isBmp581 = false;
    isBaroAvailable = true;
    bmpOk = true;
    return true;
  }

  isBmp581 = false;
  isBaroAvailable = false;
  bmpOk = false;
  return false;
}

float getSystemAltitude() {
  return ride.altM;
}

float calculateGradePercentage() {
  if (!isBaroAvailable) {
    return 0.0f; // Return flat grade state on UI layout until hardware is active
  }
  return ride.gradePercent;
}'''

    if target_begin in content:
        content = content.replace(target_begin, replacement_begin)
        print(f'{path}: updated beginBarometer')
    else:
        print(f'{path}: target_begin not found')

    # 3. Update JSON telemetry & status
    old_doc = 'doc["barometerReady"]=bmpOk;'
    new_doc = 'doc["barometerReady"]=isBaroAvailable; doc["barometerType"]=isBmp581 ? "BMP581" : (bmpOk ? "BMP390" : "none");'
    if old_doc in content:
        content = content.replace(old_doc, new_doc)
        print(f'{path}: updated JSON doc barometerReady')

    # 4. Update setup() beginBarometer call
    old_setup_bmp = 'bmpOk = beginBarometer();'
    new_setup_bmp = 'bmpOk = isBaroAvailable = beginBarometer();'
    if old_setup_bmp in content:
        content = content.replace(old_setup_bmp, new_setup_bmp)
        print(f'{path}: updated setup bmpOk call')

    # 5. Update I2C scan debug print
    old_scan_print = 'Serial.printf("bmpOk: %d\\n", (int)bmpOk);'
    new_scan_print = 'Serial.printf("baroOk: %d (isBmp581:%d, bmpOk:%d)\\n", (int)isBaroAvailable, (int)isBmp581, (int)bmpOk);'
    if old_scan_print in content:
        content = content.replace(old_scan_print, new_scan_print)
        print(f'{path}: updated scan print')

    # 6. Update loop() reading logic
    target_loop_read = '''  // Barometer reading + GPS pressure calibration (iPhone-grade fusion)
  if(bmpOk && millis()-sensorMs>500){
    sensorMs=millis();
    if(bmp.performReading()&&isfinite(bmp.pressure)&&bmp.pressure>=30000&&bmp.pressure<=125000){
      // Continuous slow drift calibration once baseline is established or when GPS is exceptionally solid
      if (ride.gpsFixed && gps.altitude.isValid() && gps.altitude.age() < 2500 && startSatellites() >= 8 && ((gps.hdop.isValid() && gps.hdop.hdop() < 1.2f) || (geprcGps.lastPvt && geprcGps.pdop < 1.6f))) {
        float gpsAlt = (float)gps.altitude.meters();
        if (gpsAlt > -200.0f && gpsAlt < 9000.0f) {
          float pTarget = bmp.pressure / powf(1.0f - (gpsAlt / 44330.0f), 5.255f);
          if (pTarget >= 87000.0f && pTarget <= 112000.0f) {
            dynamicSeaLevelPa = (0.998f * dynamicSeaLevelPa) + (0.002f * pTarget);
            altCalibrated = true;
          }
        }
      }
      // Use the dynamically calibrated sea level pressure instead of hardcoded default
      float baroAlt = 44330.0f * (1.0f - powf(bmp.pressure / dynamicSeaLevelPa, 0.1903f));
      ride.altM = baroAlt;
    } else {
      bmpOk=false;
      uiRevision++;
    }
  }

  // Periodic sensor probe (every 5 seconds)
  static uint32_t retryMs=0;
  if(millis()-retryMs>5000){
    retryMs=millis();
    if(!bmpOk)bmpOk=beginBarometer();'''

    replacement_loop_read = '''  // Barometer reading + GPS pressure calibration (iPhone-grade fusion with 7Semi BMP581 & BMP3XX support)
  if(isBaroAvailable && millis()-sensorMs>100){
    sensorMs=millis();
    float currentPressure = 0.0f;
    bool readSuccess = false;

    if (isBmp581) {
      bmp5_sensor_data bData = {0, 0};
      if (bmp581.getSensorData(&bData) == BMP5_OK && isfinite(bData.pressure) && bData.pressure >= 30000.0f && bData.pressure <= 125000.0f) {
        currentPressure = bData.pressure;
        readSuccess = true;
      }
    } else {
      if(bmp.performReading() && isfinite(bmp.pressure) && bmp.pressure >= 30000.0f && bmp.pressure <= 125000.0f){
        currentPressure = bmp.pressure;
        readSuccess = true;
      }
    }

    if(readSuccess){
      // Continuous slow drift calibration once baseline is established or when GPS is exceptionally solid
      if (ride.gpsFixed && gps.altitude.isValid() && gps.altitude.age() < 2500 && startSatellites() >= 8 && ((gps.hdop.isValid() && gps.hdop.hdop() < 1.2f) || (geprcGps.lastPvt && geprcGps.pdop < 1.6f))) {
        float gpsAlt = (float)gps.altitude.meters();
        if (gpsAlt > -200.0f && gpsAlt < 9000.0f) {
          float pTarget = currentPressure / powf(1.0f - (gpsAlt / 44330.0f), 5.255f);
          if (pTarget >= 87000.0f && pTarget <= 112000.0f) {
            dynamicSeaLevelPa = (0.998f * dynamicSeaLevelPa) + (0.002f * pTarget);
            altCalibrated = true;
          }
        }
      }
      // Use the dynamically calibrated sea level pressure
      float baroAlt = 44330.0f * (1.0f - powf(currentPressure / dynamicSeaLevelPa, 0.1903f));
      ride.altM = baroAlt;
    } else {
      isBaroAvailable = false;
      bmpOk = false;
      uiRevision++;
    }
  }

  // Periodic sensor probe (every 5 seconds)
  static uint32_t retryMs=0;
  if(millis()-retryMs>5000){
    retryMs=millis();
    if(!isBaroAvailable) isBaroAvailable = beginBarometer();'''

    if target_loop_read in content:
        content = content.replace(target_loop_read, replacement_loop_read)
        print(f'{path}: updated loop reading logic')
    else:
        print(f'{path}: target_loop_read not found')

    # 7. Update fallback check
    old_fallback = 'if(!bmpOk || runningMode) {'
    new_fallback = 'if(!isBaroAvailable || runningMode) {'
    if old_fallback in content:
        content = content.replace(old_fallback, new_fallback)
        print(f'{path}: updated fallback check')

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

def update_quick_settings_h(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # In System Tab, update hardware names and ok array
    target_detail = '''    const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", "BMP390 Baro", "Buttons Expander", "SD Card FAT32"};
    bool ok[] = {touchReady, gpsDataReady, compass.ready && (compass.lastSample == 0 || (millis() - compass.lastSample < 3000)), bmpOk, buttonsOk, sdOk};
    for (int i = 0; i < 6; i++) {
      label(14, 76 + i * 27, names[i]);
      label(14, 88 + i * 27, ok[i] ? "READY" : "OFFLINE / NOT FOUND", 1, ok[i] ? 0x2564 : 0xFA08);
    }'''

    replacement_detail = '''    extern bool isBaroAvailable;
    extern bool isBmp581;
    const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", isBmp581 ? "BMP581 Baro" : "7Semi Baro (0x46)", "Buttons Expander", "SD Card FAT32"};
    bool ok[] = {touchReady, gpsDataReady, compass.ready && (compass.lastSample == 0 || (millis() - compass.lastSample < 3000)), isBaroAvailable, buttonsOk, sdOk};
    for (int i = 0; i < 6; i++) {
      label(14, 76 + i * 27, names[i]);
      label(14, 88 + i * 27, ok[i] ? "READY" : "OFFLINE / WAITING", 1, ok[i] ? 0x2564 : 0xFA08);
    }'''

    if target_detail in content:
        content = content.replace(target_detail, replacement_detail)
        print(f'{path}: updated System Tab hardware names')
    else:
        print(f'{path}: target_detail not found')

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

update_firmware_cpp('firmware/src/firmware.cpp')
update_firmware_cpp('schematikIO-Export/src/firmware.cpp')
update_quick_settings_h('firmware/src/quick_settings.h')
update_quick_settings_h('schematikIO-Export/src/quick_settings.h')
print('Finished applying BMP581 integration!')
