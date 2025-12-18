---
slug: vxcore-curl-pin-openssl-1x-compat
title: vxcore curl 钉 8.17.0 的 OpenSSL 1.x 兼容约束
summary: vxcore 钉 curl 8.17.0 因 curl ≥8.18.0 硬性要求 OpenSSL 3.0，rockylinux:8 CI 腿只有 1.1.1k；且 CURL_USE_OPENSSL 被普通变量遮蔽无法用 -D 关闭。
applicable_task_categories:
  - resolve-git-rebase-conflicts
  - ci-fix
  - dependency-management
aliases:
  - curl OpenSSL 兼容
  - vxcore 依赖钉
trigger_phrases:
  - OpenSSL 3.0.0 or greater required
  - OPENSSL_VERSION_STRING undeclared
  - curl 编译失败
  - rockylinux:8 CI 失败
example_queries:
  - CI 里 libcurl 编译报 OpenSSL 3.0 required 怎么处理
  - 升级 vxcore 后 curl 相关 CI 失败
negative_cues:
  - 所有目标环境均为 OpenSSL 3.x
  - 项目不使用 WebDAV/curl
applicable_categories:
  - 构建系统
  - CI
  - 依赖管理
artifact_types:
  - knowledge
updated_at: 2026-09-30
path: playbooks/truth/vxcore-curl-pin-openssl-1x-compat.md
source: aiplat-memory
---
# vxcore curl 钉 8.17.0 的 OpenSSL 1.x 兼容约束

## 私域事实
- vxcore 的 third_party/CMakeLists.txt 用 FetchContent 拉 curl 发布 tarball；WebDAV 同步后端依赖它（非 Apple 平台），随 rebase 推进 vxcore 指针后首次进入 vnote CI。
- curl 自 8.18.0（PR #18330）起 OpenSSL 后端硬性要求 OpenSSL ≥ 3.0.0：在 1.1.x 头文件下编译 vtls/openssl.c 报 #error "OpenSSL 3.0.0 or greater required" 与 OPENSSL_VERSION_STRING undeclared。
- 触发环境：CI-Linux-Qt5 的 rockylinux:8 容器腿（openssl-devel 1.1.1k）；18.22.0 钉曾在该腿失败。
- 修复：钉 curl 8.17.0（最后一个 OpenSSL 后端仍支持 1.0.2+/1.1.x 的版本，且兼容 OpenSSL 3），提交 a62501a 位于 zhangwinston/vxcore fork 的 refine 分支；已在 OpenSSL 1.1.1o 上全量静态构建验证（libcurl_static + libgit2 + WebDAV 源全部编译链接通过）。
- 陷阱一：third_party/CMakeLists.txt 内 set(CURL_USE_OPENSSL ON) 是普通变量，CMP0077 NEW 下遮蔽命令行 -DCURL_USE_OPENSSL=OFF 的 cache 值，无法用 -D 关闭 OpenSSL 后端规避。
- 陷阱二：curl tarball 不内置任何备用 TLS 库（mbedtls/wolfssl/bearssl 均缺），关掉 OpenSSL 即无 https 的 WebDAV 传输。
- 后续约束：升级 vxcore 或 bump curl 版本前，先确认所有 CI 腿的 OpenSSL 版本，或为旧腿提供 OpenSSL 3 工具链。
