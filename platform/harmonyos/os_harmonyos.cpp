/**************************************************************************/
/*  os_harmonyos.cpp - HarmonyOS OS Layer Implementation                  */
/**************************************************************************/

#include "os_harmonyos.h"
#include "dir_access_harmonyos.h"
#include "file_access_harmonyos.h"
#include "joypad_harmonyos.h"

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "main/main.h"

#include <dlfcn.h>
#include <unistd.h>

#ifdef HARMONYOS_ENABLED
#include <hilog/log.h>
#endif

String OS_HarmonyOS::get_name() const {
	return "HarmonyOS";
}

String OS_HarmonyOS::get_user_data_dir(const String &p_user_dir) const {
	if (!data_dir_cache.is_empty()) {
		return data_dir_cache;
	}

	if (!p_user_dir.is_empty()) {
		data_dir_cache = p_user_dir;
	} else {
		// OHOS sandbox path for user data — uses module name from os_harmonyos.h
		data_dir_cache = String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME + "/files";
	}
	return data_dir_cache;
}

String OS_HarmonyOS::get_cache_path() const {
	if (!cache_dir_cache.is_empty()) {
		return cache_dir_cache;
	}

	cache_dir_cache = String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME + "/cache";
	return cache_dir_cache;
}

String OS_HarmonyOS::get_config_path() const {
	return get_user_data_dir("").path_join("config");
}

String OS_HarmonyOS::get_data_path() const {
	return get_user_data_dir("").path_join("data");
}

void OS_HarmonyOS::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

MainLoop *OS_HarmonyOS::get_main_loop() const {
	return main_loop;
}

void OS_HarmonyOS::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
		main_loop = nullptr;
	}
}

void OS_HarmonyOS::initialize_core() {
	// Call Unix base class to set up POSIX threads and clock
	OS_Unix::initialize_core();

	// Override with HarmonyOS sandbox-aware implementations
	FileAccess::make_default<FileAccessHarmonyOS>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessHarmonyOS>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessHarmonyOS>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessHarmonyOS>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessHarmonyOS>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessHarmonyOS>(DirAccess::ACCESS_FILESYSTEM);
}

void OS_HarmonyOS::initialize() {
	// Core subsystem setup: FileAccess/DirAccess defaults must be registered
	// before any file I/O occurs (e.g. ProjectSettings::_load_resource_pack
	// during Main::setup). initialize_core() sets up HarmonyOS sandbox-aware
	// FileAccess and DirAccess implementations.
	initialize_core();

#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "OS_HarmonyOS::initialize()");
#endif
}

void OS_HarmonyOS::initialize_joypads() {
	joypad_harmonyos = memnew(JoypadHarmonyOS());
	if (joypad_harmonyos->initialize() != OK) {
		ERR_PRINT("Could not initialize HarmonyOS joypad input driver.");
		memdelete(joypad_harmonyos);
		joypad_harmonyos = nullptr;
	}
}

void OS_HarmonyOS::finalize() {
	// Cleanup before engine shutdown
	if (joypad_harmonyos) {
		memdelete(joypad_harmonyos);
		joypad_harmonyos = nullptr;
	}
#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "OS_HarmonyOS::finalize()");
#endif
}

void OS_HarmonyOS::finalize_core() {
	// Core cleanup
}

Error OS_HarmonyOS::get_entropy(uint8_t *r_buffer, int p_bytes) {
	// Use /dev/urandom for cryptographically secure random bytes
	FILE *fp = fopen("/dev/urandom", "rb");
	if (!fp) {
		return ERR_CANT_OPEN;
	}
	size_t read = fread(r_buffer, 1, p_bytes, fp);
	fclose(fp);
	return (read == (size_t)p_bytes) ? OK : FAILED;
}

Error OS_HarmonyOS::open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data) {
	String path = p_path;

	if (!FileAccess::exists(path)) {
		path = get_user_data_dir("").path_join(p_path);
		if (!FileAccess::exists(path)) {
			return ERR_FILE_NOT_FOUND;
		}
	}

	p_library_handle = dlopen(path.utf8().get_data(), RTLD_NOW);
	if (!p_library_handle) {
		return ERR_CANT_OPEN;
	}

	return OK;
}

bool OS_HarmonyOS::is_userfs_persistent() const {
	return true;
}

String OS_HarmonyOS::get_executable_path() const {
	return String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME;
}

Error OS_HarmonyOS::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	// Process execution restricted in OHOS sandbox
	return ERR_UNAVAILABLE;
}

Error OS_HarmonyOS::kill(const ProcessID &p_pid) {
	return ERR_UNAVAILABLE;
}

int OS_HarmonyOS::get_process_id() const {
	return getpid();
}

bool OS_HarmonyOS::_check_internal_feature_support(const String &p_feature) {
	if (p_feature == "pc") {
		return true;
	}
	if (p_feature == "system_fonts") {
		// HarmonyOS 使用 musl libc，具备链接 fontconfig 的可能性。
		// OHOS NDK 默认不包含 fontconfig 的 dev 包，但运行时系统镜像中
		// 通常已预置 fontconfig 库（/system/lib64/libfontconfig.so）。
		// 当前暂返回 true 声明能力；如运行时检测到 fontconfig 不可用，
		// 需在 OS_Unix::get_system_fonts() 中降级为空列表。
		return true;
	}
	return false;
}

void OS_HarmonyOS::run() {
	if (!main_loop) {
		return;
	}

	main_loop->initialize();

	while (true) {
		if (joypad_harmonyos) {
			joypad_harmonyos->process_events();
		}
		if (Main::iteration()) {
			break;
		}
	}
}

bool OS_HarmonyOS::main_loop_iterate() {
	if (!main_loop) {
		return true;
	}
	return Main::iteration();
}

OS_HarmonyOS::OS_HarmonyOS() {
#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "Godot HarmonyOS OS layer initialized");
#endif
}

OS_HarmonyOS::~OS_HarmonyOS() {
#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "Godot HarmonyOS OS layer destroyed");
#endif
}
