# 源码支持与重建

[English](../en/rebuilding.md) · [简体中文](rebuilding.md) · [首页](../../README.zh-CN.md)

此附件提供同一 Release 固件对应的 Arduino-ESP32 库源码、使用这些库的完整 FrostBridge 源码、其他直接依赖源码、SDK 源码归档／许可和准确构建信息。正常安装无需下载；不含工具链可执行程序或个人设备配置。

Arduino-ESP32 使用 LGPL-2.1-or-later，许可见 `libraries/arduino-esp32/LICENSE.md`。本项目不限制修改该库，或为调试这些修改而进行逆向工程。其他组件保留各自许可，见 `third_party/` 及源码文件中的声明。SDK Wi-Fi／PHY／蓝牙／共存二进制组件保留 Espressif 的条款，用于 Espressif 设备。

## 使用修改后的库重建应用

1. 将 `FrostBridge-source.zip` 解压到独立目录，运行 `python3 frostbridge.py setup`，再运行一次 `python3 frostbridge.py build`，取得固定版本的 SDK 和工具链。
2. 本次使用的 Arduino 库源码在 `libraries/arduino-esp32`。修改项目 `.platformio/packages/framework-arduinoespressif32/` 下的 `cores`、`libraries` 或 `variants`；也可先从此附件复制对应源码目录再修改，保留安装后的包元数据及构建工具。HomeSpan、NimBLE-Arduino 和 ArduinoJson 也在附件 `libraries/` 中；实际构建副本位于项目 `.pio/libdeps/gateway/` 或 `.pio/libdeps/gateway_zh/`。
3. 强制清理并重建，使修改后的源码重新编译和链接：

```sh
# macOS/Linux；Windows 对应使用 .venv\Scripts\python.exe
./scripts/pio run -e gateway -t clean
python3 frostbridge.py build
```

直接调用 PlatformIO 时，将 `PLATFORMIO_CORE_DIR` 指向项目的 `.platformio` 目录；macOS/Linux 可使用 `./scripts/pio run -e gateway -t clean` 自动设置。中文 HomeKit 名称版本清理 `gateway_zh`，然后执行 `python3 frostbridge.py build --homekit-language zh-CN`。应用产物位于 `.pio/build/环境名/firmware.bin`；正常源码 `flash` 命令会构建并经 USB 写入修改后的应用，不需要签名密钥、在线批准或反修改检查。

这里提供完整源码，可自行重编译和重新链接，不依赖原构建者的目标文件。`BUILD-INFO.json`、`sdk-build/versions.txt`、SDK 构建参数和固定 PlatformIO 配置记录输入。构建前缀映射移除本机私人路径；不承诺不同主机／时间戳产物逐字节一致。

## SDK 资料

`sdk-sources/` 包含准确 ESP-IDF 提交归档、Arduino 库构建器版本和相关 mbedTLS／lwIP／TLSF／mDNS 源码归档；Git 子模块版本和上游声明各自保留。完整 SDK 工具包由项目固定依赖流程取得。`sdk-sources/sdk-materials.json` 记录 Espressif 二进制组件版本与许可路径；这是构建支持清单，不表示整个 SDK 都被链接进 FrostBridge。

附件的 `SHA256SUMS.txt` 覆盖各文件，`BUILD-INFO.json` 记录两份发布映像的哈希。安装或重建都不需要维护者的设备凭据。
