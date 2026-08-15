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
#include "core/error/error_macros.h"
#include "core/input/input.h"
#include "core/os/mutex.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/vector2.h"
#include "main/main.h"

#include <napi/native_api.h>
#include <node_api.h>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <future>
#include <sys/stat.h>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <rawfile/raw_file.h>
#include <rawfile/raw_file_manager.h>

#define OHOS_LOG_DOMAIN 0xD001
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

// ---- 引擎日志桥接 hilog（诊断用）----
// 鸿蒙应用无 stdout/stderr 终端，Godot print_line/print_error 输出需转发到
// hilog 才能通过 hdc shell hilog 查看（print_string.h 的 print handler 机制）。
static PrintHandlerList ohos_print_handler;
static ErrorHandlerList ohos_error_handler;

static void ohos_print_func(void *p_userdata, const String &p_string, bool p_error, bool p_rich) {
	// 转印到 hilog（LOG_APP 域），便于 hdc 抓取引擎初始化/渲染日志
	if (p_error) {
		OH_LOG_Print(LOG_APP, LOG_ERROR, OHOS_LOG_DOMAIN, OHOS_LOG_TAG, "%.*s", (int)p_string.length(), p_string.utf8().get_data());
	} else {
		OH_LOG_Print(LOG_APP, LOG_INFO, OHOS_LOG_DOMAIN, OHOS_LOG_TAG, "%.*s", (int)p_string.length(), p_string.utf8().get_data());
	}
}

static void ohos_error_func(void *p_userdata, const char *p_func, const char *p_file, int p_line, const char *p_error, const char *p_descr, bool p_editor_notify, ErrorHandlerType p_type) {
	OH_LOG_Print(LOG_APP, LOG_FATAL, OHOS_LOG_DOMAIN, OHOS_LOG_TAG,
			"ERR[%s] %s:%d: %s\n  %s", p_func, p_file, p_line, p_error, p_descr ? p_descr : "");
}

// ---- 通用跨线程 JS 调用调度器（第 10 轮修复）----
// 问题：HarmonyOS 的 napi_call_function 只能在 JS 主线程执行。
// 引擎线程（std::thread）直接调用会触发 EcmaVM::CheckThread() 断言，
// 导致 Fatal: ecma_vm cannot run in multi-thread（SIGABRT 崩溃）。
// 官方推荐用 napi_threadsafe_function 将调用安全投递回 JS 主线程执行。
//
// 本调度器封装一次「引擎线程 -> JS 主线程」的函数调用：
//  - ohos_tsfn_invoke() 由引擎线程发起，构造参数块后投递；
//  - call_js_cb() 在 JS 主线程执行，解析参数并 napi_call_function；
//  - 需要同步结果的调用使用 std::promise/future 阻塞等待（blocking 模式）。
struct OHOS_TSFNArg {
	enum class Type { INT, DOUBLE, BOOL, STRING, NONE } type = Type::NONE;
	int32_t i = 0;
	double d = 0.0;
	bool b = false;
	std::string s; // 字符串参数（值拷贝，随 call 生命周期管理，避免悬垂指针）
};

struct OHOS_TSFNCall {
	napi_ref fn_ref = nullptr;                  // 目标 JS 函数引用（call_js_cb 内解析）
	Vector<OHOS_TSFNArg> args;                  // 参数列表（0~8）
	// 非空时阻塞等待 JS 返回字符串。shared_ptr 保证引擎线程超时返回后，
	// JS 侧回调仍能安全 set_value（避免栈上 promise 悬垂）
	std::shared_ptr<std::promise<std::string>> done_str;
};

// JS 主线程回调：解析参数并调用目标函数（tsfn 的 call_js_cb，运行在 JS 主线程）
static void ohos_tsfn_call_js(napi_env env, napi_value js_cb, void *context, void *data) {
	OHOS_TSFNCall *call = static_cast<OHOS_TSFNCall *>(data);
	if (!call || !call->fn_ref) {
		if (call && call->done_str) {
			call->done_str->set_value("");
		}
		delete call;
		return;
	}
	napi_value global = nullptr;
	napi_get_global(env, &global);
	napi_value fn = nullptr;
	napi_get_reference_value(env, call->fn_ref, &fn);
	napi_value result = nullptr;
	if (call->args.is_empty()) {
		napi_call_function(env, global, fn, 0, nullptr, &result);
	} else {
		napi_value argv[8];
		for (int i = 0; i < call->args.size(); i++) {
			const OHOS_TSFNArg &a = call->args[i];
			switch (a.type) {
				case OHOS_TSFNArg::Type::INT:
					napi_create_int32(env, a.i, &argv[i]);
					break;
				case OHOS_TSFNArg::Type::DOUBLE:
					napi_create_double(env, a.d, &argv[i]);
					break;
				case OHOS_TSFNArg::Type::BOOL:
					napi_create_int32(env, a.b ? 1 : 0, &argv[i]);
					break;
				case OHOS_TSFNArg::Type::STRING:
					napi_create_string_utf8(env, a.s.c_str(), a.s.size(), &argv[i]);
					break;
				default:
					napi_get_undefined(env, &argv[i]);
					break;
			}
		}
		napi_call_function(env, global, fn, call->args.size(), argv, &result);
	}
	// 需要字符串返回值（clipboard_get）：读取函数返回值
	if (call->done_str) {
		size_t len = 0;
		if (result) {
			napi_get_value_string_utf8(env, result, nullptr, 0, &len);
		}
		std::string s(len, '\0');
		if (len > 0) {
			napi_get_value_string_utf8(env, result, &s[0], len + 1, &len);
		}
		call->done_str->set_value(s);
	}
	delete call;
}

// 通用跨线程调用入口（引擎线程调用）：p_sync=true 时阻塞等待 JS 执行并返回其字符串值。
// 复用单一全局 tsfn（initial_thread_count=1，调用方固定为引擎线程）。
static napi_threadsafe_function ohos_tsfn = nullptr;
static Mutex ohos_tsfn_mutex;

static void ohos_tsfn_ensure(napi_env env) {
	// 加锁保护：注册与引擎线程可能在初始化期并发首次调用，避免重复创建/竞态
	MutexLock lock(ohos_tsfn_mutex);
	if (ohos_tsfn) {
		return;
	}
	napi_value resource_name = nullptr;
	napi_create_string_utf8(env, "GodotOHOSCallJS", NAPI_AUTO_LENGTH, &resource_name);
	napi_create_threadsafe_function(env, nullptr, nullptr, resource_name, 0, 1,
			nullptr, nullptr, nullptr, ohos_tsfn_call_js, &ohos_tsfn);
}

static String ohos_tsfn_invoke(napi_env p_env, napi_ref p_fn_ref, const Vector<OHOS_TSFNArg> &p_args, bool p_sync) {
	if (!p_env || !p_fn_ref) {
		return String();
	}
	ohos_tsfn_ensure(p_env);
	if (!ohos_tsfn) {
		return String();
	}
	OHOS_TSFNCall *call = memnew(OHOS_TSFNCall);
	call->fn_ref = p_fn_ref;
	call->args = p_args;
	std::future<std::string> fut;
	if (p_sync) {
		call->done_str = std::make_shared<std::promise<std::string>>();
		fut = call->done_str->get_future();
	}
	napi_status st = napi_call_threadsafe_function(ohos_tsfn, call, p_sync ? napi_tsfn_blocking : napi_tsfn_nonblocking);
	if (st != napi_ok) {
		delete call;
		return String();
	}
	if (p_sync) {
		// 有界等待（2s）：若 JS 主线程被占住（例如引擎 stop 时 ArkUI 主线程
		// 正在 join 引擎线程），无限等待会形成 JS↔引擎互等死锁。超时返回空串
		//（剪贴板读取失败语义，可接受）。
		if (fut.wait_for(std::chrono::seconds(2)) != std::future_status::ready) {
			return String();
		}
		std::string s = fut.get();
		return String::utf8(s.c_str());
	}
	return String();
}

// 便捷构造：无参调用（通知型）
static void ohos_tsfn_notify(napi_env p_env, napi_ref p_fn_ref) {
	(void)ohos_tsfn_invoke(p_env, p_fn_ref, Vector<OHOS_TSFNArg>(), false);
}

// 便捷构造：单 int32 参数
static void ohos_tsfn_int(napi_env p_env, napi_ref p_fn_ref, int32_t p_val) {
	Vector<OHOS_TSFNArg> args;
	OHOS_TSFNArg a;
	a.type = OHOS_TSFNArg::Type::INT;
	a.i = p_val;
	args.push_back(a);
	(void)ohos_tsfn_invoke(p_env, p_fn_ref, args, false);
}

// 便捷构造：单 bool 参数
static void ohos_tsfn_bool(napi_env p_env, napi_ref p_fn_ref, bool p_val) {
	Vector<OHOS_TSFNArg> args;
	OHOS_TSFNArg a;
	a.type = OHOS_TSFNArg::Type::BOOL;
	a.b = p_val;
	args.push_back(a);
	(void)ohos_tsfn_invoke(p_env, p_fn_ref, args, false);
}

// 便捷构造：单字符串参数
static void ohos_tsfn_str(napi_env p_env, napi_ref p_fn_ref, const String &p_val) {
	Vector<OHOS_TSFNArg> args;
	OHOS_TSFNArg a;
	a.type = OHOS_TSFNArg::Type::STRING;
	a.s = p_val.utf8().get_data();
	args.push_back(a);
	(void)ohos_tsfn_invoke(p_env, p_fn_ref, args, false);
}

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
	// 写剪贴板：调用 ArkTS 注册的回调（@ohos.pasteboard setData）。
	// 引擎线程调用，经 tsfn 投递到 JS 主线程执行（避免 CheckThread 崩溃）。
	MutexLock lock(clipboard_mutex);
	if (!clipboard_env || !clipboard_set_ref) {
		return;
	}
	ohos_tsfn_str(clipboard_env, clipboard_set_ref, p_text);
}

String ohos_clipboard_get_text() {
	// 读剪贴板：调用 ArkTS 注册的回调（@ohos.pasteboard getPasteData）。
	// 需同步返回结果：tsfn 阻塞模式等待 JS 主线程执行完成。
	MutexLock lock(clipboard_mutex);
	if (!clipboard_env || !clipboard_get_ref) {
		return String();
	}
	return ohos_tsfn_invoke(clipboard_env, clipboard_get_ref, Vector<OHOS_TSFNArg>(), true);
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

	// 经 tsfn 投递到 JS 主线程（引擎线程不能直接 napi_call_function）
	Vector<OHOS_TSFNArg> args;
	OHOS_TSFNArg a1;
	a1.type = OHOS_TSFNArg::Type::STRING;
	a1.s = p_title.utf8().get_data();
	OHOS_TSFNArg a2;
	a2.type = OHOS_TSFNArg::Type::INT;
	a2.i = p_mode;
	args.push_back(a1);
	args.push_back(a2);
	(void)ohos_tsfn_invoke(picker_env, picker_handler_ref, args, false);
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
	// 通知 ArkTS 应用窗口模式（全屏/最大化等）。引擎线程调用，经 tsfn 投递。
	MutexLock lock(window_mutex);
	if (!window_env || !window_mode_handler_ref) {
		return;
	}
	ohos_tsfn_int(window_env, window_mode_handler_ref, p_mode);
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
	// 经 tsfn 投递到 JS 主线程（引擎线程不能直接 napi_call_function）
	Vector<OHOS_TSFNArg> args;
	OHOS_TSFNArg v;
	auto add_int = [&](int32_t val) {
		v.type = OHOS_TSFNArg::Type::INT;
		v.i = val;
		args.push_back(v);
	};
	add_int(p_op);
	add_int(p_id);
	add_int(p_a);
	add_int(p_b);
	add_int(p_c);
	add_int(p_d);
	OHOS_TSFNArg t;
	t.type = OHOS_TSFNArg::Type::STRING;
	t.s = p_title ? p_title : "";
	args.push_back(t);
	(void)ohos_tsfn_invoke(subwindow_env, subwindow_handler_ref, args, false);
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
	// 设置系统指针可见性（对应 macOS CGDisplayHideCursor）。引擎线程调用，tsfn 投递。
	MutexLock lock(pointer_mutex);
	if (!pointer_env || !pointer_handler_ref) {
		return;
	}
	ohos_tsfn_bool(pointer_env, pointer_handler_ref, p_visible);
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
	// 引擎线程调用，经 tsfn 投递到 JS 主线程执行。
	MutexLock lock(cursor_mutex);
	if (!cursor_env || !cursor_handler_ref) {
		return;
	}
	ohos_tsfn_int(cursor_env, cursor_handler_ref, p_shape);
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
	// 注意：本函数由引擎线程（OS::initialize_joypads）调用，不能直接
	// napi_call_function（会触发 EcmaVM::CheckThread 崩溃），必须投递到 JS 主线程。
	MutexLock lock(gamepad_mutex);
	if (!gamepad_env || !gamepad_handler_ref) {
		return;
	}
	ohos_tsfn_notify(gamepad_env, gamepad_handler_ref);
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

// ---- 引擎线程诊断日志（与 display_server_ohos.cpp 的 ohos_diag_log 同路径） ----
// 卡死/崩溃场景下 hilog 的 LOG_APP 域可能丢失，关键阶段同时写沙盒 cacheDir
// 下的诊断文件，事后 hdc file recv 直接读取定位卡住阶段。
static void ohos_engine_diag(const char *p_fmt, ...) {
	// 写应用沙盒 cacheDir（NAPI 注入的 sandbox_cache_dir）；沙盒外路径不可写。
	std::string diag_path = sandbox_cache_dir;
	if (diag_path.empty()) {
		diag_path = "/data/storage/el2/base/haps/entry/cache";
	}
	diag_path += "/godot_engine_diag.log";
	FILE *f = fopen(diag_path.c_str(), "a");
	if (f) {
		va_list args;
		va_start(args, p_fmt);
		vfprintf(f, p_fmt, args);
		va_end(args);
		fprintf(f, "\n");
		fclose(f);
	}
}

// ---- 引擎线程入口：启动 + 主循环迭代 ----
static void engine_thread_main() {
	// 引擎线程：跑主循环直到停止。支持进程内重启（第 10 轮修复）：
	// 鸿蒙 App 进程由 appspawn 创建，无法 fork/exec 新进程（见
	// OS_OHOS::create_instance 注释），ProjectManager 打开项目时经
	// create_instance 记录参数（--path <proj> --editor）并退出主循环；
	// 本线程在 Main::cleanup 完成后消费参数重新 Main::setup + Main::start，
	// 以新 main loop（EditorNode）继续迭代 —— 与桌面端「新进程打开编辑器」
	// 语义等价，修复「创建项目后引擎核心卡住」问题。
	//
	// 鸿蒙无传统 main(argc, argv)：每轮构造参数列表。
	// 首轮若沙盒内存在 main.pck（从 HAP rawfile 提取），以 --main-pack 加载导出游戏。
	//
	// 注意：此处位于 Main::setup() 之前，OS_Unix::initialize_core() 尚未执行
	//（FileAccess::create_func / DirAccess 等均未注册），不能使用 FileAccess 检查文件。
	// 改用标准 C stat() 检测 main.pck 是否存在，避免 create_func 为 null 导致
	// FileAccess::create 返回空 Ref 后 FileAccess::exists 空指针崩溃（SIGSEGV）。
	bool first_run = true;
	List<String> restart_args;
	bool restart = true;

	while (restart && engine_running) {
		// 等待 XComponent Surface 就绪（第 10 轮修复）：
		// DisplayServerOHOS 构造时需要 OHNativeWindow 已就绪才能创建 Vulkan Surface，
		// 否则 RenderingDevice::initialize 因 main_surface==0 返回 FAILED，
		// RendererCompositorRD::make_current() 不被调用，引擎在渲染服务器 _init 崩溃。
		// on_surface_created 回调运行在 ArkUI 主线程，本子线程 sleep 不阻塞它。
		// 进程内重启时 Surface 仍在（XComponent 未销毁），此轮询立即通过。
		if (ohos_xcomponent) {
			for (int i = 0; i < 200 && !ohos_xcomponent->is_surface_ready(); i++) {
				std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 最多等 10s
			}
			// 注意：LOG_APP 域 + 合法 domain；faultlog 只含系统 core 域，
			// 关键路径另有文件日志（display_server_ohos.cpp 的 ohos_diag_log）
			OH_LOG_Print(LOG_APP, LOG_INFO, 0xD001, "GodotOHOS", "engine_thread: surface ready = %d (run %d)", (int)ohos_xcomponent->is_surface_ready(), first_run ? 1 : 2);
		}

		std::vector<std::string> arg_strs;
		arg_strs.push_back("godot");
		// 强制指定 Vulkan 渲染后端（第 10 轮修复）：
		// DisplayServerOHOS 构造时仅在 rendering_driver == "vulkan" 时才初始化
		// RenderingContextDriverVulkanOHOS 并调用 RendererCompositorRD::make_current()。
		// 若不显式传参，Main::setup 会从项目设置读渲染方法/driver，可能得到空值或
		// 非 vulkan 值，导致 _create_func 仍为 null，RenderingServerDefault::_init()
		// 中 RendererCompositor::create() 返回 nullptr 后空指针崩溃（SIGSEGV）。
		arg_strs.push_back("--rendering-driver");
		arg_strs.push_back("vulkan");
		arg_strs.push_back("--rendering-method");
		arg_strs.push_back("forward_plus");

		if (first_run) {
			first_run = false;
			String main_pack = String::utf8(sandbox_files_dir.c_str()).path_join("main.pck");
			struct stat st;
			if (stat(main_pack.utf8().get_data(), &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG) {
				arg_strs.push_back("--main-pack");
				arg_strs.push_back(main_pack.utf8().get_data());
			}
		} else {
			// 进程内重启：追加 create_instance 记录的参数
			//（--path <proj> --editor [--recovery-mode] [--verbose] [--run-upgrade-tool]）
			for (const String &a : restart_args) {
				arg_strs.push_back(a.utf8().get_data());
			}
			OH_LOG_Print(LOG_APP, LOG_INFO, 0xD001, "GodotOHOS", "engine_thread: in-process restart with %d extra args", (int)restart_args.size());
		}

		std::vector<char *> argv;
		for (const std::string &s : arg_strs) {
			argv.push_back(const_cast<char *>(s.c_str()));
		}
		int argc = static_cast<int>(argv.size());

		ohos_engine_diag("engine_thread: Main::setup begin (argc=%d)", argc);
		if (Main::setup(argv[0], argc, argv.data()) != OK) {
			// 启动失败：直接结束
			ohos_engine_diag("engine_thread: Main::setup FAILED");
			OH_LOG_Print(LOG_APP, LOG_ERROR, 0xD001, "GodotOHOS", "engine_thread: Main::setup failed");
			engine_running = false;
			return;
		}
		ohos_engine_diag("engine_thread: Main::setup done, calling Main::start");
		Main::start();
		ohos_engine_diag("engine_thread: Main::start done, entering iteration loop");

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
				ohos_engine_diag("engine_thread: Main::iteration requested quit");
				break;
			}
		}

		// 收尾：Main::cleanup 内部会删除主循环
		ohos_engine_diag("engine_thread: Main::cleanup begin");
		Main::cleanup();
		ohos_engine_diag("engine_thread: Main::cleanup done");

		// 进程内重启检查（第 10 轮修复）：有 pending 参数则再跑一轮
		restart = OS_OHOS::consume_pending_restart_args(restart_args);
		if (restart) {
			ohos_engine_diag("engine_thread: in-process restart with %d args", (int)restart_args.size());
			OH_LOG_Print(LOG_APP, LOG_INFO, 0xD001, "GodotOHOS", "engine_thread: restarting engine in-process");
		}
	}
	ohos_engine_diag("engine_thread: exit");
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
	// 实际回调注册依赖 OH_NativeXComponent，需先保存原生句柄再注册回调；
	// 若先 register_callbacks() 再 set_xcomponent()，注册时 native_xcomponent
	// 仍为 nullptr，OH_NativeXComponent_RegisterCallback 会失败（第 10 轮修复）。
	// 注：OH_NativeXComponent_RegisterCallback 在任意时刻调用均可，
	// 回调触发依赖 Surface 生命周期。
	ohos_xcomponent->set_xcomponent(native_xcomponent);
	ohos_xcomponent->register_callbacks();

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

	// 注册引擎日志桥接（print_line/print_error -> hilog），便于 hdc 诊断
	ohos_print_handler.printfunc = ohos_print_func;
	add_print_handler(&ohos_print_handler);
	ohos_error_handler.errfunc = ohos_error_func;
	add_error_handler(&ohos_error_handler);

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
