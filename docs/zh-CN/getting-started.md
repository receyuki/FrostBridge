# 编译、刷写与配对

可下载的预编译固件见[免编译安装](prebuilt.md)。以下保留源码编译流程。

[English](../en/getting-started.md) · [简体中文](getting-started.md) · [首页](../../README.zh-CN.md)

## 准备条件

- 实测开发板：**微雪（Waveshare）ESP32-DEV-KIT-WROOM-32E-N4**（经典ESP32，芯片ESP32-D0WD-V3，4MB Flash，无PSRAM）。C3/S3/S2及其他冰箱报文布局未确认兼容，不能只凭型号带ESP32就换板型。
- USB数据线和稳定USB供电；退出占用冰箱连接的其他蓝牙客户端。
- C15或使用相同协议的冰箱。目前只有作者的C15做过实机验证，其他型号请按[兼容性流程](compatibility.md)核验。
- 2.4GHz个人Wi-Fi。当前配网入口支持SSID 1–32 UTF-8字节、密码8–63字节；不支持开放/企业网络或64字符十六进制PSK。
- Python3.10+及venv/pip；首次下载工具链需要联网和数GB磁盘空间。本地构建不需要Node.js或AI服务。Windows按板厂说明安装USB驱动；Linux按[PlatformIO说明](https://docs.platformio.org/en/latest/core/installation/requirements.html)配置串口权限/udev。
- 用于配对的iPhone/iPad和家庭App。中枢自动化及外出控制需要家庭中枢；局域网网页控制可独立工作。

本项目物理验证的平台为Intel Mac。下方提供Windows/Linux使用方式，但不代表所有驱动和板型组合均已通过实测。

## 1. 安装工具

在解压后的项目目录打开终端：

```sh
python3 frostbridge.py setup
python3 frostbridge.py --lang zh-CN doctor
```

Windows把`python3`替换为`py -3`。入口自动创建`.venv`，把包放入`.platformio`。普通安装只需最小`requirements.txt`，不用安装完整开发快照锁文件；VS Code PlatformIO扩展可选。

## 2. 编译生产固件

```sh
python3 frostbridge.py build
```

默认`gateway`提供英文家庭配件名；需要中文时，在build/flash命令加`--homekit-language zh-CN`选择`gateway_zh`。`gateway_en`保留为英文兼容别名。网页首次固定英文，不按浏览器语言自动切换，可点语言按钮选择中文。既有中文安装更新时可明确选择中文配置保留名称，不需要重置或重新配对。

POSIX也可用`./scripts/pio run -e gateway`；直接调用PlatformIO时注意包目录，统一入口会自动设置。普通`pio run`默认生产gateway，不再是smoke。构建前自动生成网页资源。

## 3. USB刷写

已有其他项目的板子，先备份原应用和数据。当前采用4MB的`huge_app`分区：单个3MB应用槽、USB更新，没有OTA或自动回滚。

```sh
python3 frostbridge.py ports
python3 frostbridge.py flash --port PORT
```

将`PORT`换成实际设备；`COM5`、`/dev/ttyUSB0`、macOS的`/dev/cu.*`只是示例。先关闭其他串口监视器。自动下载模式失败时，按对应板厂的BOOT/EN步骤重试，检查数据线和USB稳定性。

相同布局的正常应用更新保留Wi-Fi、冰箱绑定、家庭配对和备用配置，工具不会擦除Flash；这不保证跨其他项目分区变化时也能保留。不要把erase-flash作为常规更新步骤。

## 4. 配置Wi-Fi

```sh
python3 frostbridge.py --lang zh-CN wifi --port PORT
```

交互输入SSID和隐藏的密码，仅通过USB发给ESP32，不写项目文件。如只需修改SSID，用`wifi --keep-password --port PORT`保留设备已存密码。保存成功后ESP32重启。

打开串口本身也可能重启ESP32。先完成安装再开启完整日学习，避免在需要连续记录的一天中途打开监视器。

## 5. 选择自己的冰箱

给C15通电并保持蓝牙范围，退出原厂App/nRF Connect：

```sh
python3 frostbridge.py --lang zh-CN bind --port PORT
```

程序在同一次USB会话扫描、列出名称/地址/RSSI、询问序号，等待新鲜状态与保存确认。留空取消，已有选择默认保留。明确更换冰箱时用`bind --replace --port PORT`，不要复用上一次会话序号。

绑定只做发现和已知状态查询，不请求改变温度、电源或模式。另一台设备广播相同服务不代表协议兼容。

```sh
python3 frostbridge.py status
```

mDNS无法解析时，用`status --host 当前局域网IP`或在路由器查找。status只读HTTP，不打开USB，也不重启设备。

## 6. Apple家庭配对

```sh
python3 frostbridge.py --lang zh-CN pair --port PORT
```

命令在`.local/pairing/`生成含SVG的独立私人HTML二维码页，并在电脑浏览器打开，不使用第三方二维码网站。用iPhone/iPad家庭→＋→添加配件扫描电脑屏幕上的码，或通过更多选项手动输入8位设置码。纯终端电脑可用`pair --no-open --port PORT`仅生成文件。它只用于HomeKit配对，不能替代前面的Wi-Fi配网；设置码、URI和文件请保密。如果出现未经认证提示，仅对你自己的设备确认继续，再分配房间和名称。Mac家庭App不能添加新配件，见[Apple说明](https://support.apple.com/en-us/104998)。

正常配对不需要诊断用HomeKit客户端或额外Python配对库，此命令不会移除已有家庭。随后按[使用说明](usage.md)配置连通检测/检测回应。

## 7. 日常使用与更新

打开**http://frostbridge.local**或当前局域网IP，可存书签/主屏幕入口。不再刷写时可换普通USB充电器，Mac不是网关；冰箱和ESP32都需要供电。

更新时关闭串口监视器，从新源码重复相同配件语言的flash命令。既有配置正常就不用重新配网、绑定或配对。新设备每日备用默认关闭，启用前阅读[使用说明](usage.md)。
