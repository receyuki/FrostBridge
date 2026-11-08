# Architecture and validation

[English](development.md) · [简体中文](../zh-CN/development.md) · [Contributing](../../CONTRIBUTING.md)

```text
Web API / HomeKit callbacks → bounded mailbox → one BLE worker
                                             → ControlRuntime / Controller → BLE → C15
                                             ← fresh protocol observations ← BLE
                                             → DailyReplay → verified dual-key NVS
```

Protocol decodes/encodes bounded known layouts and preserves non-target fields. Controller holds intended operations separately from observed device status, validates connection epoch/freshness, and requires new readback. ControlRuntime owns trusted millisecond time, automation completion and backup dispatch. HAP callbacks enqueue; no flash writes or blocking BLE operations run inside them. The mailbox publishes fixed summaries and eight-row pages, not whole daily tables in every HAP snapshot.

DailyReplay keeps the candidate in RAM and the sealed sample/config/override in an explicitly versioned CRC journal, two alternating keys. A newly complete empty day is valid. Known gaps keep the previous sample. Time is not persisted as if a cold boot had a reliable clock. Old weekly-storage bytes are kept, but production weekly API/editor/scheduler is removed.

## Build profiles and limits

`platformio.ini` pins Espressif32 8.0.0, Arduino package 4.30312.261024/core3.3.12 and ESP32 support3.3.12, HomeSpan2.1.8, NimBLE2.5.1, ArduinoJson7.4.3. Python setup pins PlatformIO6.2.0, PySerial3.5 and qrcode8.2. `requirements-tools.lock` is a fuller historical developer snapshot, not the minimal installation entry.

`gateway` is production with English accessory names by default; `gateway_zh` selects Chinese. `gateway_en` is an English compatibility alias. AIDs/service layout are unchanged. `basic`, `coexistence`, `readout` are diagnostics. Both production profiles use classic `esp32dev`, 4MB flash and `huge_app.csv`. There is no network OTA, PSRAM requirement or automatic application rollback.

The existing worker stack is 12KiB. Daily owner/store target ≤2304B and worker remaining reserve ≥2048B are resource goals; inspect real hardware paths and static frame differences, not just compilation. Web gzip ≤24KiB and neumorphic styling delta ≤1KiB are enforced before generated assets replace the tracked header. New translations run in the browser; they do not allocate a language table on the MCU heap.

## Reproduction

```sh
python3 frostbridge.py setup
python3 frostbridge.py build
python3 frostbridge.py test
python3 frostbridge.py build --homekit-language en
python3 frostbridge.py export --output .local/dist/FrostBridge-source.zip
```

Unzip the public source in a fresh directory and repeat setup/build there to catch accidental private-workspace dependencies. The source export is defined by public-files.txt, does not need Git, and excludes its history/internal records. Do not copy .venv/.platformio/private logs into the public package.

## Acceptance scope

Manual Web/Home power, ECO, positive/negative temperature, BLE contention/reconnect and independent USB power have prior physical evidence on one C15/ESP32. Host tests cover protocol, Controller, clock, daily rules/journal, runtime, HTTP/mailbox/API and SDK callback behavior. A test fixture representing a full day is not a real day of collection.

On 2026-11-08, the clean exported source independently installed its tools, built both production profiles and passed all 11 host suites on Intel macOS. Both Web languages passed isolated Chrome checks with simulated and real status. The public CLI updated an existing board while retaining its target, Wi-Fi, pairing and backup configuration. Its local QR page was independently decoded to the USB-derived URI; a new-device pairing was not repeated. Windows/Linux hardware workflows and the bilingual UI on iPhone Safari were not revalidated in this round.

Real full-day sample sealing/reboot loading, takeover/recovery with a real sample, natural HomePod voice recheck, remote HomeKit, update-interruption rescue and 48h stability remain separate pending acceptance. Prior low-heap lifecycle samples have an unresolved cause; a short newer run cannot erase them. New bilingual browser/clean-build/USB results should identify exactly the layer executed. GitHub Actions files are configuration until actually run on the host service.

The serial BLE_STATUS and FROST_BIND_SAVED/FAILED acknowledgements support independent onboarding without revealing binding identities on HTTP. Normal pairing uses USB-derived, locally rendered SVG QR; it never clears existing homes or uses an external QR service.

HTTP error messages are English; clients should use the stable error codes for localization. Web supplies optional Chinese translations. The optional Tk provisioning helper also defaults to English and accepts `--lang zh-CN`; it requires a Python installation with Tk, while normal terminal provisioning does not.

The later English-default change passed host, Chrome and both production-build checks. It was not reflashed on the physical board in this round; the Tk dialog's translated layout was source-checked without opening another provisioning window.
