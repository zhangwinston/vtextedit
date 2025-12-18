---
slug: vnote-refine-submodule-fork-chain
title: vnote refine 分支的子模块 fork 拓扑与推送顺序
summary: vnote refine 链上需修改的子模块（libgit2、vxcore、vtextedit）一律走 zhangwinston fork 的 refine 分支并从最内层逐层向外推送；仅引用的 cmark 保持上游钉 44e388cc。
applicable_task_categories:
  - resolve-git-rebase-conflicts
  - git-submodule-maintenance
  - ci-fix
aliases:
  - fork 推送链
  - 子模块 fork 拓扑
trigger_phrases:
  - 没有权限推送主库
  - fork 一个 repo 合入修改
  - 子模块修改变更推送
  - Could not read 子模组
example_queries:
  - 因主库没有权限，请 fork 一个 repo 并将修改合入 refine 分支
  - 子模块需要修改变更时如何推送
negative_cues:
  - 拥有上游仓库直接推送权限
  - 子模块仅升级指针无需改动内容
applicable_categories:
  - git
  - 子模块管理
  - GitHub
artifact_types:
  - knowledge
updated_at: 2026-09-30
path: playbooks/truth/vnote-refine-submodule-fork-chain.md
source: aiplat-memory
---
# vnote refine 分支的子模块 fork 拓扑与推送顺序

## 私域事实
- 用户规则：各 repo 若需修改变更，必须基于用户 fork 修改；仅引用不改动的子模块保持上游，无需 fork。
- 当前 fork 拓扑（推送顺序必须从最内层到最外层）：
  - libgit2（vxcore 的嵌套子模块 third_party/libgit2）：上游 libgit2/libgit2 无推送权 → fork zhangwinston/libgit2，修复推 refine 分支（eb80a18ec，/GL 门控）；vxcore 的 .gitmodules 已将其 URL 指向该 fork
  - vxcore：vnotex/vxcore 无推送权 → fork zhangwinston/vxcore，修复推 refine 分支（curl 钉 a62501a；b0eeec5 将 libgit2 指向 fork）；父仓 vnote 的 .gitmodules 中 libs/vxcore URL 已改为 fork
  - vtextedit：修复推 zhangwinston/vtextedit 的 refine 分支（如 cmark 重钉的 9805fd5）
  - cmark：仅引用，保持上游 URL，钉在 44e388cc（含 42e04cb 引入的 cmark_parser_set_reference_destination_callback，且为旧 refine 钉 a3dd038 的超集）
- 推送链操作：内层 fork 先 push HEAD:refs/heads/refine → 外层改 .gitmodules URL + git submodule sync + 提交 gitlink bump → 逐层向外；外层 gitlink 引用的 SHA 必须先在远端可达，否则 CI 拉不到子模块。
- 认证与工具：本机 gh CLI 不存在；用 git credential fill 取 token 走 REST API（POST /repos/<upstream>/forks 创建 fork、Actions API 查 CI）。
