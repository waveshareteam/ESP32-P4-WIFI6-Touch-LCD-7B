# 持续集成

[English](CI.md)

本仓库将始终可见的轻量验证任务与耗时的 ESP-IDF 构建矩阵分开。这样，每个
Pull Request 都能获得确定性的仓库检查和变更路由结果，而仅文档变更无需执行
产品构建。

## 仓库检查

`Public repository checks` 工作流会在每个 Pull Request 中检查面向用户的目录
结构、本地 Markdown 链接、生成文件边界、路径大小写以及公开文本规范；PR 使用
base/head 差异运行仓库自包含 Markdown 审计，其他事件执行严格全量盘点。

`ESP-IDF examples` 工作流会重复这些检查，运行发现与路由契约测试，然后仅启动
选中的构建。同一 Pull Request 推送新提交时会取消旧运行，但不会影响其他分支或
发布工作。

独立的 `Arduino examples` 工作流会发现并编译 `examples/arduino/examples/` 下的
Arduino 草图。它直接构建所有匹配草图，使用 Arduino-ESP32 3.3.11 和 Rev3.x
`ChipVariant=postv3` profile。

## 必需的 ESP-IDF 矩阵

默认示例矩阵只包含 `examples/esp-idf/` 直属目录下的一方工程。组件
`test_apps`、`firmware/brookesia` 以及预编译出厂固件会被单独盘点，不属于产品
示例。

| 设置 | 值 |
| --- | --- |
| Target | `esp32p4` |
| ESP-IDF 5.5 稳定线 | [`v5.5.5`](https://github.com/espressif/esp-idf/releases/tag/v5.5.5) |
| ESP-IDF 6 稳定线 | [`v6.0.2`](https://github.com/espressif/esp-idf/releases/tag/v6.0.2) |
| 管理型开发板 BSP | [`waveshare/esp32_p4_wifi6_touch_lcd_7b` 3.0.1](https://components.espressif.com/components/waveshare/esp32_p4_wifi6_touch_lcd_7b/versions/3.0.1) |
| BSP 的 LVGL 集成 | BSP 管理的 `espressif/esp_lvgl_adapter` `~0.6` 版本线 |
| 纳入矩阵的一方工程 | 18 个 |
| IDF 5.5 覆盖范围 | 18 个工程 |
| IDF 6.0 覆盖范围 | 18 个工程 |
| 完整手动触发矩阵 | 46 项 |

纳入矩阵的一方工程都会在 IDF 5.5.5 和 IDF 6.0.2 上构建，其中包括使用 LVGL 9 的
Brookesia phone 示例。`04_sdmmc` 会构建格式化挂载失败配置，
`12_usb_extend_screen` 会在每条支持的版本线上构建以下 5 种配置：

- `default`
- 功能评估板兼容配置
- `no_hid_uac`
- `without_hid`
- `without_uac`

这 46 个任务都会追加 `rev3_x` defaults，保留 14 天的构件名也包含 `rev3_x`；芯片
profile 不会把示例矩阵倍增。`firmware/brookesia` 单独维护，目前没有 GitHub Actions
工作流构建或打包它。示例的每种配置同样使用绝对 CMake 缓存路径，因此打包器读取的是
IDF 实际生成的 `build/<configuration>/sdkconfig`。

每个已跟踪的 `sdkconfig.ci*` 文件都会被提交为上述配置之一，或由发现契约明确
分类。空的上游占位文件、仅包含 Wi-Fi 凭据的输入，以及 ESP32-C2/ESP32-S3
target 配置均有意排除在本 ESP32-P4 产品矩阵之外。

每个成功的矩阵任务还会打包一个可烧录的 CI ZIP。因此完整的 46 项矩阵会生成 46 个
唯一命名的构件：`04_sdmmc` 产生 4 个，`12_usb_extend_screen` 产生 10 个，其余 16 个
纳入矩阵的示例各产生 2 个。下载和烧录边界见[固件与 CI 包](firmware_ZH.md)。

## Arduino 构建

`Arduino examples` 工作流会发现 `examples/arduino/examples/` 下的 12 个单草图目录，
并分别编译。它安装固定版本的 Arduino CLI 和 Arduino-ESP32 `3.3.11`，然后使用以下
FQBN：

```text
esp32:esp32:esp32p4:ChipVariant=postv3,PSRAM=enabled,FlashSize=32M,FlashMode=qio,FlashFreq=80,PartitionScheme=app13M_data7M_32MB,UploadMode=default,UploadSpeed=921600
```

工作流的 `target` 手动触发输入支持 `all`、示例名或示例路径。它只提供编译覆盖，不
上传 Arduino 二进制或烧录包。工作流显式使用 `examples/arduino/libraries/` 作为库路径，
以便从仓库构建开发板的 EK79007 DSI、GT911 触摸和 LVGL 依赖。

## 变更文件路由

发现脚本读取完整且识别重命名的 base/head 差异，并先按文件类型、再按目录归属
应用规则：

| 变更路径 | 示例构建 |
| --- | --- |
| 根目录或任意子目录中的 Markdown | 不构建 |
| `assets/` 或 `docs/` 下的文档图片 | 不构建 |
| Issue/PR 模板与治理文件 | 不构建 |
| `examples/arduino/**` 或 Arduino 工作流输入 | 不进入 ESP-IDF 矩阵；Arduino 工作流独立编译草图 |
| 单个示例中的直接源码或配置 | 仅构建该示例 |
| 共享芯片版本配置、打包器或工作流安全输入 | 构建全部 18 个纳入矩阵的示例 |
| `firmware/brookesia` 源码/配置 | 不构建示例；单独报告固件范围 |
| 固件 Markdown、出厂 BIN 或交付归档 | 不构建；单独报告固件/发布范围 |
| 完整差异中无法分类的非文档路径 | 构建全部 18 个纳入矩阵的示例并报告未知路径 |
| 重命名或删除 | 同时计入旧路径的构建影响 |
| 空白或不可用的差异 | 发现任务失败 |

工作流检出并构建 Pull Request 的 head SHA，而不是 GitHub 生成的合并提交。因此，
矩阵数量和结果必须与该 head 精确对应。

## 固件边界

`firmware/brookesia` 是单独维护的交付源码工程。仓库内的出厂 `.bin` 是不可变的
预编译工件，绝不是 CI 构建输出。这两个范围都不会进入默认示例 CI；文档或示例
维护也不得顺带重构、重新打包或替换它们。轻量公开仓库检查只验证该二进制已公布的
SHA-256 身份，不据此宣称完成固件构建或硬件测试。

## 手动触发

手动运行工作流时可设置 `project`：

| 值 | 含义 |
| --- | --- |
| `all` | 构建全部 46 项已纳入示例矩阵 |
| `09_lvgl_demo_v9` | 通过目录名在两条 IDF 版本线上构建一个示例 |
| `examples/esp-idf/09_lvgl_demo_v9` | 通过路径在两条 IDF 版本线上构建一个示例 |

示例工作流有意不接受固件路径。

## 静态自检

以下命令只验证仓库策略和路由，不编译任何固件：

```bash
python .github/scripts/check_public_repo.py
python .github/scripts/test_audit_markdown.py
python .github/scripts/audit_markdown.py . --all --strict --config .github/markdown-audit.json
python .github/scripts/test_discover_esp_idf_examples.py
python .github/scripts/test_review_boundaries.py
python .github/scripts/discover_esp_idf_examples.py --example all
```

示例与 Arduino 编译证据以最终审核提交上的必需 GitHub Actions 矩阵为准。被排除的
Brookesia 源码仍是单独维护、需要手动验证的固件范围。
