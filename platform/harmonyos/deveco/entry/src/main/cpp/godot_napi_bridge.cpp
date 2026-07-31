/**************************************************************************/
/*  godot_napi_bridge.cpp - Thin NAPI Bridge to libgodot.so               */
/*                                                                        */
/*  MINIMAL TEST: all heavy work (dlopen + init) on pthread.              */
/*  Polling via atomics, no threadsafe_function (avoids NAPI crash).      */
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
#include <dlfcn.h>
#include <pthread.h>
#include <atomic>
#include <string>

typedef int (*godot_init_t)();
typedef void (*godot_start_t)();
typedef void (*godot_cleanup_t)();
typedef int (*godot_surface_created_t)(const char *, int, int);
typedef int (*godot_surface_destroy_t)();
typedef void (*godot_key_event_t)(int, int, const char *);
typedef void (*godot_mouse_event_t)(int, int, double, double, double, double);
typedef void (*godot_touch_event_t)(int, int, double, double);
typedef void (*godot_input_text_t)(const char *);
typedef void (*godot_on_pause_t)();
typedef void (*godot_on_resume_t)();
typedef void (*godot_on_back_press_t)();

static void *g_libgodot = nullptr;
static godot_init_t g_init_func = nullptr;
static godot_start_t g_start_func = nullptr;
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

static napi_ref g_title_callback_ref = nullptr;
static napi_env g_title_callback_env = nullptr;

// The XComponent native handle, captured at module init from the exports
// object's OH_NATIVE_XCOMPONENT_OBJ property. The XComponent `libraryname`
// mechanism injects this property into the module's exports when the
// component is created on the ArkTS side.
static OH_NativeXComponent *g_xcomponent = nullptr;
static napi_ref g_module_exports_ref = nullptr;
static napi_env g_module_env = nullptr;

// ---- Async init ----
static std::atomic<int> g_init_status{-1};

static bool load_libgodot() {
	if (g_libgodot) return true;
	g_libgodot = dlopen("libgodot.so", RTLD_NOW | RTLD_GLOBAL);
	if (!g_libgodot) {
		g_libgodot = dlopen("libgodot.harmonyos.editor.arm64.so", RTLD_NOW | RTLD_GLOBAL);
	}
	if (!g_libgodot) {
		return false;
	}
	g_init_func              = (godot_init_t)dlsym(g_libgodot, "harmonyos_godot_init");
	g_start_func             = (godot_start_t)dlsym(g_libgodot, "harmonyos_godot_start");
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
	return true;
}

static void *init_worker(void *) {
	bool ok = load_libgodot();
	if (!ok) {
		g_init_status.store(-2);
		return nullptr;
	}
	// harmonyos_godot_init on worker thread (avoids ANR on main).
	// Phase 1: engine core setup (Main::setup) — returns quickly so the
	// ArkTS polling loop can switch to the GodotSurface page.
	int status = g_init_func ? g_init_func() : -1;
	g_init_status.store(status);

	// Phase 2: main loop + frame loop, still on this engine thread.
	// harmonyos_godot_start() waits for the XComponent surface, then calls
	// Main::start() and iterates until cleanup. It returns only after the
	// engine has fully shut down.
	if (status == 0 && g_start_func) {
		g_start_func();
	}
	return nullptr;
}

// ---- NAPI ----

static napi_value NAPI_LoadLibrary(napi_env env, napi_callback_info info) {
	if (g_init_status.load() >= 0) {
		napi_value r; napi_create_int32(env, 0, &r); return r;
	}
	pthread_t t;
	pthread_create(&t, nullptr, init_worker, nullptr);
	pthread_detach(t);
	napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_IsLibraryLoaded(napi_env env, napi_callback_info info) {
	int s = g_init_status.load();
	napi_value r;
	napi_create_int32(env, s >= 0 ? 1 : (s == -2 ? -1 : 0), &r);
	return r;
}

static napi_value NAPI_StartEngine(napi_env env, napi_callback_info info) {
	int s = g_init_status.load();
	napi_value r; napi_create_int32(env, s, &r); return r;
}

static napi_value NAPI_Cleanup(napi_env env, napi_callback_info info) {
	if (g_cleanup_func) g_cleanup_func();
	if (g_libgodot) { dlclose(g_libgodot); g_libgodot = nullptr; }
	napi_value r; napi_create_int32(env, 0, &r); return r;
}

static napi_value NAPI_OnSurfaceCreated(napi_env env, napi_callback_info info) {
	size_t argc = 3; napi_value args[3];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	char id[256] = {0}; size_t len;
	int32_t w = 0, h = 0;
	if (argc >= 1 && args[0]) {
		napi_get_value_string_utf8(env, args[0], id, sizeof(id), &len);
	}
	if (argc >= 3 && args[1] && args[2]) {
		napi_get_value_int32(env, args[1], &w);
		napi_get_value_int32(env, args[2], &h);
	}
	int s = g_surface_created_func ? g_surface_created_func(id, (int)w, (int)h) : -1;
	napi_value r; napi_create_int32(env, s, &r); return r;
}

// Attempt to capture the OH_NativeXComponent from the module exports object.
// Returns true on success.
static bool capture_xcomponent(napi_env env) {
	if (g_xcomponent != nullptr) {
		return true;
	}
	if (g_module_exports_ref == nullptr) {
		return false;
	}
	napi_value exports = nullptr;
	if (napi_get_reference_value(env, g_module_exports_ref, &exports) != napi_ok || exports == nullptr) {
		return false;
	}
	napi_value export_instance = nullptr;
	if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &export_instance) != napi_ok) {
		return false;
	}
	OH_NativeXComponent *xcomponent = nullptr;
	if (napi_unwrap(env, export_instance, reinterpret_cast<void **>(&xcomponent)) != napi_ok || xcomponent == nullptr) {
		return false;
	}
	g_xcomponent = xcomponent;
	OH_LOG_INFO(LOG_APP, "capture_xcomponent: captured native XComponent from exports");
	return true;
}

// Called from ArkTS XComponent.onLoad. The `libraryname` mechanism normally
// injects the OH_NativeXComponent into the module exports at init time; this
// is a fallback retry in case the injection happens later (onLoad ordering).
static napi_value NAPI_InitXComponent(napi_env env, napi_callback_info info) {
	capture_xcomponent(env);
	napi_value r; napi_create_int32(env, g_xcomponent ? 0 : -1, &r); return r;
}

extern "C" OH_NativeXComponent *harmonyos_get_xcomponent() {
	return g_xcomponent;
}

static napi_value NAPI_OnSurfaceDestroy(napi_env env, napi_callback_info info) {
	int s = g_surface_destroy_func ? g_surface_destroy_func() : -1;
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
	size_t argc = 1; napi_value a[1]; napi_get_cb_info(env, info, &argc, a, nullptr, nullptr);
	napi_valuetype t; napi_typeof(env, a[0], &t);
	if (t != napi_function) { napi_value r; napi_create_int32(env, -1, &r); return r; }
	if (g_title_callback_ref) napi_delete_reference(g_title_callback_env, g_title_callback_ref);
	g_title_callback_env = env; napi_create_reference(env, a[0], 1, &g_title_callback_ref);
	napi_value r; napi_create_int32(env, 0, &r); return r;
}

extern "C" void harmonyos_notify_window_title(const char *title) {
	if (!g_title_callback_ref || !g_title_callback_env || !title) return;
	napi_value cb; napi_get_reference_value(g_title_callback_env, g_title_callback_ref, &cb);
	napi_value global; napi_get_global(g_title_callback_env, &global);
	napi_value arg; napi_create_string_utf8(g_title_callback_env, title, NAPI_AUTO_LENGTH, &arg);
	napi_call_function(g_title_callback_env, global, cb, 1, &arg, nullptr);
}

EXTERN_C_START
static napi_value GodotModuleInit(napi_env env, napi_value exports) {
	// Keep a reference to the exports object so onLoad can retry the
	// XComponent capture if it wasn't injected yet at module init time.
	g_module_env = env;
	if (g_module_exports_ref == nullptr) {
		napi_create_reference(env, exports, 1, &g_module_exports_ref);
	}
	if (capture_xcomponent(env)) {
		OH_LOG_INFO(LOG_APP, "GodotModuleInit: native XComponent captured at init");
	} else {
		OH_LOG_INFO(LOG_APP, "GodotModuleInit: XComponent not yet available, will retry on onLoad");
	}

	napi_value o; napi_create_object(env, &o);
	napi_property_descriptor d[] = {
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
