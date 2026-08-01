/**************************************************************************/
/*  main_ohos.cpp                                                         */
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

#include "crash_handler_ohos.h"
#include "display_server_ohos.h"
#include "ohos_bridge.h"
#include "ohos_xcomponent.h"
#include "os_ohos.h"
#include "rendering_context_driver_vulkan_ohos.h"

#include "core/string/print_string.h"
#include "core/input/input.h"
#include "core/os/mutex.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/vector2.h"
#include "main/main.h"

#include <napi/native_api.h>
#include <string>
#include <thread>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

#define OHOS_LOG_DOMAIN 0xD002D01
#define OHOS_LOG_TAG "GodotOHOS"

/* main_ohos.cpp —— Godot HarmonyOS NAPI 入口。
 *
 * 对应 macOS 的 godot_main_macos.mm（main 函数）职责，但鸿蒙无 main 入口：
 * 应用生命周期由 ArkTS UIAbility 驱动，C++ 侧通过 NAPI 导出初始化/启动/停止接口，
 * ArkTS 侧在 onWindowStageCreate / onForeground / onBackground 中调用。
 *
 * 线程模型（见移植方案 1.1）：
 * - ArkUI 主线程：执行 NAPI 调用（engine_initialize / engine_start / engine_stop）；
 * - 引擎线程（std::thread）：执行 Main::start + Main::iteration（本文件创建）；
 * - 渲染线程：Vulkan 内部（由渲染驱动管理）。
 *
 * 第 2 轮（功能深化）：
 * - 新增 engine_set_xcomponent：从 ArkTS XComponent onLoad 上下文获取
 *   OH_NativeXComponent 句柄，创建 OHOS_XComponent 并注册触摸/鼠标/键盘回调；
 * - engine_initialize 增加系统语言/设备型号/屏幕密度注入；
 * - 输入事件：ArkUI 主线程回调入队，引擎线程在 process_events 消费。
 */

// 全局崩溃处理器（initialize 时注册）
static CrashHandlerOHOS crash_handler;

// NAPI 侧注入的沙盒路径
static std::string sandbox_files_dir;
static std::string sandbox_cache_dir;

// XComponent 宿主（编辑器主窗口，由 engine_set_xcomponent 创建）
static OHOS_XComponent *ohos_xcomponent = nullptr;

// 引擎线程（Main::iteration 循环）
static std::thread engine_thread;
static bool engine_running = false;

// 屏幕刷新率（Index.ets @ohos.display 注入，DisplayServer 创建后生效）
static float screen_refresh_rate = 60.0f;

// ---- 剪贴板桥（第 5 轮） ----
// ArkTS 侧通过 registerClipboard 注册 set/get 回调（@ohos.pasteboard 实现）。
// 鸿蒙 NAPI 支持跨线程调用（API 9+ 线程安全 env），引擎线程直接调用。
static napi_env clipboard_env = nullptr;
static napi_ref clipboard_set_ref = nullptr;
static napi_ref clipboard_get_ref = nullptr;
static Mutex clipboard_mutex;

void ohos_clipboard_set_text(const String &p_text) {
	// 写剪贴板：调用 ArkTS 注册的回调（@ohos.pasteboard setData）
	MutexLock lock(clipboard_mutex);
	if (!clipboard_env || !clipboard_set_ref) {
		return;
	}
	napi_value global = nullptr;
	napi_get_global(clipboard_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(clipboard_env, clipboard_set_ref, &fn);
	napi_value str = nullptr;
	napi_create_string_utf8(clipboard_env, p_text.utf8().get_data(), p_text.length(), &str);
	napi_value argv[1] = { str };
	napi_value result = nullptr;
	napi_call_function(clipboard_env, global, fn, 1, argv, &result);
}

String ohos_clipboard_get_text() {
	// 读剪贴板：调用 ArkTS 注册的回调（@ohos.pasteboard getPasteData）
	MutexLock lock(clipboard_mutex);
	if (!clipboard_env || !clipboard_get_ref) {
		return String();
	}
	napi_value global = nullptr;
	napi_get_global(clipboard_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(clipboard_env, clipboard_get_ref, &fn);
	napi_value result = nullptr;
	napi_call_function(clipboard_env, global, fn, 0, nullptr, &result);
	if (result) {
		size_t len = 0;
		napi_get_value_string_utf8(clipboard_env, result, nullptr, 0, &len);
		if (len > 0) {
			Vector<char> buf;
			buf.resize(len + 1);
			napi_get_value_string_utf8(clipboard_env, result, buf.ptrw(), len + 1, &len);
			return String::utf8(buf.ptr());
		}
	}
	return String();
}

// 注册剪贴板回调（ArkTS: godot.registerClipboard(setFn, getFn) -> void）
static napi_value engine_register_clipboard(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 2) {
		return nullptr;
	}
	MutexLock lock(clipboard_mutex);
	clipboard_env = env;
	if (clipboard_set_ref) {
		napi_delete_reference(env, clipboard_set_ref);
	}
	if (clipboard_get_ref) {
		napi_delete_reference(env, clipboard_get_ref);
	}
	napi_create_reference(env, args[0], 1, &clipboard_set_ref);
	napi_create_reference(env, args[1], 1, &clipboard_get_ref);
	return nullptr;
}

// ---- 文件选择器桥（第 5 轮：@ohos.file.picker DocumentViewPicker） ----
// C++ 侧 DisplayServer::file_dialog_show 触发，经 NAPI 请求 ArkTS 打开系统
// 文件选择器；ArkTS 选择完成后调用 engine_file_picker_result 回传路径列表，
// 在引擎线程触发保存的 Callable。
static napi_env picker_env = nullptr;
static napi_ref picker_handler_ref = nullptr; // ArkTS 侧 pick 处理函数 (title, mode) => void
static Mutex picker_mutex;
static Callable picker_callback; // 待回传的 Godot Callable（单槽）

// ArkTS 注册文件选择器处理函数（Index.ets onAppear 调用）
static napi_value engine_register_file_picker(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(picker_mutex);
	picker_env = env;
	if (picker_handler_ref) {
		napi_delete_reference(env, picker_handler_ref);
	}
	napi_create_reference(env, args[0], 1, &picker_handler_ref);
	return nullptr;
}

// ArkTS 回传选择结果（Index.ets: godot.filePickerResult(pathsJson))
// pathsJson 为 JSON 字符串数组，如 '["/data/storage/el2/base/files/a.png"]'
static napi_value engine_file_picker_result(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	size_t len = 0;
	napi_get_value_string_utf8(env, args[0], nullptr, 0, &len);
	Vector<char> buf;
	buf.resize(len + 1);
	napi_get_value_string_utf8(env, args[0], buf.ptrw(), len + 1, &len);

	// 解析 JSON 路径数组（简化：逗号分隔，含引号去除）
	PackedStringArray paths;
	String json = String::utf8(buf.ptr());
	Vector<String> parts = json.split(",");
	for (const String &part : parts) {
		String p = part.strip_edges().trim_prefix("\"").trim_suffix("\"");
		if (!p.is_empty()) {
			paths.push_back(p);
		}
	}

	// 触发待回调（引擎线程侧由 DisplayServer::process_events 分发）
	MutexLock lock(picker_mutex);
	if (picker_callback.is_valid()) {
		Callable cb = picker_callback;
		picker_callback = Callable();
		cb.call(paths);
	}
	return nullptr;
}

Error ohos_pick_files(const String &p_title, int p_mode, const Callable &p_callback) {
	// 保存回调并通知 ArkTS 打开系统文件选择器
	MutexLock lock(picker_mutex);
	if (!picker_env || !picker_handler_ref) {
		// ArkTS 未注册选择器处理：回调空结果
		p_callback.call(PackedStringArray());
		return ERR_UNAVAILABLE;
	}
	picker_callback = p_callback;

	napi_value global = nullptr;
	napi_get_global(picker_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(picker_env, picker_handler_ref, &fn);
	napi_value title = nullptr;
	napi_create_string_utf8(picker_env, p_title.utf8().get_data(), p_title.length(), &title);
	napi_value mode = nullptr;
	napi_create_int32(picker_env, p_mode, &mode);
	napi_value argv[2] = { title, mode };
	napi_value result = nullptr;
	napi_call_function(picker_env, global, fn, 2, argv, &result);
	return OK;
}

// ---- 窗口模式桥（第 5 轮：@ohos.window） ----
// DisplayServer::window_set_mode 触发，经 NAPI 请求 ArkTS 应用窗口模式
// （全屏/最大化/窗口化/置顶）。
static napi_env window_env = nullptr;
static napi_ref window_mode_handler_ref = nullptr;
static Mutex window_mutex;

// ArkTS 注册窗口模式处理函数 (mode) => void
static napi_value engine_register_window_handler(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(window_mutex);
	window_env = env;
	if (window_mode_handler_ref) {
		napi_delete_reference(env, window_mode_handler_ref);
	}
	napi_create_reference(env, args[0], 1, &window_mode_handler_ref);
	return nullptr;
}

void ohos_window_set_mode(int p_mode) {
	// 通知 ArkTS 应用窗口模式（全屏/最大化等）
	MutexLock lock(window_mutex);
	if (!window_env || !window_mode_handler_ref) {
		return;
	}
	napi_value global = nullptr;
	napi_get_global(window_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(window_env, window_mode_handler_ref, &fn);
	napi_value mode = nullptr;
	napi_create_int32(window_env, p_mode, &mode);
	napi_value argv[1] = { mode };
	napi_value result = nullptr;
	napi_call_function(window_env, global, fn, 1, argv, &result);
}

void ohos_window_set_always_on_top(bool p_enabled) {
	// 置顶经 NAPI 传负值模式标记（0 恢复 / 3 置顶由 ArkTS 侧解释）
	// 简化：置顶使用独立模式标记 1001，与 WindowMode 枚举不冲突。
	// （完整窗口 API 见第 7 轮子窗口期。）
	ohos_window_set_mode(p_enabled ? 1001 : 0);
}

// ---- 子窗口桥（第 7 轮：@ohos.window createWindow） ----
// 引擎侧 create_sub_window 触发，经 NAPI 请求 ArkTS 创建/调整原生子窗口。
// 统一 handler 签名：registerSubWindowHandler((op, id, a, b, c, title) => void)
//   op: 0=create 1=destroy 2=setTitle 3=setRect 4=setVisible
//   a/b/c: 随 op 而异的整型参数（x/y/宽/高/可见性）
static napi_env subwindow_env = nullptr;
static napi_ref subwindow_handler_ref = nullptr;
static Mutex subwindow_mutex;

static napi_value engine_register_subwindow_handler(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(subwindow_mutex);
	subwindow_env = env;
	if (subwindow_handler_ref) {
		napi_delete_reference(env, subwindow_handler_ref);
	}
	napi_create_reference(env, args[0], 1, &subwindow_handler_ref);
	return nullptr;
}

static void subwindow_call(int p_op, int p_id, int p_a, int p_b, int p_c, int p_d, const char *p_title) {
	MutexLock lock(subwindow_mutex);
	if (!subwindow_env || !subwindow_handler_ref) {
		return;
	}
	napi_value global = nullptr;
	napi_get_global(subwindow_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(subwindow_env, subwindow_handler_ref, &fn);
	napi_value op = nullptr, id = nullptr, a = nullptr, b = nullptr, c = nullptr, d = nullptr, title = nullptr;
	napi_create_int32(subwindow_env, p_op, &op);
	napi_create_int32(subwindow_env, p_id, &id);
	napi_create_int32(subwindow_env, p_a, &a);
	napi_create_int32(subwindow_env, p_b, &b);
	napi_create_int32(subwindow_env, p_c, &c);
	napi_create_int32(subwindow_env, p_d, &d);
	napi_create_string_utf8(subwindow_env, p_title ? p_title : "", p_title ? strlen(p_title) : 0, &title);
	napi_value argv[7] = { op, id, a, b, c, d, title };
	napi_value result = nullptr;
	napi_call_function(subwindow_env, global, fn, 7, argv, &result);
}

void ohos_subwindow_create(int p_id, int p_x, int p_y, int p_w, int p_h) {
	subwindow_call(0, p_id, p_x, p_y, p_w, p_h, nullptr);
}

void ohos_subwindow_destroy(int p_id) {
	subwindow_call(1, p_id, 0, 0, 0, 0, nullptr);
}

void ohos_subwindow_set_title(int p_id, const String &p_title) {
	subwindow_call(2, p_id, 0, 0, 0, 0, p_title.utf8().get_data());
}

void ohos_subwindow_set_rect(int p_id, int p_x, int p_y, int p_w, int p_h) {
	subwindow_call(3, p_id, p_x, p_y, p_w, p_h, nullptr);
}

void ohos_subwindow_set_visible(int p_id, bool p_visible) {
	subwindow_call(4, p_id, p_visible ? 1 : 0, 0, 0, 0, nullptr);
}

// ---- 指针可见性桥（第 7 轮：@ohos.multimodalInput.pointer） ----
static napi_env pointer_env = nullptr;
static napi_ref pointer_handler_ref = nullptr;
static Mutex pointer_mutex;

static napi_value engine_register_pointer_handler(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(pointer_mutex);
	pointer_env = env;
	if (pointer_handler_ref) {
		napi_delete_reference(env, pointer_handler_ref);
	}
	napi_create_reference(env, args[0], 1, &pointer_handler_ref);
	return nullptr;
}

void ohos_mouse_set_visible(bool p_visible) {
	// 设置系统指针可见性（对应 macOS CGDisplayHideCursor）
	MutexLock lock(pointer_mutex);
	if (!pointer_env || !pointer_handler_ref) {
		return;
	}
	napi_value global = nullptr;
	napi_get_global(pointer_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(pointer_env, pointer_handler_ref, &fn);
	napi_value visible = nullptr;
	napi_get_boolean(pointer_env, p_visible, &visible);
	napi_value argv[1] = { visible };
	napi_value result = nullptr;
	napi_call_function(pointer_env, global, fn, 1, argv, &result);
}

// ---- 光标形状桥（第 8 轮：@ohos.multimodalInput.pointer.setPointerStyle） ----
// DisplayServer::cursor_set_shape 触发，请求 ArkTS 切换系统光标形状
//（对应 macOS NSCursor set / resetCursorRects）。
static napi_env cursor_env = nullptr;
static napi_ref cursor_handler_ref = nullptr;
static Mutex cursor_mutex;

// ArkTS 注册光标处理函数 (shape) => void
static napi_value engine_register_cursor_handler(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(cursor_mutex);
	cursor_env = env;
	if (cursor_handler_ref) {
		napi_delete_reference(env, cursor_handler_ref);
	}
	napi_create_reference(env, args[0], 1, &cursor_handler_ref);
	return nullptr;
}

void ohos_cursor_set_shape(int p_shape) {
	// 请求 ArkTS 切换系统光标（Godot CursorShape -> PointerStyle 映射在 ArkTS 侧）
	MutexLock lock(cursor_mutex);
	if (!cursor_env || !cursor_handler_ref) {
		return;
	}
	napi_value global = nullptr;
	napi_get_global(cursor_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(cursor_env, cursor_handler_ref, &fn);
	napi_value shape = nullptr;
	napi_create_int32(cursor_env, p_shape, &shape);
	napi_value argv[1] = { shape };
	napi_value result = nullptr;
	napi_call_function(cursor_env, global, fn, 1, argv, &result);
}

// ---- 滚轮注入（第 8 轮：触控板双指滚动手势 -> 引擎滚轮事件） ----
// XComponent 原生鼠标事件不携带滚轮，触控板双指滚动由 ArkTS 手势识别后
// 经本 NAPI 注入滚轮增量（对应 macOS scrollWheel scrollingDelta）。
static napi_value engine_inject_wheel(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	double dx = 0.0;
	double dy = 0.0;
	if (argc >= 1) {
		napi_get_value_double(env, args[0], &dx);
	}
	if (argc >= 2) {
		napi_get_value_double(env, args[1], &dy);
	}
	// 入队到 XComponent 输入队列（引擎线程 process_events 消费）
	OHOS_XComponent *xc = OHOS_XComponent::get_instance();
	if (xc) {
		xc->push_wheel_event(Vector2(static_cast<float>(dx), static_cast<float>(dy)));
	}
	return nullptr;
}

// ---- 手柄设备枚举桥（第 8 轮：@ohos.multimodalInput.inputDevice） ----
// 引擎初始化时请求 ArkTS 枚举全部输入设备，过滤 joystick 后回传
//（对应 macOS IOHIDManagerCopyDevices / Android InputDevice.getDeviceIds）。
static napi_env gamepad_env = nullptr;
static napi_ref gamepad_handler_ref = nullptr;
static Mutex gamepad_mutex;

// ArkTS 注册手柄枚举请求处理函数 () => void
static napi_value engine_register_gamepad_handler(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(gamepad_mutex);
	gamepad_env = env;
	if (gamepad_handler_ref) {
		napi_delete_reference(env, gamepad_handler_ref);
	}
	napi_create_reference(env, args[0], 1, &gamepad_handler_ref);
	return nullptr;
}

void ohos_enumerate_gamepads() {
	// 请求 ArkTS 枚举输入设备（异步，结果经 engine_gamepad_devices 回传）
	MutexLock lock(gamepad_mutex);
	if (!gamepad_env || !gamepad_handler_ref) {
		return;
	}
	napi_value global = nullptr;
	napi_get_global(gamepad_env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(gamepad_env, gamepad_handler_ref, &fn);
	napi_value result = nullptr;
	napi_call_function(gamepad_env, global, fn, 0, nullptr, &result);
}

// ArkTS 回传手柄设备 JSON 数组（Index.ets: godot.gamepadDevices(json)）
// json 形如 '[{"id":1,"name":"Gamepad","vendor":0,"product":0}, ...]'
static napi_value engine_gamepad_devices(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	size_t len = 0;
	napi_get_value_string_utf8(env, args[0], nullptr, 0, &len);
	Vector<char> buf;
	buf.resize(len + 1);
	napi_get_value_string_utf8(env, args[0], buf.ptrw(), len + 1, &len);

	// 解析 JSON 设备列表并上报 Input 单例（joy_connection_changed）
	if (!Input::get_singleton()) {
		return nullptr; // 引擎尚未初始化 Input
	}
	Variant json = JSON::parse_string(String::utf8(buf.ptr()));
	if (json.get_type() != Variant::ARRAY) {
		return nullptr;
	}
	Array devices = json;
	for (int i = 0; i < devices.size(); i++) {
		if (devices[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		Dictionary d = devices[i];
		int id = static_cast<int>(d["id"]);
		String name = d.has("name") ? String(d["name"]) : "OHOS Gamepad";
		String guid = d.has("guid") ? String(d["guid"]) : String();
		if (guid.is_empty()) {
			// 无 GUID 时用设备名生成稳定 GUID（Input 层手柄映射用）
			guid = name.md5_text();
		}
		Input::get_singleton()->joy_connection_changed(id, true, name, guid);
		print_verbose(vformat("OHOS: gamepad %d \"%s\" connected", id, name));
	}
	return nullptr;
}

// ---- 屏幕枚举桥（第 6 轮：@ohos.display getAllDisplays） ----
// ArkTS 侧在 onAppear 时查询全部屏幕（位置/尺寸/DPI/刷新率）并经
// godot.updateDisplays(JSON) 回传，C++ 解析后注入 DisplayServerOHOS。
static napi_value engine_update_displays(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	size_t len = 0;
	napi_get_value_string_utf8(env, args[0], nullptr, 0, &len);
	Vector<char> buf;
	buf.resize(len + 1);
	napi_get_value_string_utf8(env, args[0], buf.ptrw(), len + 1, &len);

	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (!ds) {
		return nullptr;
	}
	// 解析 JSON 数组：[{"x":0,"y":0,"w":1920,"h":1080,"dpi":160,"refresh":60}, ...]
	Vector<DisplayServerOHOS::OHOS_ScreenInfo> screen_list;
	Variant json = JSON::parse_string(String::utf8(buf.ptr()));
	if (json.get_type() == Variant::ARRAY) {
		Array arr = json;
		for (int i = 0; i < arr.size(); i++) {
			if (arr[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary d = arr[i];
			DisplayServerOHOS::OHOS_ScreenInfo si;
			si.position = Point2i(static_cast<int>(d["x"]), static_cast<int>(d["y"]));
			si.size = Size2i(static_cast<int>(d["w"]), static_cast<int>(d["h"]));
			si.dpi = static_cast<int>(d["dpi"]);
			si.refresh_rate = static_cast<float>(static_cast<double>(d["refresh"]));
			screen_list.push_back(si);
		}
	}
	ds->set_screens(screen_list);
	return nullptr;
}

// ---- rawfile 资源桥（第 6 轮：HAP 内嵌资源读取） ----// 鸿蒙 HAP 内的 rawfile（如导出的 main.pck）不能直接以文件路径读取，
// 需经资源管理器（NativeResourceManager）API。ArkTS 侧将
// getContext(this).resourceManager 传给 NAPI，C++ 初始化后即可读取。
static NativeResourceManager *g_res_mgr = nullptr;
static Mutex res_mgr_mutex;

// 初始化资源管理器（ArkTS: godot.initResourceManager(resourceManager) -> void）
static napi_value engine_init_resource_manager(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}
	MutexLock lock(res_mgr_mutex);
	if (g_res_mgr) {
		OH_ResourceManager_ReleaseNativeResourceManager(g_res_mgr);
	}
	// 从 ArkTS resourceManager 对象初始化原生资源管理器
	g_res_mgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
	return nullptr;
}

Error ohos_extract_raw_file(const String &p_name, const String &p_dest) {
	// 从 HAP rawfile 提取文件到沙盒（如 main.pck -> <filesDir>/main.pck）
	MutexLock lock(res_mgr_mutex);
	if (!g_res_mgr) {
		return ERR_UNAVAILABLE;
	}
	RawFile *raw = OH_ResourceManager_OpenRawFile(g_res_mgr, p_name.utf8().get_data());
	if (!raw) {
		return ERR_FILE_NOT_FOUND;
	}
	long len = OH_ResourceManager_GetRawFileSize(raw);
	Vector<uint8_t> data;
	data.resize(static_cast<int>(len));
	if (len > 0) {
		int got = OH_ResourceManager_ReadRawFile(raw, data.ptrw(), static_cast<size_t>(len));
		if (got != len) {
			OH_ResourceManager_CloseRawFile(raw);
			return ERR_FILE_CORRUPT;
		}
	}
	OH_ResourceManager_CloseRawFile(raw);

	Ref<FileAccess> out = FileAccess::open(p_dest, FileAccess::WRITE);
	if (out.is_null()) {
		return ERR_CANT_OPEN;
	}
	out->store_buffer(data);
	out->close();
	return OK;
}

// ---- 内部工具：获取 NAPI 字符串参数 ----
static std::string get_string_param(napi_env env, napi_value value) {
	size_t len = 0;
	napi_get_value_string_utf8(env, value, nullptr, 0, &len);
	std::string str(len, '\0');
	napi_get_value_string_utf8(env, value, &str[0], len + 1, &len);
	return str;
}

// ---- 引擎线程入口：启动 + 主循环迭代 ----
static void engine_thread_main() {
	// 引擎线程：跑主循环直到停止
	// 鸿蒙无传统 main(argc, argv)：构造参数列表。
	// 若沙盒内存在 main.pck（从 HAP rawfile 提取），以 --main-pack 加载导出游戏。
	std::vector<std::string> arg_strs;
	arg_strs.push_back("godot");
	String main_pack = String::utf8(sandbox_files_dir.c_str()).path_join("main.pck");
	if (FileAccess::exists(main_pack)) {
		arg_strs.push_back("--main-pack");
		arg_strs.push_back(main_pack.utf8().get_data());
	}
	std::vector<char *> argv;
	for (const std::string &s : arg_strs) {
		argv.push_back(const_cast<char *>(s.c_str()));
	}
	int argc = static_cast<int>(argv.size());

	if (Main::setup(argv[0], argc, argv.data()) != OK) {
		// 启动失败：直接结束
		engine_running = false;
		return;
	}
	Main::start();

	// DisplayServer 创建后注入：屏幕刷新率 + XComponent 宿主
	// （setXComponent 调用早于引擎启动，此处统一挂接）
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->set_screen_refresh_rate(screen_refresh_rate);
		if (ohos_xcomponent) {
			ds->set_main_xcomponent(ohos_xcomponent);
		}
	}

	while (engine_running) {
		// Surface 未就绪（窗口最小化/隐藏/未创建）时休眠节流：
		// Godot 渲染无 Surface 时 present 等待不会发生，忙轮询会空转 CPU
		//（对应 macOS CVDisplayLink 帧调度；此处以 60fps 间隔兜底）。
		if (ohos_xcomponent && !ohos_xcomponent->is_surface_ready()) {
			OS::get_singleton()->delay_usec(16000);
		}
		// 单帧迭代：返回 true 表示引擎请求退出；false 表示继续
		if (Main::iteration()) {
			break;
		}
	}

	// 收尾：Main::cleanup 内部会删除主循环
	Main::cleanup();
	engine_running = false;
}

// ---- NAPI 导出函数 ----

// 设置 XComponent（ArkTS: godot.setXComponent(xcomponentContext) -> void）
// 从 XComponent onLoad 回调的 context 中取 OH_NativeXComponent 句柄，
// 创建 OHOS_XComponent 并注册 surface/touch/mouse/key 事件回调。
static napi_value engine_set_xcomponent(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	if (argc < 1) {
		return nullptr;
	}

	// context.nativeXComponent 为 XComponentNativeInstance，unrap 得到原生句柄
	napi_value native_xcomponent_val = nullptr;
	napi_get_named_property(env, args[0], "nativeXComponent", &native_xcomponent_val);
	OH_NativeXComponent *native_xcomponent = nullptr;
	if (native_xcomponent_val != nullptr) {
		napi_unwrap(env, native_xcomponent_val, reinterpret_cast<void **>(&native_xcomponent));
	}
	if (!native_xcomponent) {
		OH_LOG_Print(LOG_APP, LOG_ERROR, OHOS_LOG_DOMAIN, OHOS_LOG_TAG, "setXComponent: no nativeXComponent");
		return nullptr;
	}

	// 创建 XComponent 宿主（编辑器单 XComponent，复用已创建的实例）
	if (!ohos_xcomponent) {
		ohos_xcomponent = memnew(OHOS_XComponent);
	}

	// 模拟 SurfaceCreated 事件注册（回调在 on_surface_created 时已有窗口句柄）。
	// 实际回调注册依赖 OH_NativeXComponent，需先完成 on_surface_created；
	// 这里保存原生句柄，注册动作放在 surface 回调后由 register_callbacks 完成。
	// 为兼容「SurfaceCreated 早于 NAPI 调用」顺序，直接尝试注册。
	// 注：OH_NativeXComponent_RegisterCallback 在任意时刻调用均可，
	// 回调触发依赖 Surface 生命周期。
	ohos_xcomponent->register_callbacks();
	ohos_xcomponent->set_xcomponent(native_xcomponent);

	// 注入 XComponent 到 DisplayServer（若已创建）
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->set_main_xcomponent(ohos_xcomponent);
	}

	OH_LOG_Print(LOG_APP, LOG_INFO, OHOS_LOG_DOMAIN, OHOS_LOG_TAG, "setXComponent: OK");
	return nullptr;
}

// 初始化引擎（ArkTS: engine.initialize(filesDir, cacheDir[, locale, model, density, refreshRate]) -> void）
static napi_value engine_initialize(napi_env env, napi_callback_info info) {
	size_t argc = 6;
	napi_value args[6];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	if (argc >= 1) {
		sandbox_files_dir = get_string_param(env, args[0]);
	}
	if (argc >= 2) {
		sandbox_cache_dir = get_string_param(env, args[1]);
	}

	// 注册平台崩溃处理器
	crash_handler.initialize();

	// 创建 OS_OHOS 单例并注入沙盒路径
	OS_OHOS *os = memnew(OS_OHOS);
	os->set_sandbox_paths(String::utf8(sandbox_files_dir.c_str()), String::utf8(sandbox_cache_dir.c_str()));

	// 注入系统信息（可选参数：语言/型号/密度）
	if (argc >= 3) {
		os->set_system_locale(String::utf8(get_string_param(env, args[2]).c_str()));
	}
	if (argc >= 4) {
		os->set_model_name(String::utf8(get_string_param(env, args[3]).c_str()));
	}
	if (argc >= 5) {
		double density = 1.0;
		napi_get_value_double(env, args[4], &density);
		os->set_screen_density(static_cast<float>(density));
	}

	// 注入屏幕刷新率（@ohos.display refreshRate）
	if (argc >= 6) {
		double refresh_rate = 60.0;
		napi_get_value_double(env, args[5], &refresh_rate);
		// 记录到静态变量，DisplayServer 创建后生效
		screen_refresh_rate = static_cast<float>(refresh_rate);
	}

	// 注册 OHOS 显示驱动
	DisplayServerOHOS::register_ohos_driver();

	print_line("Godot Engine (HarmonyOS) initialized.");
	return nullptr;
}

// 启动引擎（ArkTS: engine.start(width, height) -> void）
static napi_value engine_start(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	int width = 800;
	int height = 600;
	if (argc >= 1) {
		napi_get_value_int32(env, args[0], &width);
	}
	if (argc >= 2) {
		napi_get_value_int32(env, args[1], &height);
	}

	// 提取 HAP rawfile 中的 main.pck 到沙盒（存在才提取；编辑器运行时可无）
	String pack_dest = String::utf8(sandbox_files_dir.c_str()).path_join("main.pck");
	Error extract_err = ohos_extract_raw_file("main.pck", pack_dest);
	if (extract_err == OK) {
		print_line("Godot Engine: extracted main.pck from rawfile.");
	}

	// 启动引擎线程（主循环在子线程迭代，ArkUI 主线程保持响应）
	if (engine_running) {
		return nullptr; // 已启动则忽略重复调用
	}
	engine_running = true;
	engine_thread = std::thread(engine_thread_main);

	print_line(vformat("Godot Engine started (%dx%d).", width, height));
	return nullptr;
}

// 停止引擎（ArkTS: engine.stop() -> void）
static napi_value engine_stop(napi_env env, napi_callback_info info) {
	// 请求主循环退出
	engine_running = false;
	if (engine_thread.joinable()) {
		engine_thread.join();
	}
	print_line("Godot Engine stopped.");
	return nullptr;
}

// 通知窗口聚焦状态（ArkTS: engine.notifyFocus(focused) -> void）
// XComponent 的 RegisterFocusEventCallback 仅报告获得焦点，失焦需 ArkTS 侧
// onBlur 回调通知（对应 macOS windowDidResignMain）。
static napi_value engine_notify_focus(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	bool focused = false;
	if (argc >= 1) {
		napi_get_value_bool(env, args[0], &focused);
	}
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->notify_main_surface_focus(focused);
	}
	return nullptr;
}

// 释放引擎资源（ArkTS: engine.dispose() -> void）
static napi_value engine_dispose(napi_env env, napi_callback_info info) {
	// 清空 XComponent 输入队列并销毁
	if (ohos_xcomponent) {
		ohos_xcomponent->clear_input_events();
		memdelete(ohos_xcomponent);
		ohos_xcomponent = nullptr;
	}

	// 销毁 OS_OHOS 单例
	OS_OHOS *os = OS_OHOS::get_singleton();
	if (os) {
		memdelete(os);
	}
	crash_handler.disable();
	print_line("Godot Engine (HarmonyOS) disposed.");
	return nullptr;
}

// ---- NAPI 模块注册 ----
static napi_value module_init(napi_env env, napi_value exports) {
	napi_property_descriptor props[] = {
		{ "initialize", nullptr, engine_initialize, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "setXComponent", nullptr, engine_set_xcomponent, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "start", nullptr, engine_start, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "stop", nullptr, engine_stop, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "notifyFocus", nullptr, engine_notify_focus, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerClipboard", nullptr, engine_register_clipboard, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerFilePicker", nullptr, engine_register_file_picker, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "filePickerResult", nullptr, engine_file_picker_result, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerWindowHandler", nullptr, engine_register_window_handler, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "initResourceManager", nullptr, engine_init_resource_manager, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "updateDisplays", nullptr, engine_update_displays, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerSubWindowHandler", nullptr, engine_register_subwindow_handler, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerPointerHandler", nullptr, engine_register_pointer_handler, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerCursorHandler", nullptr, engine_register_cursor_handler, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "injectWheel", nullptr, engine_inject_wheel, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "registerGamepadHandler", nullptr, engine_register_gamepad_handler, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "gamepadDevices", nullptr, engine_gamepad_devices, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "dispose", nullptr, engine_dispose, nullptr, nullptr, nullptr, napi_default, nullptr },
	};
	napi_define_properties(env, exports, 18, props);
	return exports;
}

// NAPI 模块声明（对应 CMake 中的 moduleName = "godot"）
static napi_module godot_module = {
	.nm_version = 1,
	.nm_flags = 0,
	.nm_filename = nullptr,
	.nm_register_func = module_init,
	.nm_modname = "godot",
	.nm_priv = nullptr,
	.reserved = { 0 },
};

extern "C" __attribute__((constructor)) void register_godot_module(void) {
	napi_module_register(&godot_module);
}
