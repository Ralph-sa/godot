/**************************************************************************/
/*  os_harmonyos.h - HarmonyOS OS Layer                                   */
/**************************************************************************/

#pragma once

#include "core/os/main_loop.h"
#include "drivers/unix/os_unix.h"

class JoypadHarmonyOS;

// HarmonyOS sandbox path components.
// These match the module name in build-profile.json5 and module.json5.
// If the module name changes, update these defines accordingly.
#ifndef OHOS_MODULE_NAME
#define OHOS_MODULE_NAME "entry"
#endif
#define OHOS_DATA_BASE "/data/storage/el2/base/haps"

// OS_HarmonyOS inherits from OS_Unix because:
// 1. HarmonyOS uses musl libc, providing full POSIX API support (unistd.h, dlfcn.h, pthread, etc.)
// 2. OS_Unix provides POSIX-based implementations of get_data_dir, get_cache_dir,
//    execute, get_executable_path, get_entropy, and other OS methods
// 3. Unlike Windows (OS_HarmonyOS does NOT inherit OS directly), the Unix/POSIX
//    abstraction matches the HarmonyOS native layer's API surface
// This choice is independently evaluated based on HarmonyOS's actual system APIs,
// NOT because Android does the same.
class OS_HarmonyOS : public OS_Unix {
private:
	mutable String data_dir_cache;
	mutable String cache_dir_cache;

	MainLoop *main_loop = nullptr;
	JoypadHarmonyOS *joypad_harmonyos = nullptr;

	virtual void delete_main_loop() override;

public:
	virtual String get_name() const override;
	virtual String get_user_data_dir(const String &p_user_dir) const override;
	virtual String get_cache_path() const override;

	virtual String get_config_path() const override;
	virtual String get_data_path() const override;

	virtual void initialize_core() override;
	virtual void initialize() override;
	virtual void initialize_joypads() override;
	virtual void finalize() override;
	virtual void finalize_core() override;

	virtual void set_main_loop(MainLoop *p_main_loop) override;
	virtual MainLoop *get_main_loop() const override;

	virtual Error open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data = nullptr) override;

	virtual bool is_userfs_persistent() const override;

	virtual String get_executable_path() const override;
	virtual Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override;
	virtual Error kill(const ProcessID &p_pid) override;
	virtual int get_process_id() const override;

	virtual bool _check_internal_feature_support(const String &p_feature) override;

	virtual Error get_entropy(uint8_t *r_buffer, int p_bytes) override;

	void run();
	bool main_loop_iterate();

	OS_HarmonyOS();
	~OS_HarmonyOS();
};
