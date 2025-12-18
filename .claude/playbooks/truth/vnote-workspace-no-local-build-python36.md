---
slug: vnote-workspace-no-local-build-python36
title: 本工作区无法本地编译验证且系统 python3 为 3.6
summary: 该工作区无构建目录与 Qt 前缀、仅 /usr/bin/cmake，冲突修复只能结构校验；且 python3.6 的 subprocess.run 不支持 capture_output。
applicable_task_categories:
  - resolve-git-rebase-conflicts
  - verify-code-changes
aliases:
  - 验证环境限制
trigger_phrases:
  - 无法编译验证
  - capture_output TypeError
  - build 目录不存在
example_queries:
  - 改完代码想本地编译确认一下
  - 写个 python 脚本做括号平衡检查
negative_cues:
  - 已存在可用构建目录的环境
applicable_categories:
  - 环境事实
  - 验证
artifact_types:
  - knowledge
updated_at: 2026-09-28
path: playbooks/truth/vnote-workspace-no-local-build-python36.md
source: aiplat-memory
---
# 本工作区验证环境限制

- 无现成构建目录：`/home/10001178/sources/vnote/libs/vtextedit/build*` 与 `/home/10001178/sources/vnote/build*` 均不存在，也查不到 CMakeCache 中的 Qt6 路径；工具链仅见 `/usr/bin/cmake`，无 qmake6/ninja 与 Qt 安装前缀——冲突解决后无法本地编译验证。
- 替代验证组合：冲突标记 grep（`^<<<<<<<`）、括号/圆括号 delta 平衡并与两侧 stage 对比、跨文件符号声明与调用一致性（如 Qt 测试槽数量逐名比对）、与目标 ref 的 diff 归类抽查。
- 系统 python3 为 3.6（`/usr/lib64/python3.6/`）：`subprocess.run(..., capture_output=True)` 抛 TypeError，须改用 `subprocess.check_output`；编写验证脚本时注意。
