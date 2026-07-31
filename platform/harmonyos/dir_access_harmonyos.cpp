/**************************************************************************/
/*  dir_access_harmonyos.cpp - HarmonyOS Sandbox-Aware Directory Access   */
/**************************************************************************/

#include "dir_access_harmonyos.h"

#include "core/string/print_string.h"
#include "os_harmonyos.h"

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HARMONYOS_ENABLED
#include "harmonyos_log.h"
#endif

String DirAccessHarmonyOS::sandbox_root;

/// If p_path is outside the OHOS sandbox, extract the last path component
/// (project / folder name) and redirect it under the sandbox user data dir.
/// Returns the remapped path, or the original path if already in sandbox.
static String _remap_to_sandbox(const String &p_path) {
	String fixed = p_path.simplify_path();

	// Already in sandbox — no change needed
	if (DirAccessHarmonyOS::is_sandbox_path(fixed)) {
		return fixed;
	}

	// Extract the last meaningful component as the project name
	String project_name = fixed.get_file();
	if (project_name.is_empty() || project_name == "." || project_name == ".." || project_name == "/") {
		// Fallback: use a static counter.  We cannot rely on OS::get_singleton()
		// because this may be called before engine initialization.
		static uint32_t fallback_id = 0;
		project_name = "project_" + String::num_uint64(++fallback_id);
	}

	String sandbox_dir = DirAccessHarmonyOS::get_sandbox_root();
	String remapped = sandbox_dir.path_join(project_name);

#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "DirAccessHarmonyOS: remapped %{public}s → %{public}s",
			fixed.utf8().get_data(), remapped.utf8().get_data());
#endif

	return remapped;
}

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

	// Resolve relative paths against current dir
	if (!fixed.is_absolute_path()) {
		fixed = current_dir.path_join(fixed).simplify_path();
	}

	// Delegate directly to POSIX — the OHOS kernel enforces sandbox boundaries
	// at the syscall level, so we don't need a userspace check here.  This
	// allows read-only navigation to paths the kernel permits (e.g. /system/).
	return DirAccessUnix::change_dir(p_dir);
}

Error DirAccessHarmonyOS::make_dir(String p_dir) {
	GLOBAL_LOCK_FUNCTION

	String resolved = p_dir;
	if (resolved.is_relative_path()) {
		resolved = current_dir.path_join(resolved);
	}
	resolved = fix_path(resolved);

	// Redirect external paths into the sandbox so the project manager can
	// create projects from any user-selected location.
	resolved = _remap_to_sandbox(resolved);

	return DirAccessUnix::make_dir(resolved);
}

Error DirAccessHarmonyOS::make_dir_recursive(const String &p_dir) {
	GLOBAL_LOCK_FUNCTION

	String resolved = p_dir;
	if (resolved.is_relative_path()) {
		resolved = current_dir.path_join(resolved);
	}
	resolved = fix_path(resolved);

	// Redirect external paths into the sandbox (same logic as make_dir).
	resolved = _remap_to_sandbox(resolved);

	return DirAccessUnix::make_dir_recursive(resolved);
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
