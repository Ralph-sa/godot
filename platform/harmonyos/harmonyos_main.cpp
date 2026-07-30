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

// Explicit visibility attribute for dlopen/dlsym by NAPI bridge.
// Must be on definitions, not just declarations, to override
// the -fvisibility=hidden build flag.
#define HARMONYOS_EXPORT_FN __attribute__((visibility("default")))

static HarmonyOSNativeWindow *g_native_window = nullptr;
static CrashHandlerHarmonyOS *g_crash_handler = nullptr;
static std::atomic<bool> g_engine_initialized(false);
static std::atomic<bool> g_os_created(false);
static std::atomic<bool> g_surface_created(false);

// ---- Static initialization: register platform drivers at load time ----
static struct PlatformInit {
	PlatformInit() {
		OH_LOG_INFO(LOG_APP, "Godot: Registering HarmonyOS platform driver...");
		DisplayServerHarmonyOS::register_harmonyos_driver();
	}
} g_platform_init;

HARMONYOS_EXPORT_FN int harmonyos_godot_init() {
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Initialization START =====");

	bool expected = false;
	if (!g_engine_initialized.compare_exchange_strong(expected, true)) {
		OH_LOG_WARN(LOG_APP, "Godot engine already initialized");
		return 0;
	}

	// Initialize crash handler early, before any engine setup.
	g_crash_handler = new CrashHandlerHarmonyOS();
	g_crash_handler->initialize();

	// Create native window manager
	g_native_window = new HarmonyOSNativeWindow();

	// Create the OS instance (must exist before Main::setup())
	if (!OS_HarmonyOS::get_singleton() && !g_os_created.load(std::memory_order_acquire)) {
		OS_HarmonyOS *os = memnew(OS_HarmonyOS);
		g_os_created.store(true, std::memory_order_release);
		OH_LOG_INFO(LOG_APP, "OS_HarmonyOS instance created");
	}

	// Set command-line arguments for Godot Main
	std::vector<char *> args;
	std::vector<std::string> arg_strings;
	arg_strings.push_back("godot_harmonyos");
	arg_strings.push_back("--editor");
	arg_strings.push_back("--rendering-driver");
	arg_strings.push_back("vulkan");

	for (auto &s : arg_strings) {
		args.push_back(&s[0]);
	}

	Error err = Main::setup(nullptr, 0, args.data());
	if (err != OK) {
		OH_LOG_ERROR(LOG_APP, "Main::setup failed with error: %{public}d", err);
		OH_LOG_WARN(LOG_APP, "Main::setup returned error, continuing...");
	}

	OH_LOG_INFO(LOG_APP, "===== Godot Engine Initialization DONE =====");
	return 0;
}

HARMONYOS_EXPORT_FN int harmonyos_godot_surface_created(const char *surface_id) {
	OH_LOG_INFO(LOG_APP, "Surface created: %{public}s", surface_id);

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

	if (!g_native_window->initialize_with_xcomponent(nullptr)) {
		OH_LOG_INFO(LOG_APP, "XComponent not set via NAPI bridge, waiting for native callback");
	}

	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->notify_surface_created();

		// Ensure Vulkan global context is initialized before reset_window
		bool vulkan_ok = ds->check_vulkan_global_context(true);
		if (vulkan_ok) {
			ds->reset_window();
		} else {
			OH_LOG_ERROR(LOG_APP, "Vulkan context initialization failed");
		}
	} else {
		OH_LOG_WARN(LOG_APP, "DisplayServer not ready for surface reset");
	}

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

	if (g_engine_initialized.exchange(false, std::memory_order_acq_rel)) {
		Main::cleanup();
	}

	delete g_native_window;
	g_native_window = nullptr;

	delete g_crash_handler;
	g_crash_handler = nullptr;

	g_surface_created.store(false, std::memory_order_release);

	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup DONE =====");
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_pause() {
	OH_LOG_INFO(LOG_APP, "Application paused");
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;

#if defined(VULKAN_ENABLED)
	DisplayServerHarmonyOS::free_vulkan_global_context();
#endif
}

HARMONYOS_EXPORT_FN void harmonyos_godot_on_resume() {
	OH_LOG_INFO(LOG_APP, "Application resumed");
	if (!g_engine_initialized.load(std::memory_order_acquire)) return;

#if defined(VULKAN_ENABLED)
	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		bool vulkan_ok = ds->check_vulkan_global_context(true);
		if (vulkan_ok) {
			ds->reset_window();
		} else {
			OH_LOG_ERROR(LOG_APP, "Failed to reinitialize Vulkan context on resume");
		}
	}
#endif
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
