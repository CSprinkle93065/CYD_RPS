#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-/dev/ttyUSB0}"

echo "Flashing CYD_RPS v0.1.7 to ESP32 on ${PORT} ..."
python -m esptool --chip esp32 --port "${PORT}" --baud 921600 write_flash -z 0x10000 firmware.bin

echo "Flash SUCCESS"
