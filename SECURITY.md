# Security and privacy / 安全与隐私

FrostBridge is a trusted-LAN DIY bridge. Local Web/API control uses **HTTP with no login**; HomeKit pairing/encryption is a separate protocol on port 1201. Host/Origin and input validation do not authenticate everyone on your LAN. Do not expose ports 80/1201 to the Internet or an untrusted network.

Wi-Fi setup uses physical USB and hidden terminal input. Pairing codes/URIs/QR pages are generated/read locally; the QR page has no external network resources. Private local files and backups belong under ignored `.local/`. Opening USB can reboot ESP32 and invalidate a day's continuous recording.

For a security report, use a private channel offered by the repository maintainer, rather than a public issue containing secrets. No private reporting address or response SLA is configured in this source snapshot; the maintainer should configure a contact/private advisories before public hosting. A report should identify the source revision, affected interface and a minimal redacted reproduction. Never attach raw Wi-Fi credentials, pairing databases, setup QR pages, BLE addresses or full flash backups publicly.

FrostBridge用于可信局域网。Web/API是**无登录的HTTP**；HomeKit的配对/加密是1201端口的独立协议。Host/Origin及输入验证不等于认证局域网用户，不要对公网或不可信网络开放80/1201。

Wi-Fi经物理USB与隐藏输入配置，设置码/URI/二维码在本地读取和生成，配对页不依赖外部资源。私人文件与备份应保存在忽略的.local路径。打开USB可能重启ESP32，使本日连续记录失效。

安全问题请用维护者提供的私人渠道，不要在公开Issue贴秘密。本源码快照尚未配置私人联系地址或响应时限，公开托管前由维护者设置联系/私密报告入口。报告包含版本、受影响接口和最小脱敏复现即可，不要公开凭据、配对数据库、二维码页、蓝牙地址或整片Flash备份。
