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

#include <hilog/log.h>
#include <string>
#include <cstdlib>
#include <vector>
#include <atomic>
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

// Getter provided by the NAPI bridge (libgodot_napi.so). Resolved at runtime
// through the same weak-symbol mechanism as harmonyos_notify_window_title:
// the bridge is loaded with RTLD_GLOBAL, so its symbols satisfy libgodot's
// weak undefined references.
extern "C" OH_NativeXComponent *harmonyos_get_xcomponent() __attribute__((weak));

// ---- Static initialization: register platform drivers at load time ----
static struct PlatformInit {
	PlatformInit() {
		OH_LOG_INFO(LOG_APP, "Godot: Registering HarmonyOS platform driver...");
		DisplayServerHarmonyOS::register_harmonyos_driver();
	}
} g_platform_init;

HARMONYOS_EXPORT_FN int harmonyos_godot_init() {
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

	// Step 11 — Initialize crash handler early, before any engine setup.
	OH_LOG_INFO(LOG_APP, "[INIT STEP 11/16] CrashHandler init");
	g_crash_handler = new CrashHandlerHarmonyOS();
	g_crash_handler->initialize();

	// Step 12 — Create native window manager
	OH_LOG_INFO(LOG_APP, "[INIT STEP 12/16] HarmonyOSNativeWindow create");
	g_native_window = new HarmonyOSNativeWindow();

	// Step 13 — Create the OS instance (must exist before Main::setup())
	OH_LOG_INFO(LOG_APP, "[INIT STEP 13/16] OS_HarmonyOS instance create");
	if (!OS_HarmonyOS::get_singleton() && !g_os_created.load(std::memory_order_acquire)) {
		(void)memnew(OS_HarmonyOS);
		g_os_created.store(true, std::memory_order_release);
		OH_LOG_INFO(LOG_APP, "OS_HarmonyOS instance created");
	}

	// Set command-line arguments for Godot Main.
	// Reserve capacity up-front to avoid std::string move during
	// vector reallocation. On x86_64 OHOS (libc++), this move can
	// leave moved-from strings in a state that corrupts heap
	// metadata, causing a CowData<char32_t> SIGSEGV later in
	// ProjectSettings::_load_settings_text.
	std::vector<char *> args;
	std::vector<std::string> arg_strings;
	arg_strings.reserve(4);
	arg_strings.push_back("godot_harmonyos");
#ifdef TOOLS_ENABLED
	arg_strings.push_back("--editor");
#endif
	arg_strings.push_back("--rendering-driver");
	arg_strings.push_back("vulkan");

	for (auto &s : arg_strings) {
		args.push_back(&s[0]);
	}

	// Step 14 — Godot full engine init (heaviest step)
	OH_LOG_INFO(LOG_APP, "[INIT STEP 14/16] Main::setup START (heavy — engine full init)");
	Error err = Main::setup(nullptr, (int)args.size(), args.data());
	if (err != OK) {
		OH_LOG_ERROR(LOG_APP, "Main::setup failed with error: %{public}d", err);
		OH_LOG_WARN(LOG_APP, "Main::setup returned error, continuing...");
	}
	OH_LOG_INFO(LOG_APP, "[INIT STEP 14/16] Main::setup DONE");

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
	OH_LOG_INFO(LOG_APP, "[INIT STEP 17/17] harmonyos_godot_start waiting for surface");

	// Wait until the XComponent surface is created (ArkTS onLoad → NAPI
	// onSurfaceCreated → Vulkan init). Bounded wait so a missing surface
	// cannot hang the engine thread forever.
	int waits = 0;
	while (!g_surface_created.load(std::memory_order_acquire) &&
			!g_engine_stopped.load(std::memory_order_acquire)) {
		usleep(10000);
		waits++;
		if (waits > 2000) { // 20s timeout
			OH_LOG_ERROR(LOG_APP, "Timed out waiting for rendering surface");
			g_engine_stopped.store(true);
			break;
		}
	}

	if (g_engine_stopped.load(std::memory_order_acquire)) {
		OH_LOG_WARN(LOG_APP, "Engine stopped before start, skipping main loop");
		g_engine_cleanup_done.store(true);
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

	// Frame loop — mirrors OS_Windows::run()'s Main::iteration() loop.
	// All engine iteration happens on this single engine thread.
	while (!g_engine_stopped.load(std::memory_order_acquire)) {
		if (g_paused.load(std::memory_order_acquire)) {
			usleep(10000);
			continue;
		}
		OS_HarmonyOS *os = static_cast<OS_HarmonyOS *>(OS::get_singleton());
		if (os) {
			os->process_joypad_events();
		}
		if (Main::iteration()) {
			break; // Engine requested exit.
		}
		usleep(16000); // ~60 FPS
	}

	OH_LOG_INFO(LOG_APP, "Frame loop ended, running Main::cleanup");

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
	g_engine_cleanup_done.store(true);
	OH_LOG_INFO(LOG_APP, "Engine cleanup done");
}

HARMONYOS_EXPORT_FN int harmonyos_godot_surface_created(const char *surface_id) {
	OH_LOG_INFO(LOG_APP, "[INIT STEP 16/16] Surface created: %{public}s", surface_id);

	if (!g_native_window) {
		OH_LOG_ERROR(LOG_APP, "Native window not initialized");
		return -1;
	}

	// Prevent double-initialization from dual callback paths
	bool expected = false;
	if (!g_surface_created.compare_exchange_strong(expected, true)) {
		OH_LOG_WARN(LOG_APP, "Surface already created, skipping duplicate callback");
		return 0;
	}

	// The NAPI bridge captured the XComponent from the ArkTS onLoad context via
	// OH_NativeXComponent_GetNativeXComponent. Register the native callbacks so
	// OnSurfaceCreated_CB hands us the OHNativeWindow surface handle.
	OH_NativeXComponent *xcomponent = nullptr;
	if (harmonyos_get_xcomponent) {
		xcomponent = harmonyos_get_xcomponent();
	}
	if (xcomponent) {
		if (!g_native_window->initialize_with_xcomponent(xcomponent)) {
			OH_LOG_ERROR(LOG_APP, "Failed to initialize with XComponent");
		}
	} else {
		OH_LOG_WARN(LOG_APP, "XComponent not provided via NAPI bridge, waiting for native callback");
	}

	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->notify_surface_created();

		// Ensure Vulkan global context is initialized before reset_window
		OH_LOG_INFO(LOG_APP, "[INIT STEP 16/16] Vulkan init + reset_window");
		bool vulkan_ok = ds->check_vulkan_global_context(true);
		if (vulkan_ok) {
			ds->reset_window();
		} else {
			OH_LOG_ERROR(LOG_APP, "Vulkan context initialization failed");
		}
	} else {
		OH_LOG_WARN(LOG_APP, "DisplayServer not ready for surface reset");
	}

	OH_LOG_INFO(LOG_APP, "[INIT STEP 16/16] Surface creation DONE");
	return 0;
}

HARMONYOS_EXPORT_FN int harmonyos_godot_surface_destroy() {
	OH_LOG_INFO(LOG_APP, "Surface destroyed");

	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->notify_surface_destroyed();
	}

	if (g_native_window) {
		g_native_window->destroy();
	}

	g_surface_created.store(false, std::memory_order_release);

	return 0;
}

HARMONYOS_EXPORT_FN void harmonyos_godot_cleanup() {
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup START =====");

	// Signal the engine thread to leave the frame loop. Main::cleanup() runs
	// on the engine thread itself (it must not be called from the NAPI/UI
	// thread that owns no engine state).
	g_engine_stopped.store(true, std::memory_order_release);

	int waits = 0;
	while (!g_engine_cleanup_done.load(std::memory_order_acquire) && waits < 400) {
		usleep(50000);
		waits++;
	}
	if (!g_engine_cleanup_done.load(std::memory_order_acquire)) {
		OH_LOG_ERROR(LOG_APP, "Timed out waiting for engine cleanup");
	}

	g_engine_initialized.store(false, std::memory_order_release);

	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup DONE =====");
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_pause() {
	OH_LOG_INFO(LOG_APP, "Application paused");
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;

	// Throttle the frame loop instead of tearing down the Vulkan context.
	// For a desktop-style editor process, pause is transient; destroying and
	// recreating the rendering context on resume risks crashes and state loss.
	g_paused.store(true, std::memory_order_release);
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_resume() {
	OH_LOG_INFO(LOG_APP, "Application resumed");
	g_paused.store(false, std::memory_order_release);
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
