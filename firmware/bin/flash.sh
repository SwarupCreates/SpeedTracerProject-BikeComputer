#!/usr/bin/env bash
set -e

echo "========================================================"
echo " SpeedTracer ESP32-S3 Firmware Flasher (Schematik.io Build)"
echo "========================================================"
echo ""

PORT="${1:-/dev/ttyACM0}"
echo "Using port: $PORT"
echo "Flashing bootloader, partitions, boot_app0, and firmware..."

esptool.py --chip esp32s3 -p "$PORT" -b 921600 --before default_reset --after hard_reset write_flash -z \
  --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 bootloader.bin \
  0x8000 partitions.bin \
  0xe000 boot_app0.bin \
  0x10000 firmware.bin

echo "========================================================"
echo "Flashing complete! Reset your board to run SpeedTracer."
echo "========================================================"
