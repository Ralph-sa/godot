# GodotHMOS 测试验证体系与已知限制（2026-08-17 规格驱动复查）

## 自动化测试（run_tests.py，8 项）

- T01 启动链（setup→editor→EditorNode ctor done）
- T02 渲染循环（swap 计数 10s 采样增长；express_gpu 间歇慢帧，勿用 4s 短采样）
- T03 窗口内容（截屏中央区域像素分析）
- T04 输入注入（hilog 检测 injectTouch NAPI/push_touch；input_diag 文件在 NAPI 主线程 fopen 失败，勿依赖文件）
- T05 EGL 链路（initialize/open_display/window_create 全 0）
- T06 进程存活
- T07 交互响应（点击前后截图 md5 diff；模拟器 SKIP，真机执行）
- T08 键盘注入（uitest keyEvent→push_key 打点；模拟器 uitest keyEvent 不分发，SKIP；
  NAPI 段（injectKey→push_key_event）已由 ArkTS 临时注入实证：KEYCODE_A=2017→Godot Key::A）

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
→ 坐标用 px/vp 实际比例换算。键盘 NAPI 段已实证（injectKey→push_key_event 打点）。

## T-GR 播放链路（2026-08-17 修复与遗留）

已修复（T-GR-1 验证通过）：
1. **输入事件从未被引擎消费的根因**：引擎核心 Main::iteration 只在 macOS 分支调用
   DisplayServer::process_events()，OHOS 主循环从不调用——触摸/键盘事件堆积在
   队列永不投递。修复：main_ohos.cpp 主循环每帧调用 ds->process_events()。
2. **焦点回调跨线程**：notify_main_surface_focus/resized 由 JS 主线程（NAPI）调用，
   直接 call 窗口回调触发 SceneTree 线程断言。修复：事件入队，引擎线程 process_events
   消费（display_server_ohos.h 的 pending_window_events 队列）。
3. **处理器从未注册**：XComponent 的 onAppear 在 surfaceId 路径不触发，剪贴板/文件
   选择器/子窗口/指针/光标/手柄/shell_open 处理器全部未注册。修复：统一移到根
   Stack 的 onAppear（registerEngineHandlers）。
4. **ArkUI 焦点回调不触发**：透明事件层抢走焦点，XComponent onFocus 从不触发。
   修复：触摸即 notifyFocus(true)。
5. create_instance 记录参数后未请求主循环退出（桌面走子进程，OHOS 需重启）。
   修复：create_instance 内调 SceneTree::quit()，触发 cleanup 的 restart_on_exit 分支。
6. OS_OHOS::finalize 调 finalize_core 与 Main::cleanup 尾部重复——process_map 二次
   memdelete 崩溃。修复：finalize 清空（核心清理交给 cleanup 尾部）。
7. cleanup 的 message_queue->flush() 执行指向已删除对象的 call_deferred 崩溃。
   修复：OHOS 跳过 flush，直接析构。
8. 默认项目预置 main.tscn + run/main_scene（编辑器「播放」可运行）。
9. 类型注册幂等化（部分）：ClassDB register_*_types 与 initialize_modules 首轮注册后
   跨重启复用（main.cpp ohos_types_registered 守卫 + scene ThemeDB GDREGISTER 守卫）。

遗留（T-GR-2 继续）：
- 进程内重启第二轮 Main::setup 仍在启动中崩溃：register_utility_function 重复注册
  （variant_utility 表不幂等）、ResourceUID/GDExtensionManager/IP 等单例重复创建。
  第一轮 cleanup→第二轮 setup(argc=17) 已实证走通到 StringName 配置报错之后，
  崩溃栈在引擎 core（待符号化定位逐个修复）。
- 验收②（重启后游戏模式运行）与③（停止回编辑器）未达。

## 部署注意事项（易错）

1. hvigor 缓存：.hvigor、entry/build、~/.hvigor 全删，否则 HAP 打包旧 .so/abc。
2. scons 增量对 main_ohos.cpp 等不可靠：必须 rm .so 强制重链。
3. .so 更新不走增量安装：bm uninstall + install 才可靠（versionCode 同值时）。
4. python 写 C++ 时 \n 转义易成真实换行破坏代码，写后 grep 验证。
