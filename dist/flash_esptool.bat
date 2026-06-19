@echo off
setlocal
set PORT=%1
if "%PORT%"=="" set PORT=COM3

echo Flashing CYD_RPS v0.1.7 to ESP32 on %PORT% ...
python -m esptool --chip esp32 --port %PORT% --baud 921600 write_flash -z 0x10000 firmware.bin

if %errorlevel% neq 0 (
    echo Flash FAILED
    exit /b %errorlevel%
)

echo Flash SUCCESS
