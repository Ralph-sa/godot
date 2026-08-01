/**************************************************************************/
/*  os_ohos.cpp                                                           */
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

#include "os_ohos.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/main_loop.h"
#include "core/string/ustring.h"
#include "display_server_ohos.h"
#include "main/main.h"
#include "servers/audio/audio_driver.h"

#include <sys/utsname.h>
#include <unistd.h>

// 单例指针（main_ohos.cpp 中创建）
static OS_OHOS *os_ohos_singleton = nullptr;

OS_OHOS *OS_OHOS::get_singleton() {
	return os_ohos_singleton;
}

OS_OHOS::OS_OHOS() {
	// 记录单例，供 NAPI 桥与引擎全局访问
	os_ohos_singleton = this;

	// 默认沙盒路径（NAPI 注入前先用当前目录兜底）
	sandbox_files_dir = ".";
	sandbox_cache_dir = ".";
}

OS_OHOS::~OS_OHOS() {
	os_ohos_singleton = nullptr;
}

void OS_OHOS::set_sandbox_paths(const String &p_files_dir, const String &p_cache_dir) {
	// 由 main_ohos.cpp 在 NAPI 初始化后注入鸿蒙 App 沙盒真实路径
	sandbox_files_dir = p_files_dir;
	sandbox_cache_dir = p_cache_dir;
}

void OS_OHOS::initialize_core() {
	OS_Unix::initialize_core();

	// 注册 OHOS 显示驱动（display_driver = "ohos"），供 main.cpp 的 DisplayServer 创建使用
	DisplayServerOHOS::register_ohos_driver();
}

void OS_OHOS::initialize() {
	// 初始化核心（含 DisplayServer 驱动注册）
	initialize_core();

	// 注册 OHAudio 音频驱动（AudioDriverManager 管理，main.cpp 按 audio/driver/driver 选择）
	AudioDriverManager::add_driver(&audio_driver_ohos);
}

void OS_OHOS::finalize() {
	// 清理核心子系统（OS_Unix 未实现 finalize，直接调用 finalize_core）
	finalize_core();
}

void OS_OHOS::initialize_joypads() {
	// 手柄支持（OH Gamepad）留待第 8 轮（完善期）实现
}

void OS_OHOS::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

void OS_OHOS::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

MainLoop *OS_OHOS::get_main_loop() const {
	return main_loop;
}

String OS_OHOS::get_name() const {
	return "HarmonyOS";
}

String OS_OHOS::get_distribution_name() const {
	// 发行名称：HarmonyOS NEXT（7.0.0 起）
	return "HarmonyOS";
}

String OS_OHOS::get_version() const {
	// 版本号：从 uname 提取内核版本；应用层版本后续从系统参数获取
	struct utsname info;
	if (uname(&info) == 0) {
		return String::utf8(info.release);
	}
	return "0.0";
}

String OS_OHOS::get_version_alias() const {
	return "NEXT";
}

String OS_OHOS::get_config_path() const {
	// 鸿蒙沙盒内配置目录：<filesDir>/.config
	return sandbox_files_dir.path_join(".config");
}

String OS_OHOS::get_data_path() const {
	// 鸿蒙沙盒内数据目录：<filesDir>/.local/share
	return sandbox_files_dir.path_join(".local").path_join("share");
}

String OS_OHOS::get_cache_path() const {
	// 鸿蒙沙盒内缓存目录：<cacheDir>
	return sandbox_cache_dir;
}

String OS_OHOS::get_temp_path() const {
	// 临时目录：复用缓存目录
	return sandbox_cache_dir;
}

String OS_OHOS::get_user_data_dir(const String &p_user_dir) const {
	// 用户数据目录：<data_path>/<p_user_dir>
	return get_data_path().path_join(p_user_dir);
}

String OS_OHOS::get_resource_dir() const {
	// 资源目录：鸿蒙应用资源在沙盒内，先回退到可执行目录
	return get_executable_path().get_base_dir();
}

String OS_OHOS::get_executable_path() const {
	// 从 /proc/self/exe 读取可执行文件路径（Linux 系标准做法）
	char buf[1024];
	ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (len != -1) {
		buf[len] = 0;
		return String::utf8(buf);
	}
	return String();
}

String OS_OHOS::get_locale() const {
	// 优先返回 NAPI 注入的系统语言（@ohos.i18n），如 "zh-CN"/"en-US"；
	// 未注入时回退环境变量 LANG
	if (!system_locale.is_empty()) {
		return system_locale;
	}
	const char *lang = getenv("LANG");
	if (lang) {
		return String::utf8(lang);
	}
	return "en";
}

String OS_OHOS::get_processor_name() const {
	// 从 /proc/cpuinfo 提取 CPU 型号
	Ref<FileAccess> f = FileAccess::open("/proc/cpuinfo", FileAccess::READ);
	if (f.is_valid()) {
		String line = f->get_line();
		while (line.length() > 0) {
			if (line.begins_with("model name")) {
				return line.get_slice(":", 1).strip_edges();
			}
			line = f->get_line();
		}
	}
	return "ARM";
}

String OS_OHOS::get_model_name() const {
	// 优先返回 NAPI 注入的设备型号（@ohos.deviceInfo productModel）
	if (!model_name.is_empty()) {
		return model_name;
	}
	// 兜底：从 /proc/device-tree/model 读取（Linux 系设备树）
	Ref<FileAccess> f = FileAccess::open("/proc/device-tree/model", FileAccess::READ);
	if (f.is_valid()) {
		String s = f->get_as_text();
		s = s.strip_edges();
		if (!s.is_empty()) {
			return s;
		}
	}
	return "HarmonyOS Device";
}

String OS_OHOS::get_unique_id() const {
	// 设备唯一 ID：骨架期回退 MAC 地址方案（OS_Unix 默认）；后续可走设备标识
	return OS_Unix::get_unique_id();
}

Error OS_OHOS::shell_open(const String &p_uri) {
	// 骨架期：通过 NAPI 桥（main_ohos.cpp 提供）拉起系统打开能力
	// 第 1 轮返回 ERR_UNAVAILABLE，后续轮次接入 @ohos.childProcess / startAbility
	return ERR_UNAVAILABLE;
}

String OS_OHOS::get_system_ca_certificates() {
	// 鸿蒙系统 CA 证书目录（与 Android 同构，/system/etc/security/cacerts）。
	// 沙盒内应用只读系统分区，此处仅返回路径供 TLS 加载。
	return "/system/etc/security/cacerts";
}

String OS_OHOS::get_system_dir(SystemDir p_dir, bool p_shared_storage) const {
	// 系统公共目录（桌面/文档/下载等）：鸿蒙 App 沙盒内不可直接访问系统目录。
	// 与 macOS NSSearchPathForDirectoriesInDomains 对应，但沙盒模型下
	// 全部映射到 filesDir；访问真实公共目录需 FilePicker 持久化授权
	//（第 8 轮完善期接入 @ohos.file.fileAccess）。
	return sandbox_files_dir;
}

Error OS_OHOS::get_entropy(uint8_t *r_buffer, int p_bytes) {
	// 加密安全随机数：musl 提供 arc4random_buf
	arc4random_buf(r_buffer, p_bytes);
	return OK;
}

bool OS_OHOS::_check_internal_feature_support(const String &p_feature) {
	// 声明平台特性：mobile + ohos（导出器与编辑器特性检测用）
	if (p_feature == "ohos" || p_feature == "mobile" || p_feature == "arm64") {
		return true;
	}
	// OS_Unix 未实现该纯虚函数，直接返回 false
	return false;
}

// ---- 引擎主循环驱动（由 main_ohos.cpp 在 VSync/帧回调中调用）----

void OS_OHOS::main_loop_begin() {
	// 主循环开始：引擎在 Main::start 之后由 main_ohos.cpp 驱动迭代
}

bool OS_OHOS::main_loop_iterate() {
	if (!main_loop) {
		return false;
	}
	// 单次引擎迭代：处理输入事件、更新场景、渲染提交
	return Main::iteration();
}
