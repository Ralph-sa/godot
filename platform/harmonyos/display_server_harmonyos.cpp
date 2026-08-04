/**************************************************************************/
/*  display_server_harmonyos.cpp - DisplayServer Implementation           */
/**************************************************************************/

#include "display_server_harmonyos.h"

#include "harmonyos_native_window.h"
#include "harmonyos_input.h"
#include "os_harmonyos.h"
#include "tts_harmonyos.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"

#ifdef VULKAN_ENABLED
#include "rendering_context_driver_vulkan_harmonyos.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/rendering_device.h"
#endif

#ifdef HARMONYOS_ENABLED
#include "harmonyos_log.h"
#endif

#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include <window_manager/oh_display_info.h>
#include <window_manager/oh_display_manager.h>

using HarmonyOSWindowTitleCallback = void (*)(const char *);

static std::atomic<RenderingContextDriver *> rendering_context_global(nullptr);
static std::atomic<bool> rendering_context_global_checked(false);
static std::atomic<HarmonyOSWindowTitleCallback> window_title_callback(nullptr);

extern "C" __attribute__((visibility("default"))) void harmonyos_set_window_title_callback(
		HarmonyOSWindowTitleCallback p_callback) {
	window_title_callback.store(p_callback, std::memory_order_release);
}

DisplayServerHarmonyOS *DisplayServerHarmonyOS::get_singleton() {
	return static_cast<DisplayServerHarmonyOS *>(DisplayServer::get_singleton());
}

bool DisplayServerHarmonyOS::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_SWAP_BUFFERS:
		case DisplayServerEnums::FEATURE_CLIPBOARD:
		case DisplayServerEnums::FEATURE_MOUSE:
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
		// DPI 与 scale 现由 OH_NativeDisplayManager 真实查询，并随折叠/旋转
		// 经变更监听更新，引擎可以信任这两个值。
		case DisplayServerEnums::FEATURE_HIDPI:
			return true;

		// 以下能力目前只有空壳实现。声明 false 让引擎走它自己设计好的降级路径
		// 是安全的；声明 true 却没有实现，引擎会信以为真地走进空路径，表现为
		// 一批无法归因的怪异行为。每项的复位条件见注释。
		case DisplayServerEnums::FEATURE_KEEP_SCREEN_ON:
			// screen_set_keep_on 只写内存标志，未调用任何 OHOS 电源管理接口。
			return false;
		case DisplayServerEnums::FEATURE_CURSOR_SHAPE:
			// cursor_set_shape 只存枚举值。OHOS NDK 无原生光标形状接口，
			// 对应能力在 ArkTS 侧的 @ohos.multimodalInput.pointer。
			return false;
		case DisplayServerEnums::FEATURE_IME:
			// window_set_ime_* 只写内存，且 ime_get_text / ime_get_selection
			// 未 override —— 引擎一旦当真调用会落到基类的报错分支。
			return false;

		case DisplayServerEnums::FEATURE_GLOBAL_MENU:
			// OHOS 无系统全局菜单，NativeMenu 为基础实现（不支持 GLOBAL_MENU）。
			return native_menu && native_menu->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);
		// OHOS NDK 当前无 TTS API，FEATURE_TEXT_TO_SPEECH 暂返回 false。
		// 当 HarmonyOS SDK 提供原生 TTS 接口后，可激活此 feature。
		case DisplayServerEnums::FEATURE_TEXT_TO_SPEECH:
			return false;
		default:
			return false;
	}
}

String DisplayServerHarmonyOS::get_name() const {
	return "HarmonyOS";
}

// ---- clipboard ----
//
// Integration with OH_Pasteboard API via dynamic loading (dlopen/dlsym).
// On devices with pasteboard support (API 20+), the system clipboard is used.
// Falls back to an internal static buffer when the library is unavailable.

// OH_Pasteboard API function pointer types
typedef void *(*PasteboardCreateFunc)();
typedef int32_t (*PasteboardSetDataFunc)(void *, const char *, size_t);
typedef int32_t (*PasteboardGetDataFunc)(void *, char **, size_t *);
typedef void (*PasteboardDestroyFunc)(void *);

static void *s_pasteboard_lib = nullptr;
static PasteboardCreateFunc s_pasteboard_create = nullptr;
static PasteboardSetDataFunc s_pasteboard_set_data = nullptr;
static PasteboardGetDataFunc s_pasteboard_get_data = nullptr;
static PasteboardDestroyFunc s_pasteboard_destroy = nullptr;
static bool s_pasteboard_checked = false;

static String s_clipboard_fallback;

static void _ensure_pasteboard_loaded() {
	if (s_pasteboard_checked) {
		return;
	}
	s_pasteboard_checked = true;

	s_pasteboard_lib = dlopen("libpasteboard_ndk.z.so", RTLD_LAZY);
	if (!s_pasteboard_lib) {
		return;
	}

	s_pasteboard_create = (PasteboardCreateFunc)dlsym(s_pasteboard_lib, "OH_Pasteboard_CreatePasteboard");
	s_pasteboard_set_data = (PasteboardSetDataFunc)dlsym(s_pasteboard_lib, "OH_Pasteboard_SetPasteData");
	s_pasteboard_get_data = (PasteboardGetDataFunc)dlsym(s_pasteboard_lib, "OH_Pasteboard_GetPasteData");
	s_pasteboard_destroy = (PasteboardDestroyFunc)dlsym(s_pasteboard_lib, "OH_Pasteboard_DestroyPasteboard");
}

void DisplayServerHarmonyOS::clipboard_set(const String &p_text) {
	_ensure_pasteboard_loaded();

	if (s_pasteboard_create && s_pasteboard_set_data && s_pasteboard_destroy) {
		void *pasteboard = s_pasteboard_create();
		if (pasteboard) {
			CharString utf8 = p_text.utf8();
			s_pasteboard_set_data(pasteboard, utf8.get_data(), utf8.length());
			s_pasteboard_destroy(pasteboard);
		}
	}

	s_clipboard_fallback = p_text;
}

String DisplayServerHarmonyOS::clipboard_get() const {
	_ensure_pasteboard_loaded();

	if (s_pasteboard_create && s_pasteboard_get_data && s_pasteboard_destroy) {
		void *pasteboard = s_pasteboard_create();
		if (pasteboard) {
			char *data = nullptr;
			size_t len = 0;
			if (s_pasteboard_get_data(pasteboard, &data, &len) == 0 && data && len > 0) {
				// HarmonyOS pasteboard implementations may include the terminating
				// NUL (or unused capacity after it) in `len`. Godot's explicit-length
				// UTF-8 parser treats NUL as text and emits U+FFFD, so clamp to the
				// first terminator before crossing the platform boundary.
				const void *terminator = memchr(data, '\0', len);
				size_t text_len = terminator ? static_cast<const char *>(terminator) - data : len;
				String result = text_len > 0 ? String::utf8(data, (int)text_len) : String();
				s_pasteboard_destroy(pasteboard);
				return result;
			}
			s_pasteboard_destroy(pasteboard);
		}
	}

	return s_clipboard_fallback;
}

// ---- screen ----

int DisplayServerHarmonyOS::get_screen_count() const {
	// XComponent API (OH_NativeXComponent) 不提供多屏信息查询接口，
	// 因此固定返回 1。如需支持外接显示器等场景，需通过 ArkTS 侧
	// @ohos.display.getDefaultDisplaySync() 获取屏幕列表并通过 NAPI 传递。
	return 1;
}

int DisplayServerHarmonyOS::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerHarmonyOS::screen_get_position(int p_screen) const {
	return Point2i();
}

void DisplayServerHarmonyOS::set_window_size(const Size2i &p_size) {
	window_size_x.store(p_size.width, std::memory_order_release);
	window_size_y.store(p_size.height, std::memory_order_release);
}

Size2i DisplayServerHarmonyOS::get_window_size() const {
	return Size2i(window_size_x.load(std::memory_order_acquire),
			window_size_y.load(std::memory_order_acquire));
}

Size2i DisplayServerHarmonyOS::screen_get_size(int p_screen) const {
	// The window is not the screen: in split-screen or floating-window mode the
	// surface is only part of the display. Fall back to the window size only
	// until the display manager has answered.
	int w = screen_size_x.load(std::memory_order_acquire);
	int h = screen_size_y.load(std::memory_order_acquire);
	if (w > 0 && h > 0) {
		return Size2i(w, h);
	}
	return get_window_size();
}

Rect2i DisplayServerHarmonyOS::screen_get_usable_rect(int p_screen) const {
	// Excludes nothing yet: the available-area API reports the cutout-safe
	// region, which the engine treats as the usable rect only for the primary
	// screen. Reporting the full screen is the honest approximation here.
	Size2i size = screen_get_size(p_screen);
	return Rect2i(0, 0, size.width, size.height);
}

int DisplayServerHarmonyOS::screen_get_dpi(int p_screen) const {
	return screen_dpi_val.load(std::memory_order_acquire);
}

float DisplayServerHarmonyOS::screen_get_scale(int p_screen) const {
	return screen_scale_val.load(std::memory_order_acquire);
}

float DisplayServerHarmonyOS::screen_get_refresh_rate(int p_screen) const {
	return screen_refresh_rate_val.load(std::memory_order_acquire);
}

// 只记录状态：OHOS NDK 无电源管理接口，屏幕常亮需要 ArkTS 侧调用
// window.setWindowKeepScreenOn 并经 NAPI 转发。在那之前 FEATURE_KEEP_SCREEN_ON
// 保持 false，引擎不会依赖此状态。
void DisplayServerHarmonyOS::screen_set_keep_on(bool p_enable) {
	keep_screen_on = p_enable;
}

bool DisplayServerHarmonyOS::screen_is_kept_on() const {
	return keep_screen_on;
}

bool DisplayServerHarmonyOS::is_touchscreen_available() const {
	return true;
}

// ---- window callbacks ----

template <typename... Args>
void DisplayServerHarmonyOS::_window_callback(const Callable &p_callable, bool p_deferred, const Args &...p_rest_args) const {
	if (!p_callable.is_valid()) {
		return;
	}
	p_callable.call(p_rest_args...);
}

void DisplayServerHarmonyOS::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerHarmonyOS *ds = get_singleton();
	if (ds) {
		ds->send_input_event(p_event);
	}
}

void DisplayServerHarmonyOS::window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	rect_changed_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	// Deliberately dropped rather than stored: OHOS delivers drops through the
	// ArkUI node tree (arkui/drag_and_drop.h), which needs an ArkUI_NodeHandle.
	// The surface here is a declarative ArkTS XComponent with no native node, so
	// nothing could ever fire the callback. Keeping it would only make the path
	// look wired up. Implementing this means forwarding onDrop from ArkTS.
}

void DisplayServerHarmonyOS::send_window_event(DisplayServerEnums::WindowEvent p_event, bool p_deferred) const {
	_window_callback(window_event_callback, p_deferred, window_id, int64_t(p_event));
}

void DisplayServerHarmonyOS::send_input_event(const Ref<InputEvent> &p_event) const {
	_window_callback(input_event_callback, false, p_event, window_id);
}

void DisplayServerHarmonyOS::send_input_text(const String &p_text) const {
	_window_callback(input_text_callback, false, p_text, window_id);
}

// ---- window management ----

Vector<DisplayServerEnums::WindowID> DisplayServerHarmonyOS::get_window_list() const {
	Vector<DisplayServerEnums::WindowID> ret;
	ret.push_back(window_id);
	return ret;
}

DisplayServerEnums::WindowID DisplayServerHarmonyOS::get_window_at_screen_position(const Point2i &p_position) const {
	return window_id;
}

void DisplayServerHarmonyOS::window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window) {
	OH_LOG_INFO(LOG_APP, "[DS] window_attach_instance_id win=%{public}d obj=%{public}llu",
			(int)p_window, (unsigned long long)(uint64_t)p_instance);
	// Not supported - handled via ArkTS
}

ObjectID DisplayServerHarmonyOS::window_get_attached_instance_id(DisplayServerEnums::WindowID p_window) const {
	return ObjectID();
}

void DisplayServerHarmonyOS::window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) {
	// Forward title change to ArkTS via NAPI callback.
	// The NAPI bridge installs this explicitly after dlopen/dlsym, avoiding
	// any dependency on weak-symbol resolution or loader visibility.
	HarmonyOSWindowTitleCallback callback = window_title_callback.load(std::memory_order_acquire);
	if (callback) {
		callback(p_title.utf8().get_data());
	}
}

int DisplayServerHarmonyOS::window_get_current_screen(DisplayServerEnums::WindowID p_window) const {
	return 0;
}

void DisplayServerHarmonyOS::window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window) {
}

Point2i DisplayServerHarmonyOS::window_get_position(DisplayServerEnums::WindowID p_window) const {
	return _window_position;
}

Point2i DisplayServerHarmonyOS::window_get_position_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return _window_position;
}

void DisplayServerHarmonyOS::window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window) {
}

void DisplayServerHarmonyOS::window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) {
}

void DisplayServerHarmonyOS::window_set_max_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerHarmonyOS::window_get_max_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i();
}

void DisplayServerHarmonyOS::window_set_min_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerHarmonyOS::window_get_min_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i();
}

void DisplayServerHarmonyOS::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	set_window_size(p_size);
#ifdef VULKAN_ENABLED
	RenderingContextDriver *ctx = rendering_context_global.load(std::memory_order_acquire);
	if (rendering_window_created && ctx) {
		ctx->window_set_size(p_window, p_size.width, p_size.height);
	}
#endif
}

Size2i DisplayServerHarmonyOS::window_get_size(DisplayServerEnums::WindowID p_window) const {
	Size2i s = get_window_size();
	static int dbg_n = 0;
	if ((dbg_n++ % 300) == 0) {
		OH_LOG_INFO(LOG_APP, "[DS] window_get_size(%{public}d) -> %{public}dx%{public}d (call %{public}d)",
				(int)p_window, s.width, s.height, dbg_n);
	}
	return s;
}

Size2i DisplayServerHarmonyOS::window_get_size_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return get_window_size();
}

void DisplayServerHarmonyOS::window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window) {
	// Only the requested mode is recorded. An actual fullscreen/windowed
	// transition is a system-level operation that has to go through ArkTS
	// (window.setWindowLayoutFullScreen) and be forwarded over NAPI, and the
	// bridge does not expose that entry point yet.
	//
	// This used to forward through a weak-declared harmonyos_notify_window_mode,
	// but that symbol has no definition anywhere in the repository, so the null
	// check was always false and the branch never ran. Keeping it only made the
	// path look wired up. Window-title forwarding instead uses an explicitly
	// installed callback whose registration is validated by the NAPI bridge.
	_window_mode = p_mode;
}

DisplayServerEnums::WindowMode DisplayServerHarmonyOS::window_get_mode(DisplayServerEnums::WindowID p_window) const {
	return _window_mode;
}

bool DisplayServerHarmonyOS::window_is_maximize_allowed(DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerHarmonyOS::window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window) {
}

bool DisplayServerHarmonyOS::window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerHarmonyOS::window_request_attention(DisplayServerEnums::WindowID p_window) {
}

void DisplayServerHarmonyOS::window_move_to_foreground(DisplayServerEnums::WindowID p_window) {
}

bool DisplayServerHarmonyOS::window_is_focused(DisplayServerEnums::WindowID p_window) const {
	return window_focused.load(std::memory_order_acquire);
}

void DisplayServerHarmonyOS::notify_window_focus(bool p_focused) {
	if (window_focused.load(std::memory_order_acquire) == p_focused) {
		return;
	}
	window_focused.store(p_focused, std::memory_order_release);

	// Deliver the transition to the engine. Without this the editor never
	// learns it lost focus and keeps treating itself as active — it would go
	// on auto-saving, animating and grabbing input while in the background.
	send_window_event(p_focused ? DisplayServerEnums::WINDOW_EVENT_FOCUS_IN
								: DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT);

	OH_LOG_INFO(LOG_APP, "[DS] window focus -> %{public}s", p_focused ? "IN" : "OUT");
}

bool DisplayServerHarmonyOS::window_can_draw(DisplayServerEnums::WindowID p_window) const {
	return window_can_draw_val.load(std::memory_order_acquire);
}

bool DisplayServerHarmonyOS::can_any_window_draw() const {
	static int dbg_count = 0;
	if ((dbg_count++ % 120) == 0) {
		OH_LOG_INFO(LOG_APP, "[DS] can_any_window_draw -> %{public}d (call %{public}d)",
				(int)window_can_draw_val.load(std::memory_order_acquire), dbg_count);
	}
	return window_can_draw_val.load(std::memory_order_acquire);
}

// ---- events ----

void DisplayServerHarmonyOS::process_events() {
	Input::get_singleton()->flush_buffered_events();

	// Forward pending window events to the engine.
	// Window focus/resize events are delivered via ArkTS callbacks
	// and translated through notify_surface_changed/created/destroyed.
	// Additional system events (clipboard change, etc.) can be polled here.
}

// ---- cursor ----

// 只记录形状：OHOS NDK 无原生光标接口，实际切换需经 ArkTS 侧的
// @ohos.multimodalInput.pointer。FEATURE_CURSOR_SHAPE 保持 false。
void DisplayServerHarmonyOS::cursor_set_shape(DisplayServerEnums::CursorShape p_shape) {
	cursor_shape = p_shape;
}

DisplayServerEnums::CursorShape DisplayServerHarmonyOS::cursor_get_shape() const {
	return cursor_shape;
}

// ---- mouse ----

Point2i DisplayServerHarmonyOS::mouse_get_position() const {
	// Read from the input layer, which is the only place that learns of pointer
	// movement. Mirroring it into a DisplayServer member left that member with
	// no writer, so this used to return (0,0) forever.
	return HarmonyOSInput::get_mouse_position();
}

BitField<MouseButtonMask> DisplayServerHarmonyOS::mouse_get_button_state() const {
	// Tracked by the input layer on every press/release.
	return HarmonyOSInput::get_mouse_button_mask();
}

void DisplayServerHarmonyOS::mouse_set_mode(DisplayServerEnums::MouseMode p_mode) {
	// OHOS NDK 没有指针捕获/隐藏接口（对应能力只在 ArkTS 侧的
	// @ohos.multimodalInput.pointer），只有 VISIBLE 能真正生效。这里记录
	// 未生效的请求，而不是静默吞掉 —— 否则引擎会按「已捕获」的假设继续处理
	// 鼠标，而光标其实一直可见。
	if (p_mode != DisplayServerEnums::MOUSE_MODE_VISIBLE) {
		OH_LOG_WARN(LOG_APP, "[DS] mouse mode %{public}d unsupported on HarmonyOS, staying VISIBLE", (int)p_mode);
	}
}

// GAPSCAN: ok 平台只支持 VISIBLE，理由见 mouse_set_mode
DisplayServerEnums::MouseMode DisplayServerHarmonyOS::mouse_get_mode() const {
	return DisplayServerEnums::MOUSE_MODE_VISIBLE;
}

// ---- swap buffers ----

void DisplayServerHarmonyOS::reset_swap_buffers_flag() {
	swap_buffers_flag = false;
}

bool DisplayServerHarmonyOS::should_swap_buffers() const {
	return swap_buffers_flag;
}

void DisplayServerHarmonyOS::swap_buffers() {
	// Sampled once per ~60 calls: enough to confirm the render loop is alive
	// without flooding the log at frame rate.
	static int swap_counter = 0;
	if ((swap_counter++ % 60) == 0) {
		OH_LOG_INFO(LOG_APP, "[DS] swap_buffers called (%{public}d)", swap_counter);
	}
	swap_buffers_flag = true;
}

void DisplayServerHarmonyOS::notify_surface_changed(int p_width, int p_height) {
	update_window_size(p_width, p_height);
}

void DisplayServerHarmonyOS::notify_surface_created() {
	window_can_draw_val.store(true, std::memory_order_release);
}

void DisplayServerHarmonyOS::notify_surface_destroyed() {
	window_can_draw_val.store(false, std::memory_order_release);
}

void DisplayServerHarmonyOS::update_window_size(int p_width, int p_height) {
	set_window_size(Size2i(p_width, p_height));
	_window_position = Point2i(0, 0);

#ifdef VULKAN_ENABLED
	// Mirrors the WM_SIZE path in DisplayServerWindows. Updating only the
	// DisplayServer members leaves the Vulkan surface and swapchain at their
	// previous dimensions when a 2-in-1 window is resized.
	RenderingContextDriver *ctx = rendering_context_global.load(std::memory_order_acquire);
	if (rendering_window_created && ctx) {
		ctx->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID, p_width, p_height);
	}
#endif

	// Folding, rotating or moving to another display all change density and
	// refresh rate, and all of them resize the surface first — so this is the
	// point where stale metrics can be refreshed without a change listener.
	refresh_screen_metrics();

	OH_LOG_INFO(LOG_APP, "[DS] update_window_size %{public}dx%{public}d, rect_cb_valid=%{public}d",
			p_width, p_height, (int)rect_changed_callback.is_valid());

	// Fire the rect-changed callback so the engine knows the window was resized.
	if (rect_changed_callback.is_valid()) {
		rect_changed_callback.call(Rect2i(0, 0, p_width, p_height));
	}
}

// ---- driver registration ----

Vector<String> DisplayServerHarmonyOS::get_rendering_drivers_func() {
	Vector<String> drivers;
#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif
	return drivers;
}

DisplayServer *DisplayServerHarmonyOS::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerHarmonyOS(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, p_parent_window, r_error));
	if (r_error != OK) {
		OS::get_singleton()->alert(
			"Your device does not support Vulkan.\n\nPlease ensure your HarmonyOS device supports Vulkan.",
			"Unable to initialize Vulkan video driver");
	}
	return ds;
}

void DisplayServerHarmonyOS::register_harmonyos_driver() {
	register_create_function("harmonyos", create_func, get_rendering_drivers_func);
}

// ---- Vulkan context ----

#ifdef VULKAN_ENABLED
bool DisplayServerHarmonyOS::check_vulkan_global_context(bool p_vulkan_requirements_met) {
	bool expected = false;
	if (rendering_context_global_checked.compare_exchange_strong(expected, true)) {
		Error err = ERR_CANT_CREATE;
		if (p_vulkan_requirements_met) {
			RenderingContextDriver *ctx = memnew(RenderingContextDriverVulkanHarmonyOS);
			err = ctx->initialize();
			if (err == OK) {
				rendering_context_global.store(ctx, std::memory_order_release);
			} else {
				memdelete(ctx);
			}
		}

		if (err != OK) {
			rendering_context_global.store(nullptr, std::memory_order_release);
			ERR_PRINT("Failed to initialize Vulkan context.");
			OH_LOG_ERROR(LOG_APP, "[DS] Vulkan context init FAILED err=%{public}d", (int)err);
		} else {
			OH_LOG_INFO(LOG_APP, "[DS] Vulkan context init OK");
		}
	}

	return rendering_context_global.load(std::memory_order_acquire) != nullptr;
}

void DisplayServerHarmonyOS::free_vulkan_global_context() {
	RenderingContextDriver *ctx = rendering_context_global.exchange(nullptr, std::memory_order_acq_rel);
	if (ctx != nullptr) {
		memdelete(ctx);
		rendering_context_global_checked.store(false, std::memory_order_release);
	}
}

void DisplayServerHarmonyOS::release_rendering_window() {
	window_can_draw_val.store(false, std::memory_order_release);
	if (!rendering_window_created) {
		return;
	}

	// Match DisplayServerWindows::_delete_window(): the swapchain owned by the
	// RenderingDevice must be released before the platform window held by the
	// RenderingContextDriver. OHNativeWindow is released by harmonyos_main only
	// after this method returns on the engine thread.
	if (rendering_device) {
		rendering_device->screen_free(DisplayServerEnums::MAIN_WINDOW_ID);
	}

	RenderingContextDriver *ctx = rendering_context_global.load(std::memory_order_acquire);
	if (ctx) {
		ctx->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
	}

	rendering_window_created = false;
	OH_LOG_INFO(LOG_APP, "[DS] rendering window released on engine thread");
}

bool DisplayServerHarmonyOS::reset_window() {
	OH_LOG_INFO(LOG_APP, "[DS] reset_window entry");
	RenderingContextDriver *ctx = rendering_context_global.load(std::memory_order_acquire);
	if (!ctx) {
		OH_LOG_ERROR(LOG_APP, "[DS] reset_window FAILED: Vulkan context not initialized, cannot create window surface");
		return false;
	}

	DisplayServerEnums::VSyncMode last_vsync_mode = DisplayServerEnums::VSyncMode::VSYNC_ENABLED;
	if (rendering_window_created) {
		last_vsync_mode = ctx->window_get_vsync_mode(window_id);
	}
	release_rendering_window();

	HarmonyOSNativeWindow *native_win = HarmonyOSNativeWindow::singleton;
	if (!native_win) {
		OH_LOG_ERROR(LOG_APP, "[DS] reset_window FAILED: no HarmonyOSNativeWindow");
		return false;
	}

	OHNativeWindow *oh_window = native_win->get_native_window();
	if (!oh_window) {
		OH_LOG_ERROR(LOG_APP, "[DS] reset_window FAILED: OHNativeWindow is NULL");
		return false;
	}
	OH_LOG_INFO(LOG_APP, "[DS] reset_window: OHNativeWindow=%{public}p size=%{public}dx%{public}d",
			(void *)oh_window, get_window_size().width, get_window_size().height);

	RenderingContextDriverVulkanHarmonyOS::WindowPlatformData wpd;
	wpd.native_window = oh_window;
	if (ctx->window_create(window_id, &wpd) != OK) {
		ERR_PRINT("Failed to reset Vulkan window.");
		OH_LOG_ERROR(LOG_APP, "[DS] reset_window FAILED: window_create error");
		return false;
	}
	OH_LOG_INFO(LOG_APP, "[DS] reset_window: window_create OK");

	ctx->window_set_size(window_id, get_window_size().width, get_window_size().height);
	ctx->window_set_vsync_mode(window_id, last_vsync_mode);

	if (rendering_device == nullptr) {
		OH_LOG_ERROR(LOG_APP, "[DS] reset_window: rendering_device is NULL");
		ctx->window_destroy(window_id);
		return false;
	}

	Error err = rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
	if (err != OK) {
		ERR_PRINT("Failed to create Vulkan swap chain for main window.");
		OH_LOG_ERROR(LOG_APP, "[DS] reset_window: screen_create FAILED err=%{public}d", (int)err);
		ctx->window_destroy(window_id);
		return false;
	}

	rendering_window_created = true;
	OH_LOG_INFO(LOG_APP, "[DS] reset_window: screen_create OK, swapchain size=%{public}dx%{public}d",
			rendering_device->screen_get_width(DisplayServerEnums::MAIN_WINDOW_ID),
			rendering_device->screen_get_height(DisplayServerEnums::MAIN_WINDOW_ID));
	return true;
}
#endif // VULKAN_ENABLED

// ---- IME ----

void DisplayServerHarmonyOS::ime_text(const String &p_text) {
	_ime_text = p_text;
	if (input_text_callback.is_valid()) {
		input_text_callback.call(p_text);
	}
}

void DisplayServerHarmonyOS::ime_selection(const Vector2i &p_selection) {
	_ime_selection = p_selection;
}

// 只记录状态：完整的 IME 支持还需要 override ime_get_text / ime_get_selection
// 并接入 ArkTS 侧的输入法框架。在那之前 FEATURE_IME 保持 false，引擎不会调用
// 那两个未 override 的方法（基类实现会直接报错）。
void DisplayServerHarmonyOS::window_set_ime_active(const bool p_active, DisplayServerEnums::WindowID p_window) {
	_ime_active = p_active;
}

void DisplayServerHarmonyOS::window_set_ime_position(const Point2i &p_pos, DisplayServerEnums::WindowID p_window) {
	_ime_cursor_pos = p_pos;
}

// ---- constructor / destructor ----

DisplayServerHarmonyOS::DisplayServerHarmonyOS(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	rendering_driver = p_rendering_driver;
	set_window_size(p_resolution);
	keep_screen_on = true;
	_window_mode = p_mode;

	if (p_rendering_driver != "vulkan") {
		OH_LOG_ERROR(LOG_APP, "Unsupported rendering driver: %{public}s. Only 'vulkan' is supported.", p_rendering_driver.utf8().get_data());
		r_error = ERR_UNAVAILABLE;
		return;
	}

	HarmonyOSNativeWindow *native_win = HarmonyOSNativeWindow::singleton;
	if (!native_win || !native_win->is_surface_ready() || !native_win->get_native_window() ||
			native_win->get_width() == 0 || native_win->get_height() == 0) {
		OH_LOG_ERROR(LOG_APP, "[DS] ctor: a valid OHNativeWindow must exist before Main::setup");
		r_error = ERR_UNAVAILABLE;
		return;
	}
	set_window_size(Size2i((int)native_win->get_width(), (int)native_win->get_height()));
	r_error = OK;

	// TTS framework stub — OHOS NDK 当前无 TTS API，预留框架供未来对接。
	tts = memnew(TTS_HarmonyOS);

	// NativeMenu 基础实现：OHOS 无全局菜单 API，全部功能返回默认值（false/空）。
	// 必须实例化以设置 NativeMenu::singleton，否则编辑器代码中对
	// NativeMenu::get_singleton() 的解引用会因空指针而崩溃。
	// 参照 LinuxBSD（display_server_x11.cpp:6837）的做法。
	native_menu = memnew(NativeMenu);

#if defined(VULKAN_ENABLED) && defined(RD_ENABLED)
	// Match DisplayServerWindows: the platform window is already real when
	// RenderingDevice is initialized, so Main::setup can safely perform early
	// editor draws. Initializing with INVALID_WINDOW_ID here reproduces the
	// missing-swapchain SIGSEGV seen on HarmonyOS.
	if (check_vulkan_global_context(true)) {
		RenderingContextDriver *ctx = rendering_context_global.load(std::memory_order_acquire);
		if (ctx) {
			RenderingContextDriverVulkanHarmonyOS::WindowPlatformData wpd;
			wpd.native_window = native_win->get_native_window();
			if (ctx->window_create(DisplayServerEnums::MAIN_WINDOW_ID, &wpd) != OK) {
				ERR_PRINT("Failed to create HarmonyOS Vulkan main window.");
				OH_LOG_ERROR(LOG_APP, "[DS] ctor: window_create FAILED");

				const BitField<RenderingDevice::TextureUsageBits> depth_usage =
						RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
						RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT |
						RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
				const BitField<RenderingDevice::TextureUsageBits> uint_sample_usage =
						RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
						RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
				const BitField<RenderingDevice::TextureUsageBits> vrs_fallback_usage =
						RenderingDevice::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
						RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT |
						RenderingDevice::TEXTURE_USAGE_STORAGE_BIT |
						RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
				OH_LOG_INFO(LOG_APP,
						"[VK format] depth D16=%{public}d D32=%{public}d X8D24=%{public}d",
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_D16_UNORM, depth_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_D32_SFLOAT, depth_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_X8_D24_UNORM_PACK32, depth_usage));
				OH_LOG_INFO(LOG_APP,
						"[VK format] uint-sample R8=%{public}d R16=%{public}d R32=%{public}d RGBA8=%{public}d",
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R8_UINT, uint_sample_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R16_UINT, uint_sample_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R32_UINT, uint_sample_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R8G8B8A8_UINT, uint_sample_usage));
				OH_LOG_INFO(LOG_APP,
						"[VK format] vrs-fallback R8=%{public}d R16=%{public}d R32=%{public}d RGBA8=%{public}d",
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R8_UINT, vrs_fallback_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R16_UINT, vrs_fallback_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R32_UINT, vrs_fallback_usage),
						(int)rendering_device->texture_is_format_supported_for_usage(RenderingDevice::DATA_FORMAT_R8G8B8A8_UINT, vrs_fallback_usage));
				r_error = ERR_UNAVAILABLE;
				return;
			}
			ctx->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID,
					get_window_size().width, get_window_size().height);
			ctx->window_set_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID, p_vsync_mode);

			rendering_device = memnew(RenderingDevice);
			if (rendering_device->initialize(ctx, DisplayServerEnums::MAIN_WINDOW_ID) == OK) {
				// Match DisplayServerWindows: RenderingDevice::initialize() creates
				// the device and queues, but it does not create the main-window swap
				// chain. screen_create() must succeed before RendererCompositorRD can
				// become current; otherwise Main::setup continues with no screen and
				// later dereferences invalid rendering state.
				Error screen_error = rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
				if (screen_error != OK) {
					OH_LOG_ERROR(LOG_APP, "[DS] ctor: screen_create FAILED err=%{public}d", (int)screen_error);
					memdelete(rendering_device);
					rendering_device = nullptr;
					ctx->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
					r_error = ERR_UNAVAILABLE;
					return;
				}

				RendererCompositorRD::make_current();
				rendering_window_created = true;
				window_can_draw_val.store(true, std::memory_order_release);
				OH_LOG_INFO(LOG_APP, "[DS] ctor: main window + swapchain ready %{public}dx%{public}d",
						get_window_size().width, get_window_size().height);
			} else {
				memdelete(rendering_device);
				rendering_device = nullptr;
				ctx->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
				ERR_PRINT("Failed to initialize RenderingDevice (Vulkan) with the main window.");
				OH_LOG_ERROR(LOG_APP, "[DS] ctor: RenderingDevice main-window init FAILED");
				r_error = ERR_UNAVAILABLE;
				return;
			}
		} else {
			OH_LOG_ERROR(LOG_APP, "[DS] ctor: Vulkan ctx null after check");
			r_error = ERR_UNAVAILABLE;
			return;
		}
	} else {
		ERR_PRINT("Failed to initialize Vulkan context.");
		OH_LOG_ERROR(LOG_APP, "[DS] ctor: check_vulkan_global_context FAILED");
		r_error = ERR_UNAVAILABLE;
		return;
	}
#endif

	// Route engine-generated input events back through this DisplayServer so
	// that per-window input callbacks fire. Without this registration
	// _dispatch_input_events() is never called and window input callbacks stay
	// silent. Mirrors DisplayServerWindows, which registers at the same point.
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);

	refresh_screen_metrics();
}

void DisplayServerHarmonyOS::refresh_screen_metrics() {
	// Each value is applied independently: a partial failure should not discard
	// the ones that did resolve.
	int32_t density_dpi = 0;
	if (OH_NativeDisplayManager_GetDefaultDisplayDensityDpi(&density_dpi) == DISPLAY_MANAGER_OK && density_dpi > 0) {
		screen_dpi_val.store(density_dpi, std::memory_order_release);
	}

	float virtual_pixel_ratio = 0.0f;
	if (OH_NativeDisplayManager_GetDefaultDisplayVirtualPixelRatio(&virtual_pixel_ratio) == DISPLAY_MANAGER_OK &&
			virtual_pixel_ratio > 0.0f) {
		screen_scale_val.store(virtual_pixel_ratio, std::memory_order_release);
	}

	uint32_t refresh_rate = 0;
	if (OH_NativeDisplayManager_GetDefaultDisplayRefreshRate(&refresh_rate) == DISPLAY_MANAGER_OK && refresh_rate > 0) {
		screen_refresh_rate_val.store((float)refresh_rate, std::memory_order_release);
	}

	int32_t display_width = 0;
	int32_t display_height = 0;
	if (OH_NativeDisplayManager_GetDefaultDisplayWidth(&display_width) == DISPLAY_MANAGER_OK &&
			OH_NativeDisplayManager_GetDefaultDisplayHeight(&display_height) == DISPLAY_MANAGER_OK &&
			display_width > 0 && display_height > 0) {
		screen_size_x.store(display_width, std::memory_order_release);
		screen_size_y.store(display_height, std::memory_order_release);
	}

	OH_LOG_INFO(LOG_APP, "[DS] screen metrics: %{public}dx%{public}d dpi=%{public}d scale=%{public}.2f refresh=%{public}.1f",
			screen_size_x.load(std::memory_order_acquire),
			screen_size_y.load(std::memory_order_acquire),
			screen_dpi_val.load(std::memory_order_acquire),
			(double)screen_scale_val.load(std::memory_order_acquire),
			(double)screen_refresh_rate_val.load(std::memory_order_acquire));
}

DisplayServerHarmonyOS::~DisplayServerHarmonyOS() {
	if (tts) {
		memdelete(tts);
		tts = nullptr;
	}

	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}

#if defined(VULKAN_ENABLED) && defined(RD_ENABLED)
	release_rendering_window();
	if (rendering_device) {
		memdelete(rendering_device);
		rendering_device = nullptr;
	}
	free_vulkan_global_context();
#endif
}
