for path in ['firmware/src/quick_settings.h', 'schematikIO-Export/src/quick_settings.h']:
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    target = '      const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", "BMP390 Baro", "Buttons Expander", "SD Card FAT32"};\n      bool ok[] = {touchReady, gpsDataReady, compass.ready && (compass.lastSample == 0 || (millis() - compass.lastSample < 3000)), bmpOk, buttonsOk, sdOk};\n      for (int i = 0; i < 6; i++) {\n        label(14, 76 + i * 27, names[i]);\n        label(14, 88 + i * 27, ok[i] ? "READY" : "OFFLINE / NOT FOUND", 1, ok[i] ? 0x2564 : 0xFA08);\n      }'

    replacement = '      extern bool isBaroAvailable;\n      extern bool isBmp581;\n      const char* names[] = {"Touch CST3530", "GPS u-blox M10", "Compass QMC5883L", isBmp581 ? "BMP581 Baro" : "7Semi Baro (0x46)", "Buttons Expander", "SD Card FAT32"};\n      bool ok[] = {touchReady, gpsDataReady, compass.ready && (compass.lastSample == 0 || (millis() - compass.lastSample < 3000)), isBaroAvailable, buttonsOk, sdOk};\n      for (int i = 0; i < 6; i++) {\n        label(14, 76 + i * 27, names[i]);\n        label(14, 88 + i * 27, ok[i] ? "READY" : "OFFLINE / WAITING", 1, ok[i] ? 0x2564 : 0xFA08);\n      }'

    assert target in content, f'target not found in {path}'
    content = content.replace(target, replacement, 1)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f'Successfully updated {path}')
print('Done!')
