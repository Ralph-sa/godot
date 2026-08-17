# GodotHMOS 测试验证体系与已知限制（2026-08-17 规格驱动复查）

## 自动化测试（run_tests.py，7 项）

- T01 启动链（setup→editor→EditorNode ctor done）
- T02 渲染循环（swap 计数 10s 采样增长；express_gpu 间歇慢帧，勿用 4s 短采样）
- T03 窗口内容（截屏中央区域像素分析）
- T04 输入注入（hilog 检测 injectTouch NAPI/push_touch；input_diag 文件在 NAPI 主线程 fopen 失败，勿依赖文件）
- T05 EGL 链路（initialize/open_display/window_create 全 0）
- T06 进程存活
- T07 交互响应（点击前后截图 md5 diff；模拟器 SKIP，真机执行）

## 模拟器已知限制（express_gpu）

1. eglSwapBuffers 不更新帧内容——画面仅启动首帧；窗口拉伸、交互均不刷新。
   真机 Maleoon EGL 正常。画面级验证（截图 diff、resize 跟随）必须在真机做。
2. eglSwapBuffers 间歇性慢（vsync 关闭后仍存在）——渲染计数采样需 >=10s。
3. densityDPI 报 160（density=1.0）但实际 vp->px 比例 1.9——坐标换算必须用
   XComponent onAreaChange 的 px/vp 实际比例，禁止依赖 density API。
4. libEGL 不支持 EGL_KHR_platform_ohos 与 eglCreatePlatformWindowSurface——
   必须回退 eglGetDisplay(EGL_DEFAULT_DISPLAY) + eglCreateWindowSurface。

## 输入链（真机/模拟器通用）

XComponent 触摸被 native 层消费（API 24/26 均无 nativeXComponent 无法注册回调）
→ Stack 透明层接收事件 → 注入鼠标语义事件（编辑器 GUI 桌面语义，ScreenTouch 无效）
→ 坐标用 px/vp 实际比例换算。键盘待验证。

## 部署注意事项（易错）

1. hvigor 缓存：.hvigor、entry/build、~/.hvigor 全删，否则 HAP 打包旧 .so/abc。
2. scons 增量对 main_ohos.cpp 等不可靠：必须 rm .so 强制重链。
3. .so 更新不走增量安装：bm uninstall + install 才可靠（versionCode 同值时）。
4. python 写 C++ 时 \n 转义易成真实换行破坏代码，写后 grep 验证。
