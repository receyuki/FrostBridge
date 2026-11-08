# 架构与验证

[English](../en/development.md) · [简体中文](development.md) · [参与开发](../../CONTRIBUTING.zh-CN.md)

```text
Web API / HomeKit回调 → 有界邮箱 → 唯一BLE worker
                                  → ControlRuntime / Controller → BLE → C15
                                  ← 设备新鲜协议观察态 ← BLE
                                  → DailyReplay → 双键校验NVS
```

Protocol解析/编码已知有界布局并保留非目标字段；Controller分离意图与实际观察，验证epoch/新鲜度，以新回读确认。Runtime负责可信毫秒时间、自动化终态和备用投递。HAP回调只入队，不做Flash写或阻塞BLE；邮箱发布固定摘要与8行分页，不在每次HAP快照复制全日表。

DailyReplay将候选放RAM，已封存样本/配置/覆盖存版本化CRC双键日志。完整空白日有效，已知缺口保留旧样本，不把存档时间当冷启动可靠时钟。旧周数据保留，但生产周API/编辑器/调度已移除。

## 配置与资源

platformio.ini固定Espressif32 8.0.0、Arduino包4.30312.261024/核心3.3.12、ESP32支持包3.3.12、HomeSpan2.1.8、NimBLE2.5.1、ArduinoJson7.4.3。Python最小入口固定PlatformIO6.2.0、PySerial3.5、qrcode8.2；requirements-tools.lock是更完整的历史开发快照，不是普通安装入口。

gateway默认英文配件名，gateway_zh可选中文，gateway_en为英文兼容别名；AID/服务布局不变。basic/coexistence/readout为诊断。生产配置均为经典esp32dev、4MB、huge_app.csv；没有网络OTA、PSRAM要求或应用自动回滚。

既有worker栈12KiB；日owner/store≤2304B、worker余量≥2048B为资源目标，需实际路径和静态帧差额验证，不能只看编译。网页gzip≤24KiB、新拟物增量≤1KiB，在替换生成头文件前检查。翻译在浏览器运行，不在MCU堆加载语言表。

## 复现

```sh
python3 frostbridge.py setup
python3 frostbridge.py build
python3 frostbridge.py test
python3 frostbridge.py build --homekit-language en
python3 frostbridge.py export --output .local/dist/FrostBridge-source.zip
```

在新目录解压公开包再setup/build，检查误用私人工作区依赖。白名单导出不需要Git，且不带历史/内部资料；不要把.venv/.platformio/私人日志塞入公开包。

## 验收范围

一台C15/经典ESP32已有网页/家庭开关、ECO、正负温、蓝牙争用恢复、独立USB供电实测。主机覆盖协议、Controller、时钟、每日规则/日志、Runtime、HTTP/邮箱/API和SDK回调。模拟完整日不等于真实采集一天。

2026-11-08，干净导出源码在Intel Mac独立安装工具、编译两种生产配置并通过11套主机回归。中英网页通过隔离Chrome模拟及实际状态检查；公开CLI更新现有板子并保留目标、Wi-Fi、配对和备用配置。本地二维码页经独立解码与USB实际URI一致，没有重做新设备配对。本轮未复验Windows/Linux完整硬件流程及iPhone Safari双语界面。

真实完整日封存/重启载入、有效样本接管恢复、自然HomePod语音、远程家庭、更新中断救援与48h分别待验。历史低堆峰值原因未解，较短新窗口不能抹掉。新增双语浏览器/干净构建/USB应逐层记录，CI文件不代表远端任务已运行。

USB的BLE_STATUS和FROST_BIND_SAVED/FAILED确认支持独立安装，不在HTTP暴露绑定身份。正常配对从USB读取并在本地SVG显示二维码，不清除已有家庭，也不使用外部二维码服务。

HTTP错误说明统一英文，客户端应根据稳定错误码本地化；网页提供可选中文翻译。可选Tk配网工具也默认英文，传入`--lang zh-CN`可用中文；该工具需Python自带Tk，正常终端配网不需要Tk。

后续英文默认改动通过主机、Chrome和两种生产构建检查，本轮未再次刷写实物板；Tk弹窗翻译布局经源码核对，没有再次打开配网窗口。
