/**************************************************************************/
/*  display_server_ohos.h                                                 */
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

#pragma once

#include "ime_ohos.h"
#include "ohos_window.h"
#include "ohos_xcomponent.h"

#include "core/math/rect2.h"
#include "core/math/vector2i.h"
#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "servers/display/display_server.h"
#include "servers/display/native_menu.h"

// Vulkan 渲染上下文/设备（第 10 轮修复：需完整类型以支持 memnew/memdelete）
#ifdef VULKAN_ENABLED
#include "drivers/vulkan/rendering_context_driver_vulkan.h"
#include "servers/rendering/rendering_device.h"
#ifdef GLES3_ENABLED
#include "egl_manager_ohos_gles.h"
#endif
#endif

/* DisplayServerOHOS：HarmonyOS 显示服务器。
 *
 * 对应 macOS 的 DisplayServerMacOSBase 职责：窗口管理、输入事件派发、
 * 渲染驱动创建。鸿蒙编辑器 UI 完全由 Godot 通过 Vulkan 自绘，
 * 宿主体为主窗口的 XComponent(SURFACE)。
 *
 * 第 1 轮（骨架期）：
 * - 提供 create_func 注册入口（display_driver = "ohos"）；
 * - 主窗口概念与窗口哈希表骨架；
 * - 渲染驱动注册（vulkan）。
 * 后续轮次逐步实现窗口控制/鼠标/输入/对话框等完整接口。
 */
class DisplayServerOHOS : public DisplayServer {
public:
	// 屏幕信息结构（对应 macOS NSScreen 列表，多屏枚举用）
	struct OHOS_ScreenInfo {
		Point2i position = Point2i(0, 0);
		Size2i size = Size2i(1920, 1080);
		int dpi = 160;
		float refresh_rate = 60.0f;
	};

private:
	// 窗口哈希表：window_id -> OHOS_Window（骨架期只含主窗口）
	HashMap<DisplayServerEnums::WindowID, OHOS_Window *> windows;

	// 瞬态子窗口集合（T-DS-4）：父窗口 -> 其瞬态子窗口 id 集合
	HashMap<DisplayServerEnums::WindowID, HashSet<DisplayServerEnums::WindowID>> transient_children;

	// 跨线程窗口事件队列（T-GR 修复）：notify_* 由 JS 主线程（NAPI）调用，
	// 窗口回调（rect_changed / window_event）必须在引擎线程执行，否则触发
	// SceneTree 线程安全断言。入队后由引擎线程 process_events 消费。
	struct OHOSWindowEvent {
		enum Type { RESIZE, FOCUS_IN, FOCUS_OUT } type;
		Size2i size;
		OHOSWindowEvent() {}
		OHOSWindowEvent(Type p_type) : type(p_type) {}
	};
	Mutex window_events_mutex;
	Vector<OHOSWindowEvent> pending_window_events;

	// 主窗口 XComponent 宿主
	OHOS_XComponent *main_xcomponent = nullptr;

	// 当前渲染驱动名（"vulkan"）
	String rendering_driver;

	// ---- Vulkan 渲染上下文（第 10 轮修复：缺失导致渲染服务器初始化空指针崩溃）----
	// 对应 macOS DisplayServerMacOS 的 rendering_context/rendering_device 职责。
	// 必须在 Main::setup 创建 RenderingServer 之前完成初始化并
	// 调用 RendererCompositorRD::make_current()，否则
	// RenderingServerDefault::_init() 里 RendererCompositor::create()
	// 返回 nullptr（_create_func 为空）导致空指针解引用崩溃。
	RenderingContextDriverVulkan *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;

	// ---- GLES3/EGL 上下文管理（gl_compatibility 渲染器） ----
	// 对应 Wayland DisplayServer 的 egl_manager；驱动为 opengl3_es 时
	// 构造期创建 EGL display/context/surface（EGL_KHR_platform_ohos）。
#ifdef GLES3_ENABLED
	EGLManager *egl_manager = nullptr;
#endif

	// 主窗口尺寸
	Size2i window_size;

	// ---- 光标状态（第 2 轮：记录形状，native 光标由 ArkUI 侧实现） ----
	DisplayServerEnums::CursorShape cursor_shape = DisplayServerEnums::CURSOR_ARROW;
	Ref<Resource> custom_cursor;
	Vector2 custom_cursor_hotspot;
	DisplayServerEnums::CursorShape custom_cursor_shape = DisplayServerEnums::CURSOR_ARROW;

	// 垂直同步模式（默认开启）
	DisplayServerEnums::VSyncMode vsync_mode = DisplayServerEnums::VSYNC_ENABLED;

	// 屏幕刷新率（由 Index.ets 经 @ohos.display 注入，Hz）
	float screen_refresh_rate = 60.0f;

	// 屏幕列表（第 6 轮：多屏枚举，由 @ohos.display getAllDisplays 回传）
	Vector<OHOS_ScreenInfo> screens;

	// 窗口模式（默认窗口化；全屏/最大化经 NAPI 请求 ArkUI 窗口）
	DisplayServerEnums::WindowMode window_mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;

	// 主窗口聚焦状态（由 XComponent focus 回调维护）
	bool main_window_focused = false;

	// 输入法（第 8 轮：中文输入，inputmethod C API 接入）
	IME_OHOS *ime = nullptr;

	// 鼠标模式（编辑器轨道控制用；warp 第 8 轮经 NAPI 模拟）
	DisplayServerEnums::MouseMode mouse_mode = DisplayServerEnums::MOUSE_MODE_VISIBLE;

	// 原生菜单（第 11 轮修复：NativeMenu 单例缺失导致 ProjectManager/编辑器
	// 空指针崩溃——鸿蒙无全局菜单，用基类默认实现（has_feature=false））
	NativeMenu *native_menu = nullptr;

public:
	// ---- 注册入口（main_ohos.cpp / 全局初始化时调用） ----
	static void register_ohos_driver();
	// 平台单例访问（DisplayServer 非 Object 派生类，不能 cast_to，用静态指针）
	static DisplayServerOHOS *get_singleton_ohos();

	DisplayServerOHOS(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, const Vector2i &p_size);
	virtual ~DisplayServerOHOS();

	// ---- 渲染驱动 ----
	static Vector<String> get_rendering_drivers_func();
	String get_rendering_driver() const { return rendering_driver; }

	// ---- GL 渲染桥（gl_compatibility；对应 Wayland 同名实现） ----
	virtual void gl_window_make_current(DisplayServerEnums::WindowID p_window_id) override;
	virtual void swap_buffers() override;
	virtual void release_rendering_thread() override;
	virtual int64_t window_get_native_handle(DisplayServerEnums::HandleType p_handle_type, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;

	// ---- 光标与鼠标（编辑器必需，第 2 轮） ----
	virtual void cursor_set_shape(DisplayServerEnums::CursorShape p_shape) override;
	virtual DisplayServerEnums::CursorShape cursor_get_shape() const override;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, DisplayServerEnums::CursorShape p_shape = DisplayServerEnums::CURSOR_ARROW, const Vector2 &p_hotspot = Vector2()) override;
	virtual Point2i mouse_get_position() const override;
	virtual void mouse_set_mode(DisplayServerEnums::MouseMode p_mode) override;
	virtual DisplayServerEnums::MouseMode mouse_get_mode() const override;

	// ---- 垂直同步 ----
	virtual void window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual DisplayServerEnums::VSyncMode window_get_vsync_mode(DisplayServerEnums::WindowID p_window) const override;

	// ---- 剪贴板（第 5 轮：经 NAPI 桥 @ohos.pasteboard） ----
	virtual void clipboard_set(const String &p_text) override;
	virtual String clipboard_get() const override;
	virtual bool clipboard_has() const override;

	// ---- 文件对话框（第 5 轮：经 NAPI 桥 @ohos.file.picker） ----
	virtual Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, DisplayServerEnums::FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback, DisplayServerEnums::WindowID p_window_id = DisplayServerEnums::MAIN_WINDOW_ID) override;

	// ---- 窗口（骨架：主窗口尺寸/标题） ----
	virtual Size2i window_get_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual void window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;

	// 内部辅助（非基类虚函数，供编辑器与导出器使用）
	bool has_window(DisplayServerEnums::WindowID p_window) const;
	String window_get_title(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const;
	bool window_is_visible(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const;

	// ---- 屏幕 ----
	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual Point2i screen_get_position(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = DisplayServerEnums::SCREEN_OF_MAIN_WINDOW) const override;

	// ---- 窗口管理（骨架期给出合理默认，完善期接入 ArkUI Window API） ----
	virtual Vector<DisplayServerEnums::WindowID> get_window_list() const override;
	virtual DisplayServerEnums::WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	// ---- 子窗口（第 7 轮：编辑器子窗口，经 NAPI 桥 @ohos.window 创建原生窗口） ----
	virtual DisplayServerEnums::WindowID create_sub_window(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect = Rect2i(), bool p_exclusive = false, DisplayServerEnums::WindowID p_transient_parent = DisplayServerEnums::INVALID_WINDOW_ID) override;
	virtual void show_window(DisplayServerEnums::WindowID p_id) override;
	virtual void delete_sub_window(DisplayServerEnums::WindowID p_id) override;

	virtual void window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;

	virtual int window_get_current_screen(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual void window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;

	virtual Point2i window_get_position(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual Point2i window_get_position_with_decorations(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual void window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;

	virtual void window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) override;

	virtual void window_set_max_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual Size2i window_get_max_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual void window_set_min_size(const Size2i p_size, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual Size2i window_get_min_size(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual Size2i window_get_size_with_decorations(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;

	virtual void window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual DisplayServerEnums::WindowMode window_get_mode(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;

	virtual bool window_is_maximize_allowed(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual void window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual bool window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual void window_request_attention(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual void window_move_to_foreground(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) override;
	virtual bool window_is_focused(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual bool window_can_draw(DisplayServerEnums::WindowID p_window = DisplayServerEnums::MAIN_WINDOW_ID) const override;
	virtual bool can_any_window_draw() const override;

	// ---- 事件处理（骨架期：由引擎主循环驱动的输入队列，第 4 轮实现） ----
	virtual void process_events() override;

	// ---- 窗口通知（由 OHOS_XComponent 回调调用，ArkUI 主线程） ----
	// Surface 尺寸变化：同步窗口尺寸并触发 rect_changed 回调
	void notify_main_surface_resized();
	// 窗口聚焦状态变化：触发 WINDOW_EVENT_FOCUS_IN/OUT
	void notify_main_surface_focus(bool p_focused);

	// ---- 能力声明 ----
	virtual String get_name() const override;
	virtual bool has_feature(DisplayServerEnums::Feature p_feature) const override;

	// ---- 访问器 ----
	OHOS_XComponent *get_main_xcomponent() const { return main_xcomponent; }
	void set_main_xcomponent(OHOS_XComponent *p_xc) { main_xcomponent = p_xc; }
	// 注入屏幕刷新率（Index.ets @ohos.display 传入）
	void set_screen_refresh_rate(float p_rate) { screen_refresh_rate = p_rate; }

	// 更新屏幕列表（第 6 轮：@ohos.display getAllDisplays JSON 解析后注入）
	void set_screens(const Vector<OHOS_ScreenInfo> &p_screens) { screens = p_screens; }

	// ---- 输入法（第 8 轮：中文输入） ----
	// 获取输入法实例（首次访问时惰性创建）
	IME_OHOS *get_ime();
	// 文本控件聚焦：附加输入法服务（window_set_input_text_callback 触发）
	void ime_attach_for_text_input();
	// 文本控件失焦/窗口失焦：分离输入法
	void ime_detach_on_blur();

	// 拖拽文件投递（T-XC-1：ArkTS onDrop -> NAPI -> 主窗口 drop_files_callback）
	void notify_drop_files(const Vector<String> &p_files);
};