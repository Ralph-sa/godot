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

#include "os_ohos.h"

#include "core/config/project_settings.h"
#include "core/string/print_string.h"
#include "main/main.h"

// 静态创建函数（DisplayServer 注册用），对应 macOS create_func
static DisplayServer *create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	// 骨架期：直接创建 DisplayServerOHOS；渲染驱动不匹配时返回错误
	if (p_rendering_driver != "vulkan") {
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
	return drivers;
}

void DisplayServerOHOS::register_ohos_driver() {
	// 注册 display_driver = "ohos"，与 main.cpp 的 display_driver 枚举对应
	register_create_function("ohos", create_func, get_rendering_drivers_func);
}

DisplayServerOHOS::DisplayServerOHOS(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, const Vector2i &p_size) {
	rendering_driver = p_rendering_driver;
	window_size = p_size;

	// 创建主窗口对象（骨架期：XComponent 宿主由 main_ohos.cpp 注入）
	OHOS_Window *main_window = memnew(OHOS_Window(OHOS_Window::WINDOW_TYPE_MAIN));
	main_window->set_title("Godot Editor (HarmonyOS)");
	main_window->set_rect(Rect2i(0, 0, p_size.x, p_size.y));
	windows[DisplayServerEnums::MAIN_WINDOW_ID] = main_window;

	print_line(vformat("DisplayServerOHOS initialized (%s), window size %dx%d", p_rendering_driver, p_size.x, p_size.y));
}

DisplayServerOHOS::~DisplayServerOHOS() {
	// 释放窗口对象
	for (const KeyValue<DisplayServerEnums::WindowID, OHOS_Window *> &E : windows) {
		memdelete(E.value);
	}
	windows.clear();
}

Size2i DisplayServerOHOS::window_get_size(DisplayServerEnums::WindowID p_window) const {
	// 骨架期：返回记录的主窗口尺寸（后续从 XComponent 实际尺寸读取）
	return window_size;
}

void DisplayServerOHOS::window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	windows[p_window]->set_title(p_title);
	// 骨架期：不真正修改系统窗口标题（ArkUI Window 桥接后续轮次实现）
}

String DisplayServerOHOS::window_get_title(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), String(), "Invalid window ID.");
	return windows[p_window]->get_title();
}

void DisplayServerOHOS::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	ERR_FAIL_COND_MSG(!windows.has(p_window), "Invalid window ID.");
	window_size = p_size;
	windows[p_window]->resize(p_size);
	// 通知渲染驱动 swapchain 重建（渲染驱动骨架期由 main_ohos.cpp 处理）
}

bool DisplayServerOHOS::window_is_visible(DisplayServerEnums::WindowID p_window) const {
	ERR_FAIL_COND_V_MSG(!windows.has(p_window), false, "Invalid window ID.");
	return windows[p_window]->is_visible();
}

bool DisplayServerOHOS::has_window(DisplayServerEnums::WindowID p_window) const {
	return windows.has(p_window);
}

int DisplayServerOHOS::get_screen_count() const {
	// 骨架期：主屏幕 = 1（多屏支持后续轮次接入 OH_DisplayManager）
	return 1;
}

int DisplayServerOHOS::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerOHOS::screen_get_position(int p_screen) const {
	// 骨架期：主屏幕原点 (0,0)
	return Point2i(0, 0);
}

Size2i DisplayServerOHOS::screen_get_size(int p_screen) const {
	// 骨架期：屏幕尺寸 = 主窗口尺寸
	return window_size;
}

Rect2i DisplayServerOHOS::screen_get_usable_rect(int p_screen) const {
	// 骨架期：可用区域 = 屏幕区域
	return Rect2i(0, 0, window_size.x, window_size.y);
}

int DisplayServerOHOS::screen_get_dpi(int p_screen) const {
	// 骨架期：默认 DPI 96（完整实现后续通过系统参数获取）
	return 96;
}

float DisplayServerOHOS::screen_get_refresh_rate(int p_screen) const {
	// 骨架期：默认 60Hz（完整实现后续通过 OH_DisplayManager 获取）
	return 60.0;
}

void DisplayServerOHOS::process_events() {
	// 骨架期：输入事件队列处理留待第 4 轮（输入事件分发）实现
	// 引擎主循环通过 Main::iteration 间接调用本方法
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
	// 骨架期：窗口模式（全屏/最大化等）后续接入 ArkUI Window API
}

DisplayServerEnums::WindowMode DisplayServerOHOS::window_get_mode(DisplayServerEnums::WindowID p_window) const {
	return DisplayServerEnums::WINDOW_MODE_WINDOWED; // 骨架期：窗口模式
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
	// 骨架期：窗口前置后续实现
}

bool DisplayServerOHOS::window_is_focused(DisplayServerEnums::WindowID p_window) const {
	// 骨架期：主窗口视为聚焦
	return p_window == DisplayServerEnums::MAIN_WINDOW_ID;
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
			// 编辑器多窗口支持（第 5 轮实现后置 true）
			return false;
		case DisplayServerEnums::FEATURE_HIDPI:
			return true;
		default:
			return false;
	}
}
