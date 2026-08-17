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
#include "ohos_bridge.h"
#include "servers/audio/audio_driver.h"

#include "core/os/mutex.h"

#include <sys/utsname.h>
#include <unistd.h>

// 单例指针（main_ohos.cpp 中创建）
static OS_OHOS *os_ohos_singleton = nullptr;

OS_OHOS *OS_OHOS::get_singleton() {
	return os_ohos_singleton;
}

// ---- 进程内重启（第 10 轮修复：create_instance 不支持 fork/exec） ----
// 鸿蒙应用进程由 appspawn 创建，fork()+execvp(/proc/self/exe) 出的子进程
// 没有 Ability/ACE 运行时上下文（无窗口/无 NAPI 宿主），编辑器实例必然
// 启动失败或黑屏卡死 —— 这正是「项目管理器创建项目后编辑器打不开」的根因。
// 因此 create_instance 改为进程内重启：参数记录到 pending 列表，
// 引擎线程（main_ohos.cpp）在 Main::cleanup 完成后以新参数重新
// Main::setup + Main::start，实现与桌面端「新进程」等价的实例语义。
static List<String> ohos_pending_restart_args;
static Mutex ohos_pending_restart_mutex;

Error OS_OHOS::create_instance(const List<String> &p_arguments, ProcessID *r_child_id) {
	MutexLock lock(ohos_pending_restart_mutex);
	ohos_pending_restart_args = p_arguments;

	// 首次调用（ProjectManager 打开项目）：置 restart_on_exit，PM 随后
	// get_tree()->quit()，Main::cleanup 走重启分支再次调用本函数；
	// 二次调用时 restart_on_exit 已置位，直接返回即可（避免覆盖标志）。
	if (!is_restart_on_exit_set()) {
		set_restart_on_exit(true, p_arguments);
	}

	if (r_child_id) {
		// 虚拟实例 ID（非真实 pid；OHOS 单进程内无独立子进程）
		*r_child_id = 1;
	}
	return OK;
}

Error OS_OHOS::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	// 指向自身可执行文件（/proc/self/exe）的进程创建请求转进程内重启：
	// 参照 Android 的 create_process 守卫（ANDROID_EXEC_PATH 分支）。
	// GDScript OS.create_instance() 默认走 create_process(get_executable_path())。
	if (p_path == get_executable_path()) {
		return create_instance(p_arguments, r_child_id);
	}
	// 其他路径（外部二进制）：沙盒内 fork/exec 会因缺少 appspawn 上下文失败，
	// OS_Unix 实现会返回错误/子进程自杀，不会卡住主流程。
	return OS_Unix::create_process(p_path, p_arguments, r_child_id, p_open_console);
}

// 引擎线程取走待重启参数（main_ohos.cpp 在 Main::cleanup 后调用）。
// 有参数返回 true 并清空列表；无参数返回 false（正常退出）。
bool OS_OHOS::consume_pending_restart_args(List<String> &r_args) {
	MutexLock lock(ohos_pending_restart_mutex);
	if (ohos_pending_restart_args.is_empty()) {
		return false;
	}
	r_args = ohos_pending_restart_args;
	ohos_pending_restart_args.clear();
	return true;
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

	// 注册 OHAudio 音频驱动（AudioDriverManager 管理，main.cpp 按 audio/driver/driver 选择）。
	// 进程内重启会再次调用 initialize()（Main::setup -> OS::initialize），
	// AudioDriverManager 驱动表为静态数组且不随 finalize_core 清空，
	// 重复注册会耗尽 MAX_DRIVERS，因此只注册一次（第 10 轮修复）。
	static bool audio_driver_registered = false;
	if (!audio_driver_registered) {
		AudioDriverManager::add_driver(&audio_driver_ohos);
		audio_driver_registered = true;
	}
}

void OS_OHOS::finalize() {
	// 清理核心子系统（OS_Unix 未实现 finalize，直接调用 finalize_core）
	finalize_core();
}

void OS_OHOS::initialize_joypads() {
	// 手柄支持（第 8 轮）：经 NAPI 桥请求 ArkTS 枚举输入设备
	//（@ohos.multimodalInput.inputDevice.getDeviceList），过滤 joystick 设备后
	// 由 engine_gamepad_devices 回传，Input 单例 joy_connection_changed 上报。
	// 对应 macOS IOHIDManager 的 HID 设备枚举（GodotJoypad 连接回调）。
	ohos_enumerate_gamepads();
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
	// 返回空（第 11 轮真机修复）：ProjectSettings::_setup 检测到非空
	// resource_dir 会直接加载 res://project.godot 并返回（可执行目录里
	// 没有项目文件 → 加载失败回退项目管理器）。返回空让 _setup 走
	// filesystem 分支（change_dir(p_path) 加载项目目录）。
	// 对比 Android：get_resource_dir 为空，同语义。
	return String();
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
	// T-OS-1：经 NAPI 桥请求 ArkTS 侧 startAbility 打开 URI
	//（编辑器帮助文档/AssetLib 链接等场景）。桥未注册时返回 ERR_UNAVAILABLE。
	if (p_uri.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	// 桥注册状态由 main_ohos.cpp 的注册调用建立；此处直接发起请求，
	// 若 ArkTS 侧未注册（引擎独立运行），桥内静默忽略。
	ohos_shell_open(p_uri);
	return OK;
}

String OS_OHOS::get_system_ca_certificates() {
	// 鸿蒙系统 CA 证书目录（与 Android 同构，/system/etc/security/cacerts）。
	// 沙盒内应用只读系统分区，此处仅返回路径供 TLS 加载。
	return "/system/etc/security/cacerts";
}

String OS_OHOS::get_system_dir(SystemDir p_dir, bool p_shared_storage) const {
	// 系统公共目录（桌面/文档/下载等）：鸿蒙 App 沙盒内不可直接访问系统目录。
	// 与 macOS NSSearchPathForDirectoriesInDomains 对应，但沙盒模型下
	// 全部映射到 filesDir；访问真实公共目录需 FilePicker 选择 + 持久化授权
	//（第 8/9 轮：DocumentViewPicker + @ohos.fileshare.persistPermission）。
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
