---
slug: vnote-nested-submodule-rebase-gitlink-mapping
title: vnote 双层子模块仓库 rebase 的 gitlink 手动映射
summary: vnote→libs/vtextedit→libs/cmark 双层子模块变基时 gitlink 冲突需先变基子模块再用 update-index --cacheinfo 按旧新 sha 映射逐 pick 解决。
applicable_task_categories:
  - resolve-git-rebase-conflicts
  - git-submodule-maintenance
aliases:
  - gitlink 冲突
  - 子模块 rebase
trigger_phrases:
  - Could not read
  - 子模组内的递归合并
  - libs/vtextedit 冲突
  - update-index --cacheinfo
example_queries:
  - rebase 报 Could not read 某个 sha 怎么处理
  - 父仓 rebase 卡在 libs/vtextedit 子模块冲突
negative_cues:
  - 普通文件内容冲突
applicable_categories:
  - git
  - 子模块
artifact_types:
  - knowledge
updated_at: 2026-09-28
path: playbooks/truth/vnote-nested-submodule-rebase-gitlink-mapping.md
source: aiplat-memory
---
# vnote 双层子模块仓库 rebase 的 gitlink 手动映射

本工程为嵌套子模块结构：`/home/10001178/sources/vnote` 内嵌 `libs/vtextedit`，vtextedit 内又内嵌 `libs/cmark`。

- 变基时每个 bump 过子模块指针的 pick 都会以 `Could not read <sha>` 加“子模组内的递归合并现在只支持简单的情况”失败，git 不会自动合并 gitlink。
- 正确顺序：先完成子模块自身的变基（vtextedit refine → master），得到 旧 sha → 新 sha 映射；再回父仓逐 pick 用 `git update-index --cacheinfo 160000,<新sha>,libs/vtextedit` 解决，随后 `git commit` + `git rebase --continue`。
- vtextedit 内部的 cmark 指针冲突同理，如 `git update-index --cacheinfo 160000,028b668d81faa448fed80ad279cb840e6cb24a2c,libs/cmark`（先 `git -C libs/cmark cat-file -t <sha>` 确认提交存在）。
- 结束后用 `git ls-tree HEAD <路径>` 与 `git -C <子模块> rev-parse HEAD` 核对指针一致。
