/**************************************************************************/
/*  harmonyos_main.cpp - Godot Engine Main Entry (Phase 2-5)              */
/**************************************************************************/

#include "harmonyos_main.h"
#include "harmonyos_native_window.h"
#include "display_server_harmonyos.h"
#include "os_harmonyos.h"
#include "harmonyos_input.h"

#include "main/main.h"

#include <hilog/log.h>
#include <string>
#include <cstdlib>
#include <vector>

static HarmonyOSNativeWindow *g_native_window = nullptr;
static bool g_engine_initialized = false;
static bool g_os_created = false;

// ---- Static initialization: register platform drivers at load time ----
static struct PlatformInit {
	PlatformInit() {
		OH_LOG_INFO(LOG_APP, "Godot: Registering HarmonyOS platform driver...");
		DisplayServerHarmonyOS::register_harmonyos_driver();
	}
} g_platform_init;

int harmonyos_godot_init() {
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Initialization START =====");

	if (g_engine_initialized) {
		OH_LOG_WARN(LOG_APP, "Godot engine already initialized");
		return 0;
	}

	// Create native window manager
	g_native_window = new HarmonyOSNativeWindow();

	// Create the OS instance (must exist before Main::setup())
	// Note: OS_HarmonyOS is a singleton and Main::setup will use it
	if (!OS_HarmonyOS::get_singleton() && !g_os_created) {
		// Create OS instance via the proper Godot pattern
		// OS_HarmonyOS will be created by Main::setup() if properly configured
		// For now, ensure the singleton exists
		OS_HarmonyOS *os = memnew(OS_HarmonyOS);
		g_os_created = true;
		OH_LOG_INFO(LOG_APP, "OS_HarmonyOS instance created");
	}

	// Set command-line arguments for Godot Main
	// --editor flag enables the editor mode
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
		// Don't return error - Main might have partially initialized
		// The editor mode requires a project to function
		OH_LOG_WARN(LOG_APP, "Main::setup returned error, continuing...");
	}

	g_engine_initialized = true;
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Initialization DONE =====");
	return 0;
}

int harmonyos_godot_surface_created(const char *surface_id) {
	OH_LOG_INFO(LOG_APP, "Surface created: %{public}s", surface_id);

	if (!g_native_window) {
		OH_LOG_ERROR(LOG_APP, "Native window not initialized");
		return -1;
	}

	if (!g_native_window->initialize_with_xcomponent(nullptr)) {
		// XComponent will be obtained from callback system
		// The surface ID is used to track the XComponent
	}

	// Notify DisplayServer that surface is ready
	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->notify_surface_created();

		// Reset window with the new surface
		ds->reset_window();
	} else {
		OH_LOG_WARN(LOG_APP, "DisplayServer not ready for surface reset");
	}

	return 0;
}

int harmonyos_godot_surface_destroy() {
	OH_LOG_INFO(LOG_APP, "Surface destroyed");

	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->notify_surface_destroyed();
	}

	if (g_native_window) {
		g_native_window->destroy();
	}

	return 0;
}

void harmonyos_godot_cleanup() {
	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup START =====");

	if (g_engine_initialized) {
		Main::cleanup();
		g_engine_initialized = false;
	}

	delete g_native_window;
	g_native_window = nullptr;

	OH_LOG_INFO(LOG_APP, "===== Godot Engine Cleanup DONE =====");
}

void harmonyos_godot_on_pause() {
	OH_LOG_INFO(LOG_APP, "Application paused");
	if (!g_engine_initialized) return;

#if defined(VULKAN_ENABLED)
	DisplayServerHarmonyOS::free_vulkan_global_context();
#endif
}

void harmonyos_godot_on_resume() {
	OH_LOG_INFO(LOG_APP, "Application resumed");
	if (!g_engine_initialized) return;

#if defined(VULKAN_ENABLED)
	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->check_vulkan_global_context(true);
		ds->reset_window();
	}
#endif
}

void harmonyos_godot_on_back_press() {
	// Handle back button - could show exit confirmation
	OH_LOG_INFO(LOG_APP, "Back pressed");
}

void harmonyos_godot_terminate(int exit_code) {
	OH_LOG_INFO(LOG_APP, "Terminate requested with code: %{public}d", exit_code);
	harmonyos_godot_cleanup();
	std::exit(exit_code);
}

// ---- Input event forwarding ----

void harmonyos_godot_key_event(int key_code, int event_type, const char *key_text) {
	if (!g_engine_initialized) return;
	HarmonyOSInput::process_key_event(key_code, event_type, key_text);
}

void harmonyos_godot_mouse_event(int button, int action, double x, double y,
                                  double offset_x, double offset_y) {
	if (!g_engine_initialized) return;
	HarmonyOSInput::process_mouse_event(button, action, x, y, offset_x, offset_y);
}

void harmonyos_godot_touch_event(int touch_id, int action, double x, double y) {
	if (!g_engine_initialized) return;
	HarmonyOSInput::process_touch_event(touch_id, action, x, y);
}

void harmonyos_godot_input_text(const char *text) {
	if (!g_engine_initialized) return;
	HarmonyOSInput::process_input_text(text);
}
