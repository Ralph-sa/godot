/**************************************************************************/
/*  file_access_harmonyos.cpp - HarmonyOS Sandbox-Aware File Access       */
/**************************************************************************/

#include "file_access_harmonyos.h"

#include "dir_access_harmonyos.h"

#include "core/string/print_string.h"

#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

Error FileAccessHarmonyOS::open_internal(const String &p_path, int p_mode_flags) {
	// Resolve path to absolute form for sandbox boundary check
	String resolved = fix_path(p_path);
	if (!resolved.is_absolute_path()) {
		// Use current working directory to resolve relative paths
		char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd)) != nullptr) {
			String cwd_str;
			if (cwd_str.append_utf8(cwd) == OK) {
				resolved = cwd_str.path_join(resolved);
				resolved = resolved.simplify_path();
			}
		}
	}

	// Sandbox boundary check for write operations:
	// - READ: allow reading from outside sandbox (POSIX will enforce permissions)
	// - WRITE / READ_WRITE / WRITE_READ: must be within sandbox
	if (p_mode_flags & WRITE) {
		if (!DirAccessHarmonyOS::is_sandbox_path(resolved)) {
#ifdef HARMONYOS_ENABLED
			print_verbose(vformat("FileAccessHarmonyOS: write denied for sandbox-external path: %s", resolved));
#endif
			return ERR_FILE_CANT_OPEN;
		}
	}

	return FileAccessUnix::open_internal(p_path, p_mode_flags);
}

bool FileAccessHarmonyOS::file_exists(const String &p_path) {
	// Delegate to FileAccessUnix — POSIX stat determines existence.
	// Sandbox enforcement is at the kernel level in OHOS.
	return FileAccessUnix::file_exists(p_path);
}
