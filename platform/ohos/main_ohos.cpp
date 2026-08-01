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
#include "ohos_xcomponent.h"
#include "os_ohos.h"
#include "rendering_context_driver_vulkan_ohos.h"

#include "core/string/print_string.h"
#include "main/main.h"

#include <napi/native_api.h>
#include <string>
#include <thread>

#include <hilog/log.h>

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
 * 第 1 轮（骨架期）：导出 4 个 NAPI 函数（init/start/stop/dispose），
 * 完成 OS_OHOS 创建、引擎启动/迭代/停止的最小闭环。
 */

// 全局崩溃处理器（initialize 时注册）
static CrashHandlerOHOS crash_handler;

// NAPI 侧注入的沙盒路径
static std::string sandbox_files_dir;
static std::string sandbox_cache_dir;

// 引擎线程（Main::iteration 循环）
static std::thread engine_thread;
static bool engine_running = false;

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
	// 注意：鸿蒙无传统 main(argc, argv)，构造空参数列表（后续轮次从 NAPI 接收启动参数）
	const char *execpath = "";
	int argc = 0;
	char **argv = nullptr;

	if (Main::setup(execpath, argc, argv) != OK) {
		// 启动失败：直接结束
		engine_running = false;
		return;
	}
	Main::start();

	while (engine_running) {
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

// 初始化引擎（ArkTS: engine.initialize(filesDir, cacheDir) -> void）
static napi_value engine_initialize(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2];
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

// 释放引擎资源（ArkTS: engine.dispose() -> void）
static napi_value engine_dispose(napi_env env, napi_callback_info info) {
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
		{ "start", nullptr, engine_start, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "stop", nullptr, engine_stop, nullptr, nullptr, nullptr, napi_default, nullptr },
		{ "dispose", nullptr, engine_dispose, nullptr, nullptr, nullptr, napi_default, nullptr },
	};
	napi_define_properties(env, exports, 4, props);
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
