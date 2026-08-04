/**************************************************************************/
/*  harmonyos_main.cpp - Godot Engine Main Entry (Phase 2-5)              */
/**************************************************************************/

#include "harmonyos_main.h"
#include "harmonyos_native_window.h"
#include "display_server_harmonyos.h"
#include "os_harmonyos.h"
#include "harmonyos_input.h"
#include "crash_handler_harmonyos.h"

#include "main/main.h"
#include "core/os/main_loop.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"

#include "harmonyos_log.h"
#include <string>
#include <cstdlib>
#include <vector>
#include <atomic>
#include <deque>
#include <mutex>
#include <unistd.h>

// Explicit visibility attribute for dlopen/dlsym by NAPI bridge.
// Must be on definitions, not just declarations, to override
// the -fvisibility=hidden build flag.
#define HARMONYOS_EXPORT_FN __attribute__((visibility("default")))

static HarmonyOSNativeWindow *g_native_window = nullptr;
static CrashHandlerHarmonyOS *g_crash_handler = nullptr;
static std::atomic<bool> g_engine_initialized(false);
static std::atomic<bool> g_os_created(false);
static std::atomic<bool> g_surface_created(false);
static std::atomic<bool> g_engine_started(false);
static std::atomic<bool> g_engine_stopped(false);
static std::atomic<bool> g_engine_cleanup_done(false);
static std::atomic<bool> g_paused(false);
static std::atomic<bool> g_swapchain_created(false);
static std::atomic<uint64_t> g_active_surface_id(0);
static std::atomic<uint64_t> g_active_surface_generation(0);
static std::atomic<uint64_t> g_last_destroyed_surface_generation(0);

enum class SurfaceRequestType {
	CREATE,
	DESTROY,
	UPDATE_XCOMPONENT,
};

struct SurfaceRequest {
	SurfaceRequestType type = SurfaceRequestType::DESTROY;
	uint64_t surface_id = 0;
	uint64_t generation = 0;
	int width = 0;
	int height = 0;
	void *native_xcomponent = nullptr;
};

static std::mutex g_surface_request_mutex;
static std::deque<SurfaceRequest> g_surface_requests;

static bool _parse_surface_id(const char *p_surface_id, uint64_t &r_surface_id) {
	if (!p_surface_id || p_surface_id[0] == '\0') {
		return false;
	}

	char *end = nullptr;
	uint64_t surface_id = strtoull(p_surface_id, &end, 10);
	if (end == p_surface_id || !end || *end != '\0' || surface_id == 0) {
		return false;
	}

	r_surface_id = surface_id;
	return true;
}

static void _record_destroyed_surface_generation(uint64_t p_generation) {
	uint64_t previous = g_last_destroyed_surface_generation.load(std::memory_order_acquire);
	while (previous < p_generation &&
			!g_last_destroyed_surface_generation.compare_exchange_weak(
					previous, p_generation, std::memory_order_acq_rel, std::memory_order_acquire)) {
	}
}

static bool _apply_surface_destroyed_on_engine_thread(uint64_t p_surface_id,
		uint64_t p_generation, bool p_force = false) {
	// A destroy is a monotonic lifecycle fact even when it refers to a surface
	// that is no longer active. Record the tombstone before any stale-request
	// filtering so a delayed create can never resurrect this generation.
	_record_destroyed_surface_generation(p_generation);
	uint64_t active_surface_id = g_active_surface_id.load(std::memory_order_acquire);
	uint64_t active_generation = g_active_surface_generation.load(std::memory_order_acquire);
	if (!p_force && (p_surface_id != active_surface_id || p_generation != active_generation)) {
		OH_LOG_WARN(LOG_APP,
				"Ignoring stale surface destroy sid=%{public}llu generation=%{public}llu active=%{public}llu/%{public}llu",
				(unsigned long long)p_surface_id, (unsigned long long)p_generation,
				(unsigned long long)active_surface_id, (unsigned long long)active_generation);
		return false;
	}

	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->notify_surface_destroyed();
#ifdef VULKAN_ENABLED
		ds->release_rendering_window();
#endif
	}

	// The RenderingDevice swapchain and RenderingContext window no longer
	// reference this handle, so it is now safe to release the OHNativeWindow.
	if (g_native_window) {
		g_native_window->destroy();
	}

	g_surface_created.store(false, std::memory_order_release);
	g_swapchain_created.store(false, std::memory_order_release);
	g_active_surface_id.store(0, std::memory_order_release);
	g_active_surface_generation.store(0, std::memory_order_release);
	OH_LOG_INFO(LOG_APP, "Surface resources released on engine thread");
	return true;
}

static int _apply_surface_created_on_engine_thread(uint64_t p_surface_id, int p_width, int p_height,
		uint64_t p_generation) {
	if (!g_native_window || p_surface_id == 0 || p_width <= 0 || p_height <= 0 || p_generation == 0) {
		OH_LOG_ERROR(LOG_APP, "Invalid surface request on engine thread");
		return -1;
	}
	uint64_t destroyed_generation = g_last_destroyed_surface_generation.load(std::memory_order_acquire);
	if (p_generation <= destroyed_generation) {
		OH_LOG_WARN(LOG_APP,
				"Ignoring destroyed surface generation=%{public}llu tombstone=%{public}llu",
				(unsigned long long)p_generation, (unsigned long long)destroyed_generation);
		return 0;
	}

	uint64_t active_surface_id = g_active_surface_id.load(std::memory_order_acquire);
	uint64_t active_generation = g_active_surface_generation.load(std::memory_order_acquire);
	if (active_generation > p_generation) {
		OH_LOG_WARN(LOG_APP,
				"Ignoring stale surface create sid=%{public}llu generation=%{public}llu active_generation=%{public}llu",
				(unsigned long long)p_surface_id, (unsigned long long)p_generation,
				(unsigned long long)active_generation);
		return 0;
	}

	if (g_surface_created.load(std::memory_order_acquire) &&
			active_surface_id == p_surface_id && active_generation == p_generation) {
		g_native_window->update_surface_size((uint64_t)p_width, (uint64_t)p_height);
		if (DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton()) {
			ds->notify_surface_changed(p_width, p_height);
		}
		OH_LOG_INFO(LOG_APP,
				"Active surface resized sid=%{public}llu generation=%{public}llu size=%{public}dx%{public}d",
				(unsigned long long)p_surface_id, (unsigned long long)p_generation, p_width, p_height);
		return 0;
	}

	if (g_surface_created.load(std::memory_order_acquire) || g_native_window->is_surface_ready()) {
		OH_LOG_WARN(LOG_APP, "Replacing an active surface on engine thread");
		_apply_surface_destroyed_on_engine_thread(active_surface_id, active_generation, true);
	}

	if (!g_native_window->initialize_with_surface_id(
				p_surface_id, (uint64_t)p_width, (uint64_t)p_height)) {
		OH_LOG_ERROR(LOG_APP, "surface_id init FAILED sid=%{public}llu", (unsigned long long)p_surface_id);
		return -1;
	}

	g_surface_created.store(true, std::memory_order_release);
	g_swapchain_created.store(false, std::memory_order_release);
	g_active_surface_id.store(p_surface_id, std::memory_order_release);
	g_active_surface_generation.store(p_generation, std::memory_order_release);
	if (DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton()) {
		ds->notify_surface_changed(p_width, p_height);
	}

	OH_LOG_INFO(LOG_APP,
			"Native surface applied on engine thread sid=%{public}llu generation=%{public}llu size=%{public}dx%{public}d",
			(unsigned long long)p_surface_id, (unsigned long long)p_generation, p_width, p_height);
	return 0;
}

static void _queue_surface_request(const SurfaceRequest &p_request) {
	std::lock_guard<std::mutex> lock(g_surface_request_mutex);
	g_surface_requests.push_back(p_request);
}

static void _clear_surface_requests() {
	std::lock_guard<std::mutex> lock(g_surface_request_mutex);
	g_surface_requests.clear();
}

static void _process_surface_requests_on_engine_thread() {
	std::deque<SurfaceRequest> requests;
	{
		std::lock_guard<std::mutex> lock(g_surface_request_mutex);
		requests.swap(g_surface_requests);
	}

	for (const SurfaceRequest &request : requests) {
		if (request.type == SurfaceRequestType::DESTROY) {
			_apply_surface_destroyed_on_engine_thread(request.surface_id, request.generation);
		} else if (request.type == SurfaceRequestType::CREATE && _apply_surface_created_on_engine_thread(
					request.surface_id, request.width, request.height, request.generation) != 0) {
			OH_LOG_ERROR(LOG_APP, "Queued surface create failed sid=%{public}llu generation=%{public}llu",
					(unsigned long long)request.surface_id, (unsigned long long)request.generation);
		} else if (request.type == SurfaceRequestType::UPDATE_XCOMPONENT &&
				g_native_window && request.native_xcomponent &&
				!g_native_window->initialize_with_xcomponent(
						static_cast<OH_NativeXComponent *>(request.native_xcomponent))) {
			OH_LOG_WARN(LOG_APP, "Queued XComponent input-handle refresh failed");
		}
	}
}

// ---- Static initialization: register platform drivers at load time ----
static struct PlatformInit {
	PlatformInit() {
		OH_LOG_INFO(LOG_APP, "Godot: Registering HarmonyOS platform driver...");
		DisplayServerHarmonyOS::register_harmonyos_driver();
	}
} g_platform_init;

HARMONYOS_EXPORT_FN int harmonyos_godot_init(const char *project_path, const char *files_dir, const char *cache_dir,
		const char *temp_dir, const char *surface_id, int surface_width, int surface_height,
		unsigned long long surface_generation, void *native_xcomponent) {
	OH_LOG_INFO(LOG_APP, "[INIT STEP 10/16] harmonyos_godot_init entry");

	bool expected = false;
	if (!g_engine_initialized.compare_exchange_strong(expected, true)) {
		OH_LOG_WARN(LOG_APP, "Godot engine already initialized");
		return 0;
	}

	g_engine_stopped.store(false);
	g_engine_started.store(false);
	g_engine_cleanup_done.store(false);
	g_surface_created.store(false);
	g_paused.store(false);
	g_swapchain_created.store(false);
	g_active_surface_id.store(0);
	g_active_surface_generation.store(0);
	_clear_surface_requests();

	// Step 11 — Initialize crash handler early, before any engine setup.
	OH_LOG_INFO(LOG_APP, "[INIT STEP 11/16] CrashHandler init");
	g_crash_handler = new CrashHandlerHarmonyOS();
	g_crash_handler->initialize();

	// Step 12 — Create native window manager
	OH_LOG_INFO(LOG_APP, "[INIT STEP 12/16] HarmonyOSNativeWindow create");
	g_native_window = new HarmonyOSNativeWindow();
	OH_NativeXComponent *xcomponent = static_cast<OH_NativeXComponent *>(native_xcomponent);
	if (!xcomponent) {
		OH_LOG_ERROR(LOG_APP, "Native XComponent handle is required before engine initialization");
		g_engine_stopped.store(true, std::memory_order_release);
		g_engine_initialized.store(false, std::memory_order_release);
		delete g_crash_handler;
		g_crash_handler = nullptr;
		delete g_native_window;
		g_native_window = nullptr;
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return -1;
	}
	if (!g_native_window->initialize_with_xcomponent(xcomponent)) {
		OH_LOG_WARN(LOG_APP, "Native XComponent input registration failed; wheel input is unavailable");
	}

	// Step 13 — Create the OS instance (must exist before Main::setup())
	OH_LOG_INFO(LOG_APP, "[INIT STEP 13/16] OS_HarmonyOS instance create");
	if (!OS_HarmonyOS::get_singleton() && !g_os_created.load(std::memory_order_acquire)) {
		(void)memnew(OS_HarmonyOS);
		g_os_created.store(true, std::memory_order_release);
		OH_LOG_INFO(LOG_APP, "OS_HarmonyOS instance created");
	}
	OS_HarmonyOS *os_harmonyos = static_cast<OS_HarmonyOS *>(OS::get_singleton());
	if (!os_harmonyos || !files_dir || !cache_dir || !temp_dir ||
			os_harmonyos->configure_sandbox_paths(files_dir, cache_dir, temp_dir) != OK) {
		OH_LOG_ERROR(LOG_APP, "HarmonyOS sandbox configuration failed before Main::setup");
		g_engine_stopped.store(true, std::memory_order_release);
		g_engine_initialized.store(false, std::memory_order_release);
		delete g_crash_handler;
		g_crash_handler = nullptr;
		delete g_native_window;
		g_native_window = nullptr;
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return -1;
	}

	// Windows has a real main window before Main::setup constructs the
	// DisplayServer. Create the HarmonyOS equivalent now; delaying this until
	// ArkTS sees isEngineReady leaves early editor draws without a swapchain.
	OH_LOG_INFO(LOG_APP, "[INIT STEP 13/17] preparing surface before Main::setup");
	uint64_t initial_surface_id = 0;
	int surface_status = _parse_surface_id(surface_id, initial_surface_id) ?
			_apply_surface_created_on_engine_thread(initial_surface_id, surface_width, surface_height,
					(uint64_t)surface_generation) :
			-1;
	if (surface_status != 0 || !g_native_window || !g_native_window->is_surface_ready()) {
		OH_LOG_ERROR(LOG_APP, "Surface initialization failed before Main::setup, status=%{public}d", surface_status);
		g_engine_stopped.store(true, std::memory_order_release);
		g_engine_initialized.store(false, std::memory_order_release);
		g_surface_created.store(false, std::memory_order_release);
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return -1;
	}
	OH_LOG_INFO(LOG_APP, "[INIT STEP 13/17] native surface ready before Main::setup");

	// Set command-line arguments for Godot Main.
	// Reserve capacity up-front to avoid std::string move during
	// vector reallocation. On x86_64 OHOS (libc++), this move can
	// leave moved-from strings in a state that corrupts heap
	// metadata, causing a CowData<char32_t> SIGSEGV later in
	// ProjectSettings::_load_settings_text.
	std::vector<char *> args;
	std::vector<std::string> arg_strings;
	arg_strings.reserve(6);
	arg_strings.push_back("godot_harmonyos");
#ifdef TOOLS_ENABLED
	arg_strings.push_back("--editor");
#endif
	arg_strings.push_back("--rendering-driver");
	arg_strings.push_back("vulkan");
	if (project_path && project_path[0] != '\0') {
		arg_strings.push_back("--path");
		arg_strings.push_back(project_path);
		OH_LOG_INFO(LOG_APP, "Launching selected project: %{public}s", project_path);
	} else {
		OH_LOG_INFO(LOG_APP, "No project path supplied; launching Project Manager");
	}

	args.reserve(arg_strings.size());
	for (auto &s : arg_strings) {
		args.push_back(&s[0]);
	}

	// Step 14 — Godot full engine init (heaviest step)
	OH_LOG_INFO(LOG_APP, "[INIT STEP 14/16] Main::setup START (heavy — engine full init)");
	Error err = Main::setup(nullptr, (int)args.size(), args.data());
	if (err != OK) {
		OH_LOG_ERROR(LOG_APP, "Main::setup failed with error: %{public}d", err);
		g_engine_stopped.store(true, std::memory_order_release);
		g_engine_initialized.store(false, std::memory_order_release);
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return (int)err;
	}
	OH_LOG_INFO(LOG_APP, "[INIT STEP 14/17] Main::setup DONE, err=%{public}d", (int)err);

	DisplayServerHarmonyOS *display_server = DisplayServerHarmonyOS::get_singleton();
	if (!display_server || !display_server->is_rendering_window_created()) {
		OH_LOG_ERROR(LOG_APP, "Main::setup completed without a valid main-window swapchain");
		Main::cleanup();
		g_engine_stopped.store(true, std::memory_order_release);
		g_engine_initialized.store(false, std::memory_order_release);
		g_surface_created.store(false, std::memory_order_release);
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return (int)ERR_CANT_CREATE;
	}
	g_swapchain_created.store(true, std::memory_order_release);
	display_server->notify_surface_created();
	OH_LOG_INFO(LOG_APP, "[INIT STEP 14/17] main-window swapchain verified before Main::start");

	// NOTE: Unlike desktop platforms, we do NOT call Main::start() here.
	// Main::start() creates the main loop (SceneTree/EditorNode) and must run
	// on the engine thread AFTER the XComponent rendering surface exists.
	// harmonyos_godot_start() (invoked right after this function on the same
	// worker thread) waits for the surface and then drives the frame loop.
	OH_LOG_INFO(LOG_APP, "[INIT STEP 10/16] harmonyos_godot_init DONE, returning 0");
	return 0;
}

// Called by the NAPI bridge's init worker immediately after harmonyos_godot_init().
// Runs on the engine thread: waits for the rendering surface, starts the main
// loop, and drives Main::iteration() until cleanup is requested.
HARMONYOS_EXPORT_FN void harmonyos_godot_start() {
	OH_LOG_INFO(LOG_APP, "[INIT STEP 15/17] harmonyos_godot_start validating pre-created surface");
	_process_surface_requests_on_engine_thread();
	if (g_engine_stopped.load(std::memory_order_acquire)) {
		OH_LOG_WARN(LOG_APP, "Engine stop requested before Main::start");
		Main::cleanup();
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_initialized.store(false, std::memory_order_release);
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return;
	}

	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (!g_surface_created.load(std::memory_order_acquire) || !g_native_window ||
			!g_native_window->is_surface_ready() || !ds) {
		OH_LOG_ERROR(LOG_APP, "Cannot start: pre-created surface or DisplayServer is missing");
		g_engine_stopped.store(true, std::memory_order_release);
		Main::cleanup();
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return;
	}

	if (ds->is_rendering_window_created()) {
		g_swapchain_created.store(true, std::memory_order_release);
		OH_LOG_INFO(LOG_APP, "[INIT STEP 15/17] constructor-created swapchain is ready");
	} else if (ds->check_vulkan_global_context(true) && ds->reset_window()) {
		g_swapchain_created.store(true, std::memory_order_release);
		ds->notify_surface_created();
		OH_LOG_WARN(LOG_APP, "[INIT STEP 15/17] recovered swapchain through reset_window");
	} else {
		OH_LOG_ERROR(LOG_APP, "Cannot start: failed to create main-window swapchain");
		g_engine_stopped.store(true, std::memory_order_release);
		Main::cleanup();
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_cleanup_done.store(true, std::memory_order_release);
		return;
	}
	// Step 15 — Create the main loop (SceneTree / EditorNode).
	OH_LOG_INFO(LOG_APP, "[INIT STEP 15/16] Main::start");
	if (Main::start() != EXIT_SUCCESS) {
		OH_LOG_ERROR(LOG_APP, "Main::start failed");
		g_engine_stopped.store(true);
		Main::cleanup();
		if (g_crash_handler) {
			delete g_crash_handler;
			g_crash_handler = nullptr;
		}
		if (g_native_window) {
			delete g_native_window;
			g_native_window = nullptr;
		}
		g_engine_cleanup_done.store(true);
		return;
	}
	g_engine_started.store(true);
	OH_LOG_INFO(LOG_APP, "Engine started, entering frame loop");

	// The OS layer is responsible for booting the main loop before iterating it
	// (see OS_Windows::run() / OS_LinuxBSD::run() calling main_loop->initialize()).
	// HarmonyOS has no run(); without this call SceneTree::initialize() never
	// runs, the SceneTree root Window never enters the scene tree, its window_id
	// stays INVALID_WINDOW_ID, the rect-changed callback is never registered and
	// the root viewport is never attached to the screen — the engine draws into a
	// void and the screen stays black. Initialize the main loop here to mirror
	// the desktop platforms.
	{
		MainLoop *ml = OS::get_singleton()->get_main_loop();
		if (ml) {
			ml->initialize();
			OH_LOG_INFO(LOG_APP, "[INIT STEP 17/17] main loop initialized");
		} else {
			OH_LOG_ERROR(LOG_APP, "[INIT STEP 17/17] no main loop to initialize");
		}
	}

	// Frame loop — mirrors OS_Windows::run()'s Main::iteration() loop.
	// All engine iteration happens on this single engine thread.
	int frame_count = 0;
	while (!g_engine_stopped.load(std::memory_order_acquire)) {
		// Surface callbacks originate on the ArkTS/NAPI thread. Consume them
		// here before pause throttling so background destruction still releases
		// Vulkan resources on their owning engine thread.
		_process_surface_requests_on_engine_thread();

		if (g_paused.load(std::memory_order_acquire)) {
			usleep(10000);
			continue;
		}

		// Lazy swapchain creation: the OHNativeWindow may arrive after
		// Main::start() if the XComponent OnSurfaceCreated callback was
		// delayed. Create the swapchain on the engine thread as soon as the
		// surface is ready.
		if (!g_swapchain_created.load(std::memory_order_acquire) &&
				g_native_window && g_native_window->is_surface_ready()) {
			OH_LOG_INFO(LOG_APP, "[frame] lazy swapchain attempt: ready=%{public}d",
					(int)g_native_window->is_surface_ready());
			DisplayServerHarmonyOS *dsp = DisplayServerHarmonyOS::get_singleton();
			if (dsp && dsp->check_vulkan_global_context(true) && dsp->reset_window()) {
				g_swapchain_created.store(true);
				dsp->notify_surface_created();
				OH_LOG_INFO(LOG_APP, "Swapchain created lazily in frame loop");
			} else {
				OH_LOG_WARN(LOG_APP, "[frame] lazy swapchain failed: dsp=%{public}p", (void *)dsp);
			}
		}

		OS_HarmonyOS *os = static_cast<OS_HarmonyOS *>(OS::get_singleton());
		if (os) {
			os->process_joypad_events();
		}

		// Heartbeat. Without a log emitted from inside the loop there is no way
		// to tell a healthy engine from one that entered the loop and blocked on
		// the very first iteration — both look identical from outside: process
		// alive, no crash, startup milestones reached, screen blank. The first
		// few frames are traced on both sides of Main::iteration() so a hang can
		// be attributed to the iteration itself rather than to the loop.
		if (frame_count < 5 || (frame_count % 300) == 0) {
			OH_LOG_INFO(LOG_APP, "[frame] %{public}d begin (frames_drawn=%{public}llu)", frame_count,
					(unsigned long long)Engine::get_singleton()->get_frames_drawn());
		}

		// The XComponent surface is created before Main::start() on the JS/NAPI
		// thread, so update_window_size() fires the rect-changed callback before
		// the root Window registered it (rect_changed_callback.is_valid() == false).
		// The engine then keeps the 0x0 initial window size forever, the viewport
		// never gets a valid size, draw_viewports() skips every viewport and the
		// screen stays black. Re-fire the size notification once the engine thread
		// owns the frame loop so the callback reaches the root Window.
		if (frame_count == 0) {
			DisplayServerHarmonyOS *dsp = DisplayServerHarmonyOS::get_singleton();
			if (dsp) {
				Size2i sz = dsp->get_window_size();
				dsp->notify_surface_changed(sz.width, sz.height);
				OH_LOG_INFO(LOG_APP, "[frame] re-fired surface size %{public}dx%{public}d", sz.width, sz.height);
			}
			MainLoop *ml = OS::get_singleton()->get_main_loop();
			if (ml) {
				SceneTree *st = Object::cast_to<SceneTree>(ml);
				if (st) {
					Window *root_win = st->get_root();
					if (root_win) {
						OH_LOG_INFO(LOG_APP, "[frame] root window id=%{public}d size=%{public}dx%{public}d visible=%{public}d",
								(int)root_win->get_window_id(), (int)root_win->get_size().width,
								(int)root_win->get_size().height, (int)root_win->is_visible());
					}
				}
			}
		}

		// The editor enables low-processor mode (OS::set_low_processor_usage_mode(true))
		// so RenderingServer::draw() only runs when the scene reports changes. On a
		// headless-style startup with an empty project there is no animation or input,
		// has_changed() stays false after the first frame, and the UI is never drawn
		// again — leaving the swap chain contents black forever. Force a redraw every
		// frame on the engine thread so the editor UI becomes visible.
		OS::get_singleton()->set_low_processor_usage_mode(false);

		if (Main::iteration()) {
			OH_LOG_INFO(LOG_APP, "[frame] Main::iteration requested exit at frame=%{public}d", frame_count);
			break; // Engine requested exit.
		}

		if (frame_count < 5) {
			OH_LOG_INFO(LOG_APP, "[frame] %{public}d end", frame_count);
		}
		frame_count++;
		usleep(16000); // ~60 FPS
	}

	OH_LOG_INFO(LOG_APP, "Frame loop ended after %{public}d frames (swapchain_created=%{public}d), running Main::cleanup",
			frame_count, (int)g_swapchain_created.load(std::memory_order_acquire));
	_process_surface_requests_on_engine_thread();

	// Main::cleanup() must run on the same thread that called Main::setup/start.
	Main::cleanup();

	if (g_crash_handler) {
		delete g_crash_handler;
		g_crash_handler = nullptr;
	}
	if (g_native_window) {
		delete g_native_window;
		g_native_window = nullptr;
	}
	_clear_surface_requests();
	g_surface_created.store(false, std::memory_order_release);
	g_swapchain_created.store(false, std::memory_order_release);
	g_active_surface_id.store(0, std::memory_order_release);
	g_active_surface_generation.store(0, std::memory_order_release);
	g_engine_started.store(false, std::memory_order_release);
	g_engine_initialized.store(false, std::memory_order_release);
	g_engine_cleanup_done.store(true);
	OH_LOG_INFO(LOG_APP, "Engine cleanup done");
}

HARMONYOS_EXPORT_FN int harmonyos_godot_surface_created(const char *surface_id, int surface_width,
		int surface_height, unsigned long long surface_generation) {
	OH_LOG_INFO(LOG_APP,
			"Surface create requested: %{public}s generation=%{public}llu size=%{public}dx%{public}d",
			surface_id ? surface_id : "<null>", surface_generation, surface_width, surface_height);

	if (!g_engine_initialized.load(std::memory_order_acquire) ||
			g_engine_stopped.load(std::memory_order_acquire) || !g_native_window ||
			!surface_id || surface_id[0] == '\0' ||
			surface_width <= 0 || surface_height <= 0 || surface_generation == 0) {
		OH_LOG_ERROR(LOG_APP, "Invalid surface create request or engine state");
		return -1;
	}

	uint64_t sid = 0;
	if (!_parse_surface_id(surface_id, sid)) {
		OH_LOG_ERROR(LOG_APP, "Invalid numeric surface id: %{public}s", surface_id);
		return -1;
	}
	if ((uint64_t)surface_generation <=
			g_last_destroyed_surface_generation.load(std::memory_order_acquire)) {
		OH_LOG_WARN(LOG_APP, "Rejected create for an already destroyed surface generation=%{public}llu",
				surface_generation);
		return 0;
	}

	SurfaceRequest request;
	request.type = SurfaceRequestType::CREATE;
	request.surface_id = sid;
	request.generation = (uint64_t)surface_generation;
	request.width = surface_width;
	request.height = surface_height;
	_queue_surface_request(request);
	OH_LOG_INFO(LOG_APP, "Surface create queued for engine thread sid=%{public}llu generation=%{public}llu",
			(unsigned long long)request.surface_id, (unsigned long long)request.generation);
	return 0;
}

HARMONYOS_EXPORT_FN int harmonyos_godot_surface_destroy(const char *surface_id,
		unsigned long long surface_generation) {
	uint64_t sid = 0;
	if (!_parse_surface_id(surface_id, sid) || surface_generation == 0) {
		OH_LOG_ERROR(LOG_APP, "Invalid surface destroy identity");
		return -1;
	}

	// Record immediately, including before engine initialization. The NAPI
	// bridge also mirrors this tombstone so destroys received before dlopen are
	// replayed into libgodot before the initial surface create is evaluated.
	_record_destroyed_surface_generation((uint64_t)surface_generation);
	if (!g_engine_initialized.load(std::memory_order_acquire) ||
			g_engine_stopped.load(std::memory_order_acquire)) {
		return 0;
	}

	SurfaceRequest request;
	request.type = SurfaceRequestType::DESTROY;
	request.surface_id = sid;
	request.generation = (uint64_t)surface_generation;
	_queue_surface_request(request);
	OH_LOG_INFO(LOG_APP, "Surface destroy queued for engine thread sid=%{public}llu generation=%{public}llu",
			(unsigned long long)request.surface_id, (unsigned long long)request.generation);
	return 0;
}

HARMONYOS_EXPORT_FN int harmonyos_godot_set_xcomponent(void *native_xcomponent) {
	if (!native_xcomponent) {
		return -1;
	}
	if (!g_engine_initialized.load(std::memory_order_acquire) ||
			g_engine_stopped.load(std::memory_order_acquire)) {
		// harmonyos_godot_init receives the latest handle directly. There is no
		// engine-owned object to update before that point.
		return 0;
	}

	SurfaceRequest request;
	request.type = SurfaceRequestType::UPDATE_XCOMPONENT;
	request.native_xcomponent = native_xcomponent;
	_queue_surface_request(request);
	OH_LOG_INFO(LOG_APP, "XComponent input-handle refresh queued for engine thread");
	return 0;
}

HARMONYOS_EXPORT_FN void harmonyos_godot_set_destroyed_surface_generation(
		unsigned long long surface_generation) {
	if (surface_generation != 0) {
		_record_destroyed_surface_generation((uint64_t)surface_generation);
	}
}

HARMONYOS_EXPORT_FN void harmonyos_godot_cleanup() {
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup START =====");

	// Signal the engine thread to leave the frame loop. Main::cleanup() runs
	// on the engine thread itself (it must not be called from the NAPI/UI
	// thread that owns no engine state).
	g_engine_stopped.store(true, std::memory_order_release);
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup REQUESTED =====");
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_pause() {
	OH_LOG_INFO(LOG_APP, "Application paused");
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;

	// Throttle the frame loop instead of tearing down the Vulkan context.
	// For a desktop-style editor process, pause is transient; destroying and
	// recreating the rendering context on resume risks crashes and state loss.
	g_paused.store(true, std::memory_order_release);

	// Foreground/background transitions are the only focus signal HarmonyOS
	// gives us, so this is where the engine learns the window lost focus.
	if (DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton()) {
		ds->notify_window_focus(false);
	}
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_resume() {
	OH_LOG_INFO(LOG_APP, "Application resumed");
	g_paused.store(false, std::memory_order_release);

	if (DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton()) {
		ds->notify_window_focus(true);
	}
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_back_press() {
	OH_LOG_INFO(LOG_APP, "Back pressed");
}

HARMONYOS_EXPORT_FN void harmonyos_godot_terminate(int exit_code) {
	OH_LOG_INFO(LOG_APP, "Terminate requested with code: %{public}d", exit_code);
	harmonyos_godot_cleanup();
	std::exit(exit_code);
}

// ---- Input event forwarding ----

HARMONYOS_EXPORT_FN void harmonyos_godot_key_event(int key_code, int event_type, const char *key_text) {
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;
	HarmonyOSInput::process_key_event(key_code, event_type, key_text);
}

HARMONYOS_EXPORT_FN void harmonyos_godot_mouse_event(int button, int action, double x, double y,
                                                   double offset_x, double offset_y) {
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;
	HarmonyOSInput::process_mouse_event(button, action, x, y, offset_x, offset_y);
}

HARMONYOS_EXPORT_FN void harmonyos_godot_touch_event(int touch_id, int action, double x, double y) {
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;
	HarmonyOSInput::process_touch_event(touch_id, action, x, y);
}

HARMONYOS_EXPORT_FN void harmonyos_godot_input_text(const char *text) {
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;
	HarmonyOSInput::process_input_text(text);
}

HARMONYOS_EXPORT_FN void harmonyos_godot_ime_update(const char *text, int selection_start, int selection_length) {
	if (!g_engine_initialized.load(std::memory_order_acquire)) {
		return;
	}
	if (DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton()) {
		ds->ime_text(text ? String::utf8(text) : String());
		ds->ime_selection(Vector2i(selection_start, selection_length));
	}
}
