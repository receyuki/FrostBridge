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
| esptool (host prebuilt USB installer; not linked into firmware) | [5.6.0](https://github.com/espressif/esptool/tree/v5.6.0) | GPL-2.0-or-later | [License](third_party/licenses/esptool-GPL-2.0.txt); installed separately by pip |
| python-qrcode (local host pairing QR, not firmware) | [8.2](https://github.com/lincolnloop/python-qrcode/tree/v8.2) | BSD | [License](third_party/licenses/python-qrcode-LICENSE.txt) |

Protocol descriptions and captured vectors informed an independent implementation. Upstream Wi-Fi/MQTT application code is not imported. The deidentified C15 fixtures identify real captures versus simulated derivatives in their [README](test/fixtures/README.md).

协议参考与捕获向量用于独立实现，没有导入上游Wi-Fi/MQTT应用。脱敏C15 fixture区分真实捕获和模拟派生，见其README。

Additional protocol-layout research references / 其他协议布局研究来源:

- [dandwhelan/Alpicool50l12vfridgefreezer, 3f7ec01](https://github.com/dandwhelan/Alpicool50l12vfridgefreezer/tree/3f7ec01eced922aae431bb90e9c92ac9a60bb0c2): model-specific Set02 projection, including the model1 branch used as a reference for the C15 implementation / 按型号选择Set02字段的分析，包括C15实现所参考的model1分支。
- [johnelliott/alpicoold, 8530ecb](https://github.com/johnelliott/alpicoold/tree/8530ecba99823461bcebfc20e62082771bb55fba) and [jakub-hajek/alpicool-esp32-mqtt, 397028e](https://github.com/jakub-hajek/alpicool-esp32-mqtt/tree/397028e69fd5c26a68d44d19557f09bfb7f95449): short-status/settings-layout cross-checks / 短状态帧与设置布局交叉核对。

These are research citations, not additional linked firmware dependencies or imports of their applications. Their inclusion here does not assign them the FrostBridge MIT license. / 上述为研究引用，不是额外链接的固件依赖或应用代码导入；此处列名不表示将其改为FrostBridge的MIT许可。

## Binary distribution / 二进制分发

Firmware Releases are accompanied by a `source-support.zip` asset with the corresponding Arduino-ESP32 library source as used, the complete FrostBridge work that uses it, direct library sources, pinned SDK source archives/notices and build inputs. The LGPL-2.1-or-later [Arduino license](third_party/licenses/Arduino-ESP32-LGPL-2.1.md) is included. Users may modify the libraries and rebuild/relink; no project restriction is imposed on reverse engineering for debugging those modifications. See [rebuilding instructions](docs/en/rebuilding.md). Other components keep their own terms; this is not a claim that the entire SDK uses LGPL or that this inventory is a complete SDK SBOM. CI builds and checks source; release publication is a separate explicit step.

固件 Release 同时提供 `source-support.zip`，包含实际使用的 Arduino-ESP32 库源码、使用该库的完整 FrostBridge 源码、直接依赖源码、固定 SDK 源码归档／许可及构建信息。保留 LGPL-2.1-or-later Arduino 许可，允许修改后重建／重新链接，不附加限制调试这些修改所需逆向工程的条款，见[重建说明](docs/zh-CN/rebuilding.md)。其他组件各自许可继续适用，不将整个 SDK 笼统称为 LGPL，也不声称清单为完整 SDK SBOM。CI 负责构建检查，发布仍是单独授权的步骤。

ESP-IDF 5.5.5 and relevant component notices are retained in [SDK notices](third_party/licenses/esp-idf/README.md), including the Espressif Wi-Fi, PHY, Bluetooth and coexistence binary-component licenses. GCC runtime exception and C/C++ runtime notices are included in the source-support archive; toolchain executables are fetched separately, not redistributed in the firmware installation ZIPs.

ESP-IDF 5.5.5 及相关组件许可见 [SDK 许可](third_party/licenses/esp-idf/README.md)，包括 Espressif Wi-Fi、PHY、蓝牙与共存二进制组件。GCC 运行时例外及 C／C++ 运行时声明在源码支持附件中保留；工具链程序另行下载，不随固件安装 ZIP 分发。

## Artwork / 图形

The mechanical snowflake icon was generated with an image model for FrostBridge. Web PNG assets keep the chosen design. `/favicon.ico` serves the same PNG bytes as `/favicon.png`; it is not another artwork copy in flash. No Apple or Alpicool logos or endorsement are claimed.

The README uses a separate PNG variant with transparent exterior corners. The opaque Web/home-screen assets are retained.

机械雪花由图像模型为本项目生成，PNG保留选定设计；favicon.ico复用favicon.png字节，不重复占用Flash。没有使用Apple/Alpicool标识或声称官方背书。

README使用圆角外侧透明的独立PNG版本；网页／主屏幕的不透明图标保留。
