/**************************************************************************/
/*  harmonyos_native_window.h - XComponent NativeWindow Bridge            */
/**************************************************************************/

#pragma once

#ifdef HARMONYOS_ENABLED

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <arkui/ui_input_event.h>
#include <native_window/external_window.h>
#include "harmonyos_log.h"

class HarmonyOSNativeWindow {
public:
	static HarmonyOSNativeWindow *singleton;

	HarmonyOSNativeWindow() { singleton = this; }
	~HarmonyOSNativeWindow() { singleton = nullptr; }

	// Called from NAPI bridge when ArkTS provides the XComponent
	bool initialize_with_xcomponent(OH_NativeXComponent *p_xcomponent);

	// Called from NAPI bridge when ArkTS provides the numeric surface id
	// (ArkTS XComponent scenario). Creates the OHNativeWindow directly from
	// the surface id, avoiding the dependency on OH_NativeXComponent callbacks
	// and the libraryname injection mechanism.
	bool initialize_with_surface_id(uint64_t p_surface_id);

	void destroy();

	bool is_surface_ready() const { return surface_ready_; }
	OHNativeWindow *get_native_window() const { return native_window_; }
	uint64_t get_width() const { return width_; }
	uint64_t get_height() const { return height_; }

	// XComponent lifecycle callbacks
	static void OnSurfaceCreated_CB(OH_NativeXComponent *component, void *window);
	static void OnSurfaceChanged_CB(OH_NativeXComponent *component, void *window);
	static void OnSurfaceDestroyed_CB(OH_NativeXComponent *component, void *window);
	static void DispatchTouchEvent_CB(OH_NativeXComponent *component, void *window);

	// Mouse wheel / axis input, registered via RegisterUIInputEventCallback.
	static void DispatchAxisEvent_CB(OH_NativeXComponent *component,
			ArkUI_UIInputEvent *event, ArkUI_UIInputEvent_Type type);

private:
	OH_NativeXComponent *native_xcomponent_ = nullptr;
	OHNativeWindow *native_window_ = nullptr;
	uint64_t width_ = 0;
	uint64_t height_ = 0;
	bool surface_ready_ = false;
};

#endif // HARMONYOS_ENABLED
