# Pull Request

[English](pull_request_template.md)

## 摘要

请说明问题和用户可见结果。

## 范围

- 受影响的示例、文档、CI 或固件源码路径：
- 相关 Issue：
- 兼容性边界或依赖变更：

## 验证

- [ ] `python .github/scripts/check_public_repo.py`
- [ ] 修改 CI 路由或配置时运行 `python .github/scripts/test_discover_esp_idf_examples.py`
- [ ] 修改已覆盖的源码边界时运行 `python .github/scripts/test_review_boundaries.py`
- [ ] 最终 GitHub Actions 检查绑定到经过审阅的 head SHA
- [ ] 已提供硬件运行证据，或明确说明本修改不涉及硬件行为

## 仓库卫生

- [ ] 未包含生成的 `build/`、`managed_components/`、`dependencies.lock`、本地 `sdkconfig` 或私有数据
- [ ] 一方英文文档配有对应的 `_ZH.md` 页面
- [ ] 除经授权的发布更新外，未修改预编译出厂固件
- [ ] 如修改 `firmware/brookesia`，已与示例 CI 范围分开说明
