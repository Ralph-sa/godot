# Godot Engine — HarmonyOS Port

<p align="center">
  <img src="misc/logo/harmonyos_logo.svg" width="360" alt="HarmonyOS Godot Engine Port">
</p>

> **Godot Engine 鸿蒙（HarmonyOS）移植版本。**
>
> 本分支（`harmonyos`）基于 Godot Engine 主线代码，新增对华为 HarmonyOS（OpenHarmony）平台的原生支持，使 Godot Engine 编辑器 IDE 与游戏能够直接运行在鸿蒙设备上。
>
> | 项目 | 详情 |
> |---|---|
> | **上游项目** | [godotengine/godot](https://github.com/godotengine/godot) |
> | **移植仓库** | [Ralph-sa/godot](https://github.com/Ralph-sa/godot) |
> | **移植分支** | `harmonyos` |
> | **移植开发者** | 多罗（资深软件工程师 / 系统架构师 / 游戏设计师） |
> | **开发者 ID** | `1e77df51-b19d-48e5-8849-ca551f09ed5b` |
> | **目标设备** | 华为 MateBook Pro / MateBook Fold（麒麟 X90 + Maleoon 916） |
> | **目标系统** | HarmonyOS 6.0 / 7（API 24+，兼容 API 26 预览） |
> | **授权协议** | [MIT License](LICENSE.txt)（与上游一致） |

---

## 目标平台技术画像

| 维度 | 规格 |
|------|------|
| **设备** | 华为 MateBook Pro / MateBook Fold |
| **SoC** | 麒麟 X90 — 4+4+2 三丛集，10 核 20 线程，最高 4.2GHz |
| **ISA** | ARM64 (aarch64)，支持超线程 |
| **GPU** | Maleoon 916 — 华为自研，Tile-Based 架构，Vulkan 1.3+ |
| **NPU** | 双达芬奇架构，40 TOPS |
| **OS** | HarmonyOS 6.0 / 7 (API 24+，兼容 API 26 预览) |
| **窗口** | 自由多窗 / 分屏 / 全屏 / 悬浮窗 |
| **输入** | 原生键鼠 + 触控板 + 触屏（多模态） |
| **libc** | musl (非 glibc) |

---

## 开发环境

| 组件 | 版本 |
|------|------|
| DevEco Studio | 26.0.0 Beta2 (26.0.0.621) |
| HarmonyOS SDK | 26.0.0 Beta2 (Ohos_sdk_public 26.0.0.32) |
| API Level | 26.0.0 (Beta) |
| Hvigor | 6.26.1 |
| ohpm | 26.0.0.410 |
| Node.js | 24.14.1 |

---

## 架构概览

本移植采用 **MVVM 分层 + 模块化** 架构，使用 HarmonyOS 6.x 最新开发范式（`@ComponentV2` + `Navigation`）。

```
┌──────────────────────────────────────────────┐
│             ArkUI 层 (@ComponentV2)            │
│  EditorPage.ets  →  ViewModel  →  输入转发      │
├──────────────────────────────────────────────┤
│             NAPI 桥接层 (C++)                   │
│  godot_napi_bridge  ·  harmonyos_main          │
├──────────────────────────────────────────────┤
│          platform/harmonyos/ 平台抽象层          │
│  DisplayServer  ·  NativeWindow  ·  OS Layer   │
│  AudioDriver  ·  Input  ·  Vulkan Context      │
├──────────────────────────────────────────────┤
│           Godot 引擎核心 (C++)                   │
│  EditorNode  ·  Scene  ·  Core  ·  Servers     │
│  Vulkan 渲染后端  (drivers/vulkan/)             │
└──────────────────────────────────────────────┘
```

### 核心架构决策

| ID | 决策 | 理由 |
|----|------|------|
| ADR-01 | 使用 Godot 自带 UI 系统，不重写为 ArkUI | 工作量减少 80%，UI 体验与桌面版一致 |
| ADR-02 | Vulkan 为主渲染后端 | OHOS 6.0 Native 支持最佳，Godot 4.x 默认 |
| ADR-03 | XComponent SURFACE 模式 | 性能最优，支持 Vulkan/OpenGL 直接渲染 |
| ADR-04 | 编辑器 UI 全托管在 XComponent 内 | 避免 ArkUI 与 Godot 的双重 UI 系统冲突 |
| ADR-05 | MVVM + @ComponentV2 | 符合鸿蒙 6.x 最新范式，状态管理清晰 |
| ADR-06 | Navigation + NavPathStack | 官方推荐，废弃 Router |
| ADR-07 | SCons 编译引擎 + DevEco 打包 HAP | 不破坏 Godot 上游构建系统，各司其职 |
| ADR-08 | 仅移植编辑器，暂不做导出模板 | 编辑器是验证平台可行性的最佳目标 |

---

## 移植路线图

### Phase 0: 环境搭建与可行性验证 ✅

- 安装 DevEco Studio 26.0.0 Beta2 + OHOS SDK
- 克隆 Godot 源码
- 验证交叉编译链可用

### Phase 1: 鸿蒙工程骨架 ✅

- 按 `@ComponentV2` + `Navigation` 最新范式创建 Native C++ 工程
- NAPI 桥接层框架

### Phase 2: 平台抽象层（进行中）

- [x] SCons 交叉编译链（`detect.py` + `SCsub`）
- [x] OS 层（`os_harmonyos` + `platform_config.h`）
- [x] DisplayServer（窗口 / 事件循环）
- [x] XComponent → OHNativeWindow → VkSurfaceKHR 桥接

### Phase 3: Vulkan 渲染适配 ✅

- 适配 Vulkan 渲染到 Maleoon 916 GPU（`VK_USE_PLATFORM_OHOS`）

### Phase 4: 输入与系统集成 ✅

- PC 输入系统（键盘 / 鼠标）+ OHAudio + 文件系统

### Phase 5: 编辑器 UI 启动验证

- 验证 EditorNode 完整启动，所有 UI 面板正常渲染

### Phase 6: 构建链集成与真机验证

- SCons → .so → DevEco → .hap 完整流水线
- 模拟器 / 真机测试验证

---

## 时间预估

| Phase | 内容 | 预估 |
|-------|------|------|
| P0 | 环境搭建 + Godot CLI 交叉编译 | 1.5 周 |
| P1 | 工程骨架 + NAPI 桥接 | 1.5 周 |
| P2 | 平台抽象层 (SCons + OS + DisplayServer + NativeWindow) | 4.5 周 |
| P3 | Vulkan 渲染 + Maleoon 916 适配 | 2.5 周 |
| P4 | 编辑器 UI 启动验证 | 2 周 |
| P5 | 输入 + 音频 + 文件系统 | 2.5 周 |
| P6 | 构建链集成 + 真机测试 | 2.5 周 |
| **合计** | | **约 17 周（4 个月）** |

---

## 参考资源

- [HarmonyOS 26.0.0 Beta2 发布说明](https://developer.huawei.com/consumer/cn/doc/harmonyos-releases/2600)
- [状态管理 V2 装饰器指南](https://ost.51cto.com/posts/38700)
- [NAPI 开发 Codelab](https://developer.huawei.com/consumer/cn/codelabsPortal/carddetails/tutorials_NEXT-NativeTemplateDemo)
- [Vulkan 开发指导 (OHOS)](https://www.seaxiang.com/blog/RIFJ3G)
- [Maleoon GPU 最佳实践](https://blog.csdn.net/WEZC156465/article/details/143210006)
- [XEngine Kit](https://developer.huawei.com/consumer/cn/sdk/xengine-kit/)
- [Godot Editor 架构](https://docs.godotengine.org/en/latest/engine_details/editor/introduction_to_editor_development.html)

---

> ---
> 以下为 Godot Engine 原始 README 内容。
> ---

<p align="center">
  <a href="https://godotengine.org">
    <img src="misc/logo/logo_outlined.svg" width="400" alt="Godot Engine logo">
  </a>
</p>

## 2D and 3D cross-platform game engine

**[Godot Engine](https://godotengine.org) is a feature-packed, cross-platform
game engine to create 2D and 3D games from a unified interface.** It provides a
comprehensive set of [common tools](https://godotengine.org/features), so that
users can focus on making games without having to reinvent the wheel. Games can
be exported with one click to a number of platforms, including the major desktop
platforms (Linux, macOS, Windows), mobile platforms (Android, iOS), **HarmonyOS (via this port)**, as well as
Web-based platforms and [consoles](https://godotengine.org/consoles).

## Free, open source and community-driven

Godot is completely free and open source under the very permissive [MIT license](https://godotengine.org/license).
No strings attached, no royalties, nothing. The users' games are theirs, down
to the last line of engine code. Godot's development is fully independent and
community-driven, empowering users to help shape their engine to match their
expectations. It is supported by the [Godot Foundation](https://godot.foundation/)
not-for-profit.

Before being open sourced in [February 2014](https://github.com/godotengine/godot/commit/0b806ee0fc9097fa7bda7ac0109191c9c5e0a1ac),
Godot had been developed by [Juan Linietsky](https://github.com/reduz) and
[Ariel Manzur](https://github.com/punto-) for several years as an in-house
engine, used to publish several work-for-hire titles.

![Screenshot of a 3D scene in the Godot Engine editor](https://raw.githubusercontent.com/godotengine/godot-design/master/screenshots/editor_tps_demo_1920x1080.jpg)

## Getting the engine

### Binary downloads

Official binaries for the Godot editor and the export templates can be found
[on the Godot website](https://godotengine.org/download).

### Compiling from source

[See the official docs](https://docs.godotengine.org/en/latest/engine_details/development/compiling)
for compilation instructions for every supported platform.

## Community and contributing

Godot is not only an engine but an ever-growing community of users and engine
developers. The main community channels are listed [on the homepage](https://godotengine.org/community).

The best way to get in touch with the core engine developers is to join the
[Godot Contributors Chat](https://chat.godotengine.org).

To get started contributing to the project, see the [contributing guide](CONTRIBUTING.md).
This document also includes guidelines for reporting bugs.

## Documentation and demos

The official documentation is hosted on [Read the Docs](https://docs.godotengine.org).
It is maintained by the Godot community in its own [GitHub repository](https://github.com/godotengine/godot-docs).

The [class reference](https://docs.godotengine.org/en/latest/classes/)
is also accessible from the Godot editor.

We also maintain official demos in their own [GitHub repository](https://github.com/godotengine/godot-demo-projects)
as well as a list of [awesome Godot community resources](https://github.com/godotengine/awesome-godot).

There are also a number of other
[learning resources](https://docs.godotengine.org/en/latest/community/tutorials.html)
provided by the community, such as text and video tutorials, demos, etc.
Consult the [community channels](https://godotengine.org/community)
for more information.

[![Code Triagers Badge](https://www.codetriage.com/godotengine/godot/badges/users.svg)](https://www.codetriage.com/godotengine/godot)
[![Translate on Weblate](https://hosted.weblate.org/widgets/godot-engine/-/godot/svg-badge.svg)](https://hosted.weblate.org/engage/godot-engine/?utm_source=widget)
