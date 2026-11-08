# Contributing

[English](CONTRIBUTING.md) · [简体中文](CONTRIBUTING.zh-CN.md)

Use an ordinary terminal. No assistant, private workspace or downloaded conversation is required.

```sh
python3 frostbridge.py setup
python3 frostbridge.py build
python3 frostbridge.py test
python3 frostbridge.py build --homekit-language zh-CN
```

Host checks use `clang++` and ASan/UBSan. macOS/Linux compiler tooling is the intended test environment; Windows host sanitizer tests are not validated. Build gateway first to fetch ArduinoJson. `gateway` defaults to English; `gateway_zh` changes factory names to Chinese, not services/AIDs or protocol semantics. `gateway_en` remains an English alias. GitHub Actions uses the same public commands; a configuration file alone is not evidence of a remote CI run.

## Change boundaries

- All fridge writes go through Controller. Intent and observed state are separate; only fresh post-send readback confirms an action.
- Do not infer an unknown protocol field or write an unsupported fridge because its advertised name/service looks similar. Keep reference attribution and known capture/simulation distinctions.
- Keep HAP callbacks bounded/nonblocking; enqueue operations there. Keep NVS on the worker, outside mailbox critical sections. Classic ESP32 has no PSRAM; measure stack/heap, not only binary size.
- Preserve existing pairing, Wi-Fi, fridge binding and user settings in updates. Do not add automatic erase/reset to make tests pass.
- Keep backup source separation, UTC+8 completeness, empty-day rules, admission order and manual overrides. Simulator results are not full-day physical acceptance.

## Web changes

Edit `web/`, not `include/GeneratedWeb.h`. PlatformIO's pre-build hook regenerates deterministic gzip resources; `python3 scripts/embed_web.py` can run separately. No Node dependency is required. English is the base language. The optional Chinese catalog is at the start of `web/app.js`; HTML uses English data-i18n/data-i18n-aria keys. Preserve the English default when storage is unavailable and keep manual language choices across reloads. Translate status, errors, preview text, records and accessibility labels, not just buttons. Language switching must preserve drafts/pending previews and send no settings request.

Keep the existing one-screen controls, mechanical snowflake and lightweight static shadows. The asset budget is 24KiB gzip and the neumorphic increment is at most 1KiB; the script enforces both. `/favicon.ico` reuses the PNG data rather than embedding a second icon. Check both languages at 320×568, 375×667, 390×664, 1280×800 and 667×375, plus offline/errors and an open preview.

## Source distribution

`public-files.txt` is the reviewed allowlist. Add new public files deliberately; export fails on missing/unsafe entries instead of silently omitting them. The export has no Git history, private files, internal planning, local caches or credentials. Do not assume an old development Git history is publishable merely because an exported ZIP is clean.

Retain upstream license/NOTICE files and attribution. Own contributions use the project's MIT license; dependencies retain their own terms. Public source exports are not a substitute for a firmware-binary redistribution review.

When reporting results, distinguish source checks, host tests, cross-compilation, USB startup, BLE, browser, HomeKit pairing and real automation. Do not label unexecuted hardware/remote/48h checks PASS. Never publish raw pairing codes, BLE addresses, Wi-Fi credentials or flash backups.
