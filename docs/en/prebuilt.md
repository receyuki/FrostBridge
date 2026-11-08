# Install prebuilt FrostBridge

[English](prebuilt.md) · [简体中文](../zh-CN/prebuilt.md) · [Home](../../README.md)

Download an installation ZIP from [FrostBridge v0.1.0](https://github.com/receyuki/FrostBridge/releases/tag/v0.1.0). Choose `esp32-en.zip` for English HomeKit names and tool prompts, or `esp32-zh-CN.zip` for Chinese. Both include this guide and its Chinese translation; `--lang en` / `--lang zh-CN` overrides the host-tool language. Output from pip/esptool and technical command help remains upstream English.

These packages are for classic ESP32 boards with 4MB flash, including the tested Waveshare ESP32-DEV-KIT-WROOM-32E-N4. They are not for ESP32-C3/S3/S2. No compilation, PlatformIO, Node.js or AI assistant is needed. Install Python 3.10+ and use a USB data cable. Only Intel macOS has been physically validated so far.

## 1. Install the lightweight tools

Extract the whole ZIP and open a terminal in its directory:

```sh
python3 frostbridge.py setup
python3 frostbridge.py ports
```

Setup in this package automatically installs only esptool, PySerial, qrcode and their dependencies. Use `py -3` instead of `python3` on Windows. `PORT` below means your actual USB port, not the literal word PORT.

## 2. Install or update

For a new, dedicated ESP32:

```sh
python3 frostbridge.py install --port PORT
```

The initial install writes the bootloader, partition table, OTA metadata and application at their separate offsets. It does not write an NVS image or erase the entire chip. Back up an existing different project before repurposing its board; changing partitions does not guarantee its configuration will survive.

For an existing FrostBridge using this same partition layout:

```sh
python3 frostbridge.py install --port PORT --update
```

Update checks the existing partition table before writing only the application. It keeps Wi-Fi, the selected fridge, HomeKit pairing and backup settings. Use `--dry-run` to validate files and see the write command without opening USB. There is no network OTA or automatic rollback.

## 3. Configure your own device

For a first installation, run these commands in order:

```sh
python3 frostbridge.py wifi --port PORT
python3 frostbridge.py bind --port PORT
python3 frostbridge.py pair --port PORT
```

- Wi-Fi: enter your 2.4GHz SSID and hidden password. They are saved on your ESP32, not baked into the firmware or saved on the computer.
- Fridge: close the original app/nRF Connect, then select your own fridge from the live BLE list. No C15 name requirement or source edits. Only the author's C15 is physically verified; see [compatibility](compatibility.md) before controlling another model.
- Apple Home: a private local QR page opens on the computer. Scan it in iPhone/iPad Home → + → Add Accessory. The code belongs to this ESP32; there is no shared QR in the download. Existing paired devices do not need to be added again.

After a normal update, skip configuration that already works. `bind` keeps the existing selection unless explicitly asked to replace it; do not reset pairing to change accessory names.

Open **http://frostbridge.local**. Web defaults to English with an optional Chinese switch. This package's HomeKit naming profile is recorded in `firmware/manifest.json`; existing custom/cached Home names may stay unchanged. See [usage](usage.md) and [troubleshooting](troubleshooting.md).

## Package contents and scope

The package includes four checked firmware images, their offsets and SHA256 values in `firmware/manifest.json`, host tools, documentation, notices and a clean matching source ZIP. SHA256 detects file changes; it is not an authenticity signature. No Wi-Fi credentials, device binding or HomeKit identity is included. All installation steps are over USB; disconnect other serial monitors first.

This is the initial 0.1.0 release. Full-day backup learning/takeover, remote HomeKit and 48h stability still have separate pending acceptance. Blank-board first installation and Windows/Linux hardware flows have not been physically validated. The same Release includes corresponding library sources, SDK notices, pinned build information and rebuilding/relinking instructions in its `source-support.zip` asset. You only need that larger archive to inspect or modify dependencies, not for installation.
