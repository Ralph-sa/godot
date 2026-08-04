/**************************************************************************/
/*  harmonyos_native_window.cpp - XComponent NativeWindow Implementation  */
/**************************************************************************/

#include "harmonyos_native_window.h"
#include "display_server_harmonyos.h"
#include "harmonyos_input.h"

#ifdef HARMONYOS_ENABLED

HarmonyOSNativeWindow *HarmonyOSNativeWindow::singleton = nullptr;

bool HarmonyOSNativeWindow::initialize_with_xcomponent(OH_NativeXComponent *p_xcomponent) {
	if (!p_xcomponent) {
		OH_LOG_ERROR(LOG_APP, "Invalid XComponent pointer");
		return false;
	}

	// Surface ownership intentionally stays on the numeric surface-id path.
	// Registering OH_NativeXComponent lifecycle callbacks here would introduce
	// a second OHNativeWindow source and let an old callback overwrite the
	// current handle. The native XComponent is used only for input events that
	// ArkTS cannot expose with a real delta.
	//
	// The mouse wheel does not surface through ArkTS onMouse, and ArkTS
	// AxisEvent only exposes the step configuration rather than the actual
	// scroll delta. The native axis callback is the only path that carries
	// both direction and magnitude.
	int32_t axis_ret = OH_NativeXComponent_RegisterUIInputEventCallback(
			p_xcomponent, DispatchAxisEvent_CB, ARKUI_UIINPUTEVENT_TYPE_AXIS);
	if (axis_ret != 0) {
		OH_LOG_WARN(LOG_APP, "Axis event callback registration failed (ret=%{public}d), mouse wheel will not work",
				axis_ret);
		return false;
	}

	native_xcomponent_ = p_xcomponent;
	OH_LOG_INFO(LOG_APP, "XComponent initialized with native handle");
	return true;
}

void HarmonyOSNativeWindow::DispatchAxisEvent_CB(OH_NativeXComponent *component,
		ArkUI_UIInputEvent *event, ArkUI_UIInputEvent_Type type) {
	if (!event || type != ARKUI_UIINPUTEVENT_TYPE_AXIS) {
		return;
	}

	// BEGIN/UPDATE carry scroll deltas; END/CANCEL only close the gesture and
	// would otherwise emit a spurious zero-delta wheel click.
	int32_t action = OH_ArkUI_UIInputEvent_GetAction(event);
	if (action != UI_AXIS_EVENT_ACTION_BEGIN && action != UI_AXIS_EVENT_ACTION_UPDATE) {
		return;
	}

	double vertical = OH_ArkUI_AxisEvent_GetVerticalAxisValue(event);
	double horizontal = OH_ArkUI_AxisEvent_GetHorizontalAxisValue(event);
	if (vertical == 0.0 && horizontal == 0.0) {
		return;
	}

	float x = OH_ArkUI_PointerEvent_GetX(event);
	float y = OH_ArkUI_PointerEvent_GetY(event);

	HarmonyOSInput::process_mouse_scroll_event(x, y, horizontal, vertical);
}

bool HarmonyOSNativeWindow::initialize_with_surface_id(uint64_t p_surface_id, uint64_t p_width, uint64_t p_height) {
	if (p_surface_id == 0 || p_width == 0 || p_height == 0) {
		OH_LOG_ERROR(LOG_APP, "[XComponent] Invalid surface metadata id=%{public}llu size=%{public}llux%{public}llu",
				(unsigned long long)p_surface_id, (unsigned long long)p_width, (unsigned long long)p_height);
		return false;
	}

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

	width_ = p_width;
	height_ = p_height;
	surface_ready_ = true;
	OH_LOG_INFO(LOG_APP, "[XComponent] native window created from surface id: %{public}llu size=%{public}llux%{public}llu",
			(unsigned long long)p_surface_id, (unsigned long long)width_, (unsigned long long)height_);
	return true;
}

void HarmonyOSNativeWindow::update_surface_size(uint64_t p_width, uint64_t p_height) {
	if (p_width == 0 || p_height == 0) {
		return;
	}
	width_ = p_width;
	height_ = p_height;
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

#endif // HARMONYOS_ENABLED
