# M02 DisplayServer Spec（基于代码核对 2026-08-17）

## 代码事实
- 渲染驱动调度（vulkan/opengl3_es）：✅
- GL 桥（gl_window_make_current/swap_buffers/release_rendering_thread/force_make_current+INVALID 防护）：✅
- NativeMenu 桩：✅
- resize buffer 同步：✅
- **window_set_transient：❌ 空壳（父子关系未实现）**——子窗口无法指定父窗口
- **window_set_flag：❌ 空壳**——无边框/置顶等窗口标志不生效
- **window_request_attention：❌ 空壳**——任务栏闪烁
- **window_move_to_foreground：❌ 只有 print_verbose，NAPI 未接入**——子窗口无法置前
- get_window_at_screen_position：⚠️ 恒返回主窗口（多窗口命中检测缺失）

## 任务
- [ ] T-DS-3 window_move_to_foreground 接入 NAPI 桥（moveWindowToFront）
- [ ] T-DS-4 window_set_transient 记录父子关系（引擎侧数据 + 必要时 ArkTS 窗口 z 序）
- [ ] T-DS-5 window_set_flag 支持的标志子集（无边框/置顶）桥接 ArkTS
- [ ] T-DS-6 get_window_at_screen_position 按子窗口 rect 命中检测

## 开放项
- window_request_attention 在鸿蒙是否有对应能力（通知/任务栏闪烁）。