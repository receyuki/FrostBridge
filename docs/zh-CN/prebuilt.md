# 安装预编译 FrostBridge

[English](../en/prebuilt.md) · [简体中文](prebuilt.md) · [首页](../../README.zh-CN.md)

从 [FrostBridge v0.1.0](https://github.com/receyuki/FrostBridge/releases/tag/v0.1.0) 下载安装 ZIP。`esp32-en.zip` 默认使用英文 HomeKit 名称和工具引导；`esp32-zh-CN.zip` 默认中文。两个包均含中英说明，可用 `--lang en`／`--lang zh-CN` 覆盖工具语言。pip／esptool 的上游输出和技术命令帮助仍为英文。

此安装包用于经典 ESP32／4MB Flash，包括已实测的微雪 ESP32-DEV-KIT-WROOM-32E-N4；不适用于 ESP32-C3/S3/S2。无需编译、PlatformIO、Node.js 或 AI 助手。需要 Python 3.10+ 和 USB 数据线。目前仅 Intel macOS 完整硬件流程有实际验证。

## 1. 安装轻量工具

完整解压 ZIP，在包目录打开终端：

```sh
python3 frostbridge.py setup
python3 frostbridge.py ports
```

包内 setup 自动仅安装 esptool、PySerial、qrcode 及其依赖。Windows 将 python3 换成 `py -3`。下文 PORT 需替换为实际 USB 串口。

## 2. 首装或升级

首次安装到专用 ESP32：

```sh
python3 frostbridge.py install --port PORT
```

首装按独立地址写入引导程序、分区表、OTA 元数据和应用，不写 NVS 镜像，不擦除整片 Flash。将其他项目的板子改作本项目用途前，先备份旧项目；改变分区不能保证旧配置保留。

已有同分区布局的 FrostBridge 升级：

```sh
python3 frostbridge.py install --port PORT --update
```

升级先核对已有分区表，再仅写应用区，保留 Wi-Fi、冰箱绑定、HomeKit 配对和备用配置。加 `--dry-run` 可验证包并查看写入参数，不打开 USB。没有网络 OTA 或自动回滚。

## 3. 配置自己的设备

首次安装按顺序执行：

```sh
python3 frostbridge.py wifi --port PORT
python3 frostbridge.py bind --port PORT
python3 frostbridge.py pair --port PORT
```

- Wi-Fi：输入自己的 2.4GHz SSID 和隐藏密码，保存到 ESP32；固件包不含这些信息，电脑不保存输入。
- 冰箱：退出原厂 App／nRF Connect，从本次 BLE 扫描列表选择自己的冰箱。不要求名称为 C15，不用改源码。目前仅作者的 C15 实机验证；其他型号先按[兼容指南](compatibility.md)核对。
- Apple 家庭：电脑打开私人本地二维码页，在 iPhone／iPad「家庭 → ＋ → 添加配件」中扫描。码来自这块 ESP32，不是下载包共用的码；已有配对不需重新添加。

正常升级后，已有配置正常就跳过配网／绑定／配对。bind 默认保留已有目标；不要为了名称变化清空家庭配对。

打开 **http://frostbridge.local**。网页默认英文，可切换中文；包内 HomeKit 名称语言见 `firmware/manifest.json`，已有自定义／缓存名称可能不变。其他操作见[使用说明](usage.md)和[故障排查](troubleshooting.md)。

## 包内容与验证范围

包含四份固件、记录写入地址／SHA256 的 `firmware/manifest.json`、本机工具、文档、许可说明和匹配的干净源码 ZIP。SHA256 用于检查文件变化，不等于真实性签名。包内不含 Wi-Fi 凭据、绑定或家庭身份。安装经 USB 执行，先关闭其他串口监视器。

这是首个 0.1.0 版本。完整自然日学习／接管、远程 HomeKit 和 48 小时稳定性仍单独待验；空白板首装及 Windows／Linux 实机流程尚未验证。同一 Release 的 `source-support.zip` 附件提供对应库源码、SDK 许可、固定构建信息和重建／重新链接说明。正常安装无需下载这个较大的源码支持包，仅检查或修改依赖时需要。
