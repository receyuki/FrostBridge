# Protocol fixtures

`c15-query-01.hex` is a deidentified complete frame from the confirmed C15,
captured by the readout firmware on 2026-11-07 after Query `FE FE 03 01 02 00`.
It is 36 bytes with a normal sum16 checksum. The original notification boundaries,
serial timings and private identifiers are deliberately excluded from this source distribution.

The shared 18-byte status prefix decodes to power off, MAX, Celsius, target 4,
current 20 and supply 14.3V. These are interpretations of the pinned reference
protocol; panel confirmation is an independent M1 acceptance step. The 12 extra
bytes beyond the single-zone minimum are retained but not assigned field meanings.
No setting command was used to produce this capture.

- `c15-query-panel-5.hex`：2026-11-08用户面板设5°C并关机后，本机实际Query捕获；原厂App已对照20/5/MAX/关机。原始私人记录不随公开源码分发。主机测试中由此改字节并重算校验的响应是模拟派生，不能当真实写后帧。
