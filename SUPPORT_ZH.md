# 支持

[English](SUPPORT.md)

可使用本仓库的 Issue 表单报告可复现的示例、文档和 CI 缺陷。产品服务、购买咨询或
个案技术协助请使用[微雪官方技术支持渠道](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/Technical-Support/)。

## 提交 Issue 前

请先查阅[产品文档](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/)、
[快速开始](docs/GETTING_STARTED_ZH.md)和已有 Issue。尚未确认开发板或工具链时，
先运行 `examples/esp-idf/00_board_check`。

## 需要提供的信息

- 准确的开发板型号和可见硬件版本。
- 示例或固件路径。
- ESP-IDF 版本和主机操作系统。
- 配置变体或相关 `menuconfig` 修改。
- 最小复现步骤。
- 预期行为和实际行为。
- 故障前后的完整串口输出。
- 是否涉及 ESP32-C6 Hosted Wi-Fi 链路。

发布前请移除 Wi-Fi 凭据、令牌、密钥、设备标识、私有路径和客户数据。尽量附上文本
日志；只有截图时不便搜索和引用。

不要在公开 Issue 中披露疑似安全漏洞。本仓库目前没有公布经确认的私密漏洞报告渠道。

## 仓库支持范围

默认 CI 矩阵会编译一方示例，但不会验证物理外设、重新生成预编译出厂固件，也不会
认证定制产品镜像。硬件相关修复除 Actions 检查通过外，还应提供受影响开发板的运行
证据。
