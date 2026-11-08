# Source support and rebuilding

[English](rebuilding.md) · [简体中文](../zh-CN/rebuilding.md) · [Home](../../README.md)

This companion archive provides the corresponding Arduino-ESP32 library sources, the complete FrostBridge work that uses them, other direct library sources, SDK source archives/notices and exact build metadata for the binaries in the same Release. It is optional for normal installation. No toolchain executables or personal device configuration are included.

Arduino-ESP32 is used under LGPL-2.1-or-later; its license is included in `libraries/arduino-esp32/LICENSE.md`. The project imposes no restriction on modifying the library or on reverse engineering to debug those modifications. Other components retain their own license terms. See `third_party/` and each library's file-level notices. SDK Wi-Fi/PHY/Bluetooth/coexistence binary components retain Espressif's terms and are intended for Espressif devices.

## Rebuild the application with a modified library

1. Extract `FrostBridge-source.zip` into a separate directory and run `python3 frostbridge.py setup` there. Then run `python3 frostbridge.py build` once to obtain the pinned SDK/toolchain packages.
2. The actual Arduino library source used is supplied in `libraries/arduino-esp32`. Modify the copy under your extracted project's `.platformio/packages/framework-arduinoespressif32/cores`, `libraries` or `variants`, or copy the matching source directories from this archive there before modifying them. Keep the installed package metadata and build tools. Direct dependencies HomeSpan, NimBLE-Arduino and ArduinoJson are also provided under `libraries/`; their installed build copies are in `.pio/libdeps/gateway/` or `.pio/libdeps/gateway_zh/`.
3. Force a rebuild so the modified source is compiled and relinked:

```sh
# macOS/Linux; Windows equivalent uses .venv\Scripts\python.exe
./scripts/pio run -e gateway -t clean
python3 frostbridge.py build
```

Set `PLATFORMIO_CORE_DIR` to your project's `.platformio` directory when invoking PlatformIO directly; on macOS/Linux, `./scripts/pio run -e gateway -t clean` sets it automatically. To rebuild Chinese HomeKit names, clean `gateway_zh` and run `python3 frostbridge.py build --homekit-language zh-CN` instead. The resulting application is `.pio/build/ENVIRONMENT/firmware.bin`. The normal source `flash` command builds and flashes your modified application over USB. No signing key, online approval or anti-modification check is required.

The full work is supplied as source, so a user can recompile and relink rather than needing the original builder's object files. `BUILD-INFO.json`, `sdk-build/versions.txt`, SDK build flags and the pinned PlatformIO configuration identify the inputs. Prefix maps remove the builder's private paths; byte-for-byte reproduction across different hosts/timestamps is not promised.

## SDK materials

`sdk-sources/` includes the exact ESP-IDF commit archive, the Arduino library-builder source revision and the relevant mbedTLS/lwIP/TLSF/mDNS source archives. Git submodule pins and upstream notices remain with their respective sources; the complete SDK tool packages can be fetched by the pinned project setup. `sdk-sources/sdk-materials.json` records Espressif binary-component revisions and notice paths. This inventory is build support, not a claim that every SDK component is linked into FrostBridge.

The archive's `SHA256SUMS.txt` covers its files. `BUILD-INFO.json` records the two release image hashes. Installing or rebuilding does not require device credentials from the maintainer.
