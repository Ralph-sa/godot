/**************************************************************************/
/*  godot_napi_bridge.cpp - Thin NAPI Bridge to libgodot.so               */
/*                                                                        */
/*  This module loads libgodot.so at runtime and exposes its              */
/*  lifecycle and input functions to ArkTS via NAPI.                      */
/*                                                                        */
/*  init() is asynchronous: dlopen runs on a worker thread to prevent     */
/*  ANR from the 151MB SO load. The callback fires on the main thread     */
/*  once loading is complete.                                             */
/**************************************************************************/

#include <napi/native_api.h>
#include <hilog/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <string>
#include <atomic>

#define TAG "GodotNAPI"

// ---- Function pointer types exported from libgodot.so ----
typedef int (*godot_init_t)();
typedef void (*godot_cleanup_t)();
typedef int (*godot_surface_created_t)(const char *);
typedef int (*godot_surface_destroy_t)();
typedef void (*godot_key_event_t)(int, int, const char *);
typedef void (*godot_mouse_event_t)(int, int, double, double, double, double);
typedef void (*godot_touch_event_t)(int, int, double, double);
typedef void (*godot_input_text_t)(const char *);
typedef void (*godot_on_pause_t)();
typedef void (*godot_on_resume_t)();
typedef void (*godot_on_back_press_t)();

// ---- Loaded function pointers (initialized lazily) ----
static void *g_libgodot = nullptr;
static godot_init_t g_init_func = nullptr;
static godot_cleanup_t g_cleanup_func = nullptr;
static godot_surface_created_t g_surface_created_func = nullptr;
static godot_surface_destroy_t g_surface_destroy_func = nullptr;
static godot_key_event_t g_key_event_func = nullptr;
static godot_mouse_event_t g_mouse_event_func = nullptr;
static godot_touch_event_t g_touch_event_func = nullptr;
static godot_input_text_t g_input_text_func = nullptr;
static godot_on_pause_t g_on_pause_func = nullptr;
static godot_on_resume_t g_on_resume_func = nullptr;
static godot_on_back_press_t g_on_back_press_func = nullptr;

// ---- ArkTS callback references ----
static napi_ref g_title_callback_ref = nullptr;
static napi_env g_title_callback_env = nullptr;

// ---- Async init state ----
static std::atomic<bool> g_init_in_progress(false);
static napi_threadsafe_function g_init_tsfn = nullptr;
static napi_env g_init_env = nullptr;
static napi_ref g_init_callback_ref = nullptr;

// ---- dlopen + dlsym (may be called from worker thread) ----
static bool load_libgodot() {
	if (g_libgodot) return true;

	OH_LOG_INFO(LOG_APP, "Loading libgodot.so...");

	g_libgodot = dlopen("libgodot.so", RTLD_NOW | RTLD_GLOBAL);

	if (!g_libgodot) {
		g_libgodot = dlopen("libgodot.harmonyos.editor.arm64.so", RTLD_NOW | RTLD_GLOBAL);
	}

	if (!g_libgodot) {
		const char *err = dlerror();
		OH_LOG_ERROR(LOG_APP, "Failed to load libgodot.so: %{public}s", err ? err : "unknown");
		return false;
	}

	// Resolve all function pointers
	g_init_func              = (godot_init_t)dlsym(g_libgodot, "harmonyos_godot_init");
	g_cleanup_func           = (godot_cleanup_t)dlsym(g_libgodot, "harmonyos_godot_cleanup");
	g_surface_created_func   = (godot_surface_created_t)dlsym(g_libgodot, "harmonyos_godot_surface_created");
	g_surface_destroy_func   = (godot_surface_destroy_t)dlsym(g_libgodot, "harmonyos_godot_surface_destroy");
	g_key_event_func         = (godot_key_event_t)dlsym(g_libgodot, "harmonyos_godot_key_event");
	g_mouse_event_func       = (godot_mouse_event_t)dlsym(g_libgodot, "harmonyos_godot_mouse_event");
	g_touch_event_func       = (godot_touch_event_t)dlsym(g_libgodot, "harmonyos_godot_touch_event");
	g_input_text_func        = (godot_input_text_t)dlsym(g_libgodot, "harmonyos_godot_input_text");
	g_on_pause_func          = (godot_on_pause_t)dlsym(g_libgodot, "harmonyos_godot_on_pause");
	g_on_resume_func         = (godot_on_resume_t)dlsym(g_libgodot, "harmonyos_godot_on_resume");
	g_on_back_press_func     = (godot_on_back_press_t)dlsym(g_libgodot, "harmonyos_godot_on_back_press");

	OH_LOG_INFO(LOG_APP, "libgodot.so loaded successfully");
	return true;
}

// ===================================================================
//  Async Init: Worker thread loads SO, main thread runs Main::setup
// ===================================================================

struct InitWorkerData {
	int load_result;   // 0 = dlopen success, -1 = dlopen failure
};

// This runs on the MAIN THREAD after worker thread finishes dlopen.
// It calls g_init_func() (harmonyos_godot_init → Main::setup) and
// then invokes the ArkTS callback with the result.
static void InitCallJs(napi_env env, napi_value js_callback, void *context, void *data) {
	InitWorkerData *wd = static_cast<InitWorkerData *>(data);

	int status = -1;
	if (wd && wd->load_result == 0 && g_init_func) {
		OH_LOG_INFO(LOG_APP, "Running harmonyos_godot_init on main thread...");
		status = g_init_func();
		OH_LOG_INFO(LOG_APP, "harmonyos_godot_init returned: %{public}d", status);
	} else {
		OH_LOG_ERROR(LOG_APP, "Skipping engine init: SO load failed or init func missing");
	}

	// Call the ArkTS callback with the status code
	napi_value callback;
	napi_get_reference_value(g_init_env, g_init_callback_ref, &callback);

	napi_value args[1];
	napi_create_int32(env, status, &args[0]);

	napi_value global;
	napi_get_global(env, &global);
	napi_call_function(env, global, callback, 1, args, nullptr);

	// Release the callback reference
	if (g_init_callback_ref) {
		napi_delete_reference(g_init_env, g_init_callback_ref);
		g_init_callback_ref = nullptr;
	}
	g_init_env = nullptr;

	g_init_in_progress.store(false, std::memory_order_release);

	delete wd;
}

// Worker thread function: loads libgodot.so (151MB IO), then signals main thread.
// The tsfn release must happen HERE (after napi_call_threadsafe_function returns)
// to ensure the tsfn lives until the callback is queued.
static void *init_worker_thread(void *arg) {
	OH_LOG_INFO(LOG_APP, "Worker thread: starting dlopen of libgodot.so...");

	InitWorkerData *wd = new InitWorkerData();
	wd->load_result = load_libgodot() ? 0 : -1;

	OH_LOG_INFO(LOG_APP, "Worker thread: dlopen finished, result=%{public}d, signaling main thread",
	            wd->load_result);

	napi_call_threadsafe_function(g_init_tsfn, wd, napi_tsfn_blocking);

	// Release our ownership ref; the callback's internal ref keeps tsfn alive
	// until InitCallJs completes.
	napi_release_threadsafe_function(g_init_tsfn, napi_tsfn_release);
	g_init_tsfn = nullptr;

	return nullptr;
}

// ===================================================================
//  NAPI_Init: asynchronous version
//  Signature: init(callback: (result: number) => void): void
// ===================================================================
static napi_value NAPI_Init(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_Init (async)");

	// Prevent double-init
	bool expected = false;
	if (!g_init_in_progress.compare_exchange_strong(expected, true)) {
		OH_LOG_WARN(LOG_APP, "Init already in progress, ignoring duplicate call");
		napi_value result;
		napi_create_int32(env, -2, &result);
		return result;
	}

	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	// Validate: first argument must be a function
	napi_valuetype type;
	napi_typeof(env, args[0], &type);
	if (type != napi_function) {
		OH_LOG_ERROR(LOG_APP, "NAPI_Init: expected a callback function");
		g_init_in_progress.store(false, std::memory_order_release);
		napi_value result;
		napi_create_int32(env, -1, &result);
		return result;
	}

	// Store the callback reference
	g_init_env = env;
	napi_create_reference(env, args[0], 1, &g_init_callback_ref);

	// Create a threadsafe function for cross-thread communication
	napi_value resource_name;
	napi_create_string_utf8(env, "godot_init_worker", NAPI_AUTO_LENGTH, &resource_name);

	napi_status tsfn_status = napi_create_threadsafe_function(
		env,
		args[0],           // js_callback (used as hint; actual callback via ref)
		nullptr,           // async_resource
		resource_name,     // async_resource_name
		1,                 // max_queue_size
		1,                 // initial_thread_count
		nullptr,           // thread_finalize_data
		nullptr,           // thread_finalize_cb
		nullptr,           // context
		InitCallJs,        // call_js_cb (runs on main thread)
		&g_init_tsfn       // result
	);

	if (tsfn_status != napi_ok) {
		OH_LOG_ERROR(LOG_APP, "Failed to create threadsafe function");
		napi_delete_reference(env, g_init_callback_ref);
		g_init_callback_ref = nullptr;
		g_init_env = nullptr;
		g_init_in_progress.store(false, std::memory_order_release);
		napi_value result;
		napi_create_int32(env, -1, &result);
		return result;
	}

	// Spawn worker thread to perform dlopen
	pthread_t thread;
	int pthread_rc = pthread_create(&thread, nullptr, init_worker_thread, nullptr);
	if (pthread_rc != 0) {
		OH_LOG_ERROR(LOG_APP, "Failed to create worker thread: %{public}d", pthread_rc);
		napi_release_threadsafe_function(g_init_tsfn, napi_tsfn_release);
		g_init_tsfn = nullptr;
		napi_delete_reference(env, g_init_callback_ref);
		g_init_callback_ref = nullptr;
		g_init_env = nullptr;
		g_init_in_progress.store(false, std::memory_order_release);
		napi_value result;
		napi_create_int32(env, -1, &result);
		return result;
	}

	pthread_detach(thread);
	OH_LOG_INFO(LOG_APP, "Worker thread spawned for libgodot.so loading");

	// Worker thread will release its tsfn ref after napi_call_threadsafe_function
	// returns. We must NOT release here to avoid premature destruction.

	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

// ---- Other NAPI Functions (unchanged, synchronous) ----

static napi_value NAPI_Cleanup(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_Cleanup");

	if (g_cleanup_func) g_cleanup_func();

	if (g_libgodot) {
		dlclose(g_libgodot);
		g_libgodot = nullptr;
		g_init_func = nullptr;
		g_cleanup_func = nullptr;
		g_surface_created_func = nullptr;
		g_surface_destroy_func = nullptr;
		g_key_event_func = nullptr;
		g_mouse_event_func = nullptr;
		g_touch_event_func = nullptr;
		g_input_text_func = nullptr;
		g_on_pause_func = nullptr;
		g_on_resume_func = nullptr;
		g_on_back_press_func = nullptr;
	}

	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_OnSurfaceCreated(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	char surfaceId[256] = {0};
	size_t len = 0;
	napi_get_value_string_utf8(env, args[0], surfaceId, sizeof(surfaceId), &len);

	OH_LOG_INFO(LOG_APP, "NAPI_OnSurfaceCreated: %{public}s", surfaceId);

	int status = g_surface_created_func ? g_surface_created_func(surfaceId) : -1;

	napi_value result;
	napi_create_int32(env, status, &result);
	return result;
}

static napi_value NAPI_OnSurfaceDestroy(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_OnSurfaceDestroy");

	int status = g_surface_destroy_func ? g_surface_destroy_func() : -1;

	napi_value result;
	napi_create_int32(env, status, &result);
	return result;
}

static napi_value NAPI_SendKeyEvent(napi_env env, napi_callback_info info) {
	if (!g_key_event_func) {
		napi_value r;
		napi_create_int32(env, -1, &r);
		return r;
	}

	size_t argc = 3;
	napi_value args[3];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	int32_t keyCode = 0, eventType = 0;
	char keyText[64] = {0};
	napi_get_value_int32(env, args[0], &keyCode);
	napi_get_value_int32(env, args[1], &eventType);
	size_t textLen;
	napi_get_value_string_utf8(env, args[2], keyText, sizeof(keyText), &textLen);

	g_key_event_func(keyCode, eventType, keyText);

	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_SendMouseEvent(napi_env env, napi_callback_info info) {
	if (!g_mouse_event_func) {
		napi_value r;
		napi_create_int32(env, -1, &r);
		return r;
	}

	size_t argc = 6;
	napi_value args[6];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	int32_t button = 0, action = 0;
	double x = 0, y = 0, offsetX = 0, offsetY = 0;
	napi_get_value_int32(env, args[0], &button);
	napi_get_value_int32(env, args[1], &action);
	napi_get_value_double(env, args[2], &x);
	napi_get_value_double(env, args[3], &y);
	napi_get_value_double(env, args[4], &offsetX);
	napi_get_value_double(env, args[5], &offsetY);

	g_mouse_event_func(button, action, x, y, offsetX, offsetY);

	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_SendTouchEvent(napi_env env, napi_callback_info info) {
	if (!g_touch_event_func) {
		napi_value r;
		napi_create_int32(env, -1, &r);
		return r;
	}

	size_t argc = 4;
	napi_value args[4];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	int32_t touchId = 0, action = 0;
	double x = 0, y = 0;
	napi_get_value_int32(env, args[0], &touchId);
	napi_get_value_int32(env, args[1], &action);
	napi_get_value_double(env, args[2], &x);
	napi_get_value_double(env, args[3], &y);

	g_touch_event_func(touchId, action, x, y);

	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_SendInputText(napi_env env, napi_callback_info info) {
	if (!g_input_text_func) {
		napi_value r;
		napi_create_int32(env, -1, &r);
		return r;
	}

	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	char text[256] = {0};
	size_t len = 0;
	napi_get_value_string_utf8(env, args[0], text, sizeof(text), &len);

	g_input_text_func(text);

	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_OnPause(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_OnPause");
	if (g_on_pause_func) g_on_pause_func();
	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_OnResume(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_OnResume");
	if (g_on_resume_func) g_on_resume_func();
	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_OnBackPress(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_OnBackPress");
	if (g_on_back_press_func) g_on_back_press_func();
	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

static napi_value NAPI_RegisterTitleCallback(napi_env env, napi_callback_info info) {
	size_t argc = 1;
	napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

	napi_valuetype type;
	napi_typeof(env, args[0], &type);
	if (type != napi_function) {
		OH_LOG_ERROR(LOG_APP, "NAPI_RegisterTitleCallback: expected a function");
		napi_value result;
		napi_create_int32(env, -1, &result);
		return result;
	}

	if (g_title_callback_ref) {
		napi_delete_reference(g_title_callback_env, g_title_callback_ref);
	}

	g_title_callback_env = env;
	napi_create_reference(env, args[0], 1, &g_title_callback_ref);

	OH_LOG_INFO(LOG_APP, "NAPI_RegisterTitleCallback: registered");
	napi_value result;
	napi_create_int32(env, 0, &result);
	return result;
}

// Called from C++ side (DisplayServer) to update the ArkTS window title
extern "C" void harmonyos_notify_window_title(const char *title) {
	if (!g_title_callback_ref || !g_title_callback_env || !title) {
		return;
	}

	napi_value callback;
	napi_get_reference_value(g_title_callback_env, g_title_callback_ref, &callback);

	napi_value global;
	napi_get_global(g_title_callback_env, &global);

	napi_value arg;
	napi_create_string_utf8(g_title_callback_env, title, NAPI_AUTO_LENGTH, &arg);

	napi_call_function(g_title_callback_env, global, callback, 1, &arg, nullptr);
}

// ---- Module registration ----
EXTERN_C_START
static napi_value GodotModuleInit(napi_env env, napi_value exports) {
	napi_value godot_napi_obj;
	napi_create_object(env, &godot_napi_obj);

	napi_property_descriptor desc[] = {
		{"init",                  nullptr, NAPI_Init,                  nullptr, nullptr, nullptr, napi_default, nullptr},
		{"cleanup",               nullptr, NAPI_Cleanup,               nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onSurfaceCreated",      nullptr, NAPI_OnSurfaceCreated,      nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onSurfaceDestroy",      nullptr, NAPI_OnSurfaceDestroy,      nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendKeyEvent",          nullptr, NAPI_SendKeyEvent,          nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendMouseEvent",        nullptr, NAPI_SendMouseEvent,        nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendTouchEvent",        nullptr, NAPI_SendTouchEvent,        nullptr, nullptr, nullptr, napi_default, nullptr},
		{"sendInputText",         nullptr, NAPI_SendInputText,         nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onPause",               nullptr, NAPI_OnPause,               nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onResume",              nullptr, NAPI_OnResume,              nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onBackPress",           nullptr, NAPI_OnBackPress,           nullptr, nullptr, nullptr, napi_default, nullptr},
		{"registerTitleCallback", nullptr, NAPI_RegisterTitleCallback, nullptr, nullptr, nullptr, napi_default, nullptr},
	};
	napi_define_properties(env, godot_napi_obj, sizeof(desc) / sizeof(desc[0]), desc);

	napi_set_named_property(env, exports, "godot_napi", godot_napi_obj);
	return exports;
}
EXTERN_C_END

static napi_module godotModule = {
	.nm_version = 1,
	.nm_flags = 0,
	.nm_filename = nullptr,
	.nm_register_func = GodotModuleInit,
	.nm_modname = "godot_napi",
	.nm_priv = nullptr,
	.reserved = {nullptr},
};

extern "C" __attribute__((constructor)) void RegisterGodotNAPI() {
	napi_module_register(&godotModule);
	OH_LOG_INFO(LOG_APP, "Godot NAPI module registered (async dlopen bridge)");
}
