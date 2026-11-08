# Daily use

[English](usage.md) · [简体中文](../zh-CN/usage.md) · [Home](../../README.md)

## Web controls

Open **http://frostbridge.local** on the same LAN. The default view fits daily controls on one screen: actual box temperature, observed target, temperature entry/slider, ECO/MAX and power. Release the slider to submit, or type a whole number and tap Apply. The effective allowed range also follows fresh device limits.

A queued request is not success. Wait for **Confirmed by device**. A temperature target is not proof the box has reached it; logical power/cooling state is not proof the compressor is running. Disconnected or stale state retains its last values but disables controls. Important HomeKit/backup errors remain visible when More is collapsed.

The first visit defaults to English even in a Chinese-language browser. Use the EN/中 button to select Chinese or English. The browser remembers your selection when local storage is available. It does not send a control command, change the timezone or rename Apple Home accessories. More contains daily backup, HomeKit, activity and connection details.

## Apple Home names and automation inputs

| English (default) | Chinese (optional) | Purpose |
|---|---|---|
| Fridge Power | 冰箱电源 | Manual logical on/off |
| Fridge ECO | 冰箱节能 | On = ECO, off = MAX |
| Fridge Temperature | 冰箱温控 | Manual Celsius target and logical active state |
| Automation Power | 自动化电源 | Fixed-time automation input |
| Automation ECO | 自动化节能 | Fixed-time automation input |
| Automation Temperature | 自动化温控 | Fixed-time automation temperature input |
| Connectivity Probe | 连通检测 | Programmable Single Press event |
| Probe Reply | 检测回应 | Round-trip reply; resets automatically |

Choose names at firmware build/flash time; keep the same profile during updates of an existing home. You may rename accessories in Apple Home. The Web language switch never silently changes those names.

For daily learning, use the **Automation** accessories only in fixed-time automations. Ordinary Web/Home/USB controls and comparable panel changes establish manual override, not a learning sample. Location and external-sensor rules remain HomeKit's responsibility.

In Apple Home, configure **Connectivity Probe → Single Press → Probe Reply → On**. The reply does not operate the fridge. The gateway emits a probe about every 60s, accepts a reply within 15s, and keeps a healthy lease for 180s. A secure TCP connection alone is not evidence that automations work.

## Experimental daily backup

Software and short integration checks pass; real complete-day learning, takeover/recovery and 48h acceptance remain pending. A new installation leaves learning/backup **off**.

To enable: More → Daily backup → Enable learning and backup → Preview & save → review the effect → Save settings. Enabling ends the previous manual override and may align the current profile if a valid sample already exists and backup needs to take over. Disabling stops learning/replay without turning the fridge off. Saving settings and confirming device actions are separate outcomes.

Time is always **Beijing time (UTC+8)**, independent of UI language. The latest complete calendar day is retained; this is not a rolling 24-hour window or an export of Home app rules. The first partial day is excluded. For example, enabling Monday afternoon means the first eligible day is Tuesday, sealed at Wednesday 00:00 if coverage stays complete.

- Only actually received, device-confirmed fixed-time automation components are recorded, up to 64 temperature/power/mode actions per day.
- A known recording, network, probe, fridge-reading, clock or command-failure gap makes a day incomplete. Keep the previous valid sample and show its date and the update failure reason.
- A complete day with no actions is a valid empty sample, replacing the old one; backup then makes no changes.
- Candidate records are held in RAM. Rebooting makes that current day partial; the saved sample survives. Reopening USB can cause such a reboot.

Once a sample exists, an expired health lease can trigger backup. Cold boot allows an initial probe grace period. No trusted time, no valid sample or an effective manual override means no automatic change. Takeover/reconnect aligns the current cyclic profile, rather than replaying all missed on/off history. A healthy reply returns control to HomeKit.

Manual override persists across reconnect/reboot until the next known sample action boundary or a later admitted and confirmed automation action. If no boundary/time is known, retain it until one becomes known; do not guess expiration. The Web shows sample age/date, today's gap, override and current owner. Records are paginated eight at a time; if their version changes, reload rather than combining pages from different samples.

Continuous power allows offline timekeeping. After complete power loss there is no battery-backed RTC: wait for network sync or use Set time from this device after checking your phone/computer clock. A failed action is not displayed as confirmed; after reconnect, the gateway recomputes the current goal.

## Power, access and history

ESP32 can use an ordinary USB charger; it does not need a running Mac. Disconnecting power interrupts current-day coverage. The fridge's power supply remains separate.

The Web uses local HTTP. Save its explicit HTTP URL as a bookmark/home-screen icon. HTTPS/certificate setup and Internet-facing control are not included. Do not expose ports 80/1201 publicly.

Recent activity is bounded and boot-scoped. Keep private setup codes, Wi-Fi credentials, BLE addresses and raw logs out of public issues. [Troubleshooting](troubleshooting.md) explains normal recovery paths.
