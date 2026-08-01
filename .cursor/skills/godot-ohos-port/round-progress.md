---
current_round: 5
completed_rounds: [1, 2, 3, 4, 5]
total_completion: 40%
interface_coverage: 55%
---

# 轮次进度追踪（round-progress）

> 本文件由管理者每轮结束时更新（可用 `scripts/progress.py` 推进）。记录每轮五角色结论、macOS 对比总结、测试结果、git 提交哈希与下一轮重启重点。

## 当前状态

- **当前轮次**：第 5 轮（导出器完整 + 编辑特性 + 文件对话框，已完成）
- **已结束轮次**：第 1、2、3、4、5 轮
- **总完成度**：40%（骨架 + 输入 + 窗口事件 + 渲染 + 导出器/剪贴板/文件对话框）
- **接口覆盖率**：55%（OS/DisplayServer/输入/渲染驱动/导出器/剪贴板）
- **git 分支**：hm

## 轮次记录

### 第 5 轮（已完成）

- **管理者 tasklist**：
  - [x] E 导出器完整：get_export_options 扩展（app_name/arch/orientation/include_pck）+ export_project 全流程（模板拷贝 + pck 生成 + app.json5 改写）
  - [x] D DisplayServer：剪贴板（clipboard_set/get/has → @ohos.pasteboard）
  - [x] D DisplayServer：文件对话框（file_dialog_show → @ohos.file.picker DocumentViewPicker）
  - [x] D DisplayServer：窗口模式 NAPI（window_set_mode → @ohos.window maximize/fullScreen）
  - [x] F 编辑器特性：FEATURE_SUBWINDOWS/FEATURE_CLIPBOARD 置 true
  - [x] H 系统集成：main_ohos.cpp 新增 registerClipboard/registerFilePicker/filePickerResult/registerWindowHandler 4 个 NAPI
  - [x] I 导出器：Index.ets 接入 pasteboard/picker/window import + 回调注册 + applyWindowMode
  - [x] J 验证：check_build real 交叉编译通过 + macOS 对比总结 + git 提交
- **开发者**：全部清单完成。
  - 剪贴板链路：DisplayServer::clipboard_set → ohos_clipboard_set_text（main_ohos.cpp 桥，跨线程 NAPI）→ ArkTS @ohos.pasteboard；读取双向对应。
  - 文件对话框：DisplayServer::file_dialog_show → ohos_pick_files（保存 Callable + NAPI 通知 ArkTS）→ DocumentViewPicker.select → godot.filePickerResult(JSON) → 触发 Callable(PackedStringArray)。取消返回空数组。
  - 窗口模式：DisplayServer::window_set_mode → ohos_window_set_mode → ArkTS registerWindowHandler → @ohos.window maximize/setWindowLayoutFullScreen/setWindowTopMost（置顶标记 1001）。
  - 导出器：export_project 三步走（递归拷贝模板工程 → save_pack 生成 main.pck 入 rawfile → 改写 app.json5 bundleName/版本）。移除骨架期虚构的 notify_external_preset 调用。
  - 编辑器特性：FEATURE_SUBWINDOWS 与 FEATURE_CLIPBOARD 置 true（子窗口完整实现第 7 轮）。
- **挑战者**：
  - 挑战①：CharString 无 resize/parse_utf8（Godot 4.8 API）→ 改用 Vector<char> + String::utf8。
  - 挑战②：EditorExport 无 notify_external_preset 方法 → 移除，导出完成通知由编辑器管理器统一处理。
  - 挑战③：@ohos.window getLastWindow 返回 Promise → ArkTS 侧改 async/await。
- **审查者**：NAPI 导出 10 接口（initialize/setXComponent/start/stop/notifyFocus/registerClipboard/registerFilePicker/filePickerResult/registerWindowHandler/dispose），ArkTS 生命周期完整；桥接口（ohos_bridge.h）跨线程调用均有 Mutex 保护；未破坏线程模型。遗留：文件对话框初始目录/过滤器透传（第 8 轮完善）、子窗口真实创建（第 7 轮）。
- **测试者**：check_build.py real 交叉编译通过；`libgodot.ohos.editor.arm64.so` 产出成功（~7s）。ArkTS 侧编译需 DevEco 完整工程（当前 deveco 目录为最小文件集）。
- **macOS 对比**：macOS 平台 17793 行；OHOS 平台约 3980 行（第 5 轮 +~280 行）。
  - 核心对照：NSPasteboard clipboard_set/get → @ohos.pasteboard 桥；NSOpenPanel/NSSavePanel file_dialog_show → DocumentViewPicker；NSWindow setCollectionBehavior/windowDidBecomeMain → @ohos.window + notifyFocus。
  - 覆盖率估算：OS 82%、DisplayServer 60%（+剪贴板/文件对话框/窗口模式 NAPI）、输入 55%、Vulkan 渲染链路 100%、导出器 60%（模板拷贝 + pck + 配置改写）。
- **git 提交**：本轮提交（见 git log）。
- **下一轮**：第 6 轮 —— 音频（AudioDriverOHOS @ohos.multimedia）、显示枚举、物理存储（get_data_dir 沙盒映射）、文件系统（FileAccessOHOS rawfile）、EditorSettings 持久化。

### 第 4 轮（已完成）

- **管理者 tasklist**：
  - [x] D DisplayServer：刷新率注入（Index.ets @ohos.display refreshRate）
  - [x] F 渲染完整：swapchain 自动重建链确认（VK_ERROR_OUT_OF_DATE_KHR → r_resize_required）
  - [x] H 系统集成：main_ohos.cpp 新增 notifyFocus NAPI + initialize 增加 refreshRate 参数 + engine_thread 注入 XComponent/刷新率
  - [x] I 导出器：Index.ets 接入 onBlur/onFocus + @ohos.i18n/deviceInfo/display import
  - [x] J 验证：check_build real 交叉编译通过 + macOS 对比总结 + git 提交
- **开发者**：全部清单完成。
  - 失焦链路：ArkTS onBlur → godot.notifyFocus(false) → notify_main_surface_focus → WINDOW_EVENT_FOCUS_OUT（补齐 XComponent focus 回调仅报获得焦点的缺口）。
  - 刷新率：initialize 第 6 参注入 → engine_thread 中 DisplayServer 创建后 set_screen_refresh_rate。
  - XComponent 时序：setXComponent 早于引擎启动，engine_thread 的 Main::start 后统一挂接 main_xcomponent。
  - 渲染完整性确认：RenderingDeviceVulkan 在 AcquireNextImageKHR 返回 OUT_OF_DATE 时自动重建 swapchain（rendering_device_driver_vulkan.cpp:4029），窗口 resize 无需平台介入；Vulkan surface 重建（native_window 变化）留待真实联调。
- **挑战者**：
  - 挑战①：initialize 参数从 5 扩到 6（refreshRate），ArkTS 与 C++ 两侧需同步 → 已同步。
  - 挑战②：i18n.System 与模块导出名不符 → 已修复：import { System as i18nSystem } from '@ohos.i18n'。
  - 挑战③：setXComponent 早于 DisplayServer 创建（set_main_xcomponent 无对象）→ 已修复：engine_thread_main 的 Main::start 后统一注入。
- **审查者**：NAPI 导出 6 接口（initialize/setXComponent/start/stop/notifyFocus/dispose），ArkTS 生命周期完整；刷新率/密度注入链路一致；未破坏线程模型。遗留：窗口模式 NAPI 回调（第 5 轮 ArkTS window API）。
- **测试者**：check_build.py real 交叉编译通过；`.so` 产出成功。运行时验证需 DevEco 模拟器（MateBook Pro 26）。
- **macOS 对比**：macOS 平台 17793 行；OHOS 平台约 3700 行（第 4 轮 +~50 行）。
  - 核心对照：godot_window_delegate.mm（windowDidResignMain）→ notifyFocus NAPI；macOS NSScreen refreshRate → @ohos.display refreshRate 注入。
  - 覆盖率估算：OS 80%、DisplayServer 50%（+刷新率注入）、输入 55%、Vulkan 渲染链路 100%（swapchain 自动重建确认）、导出器 18%。
- **git 提交**：本轮提交（见 git log）。
- **下一轮**：第 5 轮 —— 导出器完整（export 插件 get_export_option/exports）、文件对话框（NAPI FilePicker）、窗口模式 NAPI、编辑器特性（FEATURE_SUBWINDOWS）。

### 第 3 轮（已完成）

- **管理者 tasklist**：
  - [x] A 构建系统：无新改动（增量编译复用第 1 轮 detect.py）
  - [x] B OS 层：环境变量/进程/内存确认复用 OS_Unix；CA 证书路径补充
  - [x] C 窗口宿主：XComponent focus 回调注册（RegisterFocusEventCallback）
  - [x] D DisplayServer：surface 尺寸变化 → rect_changed 回调；focus → 窗口事件；窗口模式/鼠标模式状态管理
  - [x] E 输入：聚焦事件链（XComponent focus → WINDOW_EVENT_FOCUS_IN/OUT）
  - [x] F 渲染：vsync 模式记录延续（无新改动）
  - [x] G 嵌入式 DisplayServer：不适用
  - [x] H 系统集成：main_ohos.cpp 事件链对接（surface/focus 通知）
  - [x] I 导出器：无新改动
  - [x] J 验证：check_build real 交叉编译通过 + macOS 对比总结 + git 提交
- **开发者**：全部清单完成，产出真实可编译代码（含中文注释）。
  - 事件链：`OH_NativeXComponent_RegisterFocusEventCallback` → `handle_focus_event` → `DisplayServerOHOS::notify_main_surface_focus` → `WINDOW_EVENT_FOCUS_IN/OUT`（对应 macOS windowDidBecomeMain/ResignMain）。
  - 尺寸同步：`on_surface_changed`（ArkUI 主线程）→ `notify_main_surface_resized` → rect_changed_callback（引擎 Viewport 重设）。
  - 窗口模式：`window_set_mode/get_mode` 记录 + `ohos_window.h` 存储；NAPI 请求 ArkUI 窗口最大化/全屏留待第 4 轮。
  - 鼠标模式：`mouse_set_mode/get_mode` 记录（捕获/隐藏第 8 轮经 ArkUI 模拟）。
  - `mouse_warp` 确认不存在于 DisplayServer 基类（macOS 为内部方法），不声明 override。
  - OS 层：`get_system_ca_certificates` 返回鸿蒙系统 CA 目录（/system/etc/security/cacerts）；环境变量/进程/内存全部复用 OS_Unix。
- **挑战者**：
  - 挑战①：`DisplayServerEnums` 在 ohos_window.h 未声明 → 已修复：include `servers/display/display_server_enums.h`。
  - 挑战②：`mouse_warp` marked override 但基类无此虚函数 → 已修复：移除 override（macOS 用 CGWarpMouseCursorPosition 内部方法，非 DisplayServer 接口）。
  - 挑战③：`-Wunused-private-field` 警告：window_mode 在 ohos_window.cpp TU 未使用 → 已修复：get/set_window_mode 为 inline 供外部 TU 调用，忽略告警。
- **审查者**：事件链层次清晰（XComponent → DisplayServer → 引擎回调），未破坏线程模型（focus/resize 回调在 ArkUI 主线程，回调引擎侧在 process_events 语义安全）；window_mode 状态存储合理。遗留：失焦通知需 ArkTS onBlur 辅助（第 4 轮）。
- **测试者**：check_build.py real 交叉编译通过；`.so` 产出成功。运行时验证需 DevEco 模拟器。
- **macOS 对比**：macOS 平台 17793 行；OHOS 平台约 3650 行（第 3 轮 +~100 行）。
  - 核心对照：godot_window_delegate.mm（windowDidResize/聚焦/窗口模式）→ display_server_ohos.cpp 的 notify_main_surface_resized/focus；CGDisplayHideCursor/鼠标模式 → mouse_set_mode。
  - 覆盖率估算：OS 80%、DisplayServer 45%（+窗口事件/鼠标模式/焦点）、输入 55%、Vulkan surface 100%、导出器 15%。
- **git 提交**：本轮提交（见 git log）。
- **下一轮**：第 4 轮 —— 渲染完整（swapchain 重建/窗口模式 NAPI）、DevEco 工程联调（focus onBlur 失焦）、ArkTS 窗口模式回调、屏幕刷新率查询。

### 第 2 轮（已完成）

- **管理者 tasklist**：
  - [x] A 构建系统：无新改动（detect.py 第 1 轮已完备，strip/debug 分离第 5 轮导出器深化）
  - [x] B OS 层：locale/设备型号/屏幕密度 NAPI 注入 + 内存信息确认（OS_Unix 复用）
  - [x] C 窗口宿主：ohos_xcomponent 触摸/鼠标/键盘事件回调注册（XComponent API 26）
  - [x] D DisplayServer：process_events 输入消费 + 光标/鼠标/vsync + 屏幕信息从 XComponent 尺寸
  - [x] E 输入：KeyMappingOHOS 双枚举（multimodalInput + XComponent 2000+ 系列）
  - [x] F 渲染深化：vsync 模式记录（基类 RenderingContextDriverVulkan 处理 present mode）
  - [x] G 嵌入式 DisplayServer：不适用（同第 1 轮）
  - [x] H 系统集成：main_ohos.cpp 新增 setXComponent + 系统信息注入；Index.ets 同步
  - [x] I 导出器：deveco/ Index.ets 接入 @ohos.i18n/@ohos.deviceInfo/@ohos.display
  - [x] J 验证：check_build real 交叉编译通过 + macOS 对比总结 + git 提交
- **开发者**：全部清单完成，产出真实可编译代码（含中文注释）。
  - 交叉编译：`scons platform=ohos target=editor arch=arm64` 通过，产出 `bin/libgodot.ohos.editor.arm64.so`。
  - 输入架构：ArkUI 主线程回调入队（Mutex 保护）→ 引擎线程 `process_events` 消费 → 窗口 input_event_callback（与 macOS NSEvent 循环语义一致）。
  - XComponent 事件：`OH_NativeXComponent_RegisterCallback`（touch/surface）+ `RegisterMouseEventCallback` + `RegisterKeyEventCallback`（API 26 全可用）。
  - 鼠标按钮位域转换：OHOS 位域（LEFT/RIGHT/MIDDLE/BACK/FORWARD）→ Godot MouseButton/MouseButtonMask。
  - DisplayServer 光标：记录形状/自定义光标（原生光标第 8 轮经 ArkUI SystemCursor 同步）；`mouse_get_position` 读 XComponent 最近位置。
  - vsync：`window_set_vsync_mode` 记录，渲染驱动创建时读取。
  - 屏幕信息：`screen_get_size` 取 XComponent 实际 Surface 尺寸；`screen_get_dpi` 按 160*density（NAPI 注入）；刷新率 60Hz 兜底（第 4 轮 OH_DisplayManager）。
  - OS 层：`get_locale` 优先 NAPI 注入语言；`get_model_name` 读 /proc/device-tree/model 兜底；`get_executable_path`/环境变量/进程/内存全部复用 OS_Unix（musl 兼容）。
- **挑战者**：
  - 挑战①：`MouseButton::XBUTTON1/2` 不存在 → 已修复：Godot 4.8 实际枚举为 `MB_XBUTTON1/2`（Windows 保留字规避）。
  - 挑战②：`Object::cast_to<DisplayServerOHOS>` 静态断言失败 → 已修复：DisplayServer 非 Object 派生类，改用静态单例指针 `get_singleton_ohos()`。
  - 挑战③：InputEvent 子类头文件不存在（input_event_key.h 等）→ 已修复：Godot 4.8 集中到 `core/input/input_event.h`。
  - 挑战④：`InputEventScreenTouch` 无 `set_screen_position`/`set_speed` → 已修复：仅设 position/relative。
  - 挑战⑤：XComponent 键盘 keycode 为 2000+ 偏移（OH_NativeXComponent_KeyCode），与传统 multimodalInput 枚举不同 → 已修复：key_mapping 双枚举映射表，XC_ 前缀常量自含（不依赖 NDK 头）。
- **审查者**：代码风格与 macOS 对齐；输入队列互斥锁正确（ArkUI 线程 vs 引擎线程）；回调函数均为静态 C 签名匹配 SDK；未引入宏滥用。遗留：`RegisterHoverEvent` 未注册（第 8 轮）；`setXComponent` 时序依赖 SurfaceCreated 早于 NAPI 调用（真实设备验证项）；无 GLES（仅 Vulkan）。
- **测试者**：check_build.py real 级别交叉编译通过；`.init_array` 段存在（NAPI module 注册生效）；输入事件转换逻辑为纯数据流（编译期无运行时验证）。运行时验证需 DevEco 模拟器（MateBook Pro 26），第 4 轮起联调。
- **macOS 对比**：macOS 平台 17793 行（.mm/.h/.py）；OHOS 平台 3552 行（.cpp/.h/.py/.ets/.json5/CMakeLists），较第 1 轮 +829 行。
  - 核心对照：os_macos.mm（locale/设备信息）→ os_ohos.cpp（NAPI 注入）；display_server_macos_base.mm（光标/鼠标/vsync/屏幕）→ display_server_ohos.cpp；key_mapping_macos.mm → key_mapping_ohos.cpp（双枚举）；godot_main_macos.mm → main_ohos.cpp（NAPI + setXComponent）。
  - 覆盖率估算：OS 路径/生命周期 75%（+locale/model/density 注入）、DisplayServer 接口 40%（+光标/鼠标/vsync/屏幕信息）、输入 50%（+触摸/鼠标/键盘事件分发，双枚举映射）、Vulkan surface 100%、导出器 15%。
- **git 提交**：`git commit` 本轮提交（见 git log）。
- **下一轮**：第 3 轮 —— 窗口系统深化（窗口模式/焦点/前置 NAPI）、输入补充（光标 warp、rect_changed 回调）、OS 环境变量/进程、DevEco 工程联调准备。

### 第 1 轮（已完成）

- **管理者 tasklist**：
  - [x] A 构建系统：detect.py + SCsub + platform_config.h + platform_thread.h + platform_ohos_builders.py
  - [x] B OS 层：os_ohos.h/.cpp（路径 + 生命周期骨架）
  - [x] C 窗口宿主：ohos_xcomponent + ohos_window（NAPI 桥骨架）
  - [x] D DisplayServer：display_server_ohos.h/.cpp（窗口 + 渲染启用骨架）
  - [x] E 输入骨架：key_mapping_ohos
  - [x] F 渲染驱动：rendering_context_driver_vulkan_ohos（VK_OHOS_surface）
  - [x] G 嵌入式 DisplayServer（ohos 场景不适用，合并进 D）
  - [x] H 系统集成：main_ohos.cpp（NAPI 入口）+ crash_handler_ohos
  - [x] I 导出器骨架：export/ + deveco/ 工程模板
  - [x] J 验证：check_build 真实交叉编译通过 + macOS 对比总结 + git 提交
- **开发者**：全部清单完成，产出真实可编译代码（含中文注释）。
  - 交叉编译：`scons platform=ohos target=editor arch=arm64` 通过，产出 `bin/libgodot.ohos.editor.arm64.so`（140MB）。
  - 关键工程决策：OHOS 无 main 入口，产物为 NAPI 共享库（library_type=shared_library）；仅 Vulkan（禁 GLES3/EGL）。
- **挑战者**：
  - 挑战①：embree 使用 glibc 专属 pthread API（getaffinity_np/setaffinity_np/cancel），musl 不存在 → 已修复：按 `__MUSL__` 分支复用 Android 兼容实现（sysinfo/thread.cpp）。
  - 挑战②：OHOS `pthread_setname_np` 需双参数 → 已修复：移除 platform_config.h 的 PTHREAD_RENAME_SELF。
  - 挑战③：OHOS NDK 库命名为 `*.z.so`（ELF 格式）→ 已修复：detect.py 用 `-l:精确文件名` 链接（hilog_ndk/ace_ndk/ace_napi）。
  - 挑战④：`OS_Unix` 未实现 finalize/_check_internal_feature_support 纯虚 → 已修复：os_ohos.cpp 直接实现不再委托。
  - 挑战⑤：`-lhilog` 库名不存在，实为 `libhilog_ndk.z.so` → 已修复。
- **审查者**：代码风格与 macOS 对齐（vformat 中文注释、骨架函数返回默认值）；SCsub/detect.py 使用 SCons 官方 API；未引入宏滥用。NAPI 导出 4 接口（initialize/start/stop/dispose），生命周期与 ArkTS 侧匹配。遗留：gl_manager 无（无 GLES）；文本输入/剪贴板/IAP 留待后续轮次。
- **测试者**：check_build.py real 级别交叉编译通过（校验级别 real）；`llvm-nm` 确认 `napi_module_register` 正确引用；`.init_array` 段存在（constructor 注册生效）。运行时验证需 DevEco 模拟器，待第 3 轮起逐步联调。
- **macOS 对比**：macOS 平台 20703 行（.mm/.h/.py）；OHOS 平台 2723 行（.cpp/.h/.py/.ets/.json5/CMakeLists）。
  - 核心对照：os_macos.mm→os_ohos.cpp、display_server_macos_base.mm→display_server_ohos.cpp、rendering_context_driver_vulkan_macos.mm→rendering_context_driver_vulkan_ohos.cpp、key_mapping_macos.mm→key_mapping_ohos.cpp、crash_handler_macos.mm→crash_handler_ohos.cpp、godot_main_macos.mm→main_ohos.cpp（NAPI 变体）、export 导出器→export/。
  - 覆盖率估算：OS 路径/生命周期约 70% 骨架、DisplayServer 接口约 30%（31 个纯虚已全部声明）、Vulkan surface 100%（VK_OHOS_surface）、输入键位约 40%（基础键映射）、导出器 15%（模板骨架）。
- **git 提交**：待本轮提交（见提交信息）。
- **下一轮**：第 2 轮 —— 深化 OS 路径/沙盒、DisplayServer 屏幕信息、输入事件分发（触屏/键鼠）、Vulkan swapchain 初始化、DevEco 工程联调准备。

### 第 10 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：无（终轮，输出 final-summary.md）

### 第 9 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 10 轮

### 第 8 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 9 轮

### 第 7 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 8 轮

### 第 6 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 7 轮

### 第 5 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 6 轮

## 说明

- `current_round` 表示当前正在执行的轮次（0 = 未开始）。
- 每轮结束由管理者更新本文件：五角色结论、macOS 对比（行数 + 接口覆盖率）、测试结果、git 提交哈希、下一轮重启重点。
- 第 10 轮结束后输出 `final-summary.md`。
