# 固件与 CI 包

[English](firmware.md)

本仓库保留三种不同的固件范围：不可变的
`firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin`、单独维护的
`firmware/brookesia/` 源码，以及由 CI 构建的 ESP-IDF 示例 ZIP。CI ZIP 是诊断性
构建工件，不会替代出厂镜像，也不会把 `firmware/brookesia` 加入示例矩阵。

## CI ZIP 约定

每个成功的 ESP-IDF 矩阵项会上传一个保留 14 天的 ZIP。名称包含示例基名、ESP-IDF
版本、配置 ID 和硅片 profile（`rev1_3`）。ZIP 记录完整源 SHA、`esp32p4`、32 MiB Flash 上限、921600 波特率、
源工程、偏移量、大小和 SHA-256，并包含构建目录 `flasher_args.json` 中每个带偏移量
的文件。

完整 48 项矩阵会产生 4 个 `04_sdmmc` ZIP、10 个 `12_usb_extend_screen` ZIP，及每个
其余一方示例的 2 个 ZIP。该 CI 打包仅覆盖 ESP-IDF 示例。独立的 Arduino 工作流仅
编译草图，不发布 Arduino 固件 ZIP；该矩阵同样不会打包 `firmware/brookesia`、
出厂二进制或 ESP32-C6 镜像。

维护的 Brookesia 工作流会产生恰好两个 IDF 5.5.5 profile 专用包（`rev1_3` 和
`rev3_x`），同样保留 14 天。包不可互换：打包器读取已解析的 `sdkconfig`，并拒绝错误
target、缺失/多个最小版本符号、profile 不匹配，或已解析 Flash 大小不是唯一 32 MB 选择。
两个包共用 Brookesia 分区布局：分区表为 `0x9000`，NVS 为 `0xa000`，PHY 初始化为
`0x10000`，factory app 自动对齐到 `0x20000`。这可避免 rev3 bootloader 大于旧
`0x8000` 分区表偏移之前 `0x6000` 空间时破坏布局。

## Windows 烧录器

请先安装 Git 和 [GitHub CLI](https://cli.github.com/)，使用 `gh auth login` 完成认证，并在
可用的 Python 环境中安装 esptool：

```console
python -m pip install esptool
gh auth status
```

运行 `Flash-CI-Firmware.cmd -ListOnly` 可列出全部 50 个预期构件（48 个示例加两个产品 profile）；运行
`Flash-CI-Firmware.cmd -SelfTest` 可在不访问 Git、GitHub、Python、串口、下载或图形
界面的情况下检查本地安全逻辑。

正常使用要求当前分支干净且未分离，并且恰好有一个已就绪的开放 Pull Request，其完整
head SHA 与本地 `HEAD` 相同。烧录器只解析该 SHA 的成功运行，按运行 ID 和构件名下载，
解压到新的带时间戳本地工具目录，并验证清单、路径、哈希、大小、偏移量和 32 MiB 范围。

除非自动检测到恰好一个 VID 为 `303A` 的当前 USB 串口设备，否则请传入 `-Port COMx`。
写入前工具会证明该端口是 ESP32-P4，并解析硅片主/次版本（`v1.10` 即 110）。低于 3.0
只允许 `rev1_3`，3.0 及以上只允许 `rev3_x`；下载并验证清单后会再次探测 ESP32-P4/profile/revision，
且 revision 必须保持一致。该硅片检查不能证明 PCB/电气版本。它仅按已验证清单的偏移量执行
`python -m esptool --chip esp32p4 --baud 921600 write_flash`，绝不执行擦除命令。必须同时
满足 esptool 成功退出以及出现 `Hash of data verified`。随后请完成对应的开发板测试，并在
对话框中选择 PASS 后才会进入下一项。完整 SHA 改变时进度会重置。

例如：

```bat
Flash-CI-Firmware.cmd -Port COMx
```

CI 编译与包验证不能证明开发板已经烧录或运行正常。请单独执行并记录人工验证。
