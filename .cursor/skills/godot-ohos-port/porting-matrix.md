# macOS → OHOS 移植映射矩阵（porting-matrix）

> 本矩阵覆盖 `platform/macos` 全部 **76 个文件 / 21474 行**，逐项映射到 `platform/ohos` 目标文件。
> 状态列取值：`待移植` / `骨架` / `进行中` / `已完成` / `不需要` / `可选`。每轮结束时由管理者更新。

## 统计总览

| 指标 | 数值 |
|---|---|
| macOS 源文件数 | 76 |
| macOS 总行数 | 21,474 |
| 已移植文件数 | 0 / 76 |
| 行数完成度 | 0% |
| 接口覆盖率 | 0% |

## A. 构建系统（阶段 A）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 1 | `detect.py` | 365 | `detect.py` | 待移植 | arm64 交叉编译，DEVECO_SDK_HOME，targetSdk=26 |
| 2 | `SCsub` | 61 | `SCsub` | 待移植 | 产出 libgodot.ohos.editor + .so |
| 3 | `platform_config.h` | 44 | `platform_config.h` | 待移植 | OHOS_ENABLED 等宏 |
| 4 | `platform_thread.h` | 33 | `platform_thread.h` | 待移植 | 线程实现换 pthread |
| 5 | `platform_macos_builders.py` | 124 | `platform_ohos_builders.py` | 待移植 | 构建辅助 |
| 6 | `msvs.py` | 11 | — | 不需要 | Xcode 工程生成，OHOS 用 CMake/hvigor |
| 7 | `macos_quartz_core_spi.h` | 94 | — | 不需要 | Quartz 私有 API，OHOS 无对应 |

## B. OS 层（阶段 B）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 8 | `os_macos.h` | 222 | `os_ohos.h` | 待移植 | 继承 OS_Unix；沙盒路径/生命周期/进程 |
| 9 | `os_macos.mm` | 1313 | `os_ohos.cpp` | 待移植 | 最重文件之一 |
| 10 | `dir_access_macos.h` | 59 | `dir_access_ohos.h` | 待移植 | 沙盒文件访问 |
| 11 | `dir_access_macos.mm` | 118 | `dir_access_ohos.cpp` | 待移植 | |

## C. 应用/窗口原生宿主（阶段 C）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 12 | `godot_application.h` | 48 | `ohos_window.h` | 待移植 | NSApplication → UIAbility/WindowStage |
| 13 | `godot_application.mm` | 196 | `ohos_window.cpp` | 待移植 | |
| 14 | `godot_application_delegate.h` | 46 | `ohos_window.h` | 待移植 | |
| 15 | `godot_application_delegate.mm` | 338 | `ohos_window.cpp` | 待移植 | |
| 16 | `godot_window.h` | 46 | `ohos_window.h` | 待移植 | NSWindow → Window（NAPI 桥） |
| 17 | `godot_window.mm` | 82 | `ohos_window.cpp` | 待移植 | |
| 18 | `godot_window_delegate.h` | 46 | `ohos_window.h` | 待移植 | |
| 19 | `godot_window_delegate.mm` | 395 | `ohos_window.cpp` | 待移植 | |
| 20 | `godot_content_view.h` | 83 | `ohos_xcomponent.h` | 待移植 | NSView → XComponent |
| 21 | `godot_content_view.mm` | 927 | `ohos_xcomponent.cpp` | 待移植 | 输入/绘制核心 |
| 22 | `godot_button_view.h` | 52 | `ohos_xcomponent.h` | 待移植 | 触摸按钮 → XComponent 触摸 |
| 23 | `godot_button_view.mm` | 139 | `ohos_xcomponent.cpp` | 待移植 | |

## D. DisplayServer 核心（阶段 D）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 24 | `display_server_macos_base.h` | 188 | `display_server_ohos.h` | 待移植 | 平台公共实现 |
| 25 | `display_server_macos_base.mm` | 728 | `display_server_ohos.cpp` | 待移植 | |
| 26 | `display_server_macos.h` | 445 | `display_server_ohos.h` | 待移植 | 45 个纯虚 + 桌面增强 |
| 27 | `display_server_macos.mm` | 4054 | `display_server_ohos.cpp` | 待移植 | 最重文件，逐轮加深 |

## E. DisplayServer 嵌入式（阶段 E，可选）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 28 | `display_server_macos_embedded.h` | 221 | `display_server_ohos_embedded.h` | 可选 | 嵌入式调试视图，优先级低 |
| 29 | `display_server_macos_embedded.mm` | 843 | `display_server_ohos_embedded.cpp` | 可选 | |
| 30 | `embedded_gl_manager.h` | 125 | `embedded_gl_manager_ohos.h` | 可选 | |
| 31 | `embedded_gl_manager.mm` | 335 | `embedded_gl_manager_ohos.cpp` | 可选 | |
| 32 | `embedded_debugger.h` | 70 | `embedded_debugger_ohos.h` | 可选 | |
| 33 | `embedded_debugger.mm` | 198 | `embedded_debugger_ohos.cpp` | 可选 | |
| 34 | `editor/embedded_game_view_plugin.h` | 76 | `editor/embedded_game_view_plugin_ohos.h` | 可选 | |
| 35 | `editor/embedded_game_view_plugin.mm` | 179 | `editor/embedded_game_view_plugin_ohos.cpp` | 可选 | |
| 36 | `editor/embedded_process_macos.h` | 140 | `editor/embedded_process_ohos.h` | 可选 | |
| 37 | `editor/embedded_process_macos.mm` | 393 | `editor/embedded_process_ohos.cpp` | 可选 | |

## F. 渲染驱动（阶段 F）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 38 | `rendering_context_driver_vulkan_macos.h` | 55 | `rendering_context_driver_vulkan_ohos.h` | 待移植 | VK_OHOS_surface |
| 39 | `rendering_context_driver_vulkan_macos.mm` | 69 | `rendering_context_driver_vulkan_ohos.cpp` | 待移植 | |
| 40 | `gl_manager_macos_legacy.h` | 95 | — | 不需要 | GLES 2 旧路径，OHOS 用 Vulkan |
| 41 | `gl_manager_macos_legacy.mm` | 211 | — | 不需要 | |
| 42 | `gl_manager_macos_angle.h` | 65 | — | 不需要 | ANGLE，OHOS 无对应 |
| 43 | `gl_manager_macos_angle.mm` | 70 | — | 不需要 | |
| 44 | `platform_gl.h` | 47 | `platform_gl.h` | 待移植 | 若需 GLES 降级 |
| 45 | `platform_egl.h` | 40 | `platform_egl.h` | 可选 | GLES 3.0 降级路径 |

## G. 输入/光标/菜单（阶段 G）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 46 | `key_mapping_macos.h` | 52 | `key_mapping_ohos.h` | 待移植 | 按键映射，复用 Android 键位表思路 |
| 47 | `key_mapping_macos.mm` | 450 | `key_mapping_ohos.cpp` | 待移植 | |
| 48 | `godot_core_cursor.h` | 45 | `cursor_ohos.h` | 待移植 | 自定义光标 |
| 49 | `godot_core_cursor.mm` | 43 | `cursor_ohos.cpp` | 待移植 | |
| 50 | `native_menu_macos.h` | 169 | — | 不需要 | 系统菜单栏，编辑器全自绘 |
| 51 | `native_menu_macos.mm` | 1561 | — | 不需要 | |
| 52 | `godot_menu_delegate.h` | 41 | — | 不需要 | |
| 53 | `godot_menu_delegate.mm` | 124 | — | 不需要 | |
| 54 | `godot_menu_item.h` | 68 | — | 不需要 | |
| 55 | `godot_menu_item.mm` | 52 | — | 不需要 | |
| 56 | `godot_open_save_delegate.h` | 66 | `file_dialog_ohos.h` | 待移植 | FilePicker 桥接 |
| 57 | `godot_open_save_delegate.mm` | 340 | `file_dialog_ohos.cpp` | 待移植 | |
| 58 | `godot_status_item.h` | 47 | — | 不需要 | 菜单栏状态图标 |
| 59 | `godot_status_item.mm` | 83 | — | 不需要 | |
| 60 | `godot_progress_view.h` | 45 | — | 不需要 | 原生进度条，编辑器自绘 |
| 61 | `godot_progress_view.mm` | 94 | — | 不需要 | |

## H. 系统集成（阶段 H）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 62 | `tts_macos.h` | 63 | `tts_ohos.h` | 可选 | 语音合成，API 26 有 textToSpeech |
| 63 | `tts_macos.mm` | 171 | `tts_ohos.cpp` | 可选 | |
| 64 | `crash_handler_macos.h` | 44 | `crash_handler_ohos.h` | 待移植 | 用 OH_HiLog + 异常捕获替代 backtrace |
| 65 | `crash_handler_macos.mm` | 231 | `crash_handler_ohos.cpp` | 待移植 | musl 无 execinfo/backtrace |
| 66 | `stack_trace_macos.h` | 85 | `stack_trace_ohos.h` | 待移植 | |
| 67 | `libgodot_macos.mm` | 73 | `main_ohos.cpp` | 待移植 | NAPI 入口 |
| 68 | `godot_main_macos.mm` | 142 | `main_ohos.cpp` | 待移植 | 引擎主循环 |

## I. 导出器（阶段 I）

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 69 | `export/export.cpp` | 60 | `export/export.cpp` | 待移植 | |
| 70 | `export/export.h` | 34 | `export/export.h` | 待移植 | |
| 71 | `export/export_plugin.cpp` | 2850 | `export/export_plugin.cpp` | 待移植 | 最重文件之一 |
| 72 | `export/export_plugin.h` | 170 | `export/export_plugin.h` | 待移植 | |
| 73 | `export/logo.svg` | 1 | `export/logo.svg` | 待移植 | 需鸿蒙图标 |
| 74 | `export/run_icon.svg` | 1 | `export/run_icon.svg` | 待移植 | |
| 75 | `doc_classes/EditorExportPlatformMacOS.xml` | 759 | `doc_classes/EditorExportPlatformOHOS.xml` | 待移植 | 文档类 |

## J. 其他

| # | macOS 源文件 | 行数 | ohos 目标文件 | 状态 | 备注 |
|---|---|---|---|---|---|
| 76 | `README.md` | 21 | `README.md` | 待移植 | 平台说明文档 |

---

## 每轮更新记录

| 轮次 | 已移植文件数 | 行数完成度 | 接口覆盖率 | 备注 |
|---|---|---|---|---|
| 第 1 轮 | — | — | — | 待执行 |
| 第 2 轮 | — | — | — | 待执行 |
| 第 3 轮 | — | — | — | 待执行 |
| 第 4 轮 | — | — | — | 待执行 |
| 第 5 轮 | — | — | — | 待执行 |
| 第 6 轮 | — | — | — | 待执行 |
| 第 7 轮 | — | — | — | 待执行 |
| 第 8 轮 | — | — | — | 待执行 |
| 第 9 轮 | — | — | — | 待执行 |
| 第 10 轮 | — | — | — | 待执行 |
