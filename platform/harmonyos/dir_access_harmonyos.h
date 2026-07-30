/**************************************************************************/
/*  dir_access_harmonyos.h - HarmonyOS Sandbox-Aware Directory Access     */
/**************************************************************************/

#pragma once

#include "drivers/unix/dir_access_unix.h"

// DirAccessHarmonyOS: inherits from DirAccessUnix (musl libc provides POSIX APIs)
// Adds sandbox boundary validation for OHOS path sandbox (/data/storage/)
class DirAccessHarmonyOS : public DirAccessUnix {
	GDSOFTCLASS(DirAccessHarmonyOS, DirAccessUnix);

	static String sandbox_root;

public:
	// Check if a path lies within the OHOS sandbox (/data/storage/...)
	static bool is_sandbox_path(const String &p_path);
	static String get_sandbox_root();

	virtual Error change_dir(String p_dir) override;
	virtual Error make_dir(String p_dir) override;
	virtual Error make_dir_recursive(const String &p_dir) override;
	virtual bool file_exists(String p_file) override;
	virtual bool dir_exists(String p_dir) override;

	DirAccessHarmonyOS();
};
