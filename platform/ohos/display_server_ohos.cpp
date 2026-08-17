/**************************************************************************/
/*  display_server_ohos.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "display_server_ohos.h"

#include "ohos_bridge.h"
#include "os_ohos.h"

#include "core/config/project_settings.h"
#include "core/string/print_string.h"
#include "main/main.h"

#include <hilog/log.h>
#include <thread>
#include <chrono>
#include <cstdio>

// 平台诊断日志辅助：hdc 抓取 hilog 时 LOG_APP 域日志可能因日志量/过滤丢失，
// 崩溃场景下 faultlog 也只含系统 core 域。因此关键路径同时写入应用 cacheDir
// 下的诊断文件，崩溃后 hdc file recv 直接读取，避免 hilog 不确定性。
// 注意：鸿蒙沙盒根为 /data/storage/el2/base/haps/<module>，
// 写 /data/storage/el2/base/cache（沙盒外）会被拒绝；必须用 NAPI 注入的
// cacheDir（OS_OHOS::get_cache_path，引擎启动前已注入）。
static void ohos_diag_log(const char *p_fmt, ...) {
	String diag_path = "/data/storage/el2/base/haps/entry/cache/godot_ds_diag.log";
	OS_OHOS *os = OS_OHOS::get_singleton();
	if (os) {
		String cache = os->get_cache_path();
		if (!cache.is_empty()) {
			diag_path = cache.path_join("godot_ds_diag.log");
		}
	}
	FILE *f = fopen(diag_path.utf8().get_data(), "a");
	if (f) {
		va_list args;
		va_start(args, p_fmt);
		vfprintf(f, p_fmt, args);
		va_end(args);
		fprintf(f, "\n");
		fclose(f);
	}
}

// Vulkan 渲染上下文与渲染设备（第 10 轮修复：Vulkan 链路缺失导致空指针崩溃）
#ifdef VULKAN_ENABLED
#include "rendering_context_driver_vulkan_ohos.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/rendering_device.h"
#endif

// GLES3/EGL（gl_compatibility 渲染器；EGL_KHR_platform_ohos）
#ifdef GLES3_ENABLED
#include "drivers/gles3/rasterizer_gles3.h"
#include "egl_manager_ohos_gles.h"
#include <native_buffer/buffer_common.h>
#endif

// 静态创建函数（DisplayServer 注册用），对应 macOS create_func
static DisplayServer *create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	// 支持的渲染驱动：vulkan（forward_plus/mobile）与 opengl3_es（gl_compatibility）
	if (p_rendering_driver != "vulkan" && p_rendering_driver != "opengl3_es") {
		r_error = ERR_UNAVAILABLE;
		return nullptr;
	}
	DisplayServer *ds = memnew(DisplayServerOHOS(p_rendering_driver, p_mode, p_vsync_mode, p_resolution));
	r_error = OK;
	return ds;
}

Vector<String> DisplayServerOHOS::get_rendering_drivers_func() {
	Vector<String> drivers;
#if defined(VULKAN_ENABLED)
	drivers.push_back("vulkan");
#endif
#if defined(GLES3_ENABLED)
	drivers.push_back("opengl3_es");
#endif
	return drivers;
}

void DisplayServerOHOS::register_ohos_driver() {
	// 注册 display_driver = "ohos"，与 main.cpp 的 display_driver 枚举对应。
	// 进程内重启（第 10 轮修复）会再次 initialize_core -> 本函数：
	// DisplayServer 的 create_function 表是静态数组且永不清理，
	// 重复注册会堆积表项直至 MAX_SERVERS=64，因此只注册一次。
	static bool driver_registered = false;
	if (driver_registered) {
		return;
	}
	register_create_function("ohos", create_func, get_rendering_drivers_func);
	driver_registered = true;
}

// 静态单例指针（DisplayServer 非 Object，无法 cast_to）
static DisplayServerOHOS *ohos_ds_singleton = nullptr;

DisplayServerOHOS *DisplayServerOHOS::get_singleton_ohos() {
	return ohos_ds_singleton;
}

// ---- GL 渲染桥（gl_compatibility） ----
void DisplayServerOHOS::gl_window_make_current(DisplayServerEnums::WindowID p_window_id) {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		// 强制 make current（第 11 轮修复）：基类的 current_window 地址检查
		// 会跳过渲染线程的上下文切换（主线程创建窗口时已 make current），
		// 导致渲染线程无 GL 上下文 → 黑屏。
		egl_manager->window_force_make_current(p_window_id);
	}
#endif
}

void DisplayServerOHOS::release_rendering_thread() {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		// 主线程释放 GL 上下文，允许渲染线程 eglMakeCurrent 成功
		egl_manager->release_current();
	}
#endif
}

void DisplayServerOHOS::swap_buffers() {
#ifdef GLES3_ENABLED
	if (egl_manager) {
		egl_manager->swap_buffers();
		// 渲染帧打点（第 11 轮：定位真机画面静止问题）
		static int swap_count = 0;
		swap_count++;
		if (swap_count <= 5 || swap_count % 300 == 0) {
			ohos_diag_log("swap_buffers: count=%d", swap_count);
		}
	}
#endif
}

int64_t DisplayServerOHOS::window_get_native_handle(DisplayServerEnums::HandleType p_handle_type, DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V(p_window != DisplayServerEnums::MAIN_WINDOW_ID, 0);
#ifdef GLES3_ENABLED
	if (!egl_manager) {
		return 0;
	}
	switch (p_handle_type) {
		case DisplayServerEnums::DISPLAY_HANDLE: {
			return reinterpret_cast<int64_t>(egl_manager->get_display(p_window));
		}
		case DisplayServerEnums::OPENGL_CONTEXT: {
			return reinterpret_cast<int64_t>(egl_manager->get_context(p_window));
		}
		case DisplayServerEnums::EGL_DISPLAY: {
			return reinterpret_cast<int64_t>(egl_manager->get_display(p_window));
		}
		case DisplayServerEnums::EGL_CONFIG: {
			return reinterpret_cast<int64_t>(egl_manager->get_config(p_window));
		}
		default: {
			return 0;
		}
	}
#else
	return 0;
#endif
}

DisplayServerOHOS::DisplayServerOHOS(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, const Vector2i &p_size) {
	ohos_ds_singleton = this;
	rendering_driver = p_rendering_driver;
	window_size = p_size;

	// 创建主窗口对象（骨架期：XComponent 宿主由 main_ohos.cpp 注入）
	OHOS_Window *main_window = memnew(OHOS_Window(OHOS_Window::WINDOW_TYPE_MAIN));
	main_window->set_title("Godot Editor (HarmonyOS)");
	main_window->set_rect(Rect2i(0, 0, p_size.x, p_size.y));
	windows[DisplayServerEnums::MAIN_WINDOW_ID] = main_window;

#ifdef VULKAN_ENABLED
	// 初始化 Vulkan 渲染上下文（对应 macOS DisplayServerMacOS 构造中的
	// RenderingContextDriverVulkanMacOS 创建 + initialize）。
	// 必须在此完成，因为 Main::setup 后续会立即创建 RenderingServer，
	// 其 _init() 依赖 RendererCompositorRD::make_current() 已注册 compositor。
	//
	// 第 10 轮修复（续）：关键路径诊断同时写入文件 + hilog（ohos_diag_log）。
	// 崩溃现场 faultlog 只保留系统 core 域（C0xxx）日志，应用 LOG_APP 域输出
	// 可能因日志量丢弃，写文件可确保崩溃后 hdc file recv 直接读到每一步结果。
	if (rendering_driver == "vulkan") {
		ohos_diag_log("DisplayServerOHOS: entering vulkan init (driver=%s)", p_rendering_driver.utf8().get_data());
		rendering_context = memnew(RenderingContextDriverVulkanOHOS);
		Error ctx_err = rendering_context->initialize();
		ohos_diag_log("DisplayServerOHOS: vulkan context initialize -> %d", (int)ctx_err);
		if (ctx_err != OK) {
			memdelete(rendering_context);
			rendering_context = nullptr;
			ERR_PRINT("DisplayServerOHOS: Vulkan rendering context initialization failed.");
		} else {
			// 注册主窗口 Vulkan Surface（第 10 轮修复）：
			// RenderingDevice::initialize 依赖 surface_get_from_window() 返回非 0 的
			// main_surface（其内部 window_surface_map 由 window_create 填充）。
			// 若此处不注册，main_surface == 0 直接 ERR_FAIL_COND 返回 FAILED，
			// 后续 RendererCompositorRD::make_current() 不会执行，导致
			// RenderingServerDefault::_init() 中 RendererCompositor::create()
			// 返回 nullptr 后空指针崩溃（SIGSEGV @ Not mapped）。
			//
			// native_window 未就绪时轮询等待（surface_created 回调在 ArkUI
			// 主线程，构造运行在引擎线程，sleep 不阻塞回调）：避免 XComponent
			// Surface 延迟创建导致 window_create 时拿不到句柄。
			OHOS_XComponent *xc = OHOS_XComponent::get_instance();
			OHNativeWindow *native_window = xc ? xc->get_native_window() : nullptr;
			for (int i = 0; i < 100 && !native_window; i++) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 最多等 5s
				native_window = xc ? xc->get_native_window() : nullptr;
			}
			ohos_diag_log("DisplayServerOHOS: native_window=%p (xc=%p)", (void *)native_window, (void *)xc);
			if (!native_window) {
				ohos_diag_log("DisplayServerOHOS: native_window not ready, cannot create Vulkan surface.");
				ERR_PRINT("DisplayServerOHOS: native_window not ready, cannot create Vulkan surface.");
			} else {
				RenderingContextDriverVulkanOHOS::WindowPlatformData wpd;
				wpd.window = native_window;
				Error surf_err = rendering_context->window_create(DisplayServerEnums::MAIN_WINDOW_ID, &wpd);
				ohos_diag_log("DisplayServerOHOS: vulkan window_create -> %d", (int)surf_err);
			}

			// 创建并初始化 RenderingDevice（全局单例，RenderingServer 使用）
			rendering_device = memnew(RenderingDevice);
			Error dev_err = rendering_device->initialize(rendering_context, DisplayServerEnums::MAIN_WINDOW_ID);
			ohos_diag_log("DisplayServerOHOS: rendering device initialize -> %d", (int)dev_err);
			if (dev_err != OK) {
				ERR_PRINT(vformat("DisplayServerOHOS: RenderingDevice initialize failed (err=%d).", dev_err));
				memdelete(rendering_device);
				rendering_device = nullptr;
			} else {
				// 创建主窗口交换链（第 10 轮修复收尾：此前缺失导致
				// screen_prepare_for_drawing 报 "A swap chain was not created"、
				// 渲染路径空指针崩溃——对照 macOS DisplayServerMacOS 构造：
				// initialize 之后必须 screen_create(MAIN_WINDOW_ID)）。
				Error sc_err = rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
				ohos_diag_log("DisplayServerOHOS: screen_create -> %d", (int)sc_err);
				if (sc_err != OK) {
					ERR_PRINT(vformat("DisplayServerOHOS: screen_create failed (err=%d).", sc_err));
					memdelete(rendering_device);
					rendering_device = nullptr;
				} else {
					// 注册 Vulkan compositor（否则 RendererCompositor::create() 返回 null）
					RendererCompositorRD::make_current();
					ohos_diag_log("DisplayServerOHOS: RendererCompositorRD made current.");
				}
			}
		}
	}
#endif

#ifdef GLES3_ENABLED
	// GLES3 初始化（gl_compatibility 渲染器，driver=opengl3_es）：
	// 对照 Wayland DisplayServerWayland 的 opengl3_es 分支——
	// 创建 EGLManager 派生类，EGL display 用鸿蒙 EGL_KHR_platform_ohos
	// 扩展（native_display = EGL_DEFAULT_DISPLAY），窗口 surface 直接用
	// surfaceId 路径创建的 OHNativeWindow。
	if (rendering_driver == "opengl3_es") {
		ohos_diag_log("DisplayServerOHOS: entering gles3 init (driver=opengl3_es)");
		egl_manager = memnew(EGLManagerOHOSGLES);
		Error init_err = egl_manager->initialize();
		ohos_diag_log("DisplayServerOHOS: egl initialize -> %d", (int)init_err);
		Error open_err = (init_err == OK) ? egl_manager->open_display(EGL_DEFAULT_DISPLAY) : init_err;
		ohos_diag_log("DisplayServerOHOS: egl open_display -> %d", (int)open_err);
		if (open_err != OK) {
			memdelete(egl_manager);
			egl_manager = nullptr;
			ERR_PRINT("DisplayServerOHOS: EGL display initialization failed.");
		} else {
			OHOS_XComponent *xc = OHOS_XComponent::get_instance();
			OHNativeWindow *native_window = xc ? xc->get_native_window() : nullptr;
			for (int i = 0; i < 100 && !native_window; i++) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 最多等 5s
				native_window = xc ? xc->get_native_window() : nullptr;
			}
			ohos_diag_log("DisplayServerOHOS: gles3 native_window=%p", (void *)native_window);
			if (!native_window) {
				ERR_PRINT("DisplayServerOHOS: native_window not ready, cannot create EGL surface.");
			} else {
				// 不做 Y 翻转（第 11 轮修订）：真机 Maleoon EGL 驱动自身处理
				// buffer 方向（画面正常）；模拟器 express_gpu 会应用 transform
				// 导致画面上下颠倒。两平台均无需 FLIP_V。
				Error we = egl_manager->window_create(DisplayServerEnums::MAIN_WINDOW_ID, EGL_DEFAULT_DISPLAY, native_window, p_size.x, p_size.y);
				ohos_diag_log("DisplayServerOHOS: egl window_create -> %d", (int)we);
			}
			// 关闭 vsync（第 11 轮模拟器修复：express_gpu 的 eglSwapBuffers
			// 在 vsync 等待时可能永久阻塞，导致渲染帧停滞在固定计数）
			egl_manager->set_use_vsync(false);
			// gles_over_gl = false：纯 GLES2/3 API（鸿蒙无桌面 GL）
			RasterizerGLES3::make_current(false);
			ohos_diag_log("DisplayServerOHOS: RasterizerGLES3 make_current(false) done");
		}
	}
#endif

	// NativeMenu 桩（第 11 轮修复）：基类默认实现，has_feature=false；
	// 缺失时 ProjectManager::ProjectManager / EditorNode 会空指针崩溃。
	native_menu = memnew(NativeMenu);

	print_line(vformat("DisplayServerOHOS initialized (%s), window size %dx%d", p_rendering_driver, p_size.x, p_size.y));
}

DisplayServerOHOS::~DisplayServerOHOS() {
	if (ohos_ds_singleton == this) {
		ohos_ds_singleton = nullptr;
	}
	// 释放输入法（内部 detach + 销毁 proxy/options）
	if (ime) {
		memdelete(ime);
		ime = nullptr;
	}
#ifdef VULKAN_ENABLED
	// 释放渲染设备与上下文（与构造顺序相反；RenderingDevice 单例由
	// RenderingServer 清理期使用，需在 DisplayServer 析构前释放）
	if (rendering_device) {
		memdelete(rendering_device);
		rendering_device = nullptr;
	}
	if (rendering_context) {
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
#endif
#ifdef GLES3_ENABLED
	if (egl_manager) {
		memdelete(egl_manager);
		egl_manager = nullptr;
	}
#endif
	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}
	// 释放窗口对象
	for (const KeyValue<DisplayServerEnums::WindowID, OHOS_Window *> &E : windows) {
		memdelete(E.value);
	}
	windows.clear();
}

Size2i DisplayServerOHOS::window_get_size(DisplayServerEnums::WindowID p_window) const {
	// 窗口尺寸：优先返回 XComponent 实际 Surface 尺寸（窗口缩放后保持同步）
	if (p_window == DisplayServerEnums::MAIN_WINDOW_ID && main_xcomponent && main_xcomponent->is_surface_ready()) {
		return main_xcomponent->get_size();
	}
	return window_size;
}

void DisplayServerOHOS::window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_title(p_title);
	// 子窗口：经 NAPI 桥更新原生窗口标题（主窗口标题由 ArkUI 页面固定）
	if (p_window != DisplayServerEnums::MAIN_WINDOW_ID) {
		ohos_subwindow_set_title(p_window, p_title);
	}
}

String DisplayServerOHOS::window_get_title(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), String(), "Invalid window ID.");
	return windows[p_window]->get_title();
}

void DisplayServerOHOS::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	window_size = p_size;
	windows[p_window]->resize(p_size);
	if (p_window != DisplayServerEnums::MAIN_WINDOW_ID) {
		// 子窗口：同步原生窗口尺寸
		Rect2i r = windows[p_window]->get_rect();
		ohos_subwindow_set_rect(p_window, r.position.x, r.position.y, p_size.x, p_size.y);
	}
	// 通知渲染驱动 swapchain 重建：XComponent 侧由 ArkUI 布局自动触发
	// SurfaceChanged 回调，引擎侧在 on_surface_changed 中同步尺寸；
	// 若 p_size 与当前 XComponent 尺寸不一致（程序化设置），
	// 第 4 轮通过 NAPI 桥请求 ArkUI 调整 XComponent 布局。
}

bool DisplayServerOHOS::window_is_visible(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), false, "Invalid window ID.");
	return windows[p_window]->is_visible();
}

bool DisplayServerOHOS::has_window(DisplayServerEnums::WindowID p_window) const {
	return windows.has(p_window);
}

int DisplayServerOHOS::get_screen_count() const {
	// 屏幕数量：多屏枚举（第 6 轮经 @ohos.display getAllDisplays 注入），
	// 未回传前默认 1（主屏）。
	if (!screens.is_empty()) {
		return screens.size();
	}
	return 1;
}

int DisplayServerOHOS::get_primary_screen() const {
	// 主屏幕：始终为 0（鸿蒙主屏即索引 0）
	return 0;
}

Point2i DisplayServerOHOS::screen_get_position(int p_screen) const {
	// 屏幕原点：多屏时使用回传坐标（扩展屏偏移）。
	// 对应 macOS NSScreen.frame.origin。
	if (!screens.is_empty()) {
		int idx = CLAMP(p_screen, 0, screens.size() - 1);
		return screens[idx].position;
	}
	return Point2i(0, 0);
}

Size2i DisplayServerOHOS::screen_get_size(int p_screen) const {
	// 屏幕尺寸：多屏时使用回传尺寸；主屏以 XComponent Surface 实际尺寸为准
	if (!screens.is_empty()) {
		int idx = CLAMP(p_screen, 0, screens.size() - 1);
		return screens[idx].size;
	}
	if (main_xcomponent && main_xcomponent->is_surface_ready()) {
		return main_xcomponent->get_size();
	}
	return window_size;
}

Rect2i DisplayServerOHOS::screen_get_usable_rect(int p_screen) const {
	// 可用区域 = 屏幕区域（鸿蒙无任务栏遮挡差异，先简化为全屏）
	Size2i sz = screen_get_size(p_screen);
	Point2i pos = screen_get_position(p_screen);
	return Rect2i(pos, sz);
}

int DisplayServerOHOS::screen_get_dpi(int p_screen) const {
	// DPI：多屏时使用回传值；否则按密度估算 160 * density
	if (!screens.is_empty()) {
		int idx = CLAMP(p_screen, 0, screens.size() - 1);
		return screens[idx].dpi;
	}
	OS_OHOS *os = OS_OHOS::get_singleton();
	if (os) {
		return static_cast<int>(160.0f * os->get_screen_density());
	}
	return 160;
}

float DisplayServerOHOS::screen_get_refresh_rate(int p_screen) const {
	// 刷新率：多屏时使用回传值；否则用注入的默认值（兜底 60Hz）
	if (!screens.is_empty()) {
		int idx = CLAMP(p_screen, 0, screens.size() - 1);
		return screens[idx].refresh_rate;
	}
	return screen_refresh_rate;
}

void DisplayServerOHOS::process_events() {
	// 输入事件分发：从 XComponent 输入队列取事件，投递到主窗口的
	// input_event_callback（引擎在 Main::iteration 中调用本方法）。
	// 对应 macOS 的 NSEvent 循环派发；触摸/鼠标/键盘事件在 ArkUI 主线程
	// 回调中入队（见 ohos_xcomponent.cpp），此处由引擎线程消费。
	if (main_xcomponent && windows.has(DisplayServerEnums::MAIN_WINDOW_ID)) {
		Callable input_cb = windows[DisplayServerEnums::MAIN_WINDOW_ID]->get_input_event_callback();
		main_xcomponent->poll_events(input_cb);
	}
}

// ---- 拖拽文件投递（T-XC-1） ----

void DisplayServerOHOS::notify_drop_files(const Vector<String> &p_files) {
	ERR_FAIL_COND_MSG(!windows.has(DisplayServerEnums::MAIN_WINDOW_ID), "Invalid main window.");
	const Callable &cb = windows[DisplayServerEnums::MAIN_WINDOW_ID]->get_drop_files_callback();
	if (cb.is_valid()) {
		cb.call(p_files);
		print_verbose(vformat("DisplayServerOHOS: dropped %d files", p_files.size()));
	} else {
		print_verbose("DisplayServerOHOS: drop_files_callback not set, ignoring dropped files.");
	}
}

// ---- 窗口通知（XComponent 回调触发） ----

void DisplayServerOHOS::notify_main_surface_resized() {
	// Surface 尺寸变化：同步窗口尺寸并触发 rect_changed 回调，
	// 编辑器 Viewport 据此重设渲染尺寸（对应 macOS windowDidResize）
	if (!main_xcomponent || !windows.has(DisplayServerEnums::MAIN_WINDOW_ID)) {
		return;
	}
	Size2i new_size = main_xcomponent->get_size();
	if (new_size == window_size) {
		return;
	}
	window_size = new_size;
	OHOS_Window *win = windows[DisplayServerEnums::MAIN_WINDOW_ID];
	win->set_rect(Rect2i(win->get_rect().position, new_size));

	// 触发 rect_changed 回调（引擎 Viewport 更新）
	Callable rect_cb = win->get_rect_changed_callback();
	if (rect_cb.is_valid()) {
		rect_cb.call(win->get_rect());
	}
}

void DisplayServerOHOS::notify_main_surface_focus(bool p_focused) {
	// 窗口聚焦状态变化：触发 WINDOW_EVENT_FOCUS_IN/OUT
	// （对应 macOS windowDidBecomeMain / windowDidResignMain）
	main_window_focused = p_focused;
	// 窗口聚焦：重新附加输入法（失焦时已分离）；失焦：分离输入法
	// （文本控件不再接收组合文本）
	if (p_focused) {
		ime_attach_for_text_input();
	} else {
		ime_detach_on_blur();
	}
	if (!windows.has(DisplayServerEnums::MAIN_WINDOW_ID)) {
		return;
	}
	Callable cb = windows[DisplayServerEnums::MAIN_WINDOW_ID]->get_window_event_callback();
	if (cb.is_valid()) {
		cb.call(p_focused ? DisplayServerEnums::WINDOW_EVENT_FOCUS_IN : DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT);
	}
}

// ---- 光标与鼠标 ----

void DisplayServerOHOS::cursor_set_shape(DisplayServerEnums::CursorShape p_shape) {
	// 记录光标形状，并经 NAPI 桥请求 ArkTS 切换系统光标
	//（@ohos.multimodalInput.pointer.setPointerStyle，对应 macOS NSCursor）。
	cursor_shape = p_shape;
	ohos_cursor_set_shape(static_cast<int>(p_shape));
}

DisplayServerEnums::CursorShape DisplayServerOHOS::cursor_get_shape() const {
	return cursor_shape;
}

void DisplayServerOHOS::cursor_set_custom_image(const Ref<Resource> &p_cursor, DisplayServerEnums::CursorShape p_shape, const Vector2 &p_hotspot) {
	// 记录自定义光标（编辑器拖拽/吸管等）。XComponent 不直接支持自定义
	// 光标，第 8 轮通过 ArkUI 层绘制或隐藏系统光标实现。
	custom_cursor = p_cursor;
	custom_cursor_shape = p_shape;
	custom_cursor_hotspot = p_hotspot;
}

Point2i DisplayServerOHOS::mouse_get_position() const {
	// 返回最近一次鼠标事件位置（由 XComponent 鼠标回调更新）
	if (main_xcomponent) {
		return main_xcomponent->get_last_mouse_position();
	}
	return Point2i();
}

void DisplayServerOHOS::mouse_set_mode(DisplayServerEnums::MouseMode p_mode) {
	// 鼠标模式（可见/隐藏/捕获）：记录状态。
	// 隐藏/捕获经 NAPI 桥调 @ohos.multimodalInput.pointer.setPointerVisible
	// 隐藏系统光标（对应 macOS CGDisplayHideCursor）；相对位移捕获留待
	// 第 8 轮经 XComponent 相对位移事件模拟。
	mouse_mode = p_mode;
	// 可见模式或部分捕获模式：系统指针可见；隐藏模式：隐藏系统指针
	bool visible = p_mode != DisplayServerEnums::MOUSE_MODE_HIDDEN;
	ohos_mouse_set_visible(visible);
}

DisplayServerEnums::MouseMode DisplayServerOHOS::mouse_get_mode() const {
	return mouse_mode;
}

// ---- 垂直同步 ----

void DisplayServerOHOS::window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window) {
	// 记录 VSync 模式。实际控制 Vulkan present mode 在渲染驱动创建时
	// 读取该值（rendering_context_driver_vulkan_ohos.cpp 第 4 轮接入）。
	vsync_mode = p_vsync_mode;
}

DisplayServerEnums::VSyncMode DisplayServerOHOS::window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const {
	return vsync_mode;
}

// ---- 剪贴板（NAPI 桥 @ohos.pasteboard） ----

void DisplayServerOHOS::clipboard_set(const String &p_text) {
	// 写剪贴板：经 NAPI 桥调用 @ohos.pasteboard（编辑器复制/粘贴）
	ohos_clipboard_set_text(p_text);
}

String DisplayServerOHOS::clipboard_get() const {
	// 读剪贴板：NAPI 桥，空内容返回空串
	return ohos_clipboard_get_text();
}

bool DisplayServerOHOS::clipboard_has() const {
	// 剪贴板是否有文本内容（非空即认为有）
	return !ohos_clipboard_get_text().is_empty();
}

// ---- 文件对话框（NAPI 桥 @ohos.file.picker） ----

Error DisplayServerOHOS::file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, DisplayServerEnums::FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback, DisplayServerEnums::WindowID p_window_id) {
	// 系统文件选择器：经 NAPI 桥调 ArkTS DocumentViewPicker。
	// 选择完成后 p_callback 收到 PackedStringArray（取消时为空）。
	// （对应 macOS 的 NSSavePanel / NSOpenPanel；当前目录受系统限制
	// 无法指定初始位置，过滤器透传后缀列表，见 ohos_pick_files。）
	return ohos_pick_files(p_title, static_cast<int>(p_mode), p_filters, p_callback);
}

// ---- 窗口管理（骨架期默认实现） ----

Vector<DisplayServerEnums::WindowID> DisplayServerOHOS::get_window_list() const {
	Vector<DisplayServerEnums::WindowID> list;
	for (const KeyValue<DisplayServerEnums::WindowID, OHOS_Window *> &E : windows) {
		list.push_back(E.key);
	}
	return list;
}

DisplayServerEnums::WindowID DisplayServerOHOS::get_window_at_screen_position(const Point2i &p_position) const {
	// 骨架期：只有主窗口，直接返回
	return DisplayServerEnums::MAIN_WINDOW_ID;
}

// ---- 子窗口（第 7 轮：编辑器子窗口，经 NAPI 桥 @ohos.window 创建原生窗口） ----

DisplayServerEnums::WindowID DisplayServerOHOS::create_sub_window(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, bool p_exclusive, DisplayServerEnums::WindowID p_transient_parent) {
	// 分配子窗口 ID（从 1000 起，避免与主窗口 0 冲突；对应 macOS 每次创建 NSWindow）
	static DisplayServerEnums::WindowID sub_window_id_counter = 1000;
	DisplayServerEnums::WindowID new_id = sub_window_id_counter++;

	OHOS_Window *win = memnew(OHOS_Window(OHOS_Window::WINDOW_TYPE_SUB));
	win->set_visible(false);
	win->set_rect(p_rect);
	win->set_window_mode(p_mode);
	windows[new_id] = win;

	// 请求 ArkTS 创建原生子窗口（@ohos.window createWindow）
	ohos_subwindow_create(static_cast<int>(new_id), p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y);
	print_verbose(vformat("DisplayServerOHOS: create sub window %d (%dx%d)", new_id, p_rect.size.x, p_rect.size.y));
	return new_id;
}

void DisplayServerOHOS::show_window(DisplayServerEnums::WindowID p_id) {
	ERR_FAIL_COND_MSG(!windows.has(p_id), "Invalid window ID.");
	if (p_id == DisplayServerEnums::MAIN_WINDOW_ID) {
		// 主窗口始终显示（ArkUI 页面承载），记录状态即可
		windows[p_id]->set_visible(true);
		return;
	}
	windows[p_id]->set_visible(true);
	ohos_subwindow_set_visible(static_cast<int>(p_id), true);
}

void DisplayServerOHOS::delete_sub_window(DisplayServerEnums::WindowID p_id) {
	ERR_FAIL_COND_MSG(!windows.has(p_id), "Invalid window ID.");
	if (p_id == DisplayServerEnums::MAIN_WINDOW_ID) {
		// 主窗口不可删除
		return;
	}
	// 请求 ArkTS 销毁原生子窗口，并释放引擎侧窗口对象
	ohos_subwindow_destroy(static_cast<int>(p_id));
	memdelete(windows[p_id]);
	windows.erase(p_id);
}

void DisplayServerOHOS::window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_instance_id(p_instance);
}

ObjectID DisplayServerOHOS::window_get_attached_instance_id(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), ObjectID(), "Invalid window ID.");
	return windows[p_window]->get_instance_id();
}

void DisplayServerOHOS::window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	// 骨架期：记录回调，尺寸变化时调用（第 3 轮接入 Surface 变化通知）
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_rect_changed_callback(p_callable);
}

void DisplayServerOHOS::window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_window_event_callback(p_callable);
}

void DisplayServerOHOS::window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_input_event_callback(p_callable);
}

void DisplayServerOHOS::window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_input_text_callback(p_callable);
	// 文本输入回调注册 = 编辑器文本控件获得输入焦点：
	// 附加系统输入法服务，启用中文/日文等 IME 组合文本
	//（对应 macOS Window 注册 input_text_callback 后 NSTextInputClient 生效）。
	ime_attach_for_text_input();
}

void DisplayServerOHOS::window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_drop_files_callback(p_callable);
}

int DisplayServerOHOS::window_get_current_screen(DisplayServerEnums::WindowID p_window) const {
	return 0; // 骨架期：主屏幕
}

void DisplayServerOHOS::window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window) {
	// 骨架期：多屏支持后续实现
}

Point2i DisplayServerOHOS::window_get_position(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), Point2i(), "Invalid window ID.");
	return windows[p_window]->get_rect().position;
}

Point2i DisplayServerOHOS::window_get_position_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return window_get_position(p_window); // 骨架期：无装饰差异
}

void DisplayServerOHOS::window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	Rect2i r = windows[p_window]->get_rect();
	r.position = p_position;
	windows[p_window]->set_rect(r);
}

void DisplayServerOHOS::window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) {
	// 骨架期：多窗口父子关系后续实现
}

void DisplayServerOHOS::window_set_max_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_max_size(p_size);
}

Size2i DisplayServerOHOS::window_get_max_size(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), Size2i(), "Invalid window ID.");
	return windows[p_window]->get_max_size();
}

void DisplayServerOHOS::window_set_min_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_min_size(p_size);
}

Size2i DisplayServerOHOS::window_get_min_size(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), Size2i(), "Invalid window ID.");
	return windows[p_window]->get_min_size();
}

Size2i DisplayServerOHOS::window_get_size_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return window_get_size(p_window); // 骨架期：无装饰差异
}

void DisplayServerOHOS::window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window) {
	// 窗口模式（窗口化/最大化/全屏）：记录状态，并请求 ArkUI 侧调整系统窗口。
	// 鸿蒙窗口由 UIAbility/WindowStage 管理，C++ 无法直接修改，
	// 通过 NAPI 桥通知 ArkTS 设置 maximize/fullScreen（第 5 轮接入 @ohos.window）。
	window_mode = p_mode;
	// 记录到窗口对象
	if (windows.has(p_window)) {
		windows[p_window]->set_window_mode(p_mode);
	}
	// 请求 ArkTS 应用窗口模式（全屏/最大化/窗口化）
	ohos_window_set_mode(static_cast<int>(p_mode));
	print_verbose(vformat("DisplayServerOHOS: window mode %d", static_cast<int>(p_mode)));
}

DisplayServerEnums::WindowMode DisplayServerOHOS::window_get_mode(DisplayServerEnums::WindowID p_window) const {
	if (windows.has(p_window)) {
		return windows[p_window]->get_window_mode();
	}
	return window_mode;
}

bool DisplayServerOHOS::window_is_maximize_allowed(DisplayServerEnums::WindowID p_window) const {
	return true;
}

void DisplayServerOHOS::window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window) {
	// 骨架期：窗口标志（无边框/置顶等）后续实现
}

bool DisplayServerOHOS::window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerOHOS::window_request_attention(DisplayServerEnums::WindowID p_window) {
	// 骨架期：闪烁任务栏等后续实现
}

void DisplayServerOHOS::window_move_to_foreground(DisplayServerEnums::WindowID p_window) {
	// 窗口前置：鸿蒙侧通过 ArkTS window.moveWindowToFront() 实现（第 4 轮 NAPI 接入）。
	// 骨架期仅记录。
	print_verbose("DisplayServerOHOS: window_move_to_foreground (NAPI pending)");
}

bool DisplayServerOHOS::window_is_focused(DisplayServerEnums::WindowID p_window) const {
	// 真实聚焦状态（XComponent focus 回调维护，对应 macOS isKeyWindow）
	if (p_window == DisplayServerEnums::MAIN_WINDOW_ID) {
		return main_window_focused;
	}
	return false;
}

bool DisplayServerOHOS::window_can_draw(DisplayServerEnums::WindowID p_window) const {
	// 骨架期：主窗口可绘制
	return p_window == DisplayServerEnums::MAIN_WINDOW_ID;
}

bool DisplayServerOHOS::can_any_window_draw() const {
	return true; // 骨架期：主窗口始终可绘制
}

String DisplayServerOHOS::get_name() const {
	return "ohos";
}

bool DisplayServerOHOS::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_MOUSE:
		case DisplayServerEnums::FEATURE_MOUSE_WARP:
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
			// 鸿蒙 PC/2in1 支持键鼠与触摸
			return true;
		case DisplayServerEnums::FEATURE_SUBWINDOWS:
			// 编辑器多窗口支持（第 5 轮：单一主 XComponent 自绘，子窗口经
			// NAPI 创建原生子窗口，第 7 轮完整实现，此处声明能力位）
			return true;
		case DisplayServerEnums::FEATURE_CLIPBOARD:
			// 剪贴板（第 5 轮：经 @ohos.pasteboard NAPI 桥实现）
			return true;
		case DisplayServerEnums::FEATURE_HIDPI:
			return true;
		case DisplayServerEnums::FEATURE_CURSOR_SHAPE:
			// 系统光标形状（第 8 轮：@ohos.multimodalInput.pointer.setPointerStyle）
			return true;
		case DisplayServerEnums::FEATURE_IME:
			// 输入法（第 8 轮：inputmethod C API 实现中文/日文等 IME）
			return true;
		default:
			return false;
	}
}

// ---- 输入法（第 8 轮：中文输入） ----

IME_OHOS *DisplayServerOHOS::get_ime() {
	// 惰性创建输入法实例（engine 启动后首次文本输入聚焦时初始化）
	if (!ime) {
		ime = memnew(IME_OHOS);
	}
	return ime;
}

void DisplayServerOHOS::ime_attach_for_text_input() {
	// 文本控件聚焦：附加系统输入法服务
	//（对应 macOS Window 的 input_text_callback 注册后，输入法自动启用）
	IME_OHOS *ime_svc = get_ime();
	Error err = ime_svc->attach();
	if (err == OK) {
		// 主窗口聚焦且为触屏模式时可请求软键盘；PC 物理键盘由输入法
		// 服务自动跟随（AttachOptions showKeyboard=false）。
		print_verbose("DisplayServerOHOS: IME attached for text input.");
	}
}

void DisplayServerOHOS::ime_detach_on_blur() {
	// 窗口失焦/文本控件失焦：分离输入法，避免候选框残留
	if (ime && ime->is_attached()) {
		ime->detach();
		print_verbose("DisplayServerOHOS: IME detached on blur.");
	}
}