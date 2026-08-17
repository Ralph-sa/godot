# M05 GLES/EGL 渲染 Spec

## 目标
gl_compatibility 渲染器在鸿蒙（模拟器 express_gpu / 真机 Maleoon）的完整链路。

## 验收标准
- ① EGL 初始化回退链：EGL_KHR_platform_ohos 不可用时回退 eglGetDisplay(EGL_DEFAULT_DISPLAY)
- ② window surface 用 eglCreateWindowSurface（platform 版不可用）
- ③ config RGBA8888+WINDOW_BIT（基类 RGB111 会选到 pbuffer config）
- ④ 渲染线程上下文强制切换（window_force_make_current）
- ⑤ vsync 关闭（express_gpu 的 vsync 等待会阻塞）
- ⑥ 验证：T05 EGL 链路 + T02 渲染循环 PASS

## 状态
✅ 完成。

## 开放项
- 无。