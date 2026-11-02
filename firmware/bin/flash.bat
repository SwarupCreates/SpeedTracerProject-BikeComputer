@echo off
echo ========================================================
echo  SpeedTracer ESP32-S3 Firmware Flasher (Schematik.io Build)
echo ========================================================
echo.
echo Make sure your Waveshare ESP32-S3-Touch-LCD-2.8 is plugged in via USB Type-C.
echo If prompted, hold the BOOT button while plugging in.
echo.
set /p PORT="Enter your COM Port (e.g. COM3, COM4): "
echo.
echo Flashing bootloader, partitions, boot_app0, and firmware...
python -m esptool --chip esp32s3 -p %PORT% -b 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size 8MB 0x0 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin
echo.
echo ========================================================
echo Flashing complete! Reset your board to run SpeedTracer.
echo ========================================================
pause
