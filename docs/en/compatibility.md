# Fridge compatibility

[简体中文](../zh-CN/compatibility.md) · [Setup](getting-started.md) · [Home](../../README.md)

FrostBridge discovers devices by Bluetooth, **not by a list of commercial model names**. You do not need to rename your fridge to C15 or edit a Bluetooth address in source. Sharing a brand, app or Bluetooth service is a useful lead, not proof of compatibility.

| Device/protocol | Current status |
|---|---|
| Author's C15, one temperature zone | Power, temperature and ECO/MAX verified against the panel/original app. |
| Another single-zone fridge using the same Alpicool-style protocol | Candidate for the checks below; not yet physically verified by this project. |
| Two-zone fridge | A single-zone state/control interface exists; **independent second-zone control is not implemented**. Do not treat both zones as supported. |
| Different GATT service, framing or settings layout | Requires protocol investigation and implementation, not a model-name setting. |

## Try an existing protocol-compatible fridge

1. Follow [setup](getting-started.md). Note the exact fridge model, number of zones and factory app/version. Select Celsius on the fridge/app, then disconnect/quit the factory app and nRF Connect to release Bluetooth.
2. Run `python3 frostbridge.py bind --port PORT`. Select **your own** fridge from the current scan. It lists named devices or devices advertising service `1234`; not every listed device is compatible. Use `--replace` only when intentionally changing an existing target. When reusing a configured gateway, pause the old fridge's Home automations and daily backup before replacing it so they do not interrupt verification of the new device.
3. Open the webpage or run `python3 frostbridge.py status`. Compare power, ECO/MAX, units, target and current temperature with the panel/app. Make one normal, visible change using the panel/app, release its Bluetooth connection and compare a second set of readings. A saved target or a plausible temperature alone does not complete this check.
4. If both state comparisons agree, test the controls you need individually. Choose a clearly distinguishable temperature within your fridge's normal range, check its panel and fresh readback, then check ECO/MAX and power during normal use. Record each result separately; verify unrelated settings remain unchanged. No repeated power cycling is needed.

An unsupported-layout error for power/ECO can coexist with working temperature readings or temperature adjustment. Report that combination rather than treating all controls as compatible. Stop testing a control if the displayed state is wrong or an unrelated setting changes. There is no extra unlock command to bypass an unsupported layout.

## What the implementation actually supports

The pinned [BrassMonkey reference](https://github.com/klightspeed/BrassMonkeyFridgeMonitor/tree/11f185eff808d6aae79eba247f826a52ed093fc0) accepts a Bluetooth address; it has no commercial-model database. Its service/command/notification UUIDs are `1234`/`1235`/`1236` under the Bluetooth base UUID. Its optional protocol `--bind` command is **different** from FrostBridge's local device-selection command. [Reference CLI](https://github.com/klightspeed/BrassMonkeyFridgeMonitor/blob/11f185eff808d6aae79eba247f826a52ed093fc0/fridge.py#L589-L668)

FrostBridge validates framing, length and checksum, then decodes the common Query01 prefix into **one** temperature zone. Celsius controls use the intersection of the reported device range and −20…20°C. Target temperature uses command `05`; it does not currently distinguish layouts by protocol model. Power/ECO use Set02, implemented for a 24-byte status frame or a 36-byte frame whose payload `D[28]` is `1`, preserving the other settings from a fresh snapshot. Other Set02 projections are not implemented. [Parser/encoder](../../src/protocol/AlpicoolProtocol.cpp)

The protocol's `model` field is not the product model printed on the fridge. A longer frame is not sufficient evidence of two zones: the author's single-zone C15 also returns 36 bytes. Newer [model-specific source](https://github.com/dandwhelan/Alpicool50l12vfridgefreezer/blob/3f7ec01eced922aae431bb90e9c92ac9a60bb0c2/index.html#L454-L465) is a development lead, not a promise of support in FrostBridge.

## If it needs developer help

Provide a **sanitized compatibility report** with:

- Exact model, one/two zones, factory app and version, ESP32 board, FrostBridge source revision and which functions worked/failed.
- GATT service/characteristic UUIDs and properties, plus the complete query responses with notification boundaries and times. `status` is a decoded snapshot, **not a raw protocol capture**.
- Two sets of panel/app readings and the corresponding decoded/raw readings; name the field changed between them. Include any unsupported-layout/error code.

`python3 frostbridge.py monitor --port PORT` can capture existing USB diagnostics (Ctrl+] exits). Opening USB may restart ESP32. Raw logs can contain addresses and private setup data: keep them locally, remove addresses, pairing codes/URIs, Wi-Fi details and unrelated devices before sharing. Read [Security](../../SECURITY.md). For query failures, nRF Connect can supply the GATT view; developers should agree a known query/capture procedure before asking you to transmit anything else.

Maintainers can implement and test parsers/encoders without owning every model, but the device owner must verify actual readback and panel behavior. New layouts need traceable samples, parser tests and field-preserving encoding. A second zone also needs state, Controller, Web and HomeKit changes. Record support **per model and function** after that validation; do not infer a whole brand from one successful device.
