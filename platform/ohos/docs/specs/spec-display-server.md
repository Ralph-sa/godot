# M02 DisplayServer Spec

## 目标
DisplayServerOHOS：窗口管理、渲染驱动调度（vulkan/opengl3_es）、GL 桥。

## 验收标准
- ① 构造期按 rendering_driver 初始化对应上下文（vulkan 或 GLES3）
- ② gl_window_make_current / swap_buffers / release_rendering_thread 正确实现
- ③ NativeMenu 基类桩存在（单例缺失会 SIGSEGV）
- ④ window_force_make_current 处理 INVALID_WINDOW_ID（不报错）
- ⑤ 窗口 resize 同步 buffer 几何
- ⑥ 验证：T02 渲染循环 + T05 EGL 链路 PASS

## 状态
✅ 主体完成。

## 剩余任务
- [ ] T-DS-1 Vulkan 路径构造分支未实测——随 M06。
- [ ] T-DS-2 多窗口（子窗口 DisplayServer 侧）——随 M10。

## 开放项
- Vulkan 上下文初始化的窗口绑定在真机路径是否与 GLES 一致（未实测）。