/**************************************************************************/
/*  dir_access_harmonyos.cpp - HarmonyOS Sandbox-Aware Directory Access   */
/**************************************************************************/

#include "dir_access_harmonyos.h"

#include "core/string/print_string.h"
#include "os_harmonyos.h"

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

String DirAccessHarmonyOS::sandbox_root;

bool DirAccessHarmonyOS::is_sandbox_path(const String &p_path) {
	// OHOS sandbox base: /data/storage/
	// Allowed paths must start with /data/storage/
	if (p_path.begins_with("/data/storage/")) {
		return true;
	}
	// Also allow the exact /data/storage (without trailing slash) for boundary cases
	if (p_path == "/data/storage") {
		return true;
	}
	return false;
}

String DirAccessHarmonyOS::get_sandbox_root() {
	if (!sandbox_root.is_empty()) {
		return sandbox_root;
	}
	// Default: use the module base directory from os_harmonyos.h defines
	sandbox_root = String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME;
	return sandbox_root;
}

Error DirAccessHarmonyOS::change_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	String fixed = fix_path(p_dir);

	// Resolve relative paths against current sandbox root
	if (!fixed.is_absolute_path()) {
		fixed = current_dir.path_join(fixed).simplify_path();
	}

	// Sandbox boundary check: if the target is outside /data/storage/,
	// the OHOS sandbox may deny access. Delegate to POSIX chdir and
	// gracefully degrade on failure.
	bool in_sandbox = is_sandbox_path(fixed);

	// For paths within sandbox, use DirAccessUnix directly
	Error err = DirAccessUnix::change_dir(p_dir);
	if (err == OK) {
		return OK;
	}

	// If POSIX chdir failed and we're outside the sandbox,
	// report the appropriate error.
	if (!in_sandbox) {
#ifdef HARMONYOS_ENABLED
		print_verbose(vformat("DirAccessHarmonyOS: access denied to sandbox-external path: %s", fixed));
#endif
		return ERR_FILE_NOT_FOUND;
	}

	return err;
}

Error DirAccessHarmonyOS::make_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	String resolved = p_dir;
	if (resolved.is_relative_path()) {
		resolved = current_dir.path_join(resolved);
	}
	resolved = fix_path(resolved);

	if (!is_sandbox_path(resolved)) {
		ERR_PRINT(vformat("DirAccessHarmonyOS: mkdir denied for sandbox-external path: %s", resolved));
		return ERR_CANT_CREATE;
	}

	return DirAccessUnix::make_dir(p_dir);
}

Error DirAccessHarmonyOS::make_dir_recursive(const String &p_dir) {
	GLOBAL_LOCK_FUNCTION

	String resolved = p_dir;
	if (resolved.is_relative_path()) {
		resolved = current_dir.path_join(resolved);
	}
	resolved = fix_path(resolved);

	if (!is_sandbox_path(resolved)) {
		ERR_PRINT(vformat("DirAccessHarmonyOS: mkdir_recursive denied for sandbox-external path: %s", resolved));
		return ERR_CANT_CREATE;
	}

	return DirAccessUnix::make_dir_recursive(p_dir);
}

bool DirAccessHarmonyOS::file_exists(String p_file) {
	GLOBAL_LOCK_FUNCTION

	// Quick reject: if outside sandbox and OS-level access would fail
	// Still let POSIX stat be the ultimate authority — if OHOS allows it, we allow it.
	return DirAccessUnix::file_exists(p_file);
}

bool DirAccessHarmonyOS::dir_exists(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	return DirAccessUnix::dir_exists(p_dir);
}

DirAccessHarmonyOS::DirAccessHarmonyOS() {
	// Initialize sandbox_root on first construction
	(void)get_sandbox_root();
}
