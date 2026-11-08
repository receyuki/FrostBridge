# Third-party notices / 第三方许可与来源

The project [MIT license](LICENSE) covers FrostBridge-owned code and documentation. It does **not** relicense the following components. This source tree keeps notices/licenses for its direct references and dependencies; normal builds download the actual frameworks/libraries through PlatformIO. File-level and transitive SDK licenses remain applicable.

项目MIT覆盖自有代码/文档，不改变下列组件许可。源码保留直接参考/依赖的许可与NOTICE，实际框架/库由PlatformIO下载；SDK中逐文件与传递依赖许可仍适用。

| Component / 组件 | Pinned source / 版本来源 | License / 许可 | Included notice / 本地文本 |
|---|---|---|---|
| Protocol reference, BrassMonkeyFridgeMonitor | [11f185eff808d6aae79eba247f826a52ed093fc0](https://github.com/klightspeed/BrassMonkeyFridgeMonitor/tree/11f185eff808d6aae79eba247f826a52ed093fc0) | MIT, ©2023 Ben Peddell | [Reference and license](third_party/BrassMonkeyFridgeMonitor/README.md) |
| HomeSpan | [2.1.8](https://github.com/HomeSpan/HomeSpan/tree/2.1.8) | MIT | [License](third_party/licenses/HomeSpan-MIT.txt) |
| NimBLE-Arduino | [2.5.1](https://github.com/h2zero/NimBLE-Arduino/tree/2.5.1) | Apache-2.0, with upstream notices | [License](third_party/licenses/NimBLE-Arduino-Apache-2.0.txt), [NOTICE](third_party/licenses/NimBLE-Arduino-NOTICE.txt) |
| Apache NimBLE bundled by NimBLE-Arduino | bundled source in the pinned NimBLE-Arduino version | Apache-2.0 and its file-level notices | [License](third_party/licenses/Apache-NimBLE-LICENSE.txt), [NOTICE](third_party/licenses/Apache-NimBLE-NOTICE.txt) |
| TinyCrypt bundled by NimBLE-Arduino | bundled source in the pinned version | BSD-style terms in upstream text | [License](third_party/licenses/TinyCrypt-LICENSE.txt) |
| ArduinoJson | [7.4.3](https://github.com/bblanchon/ArduinoJson/tree/v7.4.3) | MIT | [License](third_party/licenses/ArduinoJson-MIT.txt) |
| Arduino-ESP32 core and ESP32 support package | core 3.3.12, PlatformIO core package 4.30312.261024; support package 3.3.12 | LGPL-2.1-or-later package metadata; component-specific terms also apply | [Upstream license](third_party/licenses/Arduino-ESP32-LGPL-2.1.md), [source](https://github.com/espressif/arduino-esp32/tree/3.3.12) |
| ESP-IDF and SDK components | Arduino 3.3.12 build identifies ESP-IDF 5.5.5 | Apache-2.0 and component-specific terms, including binary components | [ESP-IDF license](https://github.com/espressif/esp-idf/blob/v5.5.5/LICENSE), [component source](https://github.com/espressif/esp-idf/tree/v5.5.5/components) |
| PlatformIO Core / Espressif32 platform (build tools) | Core 6.2.0 / platform 8.0.0 | Apache-2.0 | [Core license](third_party/licenses/PlatformIO-LICENSE.txt), [platform](https://github.com/platformio/platform-espressif32/blob/v8.0.0/LICENSE) |
| PySerial (host USB tool) | 3.5 | BSD-style | [License](third_party/licenses/PySerial-LICENSE.txt) |
| python-qrcode (local host pairing QR, not firmware) | [8.2](https://github.com/lincolnloop/python-qrcode/tree/v8.2) | BSD | [License](third_party/licenses/python-qrcode-LICENSE.txt) |

Protocol descriptions and captured vectors informed an independent implementation. Upstream Wi-Fi/MQTT application code is not imported. The deidentified C15 fixtures identify real captures versus simulated derivatives in their [README](test/fixtures/README.md).

协议参考与捕获向量用于独立实现，没有导入上游Wi-Fi/MQTT应用。脱敏C15 fixture区分真实捕获和模拟派生，见其README。

## Binary distribution / 二进制分发

The clean source export is not a complete firmware-binary license bundle or SDK SBOM. Before publishing compiled firmware, include the required upstream notices and provide the corresponding source/relinking materials required by the applicable licenses, including the LGPL components. Keep the exact source revision, build configuration, SDK/package versions and modifications. See the [Arduino core's license](third_party/licenses/Arduino-ESP32-LGPL-2.1.md), especially its library/linking distribution terms. The CI provided here builds firmware but does not publish binary releases.

干净源码导出不等于完整二进制许可包或SDK SBOM。发布编译固件前，需要保留适用上游NOTICE，并按各许可提供所需的对应源码/重新链接材料（包括LGPL组件）；保存准确源码版本、配置、SDK/包版本和修改记录。请阅读Arduino核心许可的库/链接分发条款。当前CI只构建，不发布固件二进制release。

## Artwork / 图形

The mechanical snowflake icon was generated with an image model for FrostBridge. Web PNG assets keep the chosen design. `/favicon.ico` serves the same PNG bytes as `/favicon.png`; it is not another artwork copy in flash. No Apple or Alpicool logos or endorsement are claimed.

机械雪花由图像模型为本项目生成，PNG保留选定设计；favicon.ico复用favicon.png字节，不重复占用Flash。没有使用Apple/Alpicool标识或声称官方背书。
