# 参与开发

[English](CONTRIBUTING.md) · [简体中文](CONTRIBUTING.zh-CN.md)

普通终端即可，不依赖AI助手、私人工作区或历史对话。

```sh
python3 frostbridge.py setup
python3 frostbridge.py build
python3 frostbridge.py test
python3 frostbridge.py build --homekit-language zh-CN
```

主机检查用clang++与ASan/UBSan，目标测试环境为macOS/Linux编译工具；Windows的sanitizer测试未验证。先构建gateway下载ArduinoJson。gateway默认英文，gateway_zh只将出厂名改为中文，不改服务/AID/协议；gateway_en保留为英文别名。GitHub Actions使用相同公开命令，但只有配置文件不代表已经远端CI通过。

## 改动边界

- 所有冰箱写入经过Controller，意图与观察分离，发送后的新鲜回读才能确认。
- 不猜未知协议字段，不因名称/服务相似就写另一型号；保留上游致谢，区分真实捕获与模拟。
- HAP回调有界非阻塞，仅入队；NVS在worker且不进入邮箱临界区。经典ESP32无PSRAM，要实测栈/堆，不能只看固件大小。
- 更新保留家庭、Wi-Fi、冰箱绑定和用户设置，不为通过测试自动擦除/复位配置。
- 保留备用来源区分、UTC+8完整性/空白日、入站顺序和手动覆盖；模拟不等于完整自然日验收。

## 网页

修改web目录，不直接编辑GeneratedWeb.h。构建钩子自动生成确定性gzip，也可单独运行`python3 scripts/embed_web.py`，不需要Node。英文为基础文案，可选中文词典位于app.js开头；HTML使用英文data-i18n/data-i18n-aria键。存储不可用时也默认英文，手动选择需跨重载保持。状态、错误、预览、记录和无障碍文案也需翻译，切换不能丢草稿/预览或发送设置。

保留一屏主控、机械雪花与轻量静态阴影。gzip预算24KiB、新拟物增量最多1KiB由脚本检查；favicon.ico复用PNG，不再重复嵌入图标。中英均检查320×568、375×667、390×664、1280×800、667×375及离线/错误/预览。

## 源码分发

public-files.txt为审核白名单，新文件明确加入；缺失/不安全条目会拒绝导出，不静默忽略。导出无Git历史、私人文件、内部规划、缓存或凭据；ZIP干净不代表旧开发Git历史也可公开。

保留上游许可、NOTICE与致谢。自有贡献用项目MIT，依赖继续各自许可；公开源码导出不等于完成固件二进制分发审查。

分别报告静态、主机、编译、USB、BLE、网页、家庭配对和实际自动化；未执行的硬件/远程/48h不标PASS。不要公开配对码、蓝牙地址、Wi-Fi凭据或Flash备份。
