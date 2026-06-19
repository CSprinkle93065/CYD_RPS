# CYD_RPS Firmware Release v0.1.7

**Project:** CYD_RPS  
**Version:** 0.1.7  
**Board:** CYD2USB_v3 (ESP32-2432S028R)  
**Build Environment:** esp32-2432s028r_cyd2usb  
**Workflow ID:** wvc_20260619_134606  
**Date:** 2026-06-19

## Contents

| File | Description |
|------|-------------|
| `firmware.bin` | Compiled ESP32 application image |
| `platformio.ini` | PlatformIO project configuration used for this build |
| `flash_esptool.bat` | Windows batch flash helper using esptool.py |
| `flash_esptool.sh` | Linux/macOS bash flash helper using esptool.py |
| `flash_cyd_rps.bat` | Optional Windows helper with default COM port |
| `README.md` | This file |

## Build Metrics

- **RAM:** 23.2% (used 76,036 bytes from 327,680 bytes)
- **Flash:** 74.0% (used 969,973 bytes from 1,310,720 bytes)
- **Firmware binary size:** 976,544 bytes

## Flashing Instructions

### Windows (esptool.py)

```batch
flash_esptool.bat COM3
```

Or use the helper:

```batch
flash_cyd_rps.bat COM3
```

If no port is specified, the default `COM3` is used.

### Linux / macOS

```bash
./flash_esptool.sh /dev/ttyUSB0
```

If no port is specified, the default `/dev/ttyUSB0` is used.

### Workspace helper

From the repository root, you can also use the workspace flash helper:

```bash
python execution/flash_cyd2usb.py --project CYD_RPS --env esp32-2432s028r_cyd2usb --port COM3
```

If no `--env` is given, the helper uses the default environment defined in `platformio.ini`.

### PlatformIO

If you have the full project source, you can also flash with:

```bash
pio run --target upload -e esp32-2432s028r_cyd2usb --upload-port COM3
```

## Notes

- This binary is built for the physical CYD2USB v3 hardware.
- For Wokwi simulation, build with the `esp32-2432s028r_cyd2usb_wokwi` environment, which skips NimBLE initialization.
- v0.1.7 is a bug-fix revision. It stops JOIN advertising before initiating the connection to the HOST, reduces the per-attempt NimBLE connect timeout to 5 seconds, and restarts advertising if the JOIN connection retry loop exhausts, addressing the `status=13` multiplayer connection failure documented in `docs/BugReport_CYD_RPS_v0.1.6.md`.
