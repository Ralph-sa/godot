# GodotHMOS 移植项目 · 模块规格集（Specs）

> 状态：待审核（2026-08-17）——所有 spec 由规格驱动开发流程产出，
> 用户审核通过后逐任务执行。验证门禁见技能 godot-ohos-verification
>（模拟器 7 项测试全 PASS）。

## 模块总览与优先级

| # | 模块 | Spec 文件 | 状态 | 优先级 |
|---|---|---|---|---|
| M01 | OS 层 | spec-os.md | ✅ 完成 | — |
| M02 | DisplayServer | spec-display-server.md | ✅ 主体完成 | — |
| M03 | XComponent 宿主 | spec-xcomponent.md | ✅ 主体完成 | — |
| M04 | 输入链 | spec-input.md | ✅ 完成 | — |
| M05 | GLES/EGL 渲染 | spec-gles-egl.md | ✅ 完成 | — |
| M06 | Vulkan 渲染 | spec-vulkan.md | ⏳ 部分 | P1 |
| M07 | 音频输出/输入 | spec-audio.md | ⏳ 输出未验证/输入未实现 | P2 |
| M08 | IME 输入法 | spec-ime.md | ⏳ 代码在未验证 | P2 |
| M09 | 文件对话框与 URI | spec-files.md | ⏳ 部分 | P1 |
| M10 | 子窗口与多窗口 | spec-subwindow.md | ⏳ 部分 | P2 |
| M11 | 手柄输入 | spec-gamepad.md | ⏳ 轴未实现 | P3 |
| M12 | 导出器 | spec-export.md | ⏳ 骨架 | P3 |
| M13 | 构建与签名 | spec-build.md | ✅ 调试可用 | P3（生产化） |
| M14 | 测试体系 | spec-testing.md | ✅ 完成 | — |

## 任务执行约定

1. 每个模块 spec 内的「任务」= 一个 commit；一个任务完成跑一次验证门禁。
2. 优先级 P1 先行；P3（手柄轴/导出器/生产签名）依赖系统能力或发布计划，可延后。
3. 模拟器无法验证画面级行为（express_gpu 不刷新帧），相关验收标准一律
   落到「事件到达/日志/计数」层面，不做画面断言。
