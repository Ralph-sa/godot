/**************************************************************************/
/*  display_server_harmonyos.cpp - DisplayServer Implementation           */
/**************************************************************************/

#include "display_server_harmonyos.h"

#include "harmonyos_native_window.h"
#include "os_harmonyos.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"

#ifdef VULKAN_ENABLED
#include "rendering_context_driver_vulkan_harmonyos.h"
#endif

static RenderingContextDriver *rendering_context_global = nullptr;
static bool rendering_context_global_checked = false;

DisplayServerHarmonyOS *DisplayServerHarmonyOS::get_singleton() {
	return static_cast<DisplayServerHarmonyOS *>(DisplayServer::get_singleton());
}

bool DisplayServerHarmonyOS::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_GLOBAL_MENU:
		case DisplayServerEnums::FEATURE_HIDPI:
		case DisplayServerEnums::FEATURE_SWAP_BUFFERS:
		case DisplayServerEnums::FEATURE_KEEP_SCREEN_ON:
		case DisplayServerEnums::FEATURE_CLIPBOARD:
		case DisplayServerEnums::FEATURE_CURSOR_SHAPE:
		case DisplayServerEnums::FEATURE_CUSTOM_CURSOR_SHAPE:
		case DisplayServerEnums::FEATURE_MOUSE:
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
		case DisplayServerEnums::FEATURE_NATIVE_DIALOG:
		case DisplayServerEnums::FEATURE_IME:
			return true;
		default:
			return false;
	}
}

String DisplayServerHarmonyOS::get_name() const {
	return "HarmonyOS";
}

// ---- clipboard ----

void DisplayServerHarmonyOS::clipboard_set(const String &p_text) {
	// Stub: OH pasteboard service (Phase 5)
}

String DisplayServerHarmonyOS::clipboard_get() const {
	return String();
}

// ---- screen ----

int DisplayServerHarmonyOS::get_screen_count() const {
	return 1;
}

int DisplayServerHarmonyOS::get_primary_screen() const {
	return 0;
}

Point2i DisplayServerHarmonyOS::screen_get_position(int p_screen) const {
	return Point2i();
}

Size2i DisplayServerHarmonyOS::screen_get_size(int p_screen) const {
	return window_size;
}

Rect2i DisplayServerHarmonyOS::screen_get_usable_rect(int p_screen) const {
	return Rect2i(0, 0, window_size.width, window_size.height);
}

int DisplayServerHarmonyOS::screen_get_dpi(int p_screen) const {
	return screen_dpi_val;
}

float DisplayServerHarmonyOS::screen_get_scale(int p_screen) const {
	return screen_scale_val;
}

float DisplayServerHarmonyOS::screen_get_refresh_rate(int p_screen) const {
	return screen_refresh_rate_val;
}

void DisplayServerHarmonyOS::screen_set_keep_on(bool p_enable) {
	keep_screen_on = p_enable;
}

bool DisplayServerHarmonyOS::screen_is_kept_on() const {
	return keep_screen_on;
}

bool DisplayServerHarmonyOS::is_touchscreen_available() const {
	return true;
}

// ---- window callbacks ----

template <typename... Args>
void DisplayServerHarmonyOS::_window_callback(const Callable &p_callable, bool p_deferred, const Args &...p_rest_args) const {
	if (!p_callable.is_valid()) {
		return;
	}
	p_callable.call(p_rest_args...);
}

void DisplayServerHarmonyOS::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerHarmonyOS *ds = get_singleton();
	if (ds) {
		ds->send_input_event(p_event);
	}
}

void DisplayServerHarmonyOS::window_set_window_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	window_event_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_input_event_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_input_text_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	input_text_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_rect_changed_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	rect_changed_callback = p_callable;
}

void DisplayServerHarmonyOS::window_set_drop_files_callback(const Callable &p_callable, DisplayServerEnums::WindowID p_window) {
	// Not supported
}

void DisplayServerHarmonyOS::send_window_event(DisplayServerEnums::WindowEvent p_event, bool p_deferred) const {
	_window_callback(window_event_callback, p_deferred, window_id, int64_t(p_event));
}

void DisplayServerHarmonyOS::send_input_event(const Ref<InputEvent> &p_event) const {
	_window_callback(input_event_callback, false, p_event, window_id);
}

void DisplayServerHarmonyOS::send_input_text(const String &p_text) const {
	_window_callback(input_text_callback, false, p_text, window_id);
}

// ---- window management ----

Vector<DisplayServerEnums::WindowID> DisplayServerHarmonyOS::get_window_list() const {
	Vector<DisplayServerEnums::WindowID> ret;
	ret.push_back(window_id);
	return ret;
}

DisplayServerEnums::WindowID DisplayServerHarmonyOS::get_window_at_screen_position(const Point2i &p_position) const {
	return window_id;
}

void DisplayServerHarmonyOS::window_attach_instance_id(ObjectID p_instance, DisplayServerEnums::WindowID p_window) {
	// Not supported - handled via ArkTS
}

ObjectID DisplayServerHarmonyOS::window_get_attached_instance_id(DisplayServerEnums::WindowID p_window) const {
	return ObjectID();
}

void DisplayServerHarmonyOS::window_set_title(const String &p_title, DisplayServerEnums::WindowID p_window) {
	// NAPI call to ArkTS (Phase 5)
}

int DisplayServerHarmonyOS::window_get_current_screen(DisplayServerEnums::WindowID p_window) const {
	return 0;
}

void DisplayServerHarmonyOS::window_set_current_screen(int p_screen, DisplayServerEnums::WindowID p_window) {
}

Point2i DisplayServerHarmonyOS::window_get_position(DisplayServerEnums::WindowID p_window) const {
	return Point2i();
}

Point2i DisplayServerHarmonyOS::window_get_position_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return Point2i();
}

void DisplayServerHarmonyOS::window_set_position(const Point2i &p_position, DisplayServerEnums::WindowID p_window) {
}

void DisplayServerHarmonyOS::window_set_transient(DisplayServerEnums::WindowID p_window, DisplayServerEnums::WindowID p_parent) {
}

void DisplayServerHarmonyOS::window_set_max_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerHarmonyOS::window_get_max_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i();
}

void DisplayServerHarmonyOS::window_set_min_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
}

Size2i DisplayServerHarmonyOS::window_get_min_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i();
}

void DisplayServerHarmonyOS::window_set_size(const Size2i p_size, DisplayServerEnums::WindowID p_window) {
	window_size = p_size;
}

Size2i DisplayServerHarmonyOS::window_get_size(DisplayServerEnums::WindowID p_window) const {
	return window_size;
}

Size2i DisplayServerHarmonyOS::window_get_size_with_decorations(DisplayServerEnums::WindowID p_window) const {
	return window_size;
}

void DisplayServerHarmonyOS::window_set_mode(DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::WindowID p_window) {
}

DisplayServerEnums::WindowMode DisplayServerHarmonyOS::window_get_mode(DisplayServerEnums::WindowID p_window) const {
	return DisplayServerEnums::WINDOW_MODE_WINDOWED;
}

bool DisplayServerHarmonyOS::window_is_maximize_allowed(DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerHarmonyOS::window_set_flag(DisplayServerEnums::WindowFlags p_flag, bool p_enabled, DisplayServerEnums::WindowID p_window) {
}

bool DisplayServerHarmonyOS::window_get_flag(DisplayServerEnums::WindowFlags p_flag, DisplayServerEnums::WindowID p_window) const {
	return false;
}

void DisplayServerHarmonyOS::window_request_attention(DisplayServerEnums::WindowID p_window) {
}

void DisplayServerHarmonyOS::window_move_to_foreground(DisplayServerEnums::WindowID p_window) {
}

bool DisplayServerHarmonyOS::window_is_focused(DisplayServerEnums::WindowID p_window) const {
	return window_focused;
}

bool DisplayServerHarmonyOS::window_can_draw(DisplayServerEnums::WindowID p_window) const {
	return window_can_draw_val;
}

bool DisplayServerHarmonyOS::can_any_window_draw() const {
	return window_can_draw_val;
}

// ---- events ----

void DisplayServerHarmonyOS::process_events() {
	Input::get_singleton()->flush_buffered_events();
}

// ---- cursor ----

void DisplayServerHarmonyOS::cursor_set_shape(DisplayServerEnums::CursorShape p_shape) {
	cursor_shape = p_shape;
}

DisplayServerEnums::CursorShape DisplayServerHarmonyOS::cursor_get_shape() const {
	return cursor_shape;
}

// ---- mouse ----

Point2i DisplayServerHarmonyOS::mouse_get_position() const {
	return last_mouse_pos;
}

BitField<MouseButtonMask> DisplayServerHarmonyOS::mouse_get_button_state() const {
	return MouseButtonMask(0);
}

void DisplayServerHarmonyOS::mouse_set_mode(DisplayServerEnums::MouseMode p_mode) {
}

DisplayServerEnums::MouseMode DisplayServerHarmonyOS::mouse_get_mode() const {
	return DisplayServerEnums::MOUSE_MODE_VISIBLE;
}

// ---- swap buffers ----

void DisplayServerHarmonyOS::reset_swap_buffers_flag() {
	swap_buffers_flag = false;
}

bool DisplayServerHarmonyOS::should_swap_buffers() const {
	return swap_buffers_flag;
}

void DisplayServerHarmonyOS::swap_buffers() {
	swap_buffers_flag = true;
}

void DisplayServerHarmonyOS::notify_surface_changed(int p_width, int p_height) {
	window_size = Size2i(p_width, p_height);
	if (rect_changed_callback.is_valid()) {
		rect_changed_callback.call(Rect2i(0, 0, p_width, p_height));
	}
}

void DisplayServerHarmonyOS::notify_surface_created() {
	window_can_draw_val = true;
}

void DisplayServerHarmonyOS::notify_surface_destroyed() {
	window_can_draw_val = false;
}

// ---- driver registration ----

Vector<String> DisplayServerHarmonyOS::get_rendering_drivers_func() {
	Vector<String> drivers;
#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif
	return drivers;
}

DisplayServer *DisplayServerHarmonyOS::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerHarmonyOS(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, p_parent_window, r_error));
	if (r_error != OK) {
		OS::get_singleton()->alert(
			"Your device does not support Vulkan.\n\nPlease ensure your HarmonyOS device supports Vulkan.",
			"Unable to initialize Vulkan video driver");
	}
	return ds;
}

void DisplayServerHarmonyOS::register_harmonyos_driver() {
	register_create_function("harmonyos", create_func, get_rendering_drivers_func);
}

// ---- Vulkan context ----

#ifdef VULKAN_ENABLED
bool DisplayServerHarmonyOS::check_vulkan_global_context(bool p_vulkan_requirements_met) {
	if (!rendering_context_global_checked) {
		Error err = ERR_CANT_CREATE;
		if (p_vulkan_requirements_met) {
			rendering_context_global = memnew(RenderingContextDriverVulkanHarmonyOS);
			err = rendering_context_global->initialize();
		}

		if (err != OK) {
			if (rendering_context_global != nullptr) {
				memdelete(rendering_context_global);
				rendering_context_global = nullptr;
			}
			ERR_PRINT("Failed to initialize Vulkan context.");
		}

		rendering_context_global_checked = true;
	}

	return rendering_context_global != nullptr;
}

void DisplayServerHarmonyOS::free_vulkan_global_context() {
	if (rendering_context_global != nullptr) {
		memdelete(rendering_context_global);
		rendering_context_global = nullptr;
		rendering_context_global_checked = false;
	}
}

void DisplayServerHarmonyOS::reset_window() {
	if (rendering_context_global) {
		DisplayServerEnums::VSyncMode last_vsync_mode = rendering_context_global->window_get_vsync_mode(window_id);
		rendering_context_global->window_destroy(window_id);

		HarmonyOSNativeWindow *native_win = HarmonyOSNativeWindow::singleton;
		ERR_FAIL_NULL(native_win);

		OHNativeWindow *oh_window = native_win->get_native_window();
		ERR_FAIL_NULL(oh_window);

		RenderingContextDriverVulkanHarmonyOS::WindowPlatformData wpd;
		wpd.native_window = oh_window;

		if (rendering_context_global->window_create(window_id, &wpd) != OK) {
			ERR_PRINT("Failed to reset Vulkan window.");
			return;
		}

		rendering_context_global->window_set_size(window_id, window_size.width, window_size.height);
		rendering_context_global->window_set_vsync_mode(window_id, last_vsync_mode);
	}
}
#endif // VULKAN_ENABLED

// ---- constructor / destructor ----

DisplayServerHarmonyOS::DisplayServerHarmonyOS(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	rendering_driver = p_rendering_driver;
	window_size = p_resolution;
	keep_screen_on = true;
	r_error = OK;

	// Rendering context and device are initialized separately
	// through check_vulkan_global_context() and reset_window()
	// after the surface becomes available.
}

DisplayServerHarmonyOS::~DisplayServerHarmonyOS() {
}
