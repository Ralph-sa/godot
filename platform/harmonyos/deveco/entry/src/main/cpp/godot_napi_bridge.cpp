/**************************************************************************/
/*  godot_napi_bridge.cpp - Thin NAPI Bridge to libgodot.so               */
/*                                                                        */
/*  MINIMAL TEST: all heavy work (dlopen + init) on pthread.              */
/*  Polling via atomics; C++ -> ArkTS callbacks use a thread-safe queue. */
/**************************************************************************/

#include <napi/native_api.h>

// hilog/log.h falls back to LOG_TAG = NULL when the including file has not
// defined it, and OH_LOG_Print discards records with a NULL tag — so these
// must be set before the SDK header is pulled in, or nothing this file logs
// ever reaches the hilog buffer.
#define LOG_DOMAIN 0x0000
#define LOG_TAG "GodotNAPI"

#include <hilog/log.h>
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <cerrno>
#include <mutex>
#include <string>

extern "C" void harmonyos_notify_window_title(const char *title);

typedef void (*godot_window_title_callback_t)(const char *);
typedef void (*godot_set_window_title_callback_t)(godot_window_title_callback_t);
typedef int (*godot_init_t)(const char *, const char *, const char *, const char *, const char *, int, int,
		unsigned long long, void *);
typedef void (*godot_start_t)();
typedef void (*godot_cleanup_t)();
typedef int (*godot_surface_created_t)(const char *, int, int, unsigned long long);
typedef int (*godot_surface_destroy_t)(const char *, unsigned long long);
typedef int (*godot_set_xcomponent_t)(void *);
typedef void (*godot_set_destroyed_surface_generation_t)(unsigned long long);
typedef void (*godot_key_event_t)(int, int, const char *);
typedef void (*godot_mouse_event_t)(int, int, double, double, double, double);
typedef void (*godot_touch_event_t)(int, int, double, double);
typedef void (*godot_input_text_t)(const char *);
typedef void (*godot_ime_update_t)(const char *, int, int);
typedef void (*godot_on_pause_t)();
typedef void (*godot_on_resume_t)();
typedef void (*godot_on_back_press_t)();

static void *g_libgodot = nullptr;
static godot_init_t g_init_func = nullptr;
static godot_start_t g_start_func = nullptr;
static godot_cleanup_t g_cleanup_func = nullptr;
static godot_surface_created_t g_surface_created_func = nullptr;
static godot_surface_destroy_t g_surface_destroy_func = nullptr;
static godot_set_xcomponent_t g_set_xcomponent_func = nullptr;
static godot_set_destroyed_surface_generation_t g_set_destroyed_surface_generation_func = nullptr;
static godot_key_event_t g_key_event_func = nullptr;
static godot_mouse_event_t g_mouse_event_func = nullptr;
static godot_touch_event_t g_touch_event_func = nullptr;
static godot_input_text_t g_input_text_func = nullptr;
static godot_ime_update_t g_ime_update_func = nullptr;
static godot_on_pause_t g_on_pause_func = nullptr;
static godot_on_resume_t g_on_resume_func = nullptr;
static godot_on_back_press_t g_on_back_press_func = nullptr;
static godot_set_window_title_callback_t g_set_window_title_callback_func = nullptr;

static napi_threadsafe_function g_title_callback = nullptr;
static std::mutex g_title_callback_mutex;

static void call_title_callback(napi_env env, napi_value js_callback, void *, void *data) {
	std::string *title = static_cast<std::string *>(data);
	if (title == nullptr) {
		return;
	}

	if (env != nullptr && js_callback != nullptr) {
		napi_value global = nullptr;
		napi_value argument = nullptr;
		if (napi_get_global(env, &global) == napi_ok &&
				napi_create_string_utf8(env, title->c_str(), title->size(), &argument) == napi_ok) {
			napi_call_function(env, global, js_callback, 1, &argument, nullptr);
		}
	}

	delete title;
}

static void replace_title_callback(napi_threadsafe_function new_callback) {
	std::lock_guard<std::mutex> lock(g_title_callback_mutex);
	napi_threadsafe_function previous = g_title_callback;
	g_title_callback = new_callback;
	if (previous != nullptr) {
		napi_status status = napi_release_threadsafe_function(previous, napi_tsfn_abort);
		if (status != napi_ok) {
			OH_LOG_WARN(LOG_APP, "Failed to release title callback, status=%{public}d", (int)status);
		}
	}
}

// The current XComponent native handle. The authoritative value arrives from
// ArkTS XComponent.onLoad(context); module exports are only a compatibility
// fallback because a normal ArkTS import may precede XComponent creation.
static OH_NativeXComponent *g_xcomponent = nullptr;
static napi_ref g_module_exports_ref = nullptr;
static std::mutex g_xcomponent_mutex;

static bool capture_xcomponent_from_value(napi_env env, napi_value value, const char *source);
static bool capture_xcomponent(napi_env env);
static OH_NativeXComponent *get_current_xcomponent();

// ---- Async init ----
static std::atomic<int> g_init_status{-1};
static std::atomic<bool> g_stop_requested{false};
static std::atomic<unsigned long long> g_last_destroyed_surface_generation{0};
static pthread_t g_engine_thread{};
static bool g_engine_thread_joinable = false;
static std::mutex g_engine_thread_mutex;

enum class EngineWorkerState {
	STOPPED,
	STARTING,
	RUNNING,
	STOPPING,
	JOINING,
};

static EngineWorkerState g_engine_worker_state = EngineWorkerState::STOPPED;

struct InitWorkerArgs {
	std::string project_path;
	std::string files_dir;
	std::string cache_dir;
	std::string temp_dir;
	std::string surface_id;
	int surface_width = 0;
	int surface_height = 0;
	unsigned long long surface_generation = 0;
};

static void record_destroyed_surface_generation(unsigned long long generation) {
	unsigned long long previous = g_last_destroyed_surface_generation.load(std::memory_order_acquire);
	while (previous < generation &&
			!g_last_destroyed_surface_generation.compare_exchange_weak(
					previous, generation, std::memory_order_acq_rel, std::memory_order_acquire)) {
	}
}

static bool read_utf8_string(napi_env env, napi_value value, std::string &result) {
	size_t length = 0;
	if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) {
		return false;
	}

	result.assign(length + 1, '\0');
	size_t written = 0;
	if (napi_get_value_string_utf8(env, value, &result[0], result.size(), &written) != napi_ok) {
		result.clear();
		return false;
	}
	result.resize(written);
	return true;
}

static bool is_safe_import_leaf(const std::string &name) {
	return !name.empty() && name.size() <= 255 &&
			name != "." && name != ".." &&
			name.find('/') == std::string::npos &&
			name.find('\\') == std::string::npos &&
			name.find('\0') == std::string::npos;
}

static bool is_safe_import_staging_leaf(const std::string &name) {
	if (!is_safe_import_leaf(name)) {
		return false;
	}
	const std::string marker = ".importing";
	size_t marker_position = name.find(marker);
	if (marker_position == std::string::npos || marker_position == 0) {
		return false;
	}
	for (size_t index = marker_position + marker.size(); index < name.size(); index++) {
		if (name[index] != '_') {
			return false;
		}
	}
	return is_safe_import_leaf(name.substr(0, marker_position));
}

static bool is_expected_import_staging_leaf(
		const std::string &staging_leaf, const std::string &destination_leaf) {
	if (!is_safe_import_staging_leaf(staging_leaf) || !is_safe_import_leaf(destination_leaf)) {
		return false;
	}
	const std::string expected_prefix = destination_leaf + ".importing";
	if (staging_leaf.compare(0, expected_prefix.size(), expected_prefix) != 0) {
		return false;
	}
	for (size_t index = expected_prefix.size(); index < staging_leaf.size(); index++) {
		if (staging_leaf[index] != '_') {
			return false;
		}
	}
	return true;
}

static int open_absolute_directory_nofollow(const std::string &path) {
	if (path.empty() || path.front() != '/') {
		errno = EINVAL;
		return -1;
	}

	int current_fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (current_fd < 0) {
		return -1;
	}

	size_t cursor = 1;
	while (cursor < path.size()) {
		while (cursor < path.size() && path[cursor] == '/') {
			cursor++;
		}
		if (cursor >= path.size()) {
			break;
		}
		size_t separator = path.find('/', cursor);
		std::string component = path.substr(
				cursor, separator == std::string::npos ? std::string::npos : separator - cursor);
		if (!is_safe_import_leaf(component)) {
			close(current_fd);
			errno = EINVAL;
			return -1;
		}

		int next_fd = openat(current_fd, component.c_str(),
				O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
		int operation_errno = errno;
		close(current_fd);
		if (next_fd < 0) {
			errno = operation_errno;
			return -1;
		}
		current_fd = next_fd;
		if (separator == std::string::npos) {
			break;
		}
		cursor = separator + 1;
	}
	return current_fd;
}

static int open_projects_directory(const std::string &files_dir) {
	int files_fd = open_absolute_directory_nofollow(files_dir);
	if (files_fd < 0) {
		return -1;
	}
	int projects_fd = openat(files_fd, "projects",
			O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	int operation_errno = errno;
	close(files_fd);
	if (projects_fd < 0) {
		errno = operation_errno;
	}
	return projects_fd;
}

static int remove_import_tree_at(int parent_fd, const std::string &leaf, int depth) {
	if (depth > 64 || !is_safe_import_leaf(leaf)) {
		return EINVAL;
	}

	struct stat entry_info = {};
	if (fstatat(parent_fd, leaf.c_str(), &entry_info, AT_SYMLINK_NOFOLLOW) != 0) {
		return errno == ENOENT ? 0 : errno;
	}
	if (!S_ISDIR(entry_info.st_mode)) {
		return unlinkat(parent_fd, leaf.c_str(), 0) == 0 ? 0 : errno;
	}

	int child_fd = openat(parent_fd, leaf.c_str(),
			O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (child_fd < 0) {
		return errno;
	}
	struct stat opened_info = {};
	if (fstat(child_fd, &opened_info) != 0 ||
			opened_info.st_dev != entry_info.st_dev || opened_info.st_ino != entry_info.st_ino) {
		int operation_errno = errno != 0 ? errno : ESTALE;
		close(child_fd);
		return operation_errno;
	}

	DIR *directory = fdopendir(child_fd);
	if (directory == nullptr) {
		int operation_errno = errno;
		close(child_fd);
		return operation_errno;
	}

	int result = 0;
	for (;;) {
		errno = 0;
		dirent *entry = readdir(directory);
		if (entry == nullptr) {
			if (errno != 0) {
				result = errno;
			}
			break;
		}
		std::string child_name(entry->d_name);
		if (child_name == "." || child_name == "..") {
			continue;
		}
		result = remove_import_tree_at(dirfd(directory), child_name, depth + 1);
		if (result != 0) {
			break;
		}
	}
	if (closedir(directory) != 0 && result == 0) {
		result = errno;
	}
	if (result != 0) {
		return result;
	}

	struct stat final_info = {};
	if (fstatat(parent_fd, leaf.c_str(), &final_info, AT_SYMLINK_NOFOLLOW) != 0) {
		return errno == ENOENT ? 0 : errno;
	}
	if (!S_ISDIR(final_info.st_mode) ||
			final_info.st_dev != entry_info.st_dev || final_info.st_ino != entry_info.st_ino) {
		return ESTALE;
	}
	return unlinkat(parent_fd, leaf.c_str(), AT_REMOVEDIR) == 0 ? 0 : errno;
}

static int commit_project_import(const std::string &files_dir,
		const std::string &staging_leaf, const std::string &destination_leaf) {
	if (!is_expected_import_staging_leaf(staging_leaf, destination_leaf)) {
		return EINVAL;
	}

	int projects_fd = open_projects_directory(files_dir);
	if (projects_fd < 0) {
		return errno;
	}

	struct stat staging_info = {};
	if (fstatat(projects_fd, staging_leaf.c_str(), &staging_info, AT_SYMLINK_NOFOLLOW) != 0 ||
			!S_ISDIR(staging_info.st_mode) || S_ISLNK(staging_info.st_mode)) {
		int operation_errno = errno != 0 ? errno : ENOTDIR;
		close(projects_fd);
		return operation_errno;
	}

	struct stat destination_info = {};
	if (fstatat(projects_fd, destination_leaf.c_str(), &destination_info, AT_SYMLINK_NOFOLLOW) == 0) {
		close(projects_fd);
		return EEXIST;
	}
	if (errno != ENOENT) {
		int operation_errno = errno;
		close(projects_fd);
		return operation_errno;
	}

	int staging_fd = openat(projects_fd, staging_leaf.c_str(),
			O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
	if (staging_fd < 0) {
		int operation_errno = errno;
		close(projects_fd);
		return operation_errno;
	}
	struct stat opened_staging_info = {};
	if (fstat(staging_fd, &opened_staging_info) != 0 ||
			opened_staging_info.st_dev != staging_info.st_dev ||
			opened_staging_info.st_ino != staging_info.st_ino) {
		int operation_errno = errno != 0 ? errno : ESTALE;
		close(staging_fd);
		close(projects_fd);
		return operation_errno;
	}

	int project_file_fd = openat(staging_fd, "project.godot", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (project_file_fd < 0) {
		int operation_errno = errno;
		close(staging_fd);
		close(projects_fd);
		return operation_errno;
	}
	struct stat project_file_info = {};
	int project_stat_result = fstat(project_file_fd, &project_file_info);
	int project_stat_errno = errno;
	close(project_file_fd);
	close(staging_fd);
	if (project_stat_result != 0 || !S_ISREG(project_file_info.st_mode)) {
		close(projects_fd);
		return project_stat_result != 0 ? project_stat_errno : EINVAL;
	}

	int rename_result = renameat2(projects_fd, staging_leaf.c_str(),
			projects_fd, destination_leaf.c_str(), RENAME_NOREPLACE);
	int rename_errno = errno;
	close(projects_fd);
	return rename_result == 0 ? 0 : rename_errno;
}

static int cleanup_project_import(const std::string &files_dir, const std::string &staging_leaf) {
	if (!is_safe_import_staging_leaf(staging_leaf)) {
		return EINVAL;
	}
	int projects_fd = open_projects_directory(files_dir);
	if (projects_fd < 0) {
		return errno;
	}
	int result = remove_import_tree_at(projects_fd, staging_leaf, 0);
	close(projects_fd);
	return result;
}

static bool canonicalize_directory(const std::string &path, std::string &canonical_path) {
	if (path.empty()) {
		return false;
	}

	char resolved[PATH_MAX] = {};
	if (realpath(path.c_str(), resolved) == nullptr) {
		OH_LOG_ERROR(LOG_APP, "realpath failed for directory %{public}s", path.c_str());
		return false;
	}

	struct stat info = {};
	if (stat(resolved, &info) != 0 || !S_ISDIR(info.st_mode)) {
		OH_LOG_ERROR(LOG_APP, "Path is not a directory: %{public}s", resolved);
		return false;
	}

	canonical_path = resolved;
	return true;
}

static bool path_is_within_root(const std::string &path, const std::string &root) {
	if (path == root) {
		return true;
	}
	return path.size() > root.size() &&
			path.compare(0, root.size(), root) == 0 &&
			path[root.size()] == '/';
}

static bool validate_project_path(const std::string &project_path,
		const std::string &files_dir, std::string &canonical_project_path) {
	if (!canonicalize_directory(project_path, canonical_project_path) ||
			!path_is_within_root(canonical_project_path, files_dir)) {
		OH_LOG_ERROR(LOG_APP, "Project path must resolve inside filesDir: project=%{public}s files=%{public}s",
				project_path.c_str(), files_dir.c_str());
		return false;
	}

	std::string project_file = canonical_project_path + "/project.godot";
	struct stat project_info = {};
	if (lstat(project_file.c_str(), &project_info) != 0 ||
			!S_ISREG(project_info.st_mode) || S_ISLNK(project_info.st_mode)) {
		OH_LOG_ERROR(LOG_APP, "Project path has no regular project.godot: %{public}s", project_file.c_str());
		return false;
	}

	return true;
}

static bool load_libgodot() {
	if (!g_libgodot) {
		g_libgodot = dlopen("libgodot.so", RTLD_NOW | RTLD_GLOBAL);
		if (!g_libgodot) {
			OH_LOG_ERROR(LOG_APP, "dlopen libgodot.so failed: %{public}s", dlerror());
			return false;
		}
		g_init_func = (godot_init_t)dlsym(g_libgodot, "harmonyos_godot_init");
		g_start_func = (godot_start_t)dlsym(g_libgodot, "harmonyos_godot_start");
		g_cleanup_func = (godot_cleanup_t)dlsym(g_libgodot, "harmonyos_godot_cleanup");
		g_surface_created_func = (godot_surface_created_t)dlsym(g_libgodot, "harmonyos_godot_surface_created");
		g_surface_destroy_func = (godot_surface_destroy_t)dlsym(g_libgodot, "harmonyos_godot_surface_destroy");
		g_set_xcomponent_func = (godot_set_xcomponent_t)dlsym(g_libgodot, "harmonyos_godot_set_xcomponent");
		g_set_destroyed_surface_generation_func = (godot_set_destroyed_surface_generation_t)dlsym(
				g_libgodot, "harmonyos_godot_set_destroyed_surface_generation");
		g_key_event_func = (godot_key_event_t)dlsym(g_libgodot, "harmonyos_godot_key_event");
		g_mouse_event_func = (godot_mouse_event_t)dlsym(g_libgodot, "harmonyos_godot_mouse_event");
		g_touch_event_func = (godot_touch_event_t)dlsym(g_libgodot, "harmonyos_godot_touch_event");
		g_input_text_func = (godot_input_text_t)dlsym(g_libgodot, "harmonyos_godot_input_text");
		g_ime_update_func = (godot_ime_update_t)dlsym(g_libgodot, "harmonyos_godot_ime_update");
		g_on_pause_func = (godot_on_pause_t)dlsym(g_libgodot, "harmonyos_godot_on_pause");
		g_on_resume_func = (godot_on_resume_t)dlsym(g_libgodot, "harmonyos_godot_on_resume");
		g_on_back_press_func = (godot_on_back_press_t)dlsym(g_libgodot, "harmonyos_godot_on_back_press");
		g_set_window_title_callback_func = (godot_set_window_title_callback_t)dlsym(
				g_libgodot, "harmonyos_set_window_title_callback");
	}
	if (!g_init_func || !g_start_func || !g_cleanup_func ||
			!g_surface_created_func || !g_surface_destroy_func ||
			!g_set_xcomponent_func || !g_set_destroyed_surface_generation_func ||
			!g_key_event_func || !g_mouse_event_func || !g_touch_event_func ||
			!g_input_text_func || !g_ime_update_func ||
			!g_on_pause_func || !g_on_resume_func || !g_on_back_press_func ||
			!g_set_window_title_callback_func) {
		OH_LOG_ERROR(LOG_APP, "libgodot.so is missing one or more required HarmonyOS exports");
		return false;
	}
	g_set_window_title_callback_func(harmonyos_notify_window_title);
	return true;
}

static void *init_worker(void *data) {
	InitWorkerArgs *worker_args = static_cast<InitWorkerArgs *>(data);
	std::string project_path;
	std::string files_dir;
	std::string cache_dir;
	std::string temp_dir;
	std::string surface_id;
	int surface_width = 0;
	int surface_height = 0;
	unsigned long long surface_generation = 0;
	if (worker_args != nullptr) {
		project_path = worker_args->project_path;
		files_dir = worker_args->files_dir;
		cache_dir = worker_args->cache_dir;
		temp_dir = worker_args->temp_dir;
		surface_id = worker_args->surface_id;
		surface_width = worker_args->surface_width;
		surface_height = worker_args->surface_height;
		surface_generation = worker_args->surface_generation;
		delete worker_args;
	}

	bool ok = load_libgodot();
	if (!ok) {
		g_init_status.store(-2, std::memory_order_release);
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_engine_worker_state == EngineWorkerState::STARTING) {
			g_engine_worker_state = EngineWorkerState::STOPPING;
		}
		return nullptr;
	}

	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_stop_requested.load(std::memory_order_acquire)) {
			if (g_engine_worker_state == EngineWorkerState::STARTING) {
				g_engine_worker_state = EngineWorkerState::STOPPING;
			}
			return nullptr;
		}
		// Only the worker may publish RUNNING, after every required dlsym has
		// succeeded. Cleanup can therefore distinguish a callable engine from
		// the startup window without racing partially initialized pointers.
		g_engine_worker_state = EngineWorkerState::RUNNING;
	}

	OH_NativeXComponent *xcomponent = get_current_xcomponent();
	if (!xcomponent) {
		OH_LOG_ERROR(LOG_APP, "XComponent handle disappeared before engine initialization");
		g_init_status.store(-2, std::memory_order_release);
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_engine_worker_state == EngineWorkerState::RUNNING) {
			g_engine_worker_state = EngineWorkerState::STOPPING;
		}
		return nullptr;
	}

	// Replay destroys that arrived before dlopen. If a destroy races this load,
	// NAPI publishes it again through the same setter once RUNNING is visible.
	g_set_destroyed_surface_generation_func(
			g_last_destroyed_surface_generation.load(std::memory_order_acquire));
	// harmonyos_godot_init on worker thread (avoids ANR on main). The surface
	// is mandatory here: Main::setup constructs RenderingDevice and the editor
	// may draw before this worker returns.
	int status = g_init_func ?
			g_init_func(project_path.empty() ? nullptr : project_path.c_str(),
					files_dir.c_str(), cache_dir.c_str(), temp_dir.c_str(),
					surface_id.c_str(), surface_width, surface_height, surface_generation, xcomponent) :
			-1;
	g_init_status.store(status, std::memory_order_release);

	if (status == 0 && g_stop_requested.load(std::memory_order_acquire) && g_cleanup_func) {
		// cleanup may have raced harmonyos_godot_init before it reset its own
		// stop flag. Reassert the request after setup and let start() perform the
		// required same-thread Main::cleanup path.
		g_cleanup_func();
	}

	// Phase 2: main loop + frame loop, still on this engine thread.
	// harmonyos_godot_start() waits for the XComponent surface, then calls
	// Main::start() and iterates until cleanup. It returns only after the
	// engine has fully shut down.
	if (status == 0 && g_start_func) {
		g_start_func();
	}
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_engine_worker_state == EngineWorkerState::RUNNING) {
			g_engine_worker_state = EngineWorkerState::STOPPING;
		}
	}
	return nullptr;
}

static int join_engine_worker() {
	pthread_t thread{};
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (!g_engine_thread_joinable) {
			return 0;
		}
		thread = g_engine_thread;
	}

	if (pthread_equal(pthread_self(), thread)) {
		OH_LOG_ERROR(LOG_APP, "Refusing to join the engine worker from itself");
		return -1;
	}

	int result = pthread_join(thread, nullptr);
	if (result != 0) {
		OH_LOG_ERROR(LOG_APP, "Failed to join engine worker, error=%{public}d", result);
		return result;
	}
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		g_engine_thread_joinable = false;
		g_stop_requested.store(false, std::memory_order_release);
		g_init_status.store(-1, std::memory_order_release);
		// STOPPED is the final publication point. NAPI_LoadLibrary may start a
		// new worker as soon as it observes this state, so no previous-lifecycle
		// atomics may be reset after this assignment.
		g_engine_worker_state = EngineWorkerState::STOPPED;
	}
	OH_LOG_INFO(LOG_APP, "Engine worker joined; lifecycle state is STOPPED");
	return 0;
}

static void *cleanup_worker(void *) {
	int join_result = join_engine_worker();
	if (join_result != 0) {
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		g_engine_worker_state = EngineWorkerState::STOPPING;
	}
	return nullptr;
}

// ---- NAPI ----

static napi_value NAPI_CommitProjectImport(napi_env env, napi_callback_info info) {
	size_t argc = 3;
	napi_value args[3] = { nullptr, nullptr, nullptr };
	std::string files_dir;
	std::string staging_leaf;
	std::string destination_leaf;
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 3 ||
			!read_utf8_string(env, args[0], files_dir) ||
			!read_utf8_string(env, args[1], staging_leaf) ||
			!read_utf8_string(env, args[2], destination_leaf)) {
		napi_value result;
		napi_create_int32(env, -EINVAL, &result);
		return result;
	}

	int operation_error = commit_project_import(files_dir, staging_leaf, destination_leaf);
	if (operation_error != 0) {
		OH_LOG_ERROR(LOG_APP,
				"Atomic project import commit failed: staging=%{public}s destination=%{public}s errno=%{public}d",
				staging_leaf.c_str(), destination_leaf.c_str(), operation_error);
	}
	napi_value result;
	napi_create_int32(env, operation_error == 0 ? 0 : -operation_error, &result);
	return result;
}

static napi_value NAPI_CleanupProjectImport(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2] = { nullptr, nullptr };
	std::string files_dir;
	std::string staging_leaf;
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 2 ||
			!read_utf8_string(env, args[0], files_dir) ||
			!read_utf8_string(env, args[1], staging_leaf)) {
		napi_value result;
		napi_create_int32(env, -EINVAL, &result);
		return result;
	}

	int operation_error = cleanup_project_import(files_dir, staging_leaf);
	if (operation_error != 0) {
		OH_LOG_ERROR(LOG_APP,
				"Native project import cleanup failed: staging=%{public}s errno=%{public}d",
				staging_leaf.c_str(), operation_error);
	}
	napi_value result;
	napi_create_int32(env, operation_error == 0 ? 0 : -operation_error, &result);
	return result;
}

static napi_value NAPI_LoadLibrary(napi_env env, napi_callback_info info) {
	size_t argc = 8;
	napi_value args[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 8) {
		OH_LOG_ERROR(LOG_APP, "loadLibrary requires projectPath, sandbox roots, surface identity and size");
		napi_value result; napi_create_int32(env, -1, &result); return result;
	}

	std::string project_path;
	std::string files_dir;
	std::string cache_dir;
	std::string temp_dir;
	std::string surface_id;
	std::string canonical_project_path;
	std::string canonical_files_dir;
	std::string canonical_cache_dir;
	std::string canonical_temp_dir;
	napi_valuetype width_type = napi_undefined;
	napi_valuetype height_type = napi_undefined;
	napi_valuetype generation_type = napi_undefined;
	int32_t surface_width = 0;
	int32_t surface_height = 0;
	int64_t surface_generation = 0;
	if (!read_utf8_string(env, args[0], project_path) ||
			!read_utf8_string(env, args[1], files_dir) ||
			!read_utf8_string(env, args[2], cache_dir) ||
			!read_utf8_string(env, args[3], temp_dir) ||
			!read_utf8_string(env, args[4], surface_id) ||
			napi_typeof(env, args[5], &width_type) != napi_ok || width_type != napi_number ||
			napi_typeof(env, args[6], &height_type) != napi_ok || height_type != napi_number ||
			napi_typeof(env, args[7], &generation_type) != napi_ok || generation_type != napi_number ||
			napi_get_value_int32(env, args[5], &surface_width) != napi_ok ||
			napi_get_value_int32(env, args[6], &surface_height) != napi_ok ||
			napi_get_value_int64(env, args[7], &surface_generation) != napi_ok ||
			project_path.empty() || files_dir.empty() || cache_dir.empty() || temp_dir.empty() || surface_id.empty() ||
			surface_width <= 0 || surface_height <= 0 || surface_generation <= 0) {
		OH_LOG_ERROR(LOG_APP, "Invalid loadLibrary arguments");
		napi_value result; napi_create_int32(env, -1, &result); return result;
	}

	if (!canonicalize_directory(files_dir, canonical_files_dir) ||
			!canonicalize_directory(cache_dir, canonical_cache_dir) ||
			!canonicalize_directory(temp_dir, canonical_temp_dir) ||
			!validate_project_path(project_path, canonical_files_dir, canonical_project_path)) {
		OH_LOG_ERROR(LOG_APP, "Rejected non-sandbox or invalid engine paths");
		napi_value result; napi_create_int32(env, -1, &result); return result;
	}
	OH_NativeXComponent *xcomponent = get_current_xcomponent();
	if (xcomponent == nullptr && capture_xcomponent(env)) {
		xcomponent = get_current_xcomponent();
	}
	if (xcomponent == nullptr) {
		OH_LOG_ERROR(LOG_APP, "XComponent native handle is unavailable");
		napi_value result; napi_create_int32(env, -1, &result); return result;
	}

	InitWorkerArgs *worker_args = new InitWorkerArgs();
	// Keep the UIAbilityContext spellings (normally /data/storage/...) for the
	// engine. realpath is used only for containment validation because the
	// canonical kernel path may not satisfy HarmonyOS' public sandbox prefix
	// contract even though it refers to the same directory.
	worker_args->project_path = project_path;
	worker_args->files_dir = files_dir;
	worker_args->cache_dir = cache_dir;
	worker_args->temp_dir = temp_dir;
	worker_args->surface_id = surface_id;
	worker_args->surface_width = (int)surface_width;
	worker_args->surface_height = (int)surface_height;
	worker_args->surface_generation = (unsigned long long)surface_generation;

	pthread_t thread{};
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_engine_worker_state != EngineWorkerState::STOPPED || g_engine_thread_joinable) {
			delete worker_args;
			OH_LOG_ERROR(LOG_APP, "Engine worker is not stopped; rejecting a second initialization");
			napi_value result; napi_create_int32(env, -1, &result); return result;
		}
		g_engine_worker_state = EngineWorkerState::STARTING;
		g_stop_requested.store(false, std::memory_order_release);
		g_init_status.store(-1, std::memory_order_release);

		// Keep STARTING and joinability under one lock. A concurrent cleanup can
		// never observe a worker state for which no join target has been published.
		int create_result = pthread_create(&thread, nullptr, init_worker, worker_args);
		if (create_result != 0) {
			delete worker_args;
			g_engine_worker_state = EngineWorkerState::STOPPED;
			g_init_status.store(-2, std::memory_order_release);
			OH_LOG_ERROR(LOG_APP, "Failed to create engine worker, error=%{public}d", create_result);
			napi_value result; napi_create_int32(env, -1, &result); return result;
		}
		g_engine_thread = thread;
		g_engine_thread_joinable = true;
	}
	OH_LOG_INFO(LOG_APP,
			"Engine worker started, project=%{public}s files=%{public}s surface=%{public}s generation=%{public}lld size=%{public}dx%{public}d",
			project_path.c_str(), files_dir.c_str(),
			surface_id.c_str(), (long long)surface_generation, (int)surface_width, (int)surface_height);
	napi_value result; napi_create_int32(env, 0, &result); return result;
}

static napi_value NAPI_IsLibraryLoaded(napi_env env, napi_callback_info info) {
	int status = g_init_status.load(std::memory_order_acquire);
	napi_value result;
	napi_create_int32(env, status == 0 ? 1 : (status == -1 ? 0 : -1), &result);
	return result;
}

static napi_value NAPI_StartEngine(napi_env env, napi_callback_info info) {
	int s = g_init_status.load();
	napi_value r; napi_create_int32(env, s, &r); return r;
}

static napi_value NAPI_Cleanup(napi_env env, napi_callback_info info) {
	replace_title_callback(nullptr);
	godot_cleanup_t cleanup_func = nullptr;
	godot_set_window_title_callback_t title_callback_setter = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		g_stop_requested.store(true, std::memory_order_release);
		if (g_engine_worker_state == EngineWorkerState::STOPPED) {
			napi_value result; napi_create_int32(env, 0, &result); return result;
		}
		if (g_engine_worker_state == EngineWorkerState::JOINING) {
			napi_value result; napi_create_int32(env, 0, &result); return result;
		}
		if (g_engine_worker_state == EngineWorkerState::RUNNING) {
			// RUNNING is published by the worker only after dlsym completion, so
			// these pointers are synchronized by the same mutex.
			cleanup_func = g_cleanup_func;
			title_callback_setter = g_set_window_title_callback_func;
		} else if (g_engine_worker_state == EngineWorkerState::STARTING) {
			OH_LOG_WARN(LOG_APP, "Cleanup requested while engine worker is starting; stop is deferred to worker");
		}
		g_engine_worker_state = EngineWorkerState::JOINING;
	}
	if (title_callback_setter) {
		title_callback_setter(nullptr);
	}
	if (cleanup_func) {
		cleanup_func();
	}
	pthread_t cleanup_thread{};
	int create_result = pthread_create(&cleanup_thread, nullptr, cleanup_worker, nullptr);
	if (create_result == 0) {
		pthread_detach(cleanup_thread);
	} else {
		OH_LOG_ERROR(LOG_APP, "Failed to create cleanup worker, joining synchronously: %{public}d", create_result);
		int join_result = join_engine_worker();
		if (join_result != 0) {
			std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
			g_engine_worker_state = EngineWorkerState::STOPPING;
		}
	}
	// Keep libgodot.so loaded for the process lifetime. Godot owns process-wide
	// singletons and static destructors, and unloading the code while any late
	// callback is in flight is unsafe even after the main loop requested stop.
	// The OS reclaims the mapping when the application process exits.
	napi_value result; napi_create_int32(env, 0, &result); return result;
}

static napi_value NAPI_OnSurfaceCreated(napi_env env, napi_callback_info info) {
	size_t argc = 4;
	napi_value args[4] = { nullptr, nullptr, nullptr, nullptr };
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 4) {
		napi_value r; napi_create_int32(env, -1, &r); return r;
	}
	std::string id;
	int32_t w = 0, h = 0;
	int64_t generation = 0;
	if (!read_utf8_string(env, args[0], id) ||
			napi_get_value_int32(env, args[1], &w) != napi_ok ||
			napi_get_value_int32(env, args[2], &h) != napi_ok ||
			napi_get_value_int64(env, args[3], &generation) != napi_ok ||
			id.empty() || w <= 0 || h <= 0 || generation <= 0) {
		napi_value r; napi_create_int32(env, -1, &r); return r;
	}
	int s = g_surface_created_func ?
			g_surface_created_func(id.c_str(), (int)w, (int)h, (unsigned long long)generation) :
			-1;
	napi_value r; napi_create_int32(env, s, &r); return r;
}

static bool capture_xcomponent_from_value(napi_env env, napi_value value, const char *source) {
	if (value == nullptr) {
		return false;
	}
	napi_value export_instance = nullptr;
	if (napi_get_named_property(env, value, OH_NATIVE_XCOMPONENT_OBJ, &export_instance) != napi_ok) {
		return false;
	}
	OH_NativeXComponent *xcomponent = nullptr;
	if (napi_unwrap(env, export_instance, reinterpret_cast<void **>(&xcomponent)) != napi_ok || xcomponent == nullptr) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(g_xcomponent_mutex);
		g_xcomponent = xcomponent;
	}
	OH_LOG_INFO(LOG_APP, "capture_xcomponent: refreshed native XComponent from %{public}s", source);
	return true;
}

// Compatibility fallback for runtimes that inject the native object directly
// into the module exports before GodotModuleInit returns.
static bool capture_xcomponent(napi_env env) {
	if (g_module_exports_ref == nullptr) {
		return false;
	}
	napi_value exports = nullptr;
	if (napi_get_reference_value(env, g_module_exports_ref, &exports) != napi_ok || exports == nullptr) {
		return false;
	}
	return capture_xcomponent_from_value(env, exports, "module exports");
}

static OH_NativeXComponent *get_current_xcomponent() {
	std::lock_guard<std::mutex> lock(g_xcomponent_mutex);
	return g_xcomponent;
}

// Called from ArkTS XComponent.onLoad(context). The callback context belongs
// to the `libraryname` load path and is the authoritative object containing
// OH_NATIVE_XCOMPONENT_OBJ.
static napi_value NAPI_InitXComponent(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1] = { nullptr };
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 1) {
		OH_LOG_ERROR(LOG_APP, "initXComponent requires the XComponent onLoad context");
		napi_value r; napi_create_int32(env, -1, &r); return r;
	}
	if (!capture_xcomponent_from_value(env, args[0], "onLoad context") &&
			!capture_xcomponent(env)) {
		OH_LOG_ERROR(LOG_APP, "Failed to unwrap native XComponent from onLoad context");
		napi_value r; napi_create_int32(env, -1, &r); return r;
	}

	OH_NativeXComponent *xcomponent = get_current_xcomponent();
	godot_set_xcomponent_t setter = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_engine_worker_state == EngineWorkerState::RUNNING) {
			setter = g_set_xcomponent_func;
		}
	}
	int status = setter ? setter(xcomponent) : 0;
	napi_value r; napi_create_int32(env, status, &r); return r;
}

static napi_value NAPI_OnSurfaceDestroy(napi_env env, napi_callback_info info) {
	size_t argc = 2;
	napi_value args[2] = { nullptr, nullptr };
	std::string id;
	int64_t generation = 0;
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 2 ||
			!read_utf8_string(env, args[0], id) ||
			napi_get_value_int64(env, args[1], &generation) != napi_ok ||
			id.empty() || generation <= 0) {
			napi_value r; napi_create_int32(env, -1, &r); return r;
	}
	unsigned long long destroyed_generation = (unsigned long long)generation;
	record_destroyed_surface_generation(destroyed_generation);

	godot_set_destroyed_surface_generation_t tombstone_setter = nullptr;
	godot_surface_destroy_t surface_destroy = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_engine_thread_mutex);
		if (g_engine_worker_state == EngineWorkerState::RUNNING) {
			tombstone_setter = g_set_destroyed_surface_generation_func;
			surface_destroy = g_surface_destroy_func;
		}
	}
	if (tombstone_setter) {
		tombstone_setter(destroyed_generation);
	}
	// A destroy received before dlopen is still successful: the NAPI-side
	// tombstone is replayed by init_worker before the initial create.
	int s = surface_destroy ? surface_destroy(id.c_str(), destroyed_generation) : 0;
	napi_value r; napi_create_int32(env, s, &r); return r;
}

static napi_value NAPI_SendKeyEvent(napi_env env, napi_callback_info info) {
	if (!g_key_event_func) { napi_value r; napi_create_int32(env, -1, &r); return r; }
	size_t argc = 3; napi_value a[3]; napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
	int32_t kc, et; napi_get_value_int32(env, a[0], &kc); napi_get_value_int32(env, a[1], &et);
	char kt[64] = {0}; size_t tl; napi_get_value_string_utf8(env, a[2], kt, sizeof(kt), &tl);
	g_key_event_func(kc, et, kt); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_SendMouseEvent(napi_env env, napi_callback_info info) {
	if (!g_mouse_event_func) { napi_value r; napi_create_int32(env, -1, &r); return r; }
	size_t argc = 6; napi_value a[6]; napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
	int32_t b, act; double x, y, ox, oy;
	napi_get_value_int32(env, a[0], &b); napi_get_value_int32(env, a[1], &act);
	napi_get_value_double(env, a[2], &x); napi_get_value_double(env, a[3], &y);
	napi_get_value_double(env, a[4], &ox); napi_get_value_double(env, a[5], &oy);
	g_mouse_event_func(b, act, x, y, ox, oy); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_SendTouchEvent(napi_env env, napi_callback_info info) {
	if (!g_touch_event_func) { napi_value r; napi_create_int32(env, -1, &r); return r; }
	size_t argc = 4; napi_value a[4]; napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
	int32_t tid, act; double x, y;
	napi_get_value_int32(env, a[0], &tid); napi_get_value_int32(env, a[1], &act);
	napi_get_value_double(env, a[2], &x); napi_get_value_double(env, a[3], &y);
	g_touch_event_func(tid, act, x, y); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_SendInputText(napi_env env, napi_callback_info info) {
	if (!g_input_text_func) { napi_value r; napi_create_int32(env, -1, &r); return r; }
	size_t argc = 1; napi_value a[1]; napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
	char t[256] = {0}; size_t l; napi_get_value_string_utf8(env, a[0], t, sizeof(t), &l);
	g_input_text_func(t); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_SendImeUpdate(napi_env env, napi_callback_info info) {
	if (!g_ime_update_func) { napi_value r; napi_create_int32(env, -1, &r); return r; }
	size_t argc = 3;
	napi_value args[3] = { nullptr, nullptr, nullptr };
	std::string text;
	int32_t selection_start = 0;
	int32_t selection_length = 0;
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 3 ||
			!read_utf8_string(env, args[0], text) ||
			napi_get_value_int32(env, args[1], &selection_start) != napi_ok ||
			napi_get_value_int32(env, args[2], &selection_length) != napi_ok ||
			selection_start < 0 || selection_length < 0) {
		napi_value r; napi_create_int32(env, -1, &r); return r;
	}
	g_ime_update_func(text.c_str(), (int)selection_start, (int)selection_length);
	napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_OnPause(napi_env env, napi_callback_info info) {
	if (g_on_pause_func) g_on_pause_func(); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_OnResume(napi_env env, napi_callback_info info) {
	if (g_on_resume_func) g_on_resume_func(); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_OnBackPress(napi_env env, napi_callback_info info) {
	if (g_on_back_press_func) g_on_back_press_func(); napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_RegisterTitleCallback(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1] = { nullptr };
	if (napi_get_cb_info(env, info, &argc, args, nullptr, nullptr) != napi_ok || argc < 1) {
		napi_value result; napi_create_int32(env, -1, &result); return result;
	}

	napi_valuetype type = napi_undefined;
	if (napi_typeof(env, args[0], &type) != napi_ok || type != napi_function) {
		napi_value result; napi_create_int32(env, -1, &result); return result;
	}

	napi_value resource_name = nullptr;
	if (napi_create_string_utf8(env, "GodotWindowTitle", NAPI_AUTO_LENGTH, &resource_name) != napi_ok) {
		napi_value result; napi_create_int32(env, -2, &result); return result;
	}

	napi_threadsafe_function callback = nullptr;
	napi_status status = napi_create_threadsafe_function(
			env, args[0], nullptr, resource_name, 8, 1,
			nullptr, nullptr, nullptr, call_title_callback, &callback);
	if (status != napi_ok || callback == nullptr) {
		OH_LOG_ERROR(LOG_APP, "Failed to create title TSFN, status=%{public}d", (int)status);
		napi_value result; napi_create_int32(env, -2, &result); return result;
	}

	replace_title_callback(callback);
	napi_value result; napi_create_int32(env, 0, &result); return result;
}

extern "C" void harmonyos_notify_window_title(const char *title) {
	if (title == nullptr) {
		return;
	}

	std::string *title_copy = new std::string(title);
	napi_status status = napi_invalid_arg;
	{
		std::lock_guard<std::mutex> lock(g_title_callback_mutex);
		if (g_title_callback == nullptr) {
			delete title_copy;
			return;
		}
		status = napi_call_threadsafe_function(g_title_callback, title_copy, napi_tsfn_nonblocking);
	}

	if (status != napi_ok) {
		delete title_copy;
		OH_LOG_WARN(LOG_APP, "Failed to queue title callback, status=%{public}d", (int)status);
	}
}

EXTERN_C_START
static napi_value GodotModuleInit(napi_env env, napi_value exports) {
	// Keep a reference only as a compatibility fallback. The normal path uses
	// the context supplied later by XComponent.onLoad.
	if (g_module_exports_ref == nullptr) {
		napi_create_reference(env, exports, 1, &g_module_exports_ref);
	}
	if (capture_xcomponent(env)) {
		OH_LOG_INFO(LOG_APP, "GodotModuleInit: native XComponent captured at init");
	} else {
		OH_LOG_INFO(LOG_APP, "GodotModuleInit: XComponent not yet available, waiting for onLoad context");
	}

	napi_value o; napi_create_object(env, &o);
	napi_property_descriptor d[] = {
		{"commitProjectImport", nullptr, NAPI_CommitProjectImport, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"cleanupProjectImport", nullptr, NAPI_CleanupProjectImport, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"loadLibrary", nullptr, NAPI_LoadLibrary, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"isLibraryLoaded", nullptr, NAPI_IsLibraryLoaded, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"startEngine", nullptr, NAPI_StartEngine, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"cleanup", nullptr, NAPI_Cleanup, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onSurfaceCreated", nullptr, NAPI_OnSurfaceCreated, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"initXComponent", nullptr, NAPI_InitXComponent, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onSurfaceDestroy", nullptr, NAPI_OnSurfaceDestroy, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendKeyEvent", nullptr, NAPI_SendKeyEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendMouseEvent", nullptr, NAPI_SendMouseEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendTouchEvent", nullptr, NAPI_SendTouchEvent, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendInputText", nullptr, NAPI_SendInputText, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendImeUpdate", nullptr, NAPI_SendImeUpdate, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onPause", nullptr, NAPI_OnPause, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onResume", nullptr, NAPI_OnResume, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onBackPress", nullptr, NAPI_OnBackPress, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"registerTitleCallback", nullptr, NAPI_RegisterTitleCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
	};
	napi_define_properties(env, o, sizeof(d)/sizeof(d[0]), d);
	napi_set_named_property(env, exports, "godot_napi", o);
	return exports;
}
EXTERN_C_END

static napi_module godotModule = {
	.nm_version = 1, .nm_flags = 0, .nm_filename = nullptr,
	.nm_register_func = GodotModuleInit, .nm_modname = "godot_napi",
	.nm_priv = nullptr, .reserved = {nullptr},
};

extern "C" __attribute__((constructor)) void RegisterGodotNAPI() {
	napi_module_register(&godotModule);
}
