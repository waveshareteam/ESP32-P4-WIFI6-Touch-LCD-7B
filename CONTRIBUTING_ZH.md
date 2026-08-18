# 贡献指南

[English](CONTRIBUTING.md)

欢迎改进 ESP32-P4-WIFI6-Touch-LCD-7B 的示例、文档、CI 和可维护性。

## 修改前

- 搜索已有 Issue 和 Pull Request，确认是否存在相关工作。
- 将修改范围保持在本款 7 英寸 1024 x 600 产品内。
- 说明受影响的示例路径和 ESP-IDF 版本。
- 不要提交凭据、Wi-Fi 密码、令牌、私钥、私密日志或专有媒体资源。

## 仓库边界

- 一方 ESP-IDF 示例直接位于 `examples/esp-idf/` 下。
- `firmware/brookesia` 是单独维护的交付源码工程，不属于默认示例矩阵。
- `firmware/ESP32-P4-WIFI6-Touch-LCD-7B-FactoryOnly.bin` 是不可变的预编译
  出厂制品。不要在无关修改中替换、重建或重新打包它。
- 保留导入或 managed 组件的历史。不要在产品级修改中批量格式化或翻译嵌套的
  上游组件树。
- 不要把生成的 `build/`、`managed_components/`、`dependencies.lock`、本地
  `sdkconfig` 或编辑器文件提交到 Git。

## 文档

一方英文文档应配套简体中文 `_ZH.md` 文件，在文件顶部附近提供双向语言入口。
当目标文档已有中文版本时，中文页面的内部链接应指向同语言页面。现有 `_CN.md`
路径是兼容入口，应继续指向规范的 `_ZH.md` 页面。

## 静态检查

请运行与修改相关的仓库检查：

```bash
python .github/scripts/check_public_repo.py
python .github/scripts/test_discover_esp_idf_examples.py
python .github/scripts/test_review_boundaries.py
```

不要提交生成的构建输出。产品编译由 `ESP-IDF examples` GitHub Actions 工作流在
经过审阅的 Pull Request head 上判断。仅文档和仅治理的修改仍会运行轻量公开仓库检查，
但不会启动耗时的示例矩阵。

## Pull Request

请说明用户可见行为、兼容性边界和验证证据，并在存在相关 Issue 时建立链接。
硬件相关修改还应提供开发板版本、接线或外设设置以及运行时证据；Actions 编译通过
本身不能证明硬件行为正确。
