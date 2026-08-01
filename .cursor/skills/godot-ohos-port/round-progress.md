---
current_round: 2
completed_rounds: [1, 2]
total_completion: 16%
interface_coverage: 25%
---

# 轮次进度追踪（round-progress）

> 本文件由管理者每轮结束时更新（可用 `scripts/progress.py` 推进）。记录每轮五角色结论、macOS 对比总结、测试结果、git 提交哈希与下一轮重启重点。

## 当前状态

- **当前轮次**：第 2 轮（功能深化期，已完成）
- **已结束轮次**：第 1、2 轮
- **总完成度**：16%（骨架 + 输入分发/光标/屏幕信息基础）
- **接口覆盖率**：25%（OS/DisplayServer/输入/渲染驱动核心接口）
- **git 分支**：hm

## 轮次记录

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

### 第 4 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 5 轮

### 第 3 轮（待执行）

- **管理者 tasklist**：待填充
- **开发者**：待填充
- **挑战者**：待填充
- **审查者**：待填充
- **测试者**：待填充
- **macOS 对比**：待填充
- **git 提交**：待填充
- **下一轮**：第 4 轮

## 说明

- `current_round` 表示当前正在执行的轮次（0 = 未开始）。
- 每轮结束由管理者更新本文件：五角色结论、macOS 对比（行数 + 接口覆盖率）、测试结果、git 提交哈希、下一轮重启重点。
- 第 10 轮结束后输出 `final-summary.md`。
