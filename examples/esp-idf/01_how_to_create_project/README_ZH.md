# 如何创建工程

[English](README.md)

这是面向 ESP32-P4-WIFI6-Touch-LCD-7B 的最小 ESP-IDF 工程模板。

示例仅包含工程框架和空的 `app_main()`。创建新的开发板专用 ESP-IDF 应用时，
可以将它作为干净的起点。

## 硬件要求

- ESP32-P4-WIFI6-Touch-LCD-7B 开发板。
- 用于烧录和串口监视器的 USB 线。

## 构建

```bash
cd examples/esp-idf/01_how_to_create_project
idf.py set-target esp32p4
idf.py build
```

## 复用方式

- 保留根目录的 `CMakeLists.txt`。
- 将应用源码放在 `main/` 下。
- 应用需要 BSP 时，在 `main/idf_component.yml` 中添加开发板依赖。
- 不要把 `sdkconfig`、`build/` 和 `managed_components/` 等生成文件提交到 Git。
