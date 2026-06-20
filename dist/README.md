# CYD_RPS Firmware Release v0.1.8

**Project:** CYD_RPS  
**Version:** 0.1.8  
**Board:** CYD2USB_v3 (ESP32-2432S028R)  
**Build Environment:** esp32-2432s028r_cyd2usb  
**Workflow ID:** wvc_20260619_072600  
**Date:** 2026-06-20

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
- **Flash:** 74.0% (used 970,205 bytes from 1,310,720 bytes)
- **Firmware binary size:** 976,784 bytes

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
- v0.1.8 is a bug-fix revision. It addresses the two-board discovery race (`status=13` multiplayer connection timeout) documented in `docs/BugReport_CYD_RPS_v0.1.7.md`:
  - The JOIN keeps advertising for a bounded `kJoinDiscoveryWindowMs` (2 s) after role resolution before stopping advertising and calling `NimBLEClient::connect()`, giving the HOST time to discover the JOIN and stop scanning.
  - On each failed connect attempt (except the last) the JOIN restarts advertising for `kJoinConnectInterRetryWindowMs` (2 s) before retrying.
  - Advertising is stopped only immediately before each `connect()` call.
  - During peer search / role negotiation the JOIN advertises on a short 20–30 ms interval (`kPeerSearchAdvMinInterval`/`kPeerSearchAdvMaxInterval`) to maximize the probability that the HOST's scan window hits an advertisement within each bounded window.
