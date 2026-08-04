/**************************************************************************/
/*  dir_access_harmonyos.cpp - HarmonyOS Sandbox-Aware Directory Access   */
/**************************************************************************/

#include "dir_access_harmonyos.h"

#include "core/string/print_string.h"
#include "os_harmonyos.h"

#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HARMONYOS_ENABLED
#include "harmonyos_log.h"
#endif

String DirAccessHarmonyOS::files_root;
String DirAccessHarmonyOS::cache_root;
String DirAccessHarmonyOS::temp_root;
static std::mutex sandbox_roots_mutex;
static bool sandbox_roots_configured = false;

static bool _path_is_within_root(const String &p_path, const String &p_root) {
	return !p_root.is_empty() && (p_path == p_root || p_path.begins_with(p_root + "/"));
}

static bool _path_is_strict_ancestor_of_root(const String &p_path, const String &p_root) {
	if (p_path.is_empty() || p_root.is_empty() || p_path == p_root) {
		return false;
	}
	if (p_path == "/") {
		return p_root.begins_with("/");
	}
	return p_root.begins_with(p_path + "/");
}

static bool _path_contains_symlink_or_error(const String &p_path) {
	String simplified = p_path.simplify_path();
	Vector<String> components = simplified.split("/", false);
	String current = simplified.begins_with("/") ? "/" : String();
	for (const String &component : components) {
		current = current.path_join(component);
		struct stat info = {};
		if (lstat(current.utf8().get_data(), &info) == 0) {
			if (S_ISLNK(info.st_mode)) {
#ifdef HARMONYOS_ENABLED
				OH_LOG_ERROR(LOG_APP, "DirAccessHarmonyOS: symlink component rejected: %{public}s",
						current.utf8().get_data());
#endif
				return true;
			}
			continue;
		}

		if (errno == ENOENT) {
			// The remaining suffix does not exist yet. All existing ancestors
			// have already been checked without following symbolic links.
			return false;
		}

#ifdef HARMONYOS_ENABLED
		OH_LOG_ERROR(LOG_APP, "DirAccessHarmonyOS: lstat failed for %{public}s errno=%{public}d",
				current.utf8().get_data(), errno);
#endif
		return true;
	}
	return false;
}

struct SandboxRelativePath {
	String root;
	String relative;
};

static bool _is_safe_relative_component(const String &p_component) {
	// NOTE: do not use `contains(String::chr(0))` — constructing a String that
	// holds a NUL character makes Godot's UTF-8 parser emit "Unexpected NUL"
	// errors. `find_char` compares code points directly without building one.
	return !p_component.is_empty() && p_component != "." && p_component != ".." &&
			!p_component.contains("/") && !p_component.contains("\\") &&
			p_component.find_char(0) == -1;
}

static bool _split_safe_relative_path(const String &p_relative, Vector<String> &r_components) {
	r_components.clear();
	if (p_relative.is_empty()) {
		return true;
	}
	r_components = p_relative.split("/", false);
	if (r_components.is_empty()) {
		return false;
	}
	for (const String &component : r_components) {
		if (!_is_safe_relative_component(component)) {
			return false;
		}
	}
	return true;
}

bool DirAccessHarmonyOS::get_sandbox_relative_path(const String &p_path, String &r_root, String &r_relative) {
	const String fixed = p_path.simplify_path();
	std::lock_guard<std::mutex> lock(sandbox_roots_mutex);
	const String *roots[] = { &files_root, &cache_root, &temp_root };
	for (const String *root : roots) {
		if (!_path_is_within_root(fixed, *root)) {
			continue;
		}
		r_root = *root;
		r_relative = fixed == *root ? String() : fixed.substr(root->length() + 1);
		Vector<String> components;
		return _split_safe_relative_path(r_relative, components);
	}
	return false;
}

static int _open_sandbox_root(const String &p_root) {
	const String simplified = p_root.simplify_path();
	if (!simplified.is_absolute_path()) {
		errno = EINVAL;
		return -1;
	}

	// HarmonyOS applications may open their own files/cache/temp directory, but
	// are not allowed to traverse protected ancestors such as /data by dirfd.
	// The roots are supplied by UIAbilityContext, validated before Main::setup,
	// and frozen for the process lifetime. Open the trusted root atomically with
	// O_NOFOLLOW, then keep all mutable operations relative to this descriptor.
	CharString root_utf8 = simplified.utf8();
	int root_fd = open(root_utf8.get_data(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
#ifdef O_PATH
	if (root_fd < 0 && errno == EACCES) {
		root_fd = open(root_utf8.get_data(), O_PATH | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	}
#endif
	if (root_fd < 0) {
		return -1;
	}

	struct stat root_info = {};
	if (fstat(root_fd, &root_info) != 0 || !S_ISDIR(root_info.st_mode)) {
		int operation_errno = errno != 0 ? errno : ENOTDIR;
		close(root_fd);
		errno = operation_errno;
		return -1;
	}
	return root_fd;
}

static bool _open_sandbox_parent(const SandboxRelativePath &p_path, int &r_parent_fd, String &r_leaf) {
	r_parent_fd = -1;
	r_leaf = String();
	Vector<String> components;
	if (!_split_safe_relative_path(p_path.relative, components) || components.is_empty()) {
		return false;
	}

	int current_fd = _open_sandbox_root(p_path.root);
	if (current_fd < 0) {
		return false;
	}
	for (int index = 0; index < components.size() - 1; index++) {
		CharString component_utf8 = components[index].utf8();
		int next_fd = openat(current_fd, component_utf8.get_data(),
				O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		if (next_fd < 0) {
			close(current_fd);
			return false;
		}
		close(current_fd);
		current_fd = next_fd;
	}

	r_parent_fd = current_fd;
	r_leaf = components[components.size() - 1];
	return true;
}

static void _log_dirfd_failure(const char *p_operation, const String &p_path) {
#ifdef HARMONYOS_ENABLED
	OH_LOG_ERROR(LOG_APP, "DirAccessHarmonyOS: %{public}s failed for %{public}s errno=%{public}d",
			p_operation, p_path.utf8().get_data(), errno);
#endif
}

void DirAccessHarmonyOS::configure_sandbox_roots(const String &p_files_root, const String &p_cache_root, const String &p_temp_root) {
	String new_files_root = p_files_root.simplify_path();
	String new_cache_root = p_cache_root.simplify_path();
	String new_temp_root = p_temp_root.simplify_path();
	std::lock_guard<std::mutex> lock(sandbox_roots_mutex);
	if (sandbox_roots_configured) {
		if (files_root != new_files_root || cache_root != new_cache_root || temp_root != new_temp_root) {
#ifdef HARMONYOS_ENABLED
			OH_LOG_ERROR(LOG_APP, "DirAccessHarmonyOS: refusing to replace immutable sandbox roots");
#endif
		}
		return;
	}
	files_root = new_files_root;
	cache_root = new_cache_root;
	temp_root = new_temp_root;
	sandbox_roots_configured = true;
}

bool DirAccessHarmonyOS::is_sandbox_path(const String &p_path) {
	const String fixed = p_path.simplify_path();
	std::lock_guard<std::mutex> lock(sandbox_roots_mutex);
	return _path_is_within_root(fixed, files_root) ||
			_path_is_within_root(fixed, cache_root) ||
			_path_is_within_root(fixed, temp_root);
}

String DirAccessHarmonyOS::get_sandbox_root() {
	std::lock_guard<std::mutex> lock(sandbox_roots_mutex);
	if (!files_root.is_empty()) {
		return files_root;
	}
	return String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME + "/files";
}

String DirAccessHarmonyOS::resolve_path(const String &p_path) const {
	String resolved = fix_path(p_path);
	if (!resolved.is_absolute_path()) {
		resolved = current_dir.path_join(resolved);
	}
	return resolved.simplify_path();
}

bool DirAccessHarmonyOS::is_write_path_allowed(const String &p_path) const {
	const String resolved = resolve_path(p_path);
	if (is_sandbox_path(resolved)) {
		return !_path_contains_symlink_or_error(resolved);
	}
#ifdef HARMONYOS_ENABLED
	OH_LOG_ERROR(LOG_APP, "DirAccessHarmonyOS: write denied outside sandbox roots: %{public}s",
			resolved.utf8().get_data());
#endif
	return false;
}

bool DirAccessHarmonyOS::is_sandbox_root_ancestor(const String &p_path) const {
	const String resolved = resolve_path(p_path);
	std::lock_guard<std::mutex> lock(sandbox_roots_mutex);
	return _path_is_strict_ancestor_of_root(resolved, files_root) ||
			_path_is_strict_ancestor_of_root(resolved, cache_root) ||
			_path_is_strict_ancestor_of_root(resolved, temp_root);
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
	String resolved = resolve_path(p_dir);
	// DirAccess::make_dir_recursive() walks from the filesystem root and calls
	// this virtual method for every component. HarmonyOS sandbox ancestors such
	// as /data already exist but are intentionally not writable by the app. Match
	// the desktop contract by reporting those existing ancestors as such, while
	// never attempting to create anything outside files/cache/temp.
	if (!is_sandbox_path(resolved) && is_sandbox_root_ancestor(resolved) && DirAccessUnix::dir_exists(resolved)) {
		return ERR_ALREADY_EXISTS;
	}
	SandboxRelativePath sandbox_path;
	ERR_FAIL_COND_V(!get_sandbox_relative_path(
			resolved, sandbox_path.root, sandbox_path.relative), ERR_UNAUTHORIZED);
	if (sandbox_path.relative.is_empty()) {
		return ERR_ALREADY_EXISTS;
	}

	int parent_fd = -1;
	String leaf;
	if (!_open_sandbox_parent(sandbox_path, parent_fd, leaf)) {
		_log_dirfd_failure("open parent for mkdirat", resolved);
		return ERR_CANT_CREATE;
	}
	CharString leaf_utf8 = leaf.utf8();
	int result = mkdirat(parent_fd, leaf_utf8.get_data(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	int operation_errno = errno;
	close(parent_fd);
	if (result == 0) {
		return OK;
	}
	if (operation_errno == EEXIST) {
		return ERR_ALREADY_EXISTS;
	}
	errno = operation_errno;
	_log_dirfd_failure("mkdirat", resolved);
	return ERR_CANT_CREATE;
}

Error DirAccessHarmonyOS::make_dir_recursive(const String &p_dir) {
	GLOBAL_LOCK_FUNCTION
	String resolved = resolve_path(p_dir);
	SandboxRelativePath sandbox_path;
	ERR_FAIL_COND_V(!get_sandbox_relative_path(
			resolved, sandbox_path.root, sandbox_path.relative), ERR_UNAUTHORIZED);
	if (sandbox_path.relative.is_empty()) {
		return OK;
	}

	Vector<String> components;
	ERR_FAIL_COND_V(!_split_safe_relative_path(sandbox_path.relative, components), ERR_UNAUTHORIZED);
	int current_fd = _open_sandbox_root(sandbox_path.root);
	if (current_fd < 0) {
		_log_dirfd_failure("open sandbox root for recursive mkdir", resolved);
		return ERR_CANT_CREATE;
	}

	for (const String &component : components) {
		CharString component_utf8 = component.utf8();
		if (mkdirat(current_fd, component_utf8.get_data(),
				S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) {
			int operation_errno = errno;
			close(current_fd);
			errno = operation_errno;
			_log_dirfd_failure("mkdirat recursive", resolved);
			return ERR_CANT_CREATE;
		}

		struct stat info = {};
		int stat_result = fstatat(current_fd, component_utf8.get_data(), &info, AT_SYMLINK_NOFOLLOW);
		if (stat_result != 0 || !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) {
			int operation_errno = stat_result != 0 ? errno : ENOTDIR;
			close(current_fd);
			errno = operation_errno;
			_log_dirfd_failure("fstatat recursive directory", resolved);
			return ERR_UNAUTHORIZED;
		}

		int next_fd = openat(current_fd, component_utf8.get_data(),
				O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		if (next_fd < 0) {
			int operation_errno = errno;
			close(current_fd);
			errno = operation_errno;
			_log_dirfd_failure("openat recursive directory", resolved);
			return ERR_UNAUTHORIZED;
		}
		close(current_fd);
		current_fd = next_fd;
	}
	close(current_fd);
	return OK;
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

bool DirAccessHarmonyOS::is_writable(String p_dir) {
	GLOBAL_LOCK_FUNCTION
	String resolved = resolve_path(p_dir);
	return is_write_path_allowed(resolved) && DirAccessUnix::is_writable(resolved);
}

Error DirAccessHarmonyOS::rename(String p_path, String p_new_path) {
	GLOBAL_LOCK_FUNCTION
	String source = resolve_path(p_path);
	String destination = resolve_path(p_new_path);
	SandboxRelativePath source_path;
	SandboxRelativePath destination_path;
	ERR_FAIL_COND_V(!get_sandbox_relative_path(
			source, source_path.root, source_path.relative), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(!get_sandbox_relative_path(
			destination, destination_path.root, destination_path.relative), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(source_path.relative.is_empty() || destination_path.relative.is_empty(), ERR_UNAUTHORIZED);

	int source_parent_fd = -1;
	int destination_parent_fd = -1;
	String source_leaf;
	String destination_leaf;
	if (!_open_sandbox_parent(source_path, source_parent_fd, source_leaf) ||
			!_open_sandbox_parent(destination_path, destination_parent_fd, destination_leaf)) {
		if (source_parent_fd >= 0) {
			close(source_parent_fd);
		}
		if (destination_parent_fd >= 0) {
			close(destination_parent_fd);
		}
		_log_dirfd_failure("open parent for renameat", source);
		return ERR_UNAUTHORIZED;
	}

	CharString source_utf8 = source_leaf.utf8();
	CharString destination_utf8 = destination_leaf.utf8();
	struct stat source_info = {};
	int source_stat_result = fstatat(
			source_parent_fd, source_utf8.get_data(), &source_info, AT_SYMLINK_NOFOLLOW);
	if (source_stat_result != 0 || S_ISLNK(source_info.st_mode)) {
		int operation_errno = source_stat_result != 0 ? errno : ELOOP;
		close(source_parent_fd);
		close(destination_parent_fd);
		errno = operation_errno;
		_log_dirfd_failure("fstatat rename source", source);
		return ERR_UNAUTHORIZED;
	}

	struct stat destination_info = {};
	int destination_stat_result = fstatat(destination_parent_fd, destination_utf8.get_data(),
			&destination_info, AT_SYMLINK_NOFOLLOW);
	if (destination_stat_result == 0 && S_ISLNK(destination_info.st_mode)) {
		close(source_parent_fd);
		close(destination_parent_fd);
#ifdef HARMONYOS_ENABLED
		OH_LOG_ERROR(LOG_APP, "DirAccessHarmonyOS: refusing to replace symlink destination %{public}s",
				destination.utf8().get_data());
#endif
		return ERR_UNAUTHORIZED;
	} else if (destination_stat_result != 0 && errno != ENOENT) {
		int operation_errno = errno;
		close(source_parent_fd);
		close(destination_parent_fd);
		errno = operation_errno;
		_log_dirfd_failure("fstatat rename destination", destination);
		return FAILED;
	}

	int result = renameat(source_parent_fd, source_utf8.get_data(),
			destination_parent_fd, destination_utf8.get_data());
	int operation_errno = errno;
	close(source_parent_fd);
	close(destination_parent_fd);
	if (result == 0) {
		return OK;
	}
	errno = operation_errno;
	_log_dirfd_failure("renameat", source);
	return FAILED;
}

Error DirAccessHarmonyOS::remove(String p_path) {
	GLOBAL_LOCK_FUNCTION
	String resolved = resolve_path(p_path);
	SandboxRelativePath sandbox_path;
	ERR_FAIL_COND_V(!get_sandbox_relative_path(
			resolved, sandbox_path.root, sandbox_path.relative), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(sandbox_path.relative.is_empty(), ERR_UNAUTHORIZED);

	int parent_fd = -1;
	String leaf;
	if (!_open_sandbox_parent(sandbox_path, parent_fd, leaf)) {
		_log_dirfd_failure("open parent for unlinkat", resolved);
		return ERR_UNAUTHORIZED;
	}
	CharString leaf_utf8 = leaf.utf8();
	struct stat info = {};
	int stat_result = fstatat(parent_fd, leaf_utf8.get_data(), &info, AT_SYMLINK_NOFOLLOW);
	if (stat_result != 0 || S_ISLNK(info.st_mode)) {
		int operation_errno = stat_result != 0 ? errno : ELOOP;
		close(parent_fd);
		errno = operation_errno;
		_log_dirfd_failure("fstatat remove target", resolved);
		return ERR_UNAUTHORIZED;
	}

	int flags = S_ISDIR(info.st_mode) ? AT_REMOVEDIR : 0;
	int result = unlinkat(parent_fd, leaf_utf8.get_data(), flags);
	int operation_errno = errno;
	close(parent_fd);
	if (result == 0) {
		return OK;
	}
	errno = operation_errno;
	_log_dirfd_failure("unlinkat", resolved);
	return FAILED;
}

Error DirAccessHarmonyOS::create_link(String p_source, String p_target) {
	GLOBAL_LOCK_FUNCTION
	// A link created inside the sandbox can target an object outside it and
	// later bypass lexical path checks. openat/O_NOFOLLOW would be required to
	// support links without a time-of-check/time-of-use escape, so fail closed.
	return ERR_UNAVAILABLE;
}

DirAccessHarmonyOS::DirAccessHarmonyOS() {
	bool needs_default_roots = false;
	{
		std::lock_guard<std::mutex> lock(sandbox_roots_mutex);
		needs_default_roots = !sandbox_roots_configured;
	}
	if (needs_default_roots) {
		configure_sandbox_roots(
				String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME + "/files",
				String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME + "/cache",
				String(OHOS_DATA_BASE) + "/" + OHOS_MODULE_NAME + "/temp");
	}
}
