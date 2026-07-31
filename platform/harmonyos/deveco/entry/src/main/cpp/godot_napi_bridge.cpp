/**************************************************************************/
/*  godot_napi_bridge.cpp - Thin NAPI Bridge to libgodot.so               */
/*                                                                        */
/*  MINIMAL TEST: all heavy work (dlopen + init) on pthread.              */
/*  Polling via atomics, no threadsafe_function (avoids NAPI crash).      */
/**************************************************************************/

#include <napi/native_api.h>
#include <hilog/log.h>
#include <dlfcn.h>
#include <pthread.h>
#include <atomic>
#include <string>

#define TAG "GodotNAPI"

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

static napi_ref g_title_callback_ref = nullptr;
static napi_env g_title_callback_env = nullptr;

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
	// harmonyos_godot_init on worker thread (avoids ANR on main)
	int status = g_init_func ? g_init_func() : -1;
	g_init_status.store(status);
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
	size_t argc = 1; napi_value args[1];
	napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
	char id[256] = {0}; size_t len;
	napi_get_value_string_utf8(env, args[0], id, sizeof(id), &len);
	int s = g_surface_created_func ? g_surface_created_func(id) : -1;
	napi_value r; napi_create_int32(env, s, &r); return r;
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
	napi_value o; napi_create_object(env, &o);
	napi_property_descriptor d[] = {
		{"loadLibrary", nullptr, NAPI_LoadLibrary, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"isLibraryLoaded", nullptr, NAPI_IsLibraryLoaded, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"startEngine", nullptr, NAPI_StartEngine, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"cleanup", nullptr, NAPI_Cleanup, nullptr, nullptr, nullptr, napi_default, nullptr},
		{"onSurfaceCreated", nullptr, NAPI_OnSurfaceCreated, nullptr, nullptr, nullptr, napi_default, nullptr},
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
