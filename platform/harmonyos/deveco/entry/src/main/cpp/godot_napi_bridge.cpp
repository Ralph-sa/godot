/**************************************************************************/
/*  godot_napi_bridge.cpp - Thin NAPI Bridge to libgodot.so               */
/*                                                                        */
/*  This module loads libgodot.so at runtime and exposes its              */
/*  lifecycle and input functions to ArkTS via NAPI.                      */
/**************************************************************************/

#include <napi/native_api.h>
#include <hilog/log.h>
#include <dlfcn.h>
#include <string>

#define TAG "GodotNAPI"

// Function pointer types exported from libgodot.so
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

// Loaded function pointers (initialized lazily)
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

// ArkTS callback references
static napi_ref g_title_callback_ref = nullptr;
static napi_env g_title_callback_env = nullptr;

static bool load_libgodot() {
	if (g_libgodot) return true;

	OH_LOG_INFO(LOG_APP, "Loading libgodot.so...");
	
	// Try loading from the HAP's native library path
	g_libgodot = dlopen("libgodot.so", RTLD_NOW | RTLD_GLOBAL);
	
	if (!g_libgodot) {
		// Try alternative paths
		g_libgodot = dlopen("libgodot.harmonyos.editor.arm64.so", RTLD_NOW | RTLD_GLOBAL);
	}
	
	if (!g_libgodot) {
		const char *err = dlerror();
		OH_LOG_ERROR(LOG_APP, "Failed to load libgodot.so: %{public}s", err ? err : "unknown");
		return false;
	}
	
	// Resolve function pointers
	g_init_func = (godot_init_t)dlsym(g_libgodot, "harmonyos_godot_init");
	g_cleanup_func = (godot_cleanup_t)dlsym(g_libgodot, "harmonyos_godot_cleanup");
	g_surface_created_func = (godot_surface_created_t)dlsym(g_libgodot, "harmonyos_godot_surface_created");
	g_surface_destroy_func = (godot_surface_destroy_t)dlsym(g_libgodot, "harmonyos_godot_surface_destroy");
	g_key_event_func = (godot_key_event_t)dlsym(g_libgodot, "harmonyos_godot_key_event");
	g_mouse_event_func = (godot_mouse_event_t)dlsym(g_libgodot, "harmonyos_godot_mouse_event");
	g_touch_event_func = (godot_touch_event_t)dlsym(g_libgodot, "harmonyos_godot_touch_event");
	g_input_text_func = (godot_input_text_t)dlsym(g_libgodot, "harmonyos_godot_input_text");
	g_on_pause_func = (godot_on_pause_t)dlsym(g_libgodot, "harmonyos_godot_on_pause");
	g_on_resume_func = (godot_on_resume_t)dlsym(g_libgodot, "harmonyos_godot_on_resume");
	g_on_back_press_func = (godot_on_back_press_t)dlsym(g_libgodot, "harmonyos_godot_on_back_press");
	
	OH_LOG_INFO(LOG_APP, "libgodot.so loaded successfully");
	return true;
}

// ---- NAPI Exported Functions ----

static napi_value NAPI_Init(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_Init");
	
	if (!load_libgodot()) {
		napi_value result;
		napi_create_int32(env, -1, &result);
		return result;
	}
	
	int status = g_init_func ? g_init_func() : -1;
	napi_value result;
	napi_create_int32(env, status, &result);
	return result;
}

static napi_value NAPI_Cleanup(napi_env env, napi_callback_info info) {
	OH_LOG_INFO(LOG_APP, "NAPI_Cleanup");
	
	if (g_cleanup_func) g_cleanup_func();
	
	if (g_libgodot) {
		dlclose(g_libgodot);
		g_libgodot = nullptr;
		g_init_func = nullptr;
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

	// Release old ref if exists
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

// Module registration
EXTERN_C_START
static napi_value GodotModuleInit(napi_env env, napi_value exports) {
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
	napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
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
	OH_LOG_INFO(LOG_APP, "Godot NAPI module registered (dlopen bridge)");
}
