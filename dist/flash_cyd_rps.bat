@echo off
setlocal
set PORT=%1
if "%PORT%"=="" set PORT=COM3

echo CYD_RPS v0.1.8 flasher - target port: %PORT%
python -m esptool --chip esp32 --port %PORT% --baud 921600 write_flash -z 0x10000 firmware.bin

if %errorlevel% neq 0 (
    echo CYD_RPS flash FAILED
    exit /b %errorlevel%
)

echo CYD_RPS flash SUCCESS
