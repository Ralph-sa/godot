/**************************************************************************/
/*  harmonyos_native_window.cpp - XComponent NativeWindow Implementation  */
/**************************************************************************/

#include "harmonyos_native_window.h"
#include "display_server_harmonyos.h"

#ifdef HARMONYOS_ENABLED

HarmonyOSNativeWindow *HarmonyOSNativeWindow::singleton = nullptr;

bool HarmonyOSNativeWindow::initialize_with_xcomponent(OH_NativeXComponent *p_xcomponent) {
	native_xcomponent_ = p_xcomponent;

	if (!native_xcomponent_) {
		OH_LOG_ERROR(LOG_APP, "Invalid XComponent pointer");
		return false;
	}

	// Register callbacks
	OH_NativeXComponent_Callback callback;
	callback.OnSurfaceCreated = OnSurfaceCreated_CB;
	callback.OnSurfaceChanged = OnSurfaceChanged_CB;
	callback.OnSurfaceDestroyed = OnSurfaceDestroyed_CB;
	callback.DispatchTouchEvent = DispatchTouchEvent_CB;

	OH_NativeXComponent_RegisterCallback(native_xcomponent_, &callback);

	OH_LOG_INFO(LOG_APP, "XComponent initialized with native handle");
	return true;
}

bool HarmonyOSNativeWindow::initialize_with_surface_id(uint64_t p_surface_id) {
	if (native_window_) {
		OH_NativeWindow_DestroyNativeWindow(native_window_);
		native_window_ = nullptr;
	}

	int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(p_surface_id, &native_window_);
	if (ret != 0 || !native_window_) {
		OH_LOG_ERROR(LOG_APP, "[XComponent] CreateNativeWindowFromSurfaceId failed for id %{public}llu, ret=%{public}d",
				(unsigned long long)p_surface_id, ret);
		surface_ready_ = false;
		return false;
	}

	surface_ready_ = true;
	OH_LOG_INFO(LOG_APP, "[XComponent] native window created from surface id: %{public}llu",
			(unsigned long long)p_surface_id);
	return true;
}

void HarmonyOSNativeWindow::destroy() {
	if (native_window_) {
		OH_NativeWindow_DestroyNativeWindow(native_window_);
		native_window_ = nullptr;
	}
	native_xcomponent_ = nullptr;
	surface_ready_ = false;
	OH_LOG_INFO(LOG_APP, "XComponent destroyed");
}

void HarmonyOSNativeWindow::OnSurfaceCreated_CB(OH_NativeXComponent *component, void *window) {
	OH_LOG_INFO(LOG_APP, "[XComponent] OnSurfaceCreated_CB entry window=%{public}p", window);
	if (!singleton || !window) {
		OH_LOG_WARN(LOG_APP, "[XComponent] OnSurfaceCreated_CB ignored (singleton=%{public}p window=%{public}p)",
				(void *)singleton, window);
		return;
	}

	OH_LOG_INFO(LOG_APP, "OnSurfaceCreated_CB");

	singleton->native_window_ = static_cast<OHNativeWindow *>(window);

	// Get initial dimensions
	uint64_t w = 0, h = 0;
	int32_t ret = OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
	OH_LOG_INFO(LOG_APP, "[XComponent] OnSurfaceCreated_CB size ret=%{public}d w=%{public}llu h=%{public}llu",
			(int)ret, (unsigned long long)w, (unsigned long long)h);
	if (ret == 0) {
		singleton->width_ = w;
		singleton->height_ = h;

		// Forward dimensions to DisplayServer so window_size / rect_changed
		// callbacks reflect the real surface geometry.
		DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
		if (ds) {
			ds->update_window_size((int)w, (int)h);
		}
	}

	singleton->surface_ready_ = true;

	OH_LOG_INFO(LOG_APP, "[XComponent] surface ready: %{public}llux%{public}llu",
		(unsigned long long)singleton->width_,
		(unsigned long long)singleton->height_);
}

void HarmonyOSNativeWindow::OnSurfaceChanged_CB(OH_NativeXComponent *component, void *window) {
	if (!singleton || !window) {
		return;
	}

	uint64_t w = 0, h = 0;
	OH_NativeXComponent_GetXComponentSize(component, window, &w, &h);
	singleton->width_ = w;
	singleton->height_ = h;
	OH_LOG_INFO(LOG_APP, "[XComponent] OnSurfaceChanged_CB: %{public}llux%{public}llu",
			(unsigned long long)w, (unsigned long long)h);

	// Forward size change to DisplayServer.
	DisplayServerHarmonyOS *ds = DisplayServerHarmonyOS::get_singleton();
	if (ds) {
		ds->update_window_size((int)w, (int)h);
	}

	OH_LOG_INFO(LOG_APP, "Surface changed: %{public}llux%{public}llu",
		(unsigned long long)w, (unsigned long long)h);
}

void HarmonyOSNativeWindow::OnSurfaceDestroyed_CB(OH_NativeXComponent *component, void *window) {
	if (!singleton) {
		return;
	}

	OH_LOG_INFO(LOG_APP, "OnSurfaceDestroyed_CB");
	singleton->surface_ready_ = false;
	singleton->native_window_ = nullptr;
}

void HarmonyOSNativeWindow::DispatchTouchEvent_CB(OH_NativeXComponent *component, void *window) {
	// Touch events handled via ArkTS layer for better integration
}

#endif // HARMONYOS_ENABLED
