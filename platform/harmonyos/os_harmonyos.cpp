/**************************************************************************/
/*  os_harmonyos.cpp - HarmonyOS OS Layer Implementation                  */
/**************************************************************************/

#include "os_harmonyos.h"
#include "audio_driver_ohos.h"
#include "dir_access_harmonyos.h"
#include "file_access_harmonyos.h"
#include "harmonyos_engine_logger.h"
#include "joypad_harmonyos.h"

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "main/main.h"
#include "servers/audio/audio_driver.h"

#include <dlfcn.h>
#include <unistd.h>

#ifdef HARMONYOS_ENABLED
#include "harmonyos_log.h"
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

	// Register the OHAudio audio driver so AudioServer can pick it up.
	// Mirrors OS_Windows::initialize() registering AudioDriverWASAPI.
	AudioDriverManager::add_driver(&driver_ohos);

	// Mirror Godot's engine print/printerr/print_error output into hilog and
	// a sandbox file so runtime errors (Vulkan swap chain, shaders, ...) are
	// observable. StdLogger alone writes to stdout, which is lost on OHOS.
	add_logger(memnew(HarmonyOSEngineLogger));

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

void OS_HarmonyOS::process_joypad_events() {
	if (joypad_harmonyos) {
		joypad_harmonyos->process_events();
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
	// 注意：不要在这里声明 "system_fonts"。该能力要求 override
	// get_system_fonts() / get_system_font_path()，而本平台并未 override —— 基类
	// OS_Unix 的实现依赖 fontconfig，OHOS NDK 不提供其 dev 包。声明 true 会让
	// 引擎去查询系统字体并拿到空结果，不如诚实返回 false 让它用内置字体。
	// 后续若经 ArkTS 侧字体管理接口补上真实实现，再在此声明。
	return false;
}

// 这里曾有 run() 与 main_loop_iterate() 两个方法，各自实现了一份帧循环，但
// 都不是引擎虚函数、也无任何调用点 —— 本平台的实际入口是 harmonyos_main.cpp
// 的 harmonyos_godot_start()，帧循环和手柄事件泵送都在那里。留着两份互相竞争
// 的循环实现只会误导后来者，故删除。

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
