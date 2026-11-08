# Troubleshooting

[English](troubleshooting.md) · [简体中文](../zh-CN/troubleshooting.md) · [Setup](getting-started.md)

| Symptom | Check / next step |
|---|---|
| No serial port | Use a USB data cable, another port, and the board manufacturer's driver. Linux may need udev/group permissions. Run `python3 frostbridge.py ports`. |
| Port busy / upload fails | Close monitors, IDE serial consoles and provisioning windows. Check BOOT/EN procedure for your board. Select the actual port explicitly. |
| Missing Python modules | Run `python3 frostbridge.py setup`; do not mix another project's virtualenv with this one. `doctor` checks tools without opening USB. |
| First build download is slow / incomplete | Toolchains require several GB of disk space and Internet access. PlatformIO can try another package mirror after a failed download. If it exits with an error, fix the connection/free space and rerun the same build command; no developer cache or board erasure is needed. |
| Build succeeds but only smoke output | Use the gateway/gateway_zh environment. `basic`, `coexistence`, `readout` are diagnostics, not the daily-control product. |
| Wi-Fi not connected | Confirm 2.4GHz personal Wi-Fi, SSID/password byte limits and same-network access. Use `wifi`; to correct only SSID, use `--keep-password`. Do not send passwords in issue reports. |
| frostbridge.local does not resolve | Find the current ESP32 IPv4 in the router and use `http://IP`. mDNS may be blocked on guest/VLAN networks. |
| Browser tries HTTPS | Enter the explicit `http://` address, then bookmark/add to home screen. This firmware does not provide TLS. |
| Fridge absent / stale | Power/range, factory app/nRF Connect holding BLE, selected target and its actual protocol. Keep old readings marked stale. `bind` preserves an existing target; `--replace` is explicit. |
| No fresh state after bind | A matching advertised service does not prove support. Check device model/firmware and local logs; no cooling setting is sent by bind. |
| Pair QR page not opened | Use the local HTML path printed by `pair`, or run without `--no-open` on a desktop. Scan from the iPhone/iPad **Home app**, not the Wi-Fi settings screen. Manual setup-code entry is supported. |
| Accessory not found | Complete Wi-Fi first; put iPhone/iPad, gateway and home hub on the same reachable LAN, with Bluetooth enabled. Check multicast isolation. |
| Accessory already in another home | Remove it from the home that owns it if you still have access. Reading the setup code does not clear a home. Do not erase Wi-Fi/fridge configuration as a first step. |
| HomePod says no response after a change | Check fresh device state and Web/Home errors. Publication/timing fixes exist, but real voice regression acceptance remains pending. Avoid repeatedly cycling the fridge to reproduce it. |
| Backup has no sample | It starts off; enable explicitly. The partial enabling day is excluded. Wait for a full continuous UTC+8 calendar day; inspect gap reason. |
| Sample is old | Known gaps retain the last valid sample. The UI shows date/age and last update failure; do not interpret an old sample as yesterday's schedule. |
| Empty sample | A complete day with no automation actions replaces the old sample. No backup actions are issued. |
| Waiting for time after power loss | There is no battery RTC. Wait for NTP, or set time from an accurately synchronized phone/computer. |
| Save not confirmed | Refresh and inspect actual settings before another explicit preview/save. A failed readback may still have committed flash; the tool does not claim otherwise. |

## Private diagnostics

`python3 frostbridge.py status --host LOCAL_HOST_OR_IP` is read-only HTTP. `monitor --port PORT` opens USB, may reboot ESP32 and may expose private logs; Ctrl+] exits. Do not open it during a natural-day recording you need to preserve.

The monitor accepts physical USB commands at 115200 baud. `\t` below means a real Tab, not two characters:

- `IP`: current local IP. `BLE_STATUS`: selected/connected/fresh flags, no address.
- `PAIR`: private code/URI; normal users should use the wrapper to generate the local QR page.
- `RESOURCE_STATS`: heap/stack/NVS capacity. `BACKUP_STATUS`: backup summary.
- `BACKUP_STORE_CHECK`: maximum-size diagnostic in its own temporary namespace, not a sample injection.
- `BLE_DISCONNECT`: stop BLE until selected/restarted; does not change fridge settings. `BLE_LIST`/`BLE_TARGET\tINDEX` are low-level same-session selection commands; prefer the wrapper.
- `CLOCK\tSECONDS` and `CTRL_TEMP\tVALUE`, `CTRL_MODE\t0_OR_1`, `CTRL_POWER\t0_OR_1` are advanced time/control requests, not general diagnostics. Use normal Web controls instead.

For failed first pairing **only when the controller list is empty**, the firmware has a guarded physical `HAP_NEW_ID` recovery command. It is not a general “reset my home” feature and is rejected while paired. Prefer the normal Home removal procedure and [Apple's troubleshooting](https://support.apple.com/en-us/126198). Never publish raw `HAP_INFO`, pairing files or flash backups.

## Updating and USB recovery

Normal updates repeat the same-language `flash` command. There is one application slot: no network OTA and no automatic rollback. If upload is interrupted, return the board to its documented USB download mode and flash a known build using the same partition layout. An interrupted-update rescue drill is not yet physically accepted by this project.

A full flash backup may contain credentials. Restoring an unrelated backup/partition can replace your settings; the wrapper intentionally has no automatic full-flash erase or restore command.
