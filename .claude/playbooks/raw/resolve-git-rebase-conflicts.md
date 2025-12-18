---
task_type_slug: resolve-git-rebase-conflicts
task_type_title: 解决 Git Rebase 冲突（长分支变基含子模块）
summary: 以孪生提交 patch-id、own-diff 判定与并集合并逐轮解决长分支变基冲突；子模块 gitlink 用 update-index 手动映射且需按并集需求重验指针，最后经 fork 推送链与 CI 闭环验证。
applies_when:
  - 长特性分支变基到 master 需逐 pick 处理多轮冲突
  - 仓库含子模块且 pick 涉及指针 bump
  - 双分支存在同主题不同内容的孪生提交
  - 变基后需推送远端并经 CI 验证（子模块可能无上游推送权限）
do_not_use_when:
  - 单文件少量冲突直接手工编辑
  - 只想 abort/重置 rebase
  - 无终态参照且无法判定取舍的合并
aliases:
  - rebase conflict resolution
  - 变基冲突处理
  - gitlink 冲突解决
trigger_phrases:
  - rebase master 出现冲突
  - 处理 rebase 冲突
  - 变基冲突请分析并处理
  - 子模块指针冲突
  - Could not read 子模组递归合并
  - 父目录 rebase 冲突
  - rebase 后 CI 编译失败
example_queries:
  - 当前目录 rebase master 出现一些待处理的冲突，请分析并处理一下
  - 请继续处理父目录下的 rebase 冲突
negative_cues:
  - git merge 只有一两个文件冲突
  - 帮我 abort 这次 rebase
applicable_categories:
  - git
  - 版本控制
  - 代码合并
  - 子模块管理
  - CI
artifact_types:
  - playbook
updated_at: 2026-09-30
path: playbooks/raw/resolve-git-rebase-conflicts.md
source: aiplat-memory
---
# 解决 Git Rebase 冲突（长分支变基含子模块）

## 适用场景
- 长生命周期特性分支变基到 master，需逐 pick 处理多轮冲突
- 仓库含子模块（gitlink），pick 序列中有 bump 子模块指针的提交
- 双分支存在“孪生提交”：同一功能在两侧各自演化，主题相同但内容不同
- 变基后需推送远端并经 CI 验证，且子模块仓库可能无上游推送权限（需走 fork 链）

## 不适用场景
- 单文件少量冲突的普通 rebase/merge，直接手工编辑即可
- 未明确终态参照（两侧 tip）的合并
- 用户只想 abort 或重置 rebase

## 执行步骤

### 第 1 步：侦察全局格局
1. git status 确认 rebase 状态；git rev-parse --git-dir 定位 rebase-merge/，读 git-rebase-todo（剩余 pick）、done（已完成）、onto
2. git log --oneline <merge-base>..<branch> 与 <merge-base>..<onto> 分别列出两侧新增提交，按主题配对识别孪生提交
3. 对每对孪生提交计算 git show <c> | git patch-id --stable：patch-id 相同则完全等价可 skip；不同则需逐 hunk 分析

### 第 2 步：逐轮分析冲突（每个 pick 一轮）
1. 提取 pick 自身增量：git diff <pick>^ <pick> -- <file>——这是“应当引入什么”的唯一判定标准
2. 三方对比：git show :2:<file>（ours/变基后 HEAD）vs git show :3:<file>（theirs/pick 侧）vs 工作区
3. 按判定表选择解法；theirs 侧其余差异属于旧基线残留，不在 pick own diff 中，一律不引入

### 第 3 步：子模块 gitlink 冲突
- git 不做递归子模块合并，报 Could not read <sha> / “子模组内的递归合并现在只支持简单的情况”
- 先完成子模块自身变基，记录 旧 sha → 新 sha 映射表
- 父仓逐 pick 执行 git update-index --cacheinfo 160000,<新sha>,<子模块路径>，再 git commit + git rebase --continue
- 子模块指针不可机械“忠实重放”refine 旧钉：变基后的树携带 master 侧代码时，旧钉可能缺少新代码所需 API（实测：master 代码调用 cmark_parser_set_reference_destination_callback，该 API 由 cmark 42e04cb 引入，重放的旧钉 a3dd038 早于它，导致全部 CI 腿在 markdownastwalker.cpp 编译失败）。先用 git merge-base --is-ancestor 验证候选钉血统、grep 验证所需 API 存在，再选包含双方内容的超集提交（如 master 的 44e388cc）并单独提交说明

### 第 4 步：推进与提交
- 内容冲突解决后 git add <files>；若 rebase --continue 报“工作区中存在已暂存的修改”，先 GIT_EDITOR=true git commit 落位再 continue
- ours 已完整涵盖 pick 效果形成空提交时用 git rebase --skip

### 第 5 步：每轮验证
1. 冲突标记全仓扫描：grep -rn '^<<<<<<< ' src/ tests/ 为空
2. 括号平衡：{/}、(/) delta 应为 0，并与两侧 stage 的 delta 对比（ours 原有失衡则继承同值）
3. 跨文件符号一致性：头文件声明数 == cpp 定义数 == 调用点（如 Qt 测试槽声明与定义逐名比对）；并核对全部调用点参数个数与头文件签名一致
4. 与两侧 tip 做 diff 归类：差异应全部可解释为对侧独有内容
5. 子模块指针：git ls-tree HEAD <路径> 与 git -C <子模块> rev-parse HEAD 一致

### 第 6 步：无上游推送权限时的 fork 链
- 需修改的子模块一律走用户 fork：用 git credential fill 取 token 走 REST API 创建/查询 fork（gh CLI 未必安装），把修复提交推到 fork 的 refine 分支
- 修改父级 .gitmodules 中该子模块 URL 指向 fork，git submodule sync，再提交 gitlink bump
- 推送顺序从最内层嵌套子模块开始逐层向外（例：libgit2 → vxcore → vnote），外层 gitlink 引用的 SHA 必须已在远端可达
- 仅引用不改动的子模块保持上游 URL 与既有提交，无需 fork

### 第 7 步：远端 CI 验证闭环
- 推送后用 Actions API（runs?head_sha=<sha>）后台轮询全部工作流；失败则拉 jobs/<id>/logs 定位首个编译/测试错误
- 本地无法编译验证（无 Qt 前缀/构建目录）时，CI 是唯一编译验证手段；优先修跨腿共因（缺 API/签名不匹配），再判单腿偶发（超时类先 rerun-failed 排除环境 flake）

## 判定表
| 情形 | 解法 |
|---|---|
| 孪生提交 patch-id 相同 | git rebase --skip |
| ours 为 pick 效果的严格超集（含演化等价形式） | 取 ours verbatim（用 git show :2:file 覆盖工作区），随后 skip |
| pick 含 ours 缺失的新增效果 | 并集合并：ours 为基底，仅叠加 own-diff 增量 |
| theirs-only 内容不在 pick own diff 中 | 丢弃，不引入 |
| 重放旧子模块钉致 API 缺失 | 改钉为含双方内容的上游超集提交并单独提交 |

## 常见陷阱
1. 并集拼接少闭合括号：两侧 stage 括号 delta 均为 0 而合并结果 ±1，说明冲突区共享尾部 } 只能闭合一侧函数，需在拼接点手工补一个 }（删 marker 后立即查括号平衡）
2. 把 theirs 旧基线误当 pick 内容：只认 git diff pick^ pick 的增量，不认 stage 3 全量（旧类名、旧签名、旧 helper）
3. pick 的删除可能已自动合入冲突区外：需 grep 确认废弃符号确已消失；反之死代码也可能藏在冲突区外未被清理
4. rebase --continue 被“已暂存修改”阻塞：先 commit 再 continue，不要 --skip 丢提交
5. 权限分类器瞬时拦截（Auto mode could not evaluate this action）：原样重试，或拆成更简单的只读命令；heredoc/复合命令更易被拦
6. 并行 agent 互踩 /tmp 临时文件：每个 agent 用唯一临时路径
7. 含 GBK 乱码注释的冲突块：分段小步 Edit，不要一次性整块替换
8. 机械重放子模块旧钉：变基后树的调用集合 ≠ refine 原树的调用集合，钉必须满足并集需求（见第 3 步）
9. 声明与调用点不同步：并集合并新参数后，冲突区外自动合并的旧调用点只有 CI 编译才暴露；解后立即 grep 全部调用点核对签名
10. CI 单测超时未必是回归：同 run 另一腿或 rerun 通过即为环境 flake，先重跑再改代码

## 快速检查清单
- [ ] grep -c '^<<<<<<<' <每个冲突文件> == 0
- [ ] 括号 delta == 0（或与 stage 基线一致）
- [ ] git diff --cached --stat HEAD 仅含预期文件
- [ ] git ls-tree HEAD <子模块> 指针符合映射表且经血统/API 双重验证
- [ ] 落位提交数 == 原 pick 数（skip 的除外）
- [ ] 最终 git status 干净且提示“成功变基并更新”
- [ ] fork 链各层已推送，外层 gitlink 指向的 SHA 远端可达
- [ ] CI 全部工作流绿（或已确认环境性 flake 并重跑通过）
