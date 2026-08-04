/**************************************************************************/
/*  dir_access_harmonyos.h - HarmonyOS Sandbox-Aware Directory Access     */
/**************************************************************************/

#pragma once

#include "drivers/unix/dir_access_unix.h"

// DirAccessHarmonyOS: inherits from DirAccessUnix (musl libc provides POSIX APIs)
// Adds sandbox boundary validation for OHOS path sandbox (/data/storage/)
class DirAccessHarmonyOS : public DirAccessUnix {
	GDSOFTCLASS(DirAccessHarmonyOS, DirAccessUnix);

	static String files_root;
	static String cache_root;
	static String temp_root;

	String resolve_path(const String &p_path) const;
	bool is_write_path_allowed(const String &p_path) const;
	bool is_sandbox_root_ancestor(const String &p_path) const;
	static bool get_sandbox_relative_path(const String &p_path, String &r_root, String &r_relative);

public:
	static void configure_sandbox_roots(const String &p_files_root, const String &p_cache_root, const String &p_temp_root);

	// Check if a path lies within files/cache/temp for the current application.
	static bool is_sandbox_path(const String &p_path);
	static String get_sandbox_root();

	virtual Error change_dir(String p_dir) override;
	virtual Error make_dir(String p_dir) override;
	virtual Error make_dir_recursive(const String &p_dir) override;
	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;
	virtual bool is_writable(String p_dir) override;
	virtual Error rename(String p_path, String p_new_path) override;
	virtual Error remove(String p_path) override;
	virtual Error create_link(String p_source, String p_target) override;

	DirAccessHarmonyOS();
};
