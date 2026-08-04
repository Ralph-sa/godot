/**************************************************************************/
/*  harmonyos_input.cpp - HarmonyOS Input Event Translation               */
/*                                                                        */
/*  Completes OHOS → Godot key mapping with full keyboard coverage.       */
/**************************************************************************/

#include "harmonyos_input.h"

#include "display_server_harmonyos.h"
#include "core/input/input.h"
#include "core/input/input_event.h"

#include <cstring>
#include "harmonyos_log.h"
#include <atomic>

namespace HarmonyOSInput {

// ==== OHOS Key Code Constants ====
// Reference (official): @ohos.multimodalInput.keyCode / oh_key_code.h
//   https://gitee.com/openharmony/docs (apis-input-kit)

// Number row — OHOS_KEY_0 and OHOS_KEY_9 serve as bounds for the range
// mapping in ohos_key_to_godot(); intermediate values (1-8) are covered by
// linear interpolation and do not need individual constants.
static const int OHOS_KEY_0 = 2000;
static const int OHOS_KEY_1 = 2001;
static const int OHOS_KEY_9 = 2009;

// Letters
static const int OHOS_KEY_A = 2017;
static const int OHOS_KEY_Z = 2042;

// Navigation
static const int OHOS_KEY_ESCAPE = 2070;
static const int OHOS_KEY_ENTER = 2054;
static const int OHOS_KEY_UP = 2012;  // DPAD_UP
static const int OHOS_KEY_DOWN = 2013; // DPAD_DOWN
static const int OHOS_KEY_LEFT = 2014; // DPAD_LEFT
static const int OHOS_KEY_RIGHT = 2015; // DPAD_RIGHT
// MOVE_HOME (2081), not KEYCODE_HOME (1). The latter is the system home button
// that returns to the launcher — mapping it to Key::HOME made a hardware button
// act like "go to start of line". MOVE_END (2082) is its correct counterpart.
static const int OHOS_KEY_HOME = 2081; // MOVE_HOME
static const int OHOS_KEY_END = 2082; // MOVE_END
static const int OHOS_KEY_PAGE_UP = 2068;
static const int OHOS_KEY_PAGE_DOWN = 2069;

// Modifiers
static const int OHOS_KEY_SHIFT_LEFT = 2047;
static const int OHOS_KEY_SHIFT_RIGHT = 2048;
static const int OHOS_KEY_ALT_LEFT = 2045;
static const int OHOS_KEY_ALT_RIGHT = 2046;
static const int OHOS_KEY_META_LEFT = 2076;
static const int OHOS_KEY_META_RIGHT = 2077;
static const int OHOS_KEY_CAPS_LOCK = 2074;
static const int OHOS_KEY_NUM_LOCK = 2102;
static const int OHOS_KEY_SCROLL_LOCK = 2075;

// Whitespace / punctuation
static const int OHOS_KEY_BACKSPACE = 2055; // DEL
static const int OHOS_KEY_TAB = 2049;
static const int OHOS_KEY_SPACE = 2050;
static const int OHOS_KEY_MINUS = 2057;
static const int OHOS_KEY_EQUAL = 2058;
static const int OHOS_KEY_BRACKET_LEFT = 2059;
static const int OHOS_KEY_BRACKET_RIGHT = 2060;
static const int OHOS_KEY_BACKSLASH = 2061;
static const int OHOS_KEY_SEMICOLON = 2062;
static const int OHOS_KEY_APOSTROPHE = 2063;
static const int OHOS_KEY_GRAVE = 2056;
static const int OHOS_KEY_COMMA = 2043;
static const int OHOS_KEY_PERIOD = 2044;
static const int OHOS_KEY_SLASH = 2064;
static const int OHOS_KEY_AT = 2065;
static const int OHOS_KEY_PLUS = 2066;

// Special
static const int OHOS_KEY_DELETE = 2071; // FORWARD_DEL
static const int OHOS_KEY_INSERT = 2083;
static const int OHOS_KEY_PRINT_SCREEN = 2079; // SYSRQ
static const int OHOS_KEY_PAUSE = 2080; // BREAK

// Ctrl / Context Menu
static const int OHOS_KEY_CTRL_LEFT = 2072;
static const int OHOS_KEY_CTRL_RIGHT = 2073;

// Function keys — official OHOS range: F1 (2090) .. F12 (2101)
static const int OHOS_KEY_F1 = 2090;
static const int OHOS_KEY_F12 = 2101;

// Numpad — values from official OHOS keycode spec (API 9+)
// Range: KEYCODE_NUMPAD_0 (2103) .. KEYCODE_NUMPAD_ENTER (2119)
static const int OHOS_KEY_NUMPAD_0 = 2103;
static const int OHOS_KEY_NUMPAD_1 = 2104;
static const int OHOS_KEY_NUMPAD_2 = 2105;
static const int OHOS_KEY_NUMPAD_3 = 2106;
static const int OHOS_KEY_NUMPAD_4 = 2107;
static const int OHOS_KEY_NUMPAD_5 = 2108;
static const int OHOS_KEY_NUMPAD_6 = 2109;
static const int OHOS_KEY_NUMPAD_7 = 2110;
static const int OHOS_KEY_NUMPAD_8 = 2111;
static const int OHOS_KEY_NUMPAD_9 = 2112;
static const int OHOS_KEY_NUMPAD_DIVIDE = 2113;
static const int OHOS_KEY_NUMPAD_MULTIPLY = 2114;
static const int OHOS_KEY_NUMPAD_SUBTRACT = 2115;
static const int OHOS_KEY_NUMPAD_ADD = 2116;
static const int OHOS_KEY_NUMPAD_DOT = 2117;
static const int OHOS_KEY_NUMPAD_ENTER = 2119;

// Media keys
// OHOS has no PLAY_PAUSE/STOP/NEXT/PREVIOUS keycodes; 10-13 were Android values
// that collide with unrelated OHOS system keys. Only the codes below exist.
static const int OHOS_KEY_MEDIA_PLAY = 2085;
static const int OHOS_KEY_MEDIA_PAUSE = 2086;
static const int OHOS_KEY_MEDIA_RECORD = 2089;
static const int OHOS_KEY_MEDIA_VOLUME_UP = 16;
static const int OHOS_KEY_MEDIA_VOLUME_DOWN = 17;
static const int OHOS_KEY_MEDIA_VOLUME_MUTE = 22;

// Modifier tracking
static std::atomic<bool> shift_pressed(false);
static std::atomic<bool> ctrl_pressed(false);
static std::atomic<bool> alt_pressed(false);
static std::atomic<bool> meta_pressed(false);

// Current mouse button state (bitmask of MouseButtonMask).
// Updated on press/release, read by DisplayServer::mouse_get_button_state()
// and attached to motion events so the engine sees the real held buttons.
static std::atomic<uint32_t> g_mouse_button_mask(0);

MouseButtonMask get_mouse_button_mask() {
	return MouseButtonMask(g_mouse_button_mask.load(std::memory_order_acquire));
}

// Current pointer position, updated on every mouse event and read by
// DisplayServer::mouse_get_position(). Kept here rather than as DisplayServer
// state because this is the only place that learns of pointer movement.
// Stored as two atomics: Vector2i is not trivially copyable, so it cannot be
// held in a single lock-free std::atomic.
static std::atomic<int32_t> g_mouse_pos_x(0);
static std::atomic<int32_t> g_mouse_pos_y(0);

Point2i get_mouse_position() {
	return Point2i(g_mouse_pos_x.load(std::memory_order_acquire),
			g_mouse_pos_y.load(std::memory_order_acquire));
}

// ==== Key Mapping Function ====

Key ohos_key_to_godot(int keycode) {
	// ── Number row ──
	if (keycode >= OHOS_KEY_0 && keycode <= OHOS_KEY_9) {
		if (keycode == OHOS_KEY_0) return Key::KEY_0;
		return Key(Key::KEY_1 + (keycode - OHOS_KEY_1));
	}

	// ── Letters A-Z ──
	if (keycode >= OHOS_KEY_A && keycode <= OHOS_KEY_Z) {
		return Key(Key::A + (keycode - OHOS_KEY_A));
	}

	// ── Modifiers ──
	switch (keycode) {
		case OHOS_KEY_SHIFT_LEFT:  return Key::SHIFT;
		case OHOS_KEY_SHIFT_RIGHT: return Key::SHIFT;
		case OHOS_KEY_CTRL_LEFT:   return Key::CTRL;
		case OHOS_KEY_CTRL_RIGHT:  return Key::CTRL;
		case OHOS_KEY_ALT_LEFT:    return Key::ALT;
		case OHOS_KEY_ALT_RIGHT:   return Key::ALT;
		case OHOS_KEY_META_LEFT:   return Key::META;
		case OHOS_KEY_META_RIGHT:  return Key::META;
	}

	// ── Navigation ──
	switch (keycode) {
		case OHOS_KEY_ESCAPE:    return Key::ESCAPE;
		case OHOS_KEY_ENTER:     return Key::ENTER;
		case OHOS_KEY_UP:        return Key::UP;
		case OHOS_KEY_DOWN:      return Key::DOWN;
		case OHOS_KEY_LEFT:      return Key::LEFT;
		case OHOS_KEY_RIGHT:     return Key::RIGHT;
		case OHOS_KEY_HOME:      return Key::HOME;
		case OHOS_KEY_END:       return Key::END;
		case OHOS_KEY_PAGE_UP:   return Key::PAGEUP;
		case OHOS_KEY_PAGE_DOWN: return Key::PAGEDOWN;
	}

	// ── Whitespace / Punctuation ──
	switch (keycode) {
		case OHOS_KEY_BACKSPACE: return Key::BACKSPACE;
		case OHOS_KEY_TAB:       return Key::TAB;
		case OHOS_KEY_SPACE:     return Key::SPACE;
		case OHOS_KEY_MINUS:     return Key::MINUS;
		case OHOS_KEY_EQUAL:     return Key::EQUAL;
		case OHOS_KEY_BRACKET_LEFT:  return Key::BRACKETLEFT;
		case OHOS_KEY_BRACKET_RIGHT: return Key::BRACKETRIGHT;
		case OHOS_KEY_BACKSLASH:     return Key::BACKSLASH;
		case OHOS_KEY_SEMICOLON:     return Key::SEMICOLON;
		case OHOS_KEY_APOSTROPHE:    return Key::APOSTROPHE;
		case OHOS_KEY_GRAVE:         return Key::QUOTELEFT;
		case OHOS_KEY_COMMA:         return Key::COMMA;
		case OHOS_KEY_PERIOD:        return Key::PERIOD;
		case OHOS_KEY_SLASH:         return Key::SLASH;
		case OHOS_KEY_AT:            return Key::AT;
		case OHOS_KEY_PLUS:          return Key::PLUS;
	}

	// ── Special ──
	switch (keycode) {
		case OHOS_KEY_DELETE:        return Key::KEY_DELETE;
		case OHOS_KEY_INSERT:        return Key::INSERT;
		case OHOS_KEY_PRINT_SCREEN:  return Key::PRINT;
		case OHOS_KEY_SCROLL_LOCK:   return Key::SCROLLLOCK;
		case OHOS_KEY_PAUSE:         return Key::PAUSE;
		case OHOS_KEY_CAPS_LOCK:     return Key::CAPSLOCK;
		case OHOS_KEY_NUM_LOCK:      return Key::NUMLOCK;
	}

	// ── Function keys F1-F12 ──
	if (keycode >= OHOS_KEY_F1 && keycode <= OHOS_KEY_F12) {
		return Key(Key::F1 + (keycode - OHOS_KEY_F1));
	}

	// ── Numpad ──
	switch (keycode) {
		case OHOS_KEY_NUMPAD_0:       return Key::KP_0;
		case OHOS_KEY_NUMPAD_1:       return Key::KP_1;
		case OHOS_KEY_NUMPAD_2:       return Key::KP_2;
		case OHOS_KEY_NUMPAD_3:       return Key::KP_3;
		case OHOS_KEY_NUMPAD_4:       return Key::KP_4;
		case OHOS_KEY_NUMPAD_5:       return Key::KP_5;
		case OHOS_KEY_NUMPAD_6:       return Key::KP_6;
		case OHOS_KEY_NUMPAD_7:       return Key::KP_7;
		case OHOS_KEY_NUMPAD_8:       return Key::KP_8;
		case OHOS_KEY_NUMPAD_9:       return Key::KP_9;
		case OHOS_KEY_NUMPAD_DIVIDE:   return Key::KP_DIVIDE;
		case OHOS_KEY_NUMPAD_MULTIPLY: return Key::KP_MULTIPLY;
		case OHOS_KEY_NUMPAD_SUBTRACT: return Key::KP_SUBTRACT;
		case OHOS_KEY_NUMPAD_ADD:      return Key::KP_ADD;
		case OHOS_KEY_NUMPAD_DOT:      return Key::KP_PERIOD;
		case OHOS_KEY_NUMPAD_ENTER:    return Key::KP_ENTER;
	}

	// ── Media ──
	switch (keycode) {
		case OHOS_KEY_MEDIA_PLAY:        return Key::MEDIAPLAY;
		case OHOS_KEY_MEDIA_PAUSE:       return Key::MEDIASTOP;
		case OHOS_KEY_MEDIA_RECORD:      return Key::MEDIARECORD;
		case OHOS_KEY_MEDIA_VOLUME_UP:   return Key::VOLUMEUP;
		case OHOS_KEY_MEDIA_VOLUME_DOWN: return Key::VOLUMEDOWN;
		case OHOS_KEY_MEDIA_VOLUME_MUTE: return Key::VOLUMEMUTE;
	}

	return Key::NONE;
}

// Godot maps both physical shift keys onto a single Key::SHIFT, so the side has
// to travel separately for shortcuts that distinguish left from right.
KeyLocation ohos_key_location(int keycode) {
	switch (keycode) {
		case OHOS_KEY_SHIFT_LEFT:
		case OHOS_KEY_CTRL_LEFT:
		case OHOS_KEY_ALT_LEFT:
		case OHOS_KEY_META_LEFT:
			return KeyLocation::LEFT;
		case OHOS_KEY_SHIFT_RIGHT:
		case OHOS_KEY_CTRL_RIGHT:
		case OHOS_KEY_ALT_RIGHT:
		case OHOS_KEY_META_RIGHT:
			return KeyLocation::RIGHT;
		default:
			return KeyLocation::UNSPECIFIED;
	}
}

// ==== Event Processing ====

void process_key_event(int key_code, int event_type, const char *key_text) {
	if (key_code == 0) return;

	Key godot_key = ohos_key_to_godot(key_code);
	if (godot_key == Key::NONE) return;

	// Track modifier state
	switch (key_code) {
		case OHOS_KEY_SHIFT_LEFT:
		case OHOS_KEY_SHIFT_RIGHT:
			shift_pressed.store(event_type == 0, std::memory_order_release);
			break;
		case OHOS_KEY_CTRL_LEFT:
		case OHOS_KEY_CTRL_RIGHT:
			ctrl_pressed.store(event_type == 0, std::memory_order_release);
			break;
		case OHOS_KEY_ALT_LEFT:
		case OHOS_KEY_ALT_RIGHT:
			alt_pressed.store(event_type == 0, std::memory_order_release);
			break;
		case OHOS_KEY_META_LEFT:
		case OHOS_KEY_META_RIGHT:
			meta_pressed.store(event_type == 0, std::memory_order_release);
			break;
	}

	Ref<InputEventKey> key_event;
	key_event.instantiate();

	switch (event_type) {
		case 0: // Press
			key_event->set_pressed(true);
			key_event->set_echo(false);
			break;
		case 1: // Release
			key_event->set_pressed(false);
			key_event->set_echo(false);
			break;
		case 2: // Repeat
			key_event->set_pressed(true);
			key_event->set_echo(true);
			break;
	}

	key_event->set_keycode(godot_key);
	key_event->set_physical_keycode(godot_key);
	key_event->set_key_label(Key::NONE);
	key_event->set_unicode(0);
	key_event->set_location(ohos_key_location(key_code));

	if (key_text && std::strlen(key_text) > 0) {
		// key_text is UTF-8 encoded; decode with Godot's String so multi-byte
		// characters (CJK, accents) are not truncated to their first byte.
		String decoded = String::utf8(key_text);
		if (decoded.length() > 0) {
			char32_t unicode = decoded.unicode_at(0);
			key_event->set_key_label(Key(unicode));
			key_event->set_unicode(unicode);
		}
	}

	// Attach modifiers
	key_event->set_shift_pressed(shift_pressed.load(std::memory_order_acquire));
	key_event->set_ctrl_pressed(ctrl_pressed.load(std::memory_order_acquire));
	key_event->set_alt_pressed(alt_pressed.load(std::memory_order_acquire));
	key_event->set_meta_pressed(meta_pressed.load(std::memory_order_acquire));

	Input::get_singleton()->parse_input_event(key_event);
}

void process_mouse_event(int button, int action, double x, double y,
                          double offset_x, double offset_y) {
	// Every mouse event carries a position; record it so that
	// DisplayServer::mouse_get_position() reflects reality instead of (0,0).
	g_mouse_pos_x.store((int32_t)x, std::memory_order_release);
	g_mouse_pos_y.store((int32_t)y, std::memory_order_release);

	if (action == 2) {
		// Motion event
		Ref<InputEventMouseMotion> motion_event;
		motion_event.instantiate();
		motion_event->set_position(Vector2(x, y));
		motion_event->set_relative(Vector2(offset_x, offset_y));

		// Attach the real held-button mask instead of a hardcoded all-buttons mask.
		motion_event->set_button_mask(get_mouse_button_mask());

		Input::get_singleton()->parse_input_event(motion_event);
		return;
	}

	// Map button: 0=left, 1=middle, 2=right, 3=xbutton1, 4=xbutton2
	MouseButton mouse_button;
	switch (button) {
		case 0: mouse_button = MouseButton::LEFT; break;
		case 1: mouse_button = MouseButton::MIDDLE; break;
		case 2: mouse_button = MouseButton::RIGHT; break;
		case 3: mouse_button = MouseButton::MB_XBUTTON1; break;
		case 4: mouse_button = MouseButton::MB_XBUTTON2; break;
		default: mouse_button = MouseButton::LEFT; break;
	}

	// Keep the button state in sync for motion events and DisplayServer queries.
	uint32_t mask_bit = (uint32_t)mouse_button_to_mask(mouse_button);
	if (action == 0) {
		g_mouse_button_mask.fetch_or(mask_bit, std::memory_order_acq_rel);
	} else {
		g_mouse_button_mask.fetch_and(~mask_bit, std::memory_order_acq_rel);
	}

	Ref<InputEventMouseButton> mouse_event;
	mouse_event.instantiate();
	mouse_event->set_position(Vector2(x, y));
	mouse_event->set_button_index(mouse_button);
	mouse_event->set_pressed(action == 0);
	mouse_event->set_button_mask(get_mouse_button_mask());

	Input::get_singleton()->parse_input_event(mouse_event);
}

// Godot models the wheel as buttons rather than an axis, and expects a matching
// release right after each press — without it the editor treats the wheel as
// stuck down and scrolling never stops.
static void send_wheel_click(MouseButton p_button, double p_factor, const Vector2 &p_pos) {
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(p_pos);
	ev->set_global_position(p_pos);
	ev->set_button_index(p_button);
	ev->set_factor(p_factor);
	ev->set_button_mask(get_mouse_button_mask());
	ev->set_shift_pressed(shift_pressed.load(std::memory_order_acquire));
	ev->set_ctrl_pressed(ctrl_pressed.load(std::memory_order_acquire));
	ev->set_alt_pressed(alt_pressed.load(std::memory_order_acquire));
	ev->set_meta_pressed(meta_pressed.load(std::memory_order_acquire));

	ev->set_pressed(true);
	Input::get_singleton()->parse_input_event(ev);

	Ref<InputEventMouseButton> release = ev->duplicate();
	release->set_pressed(false);
	Input::get_singleton()->parse_input_event(release);
}

void process_mouse_scroll_event(double x, double y, double offset_x, double offset_y) {
	g_mouse_pos_x.store((int32_t)x, std::memory_order_release);
	g_mouse_pos_y.store((int32_t)y, std::memory_order_release);

	const Vector2 pos(x, y);

	if (offset_y != 0.0) {
		MouseButton button = offset_y > 0.0 ? MouseButton::WHEEL_UP : MouseButton::WHEEL_DOWN;
		send_wheel_click(button, Math::abs(offset_y), pos);
	}
	if (offset_x != 0.0) {
		MouseButton button = offset_x > 0.0 ? MouseButton::WHEEL_RIGHT : MouseButton::WHEEL_LEFT;
		send_wheel_click(button, Math::abs(offset_x), pos);
	}
}

void process_touch_event(int touch_id, int action, double x, double y) {
	if (action == 2) {
		// Move → drag
		Ref<InputEventScreenDrag> drag_event;
		drag_event.instantiate();
		drag_event->set_index(touch_id);
		drag_event->set_position(Vector2(x, y));
		Input::get_singleton()->parse_input_event(drag_event);
		return;
	}

	Ref<InputEventScreenTouch> touch_event;
	touch_event.instantiate();
	touch_event->set_index(touch_id);
	touch_event->set_position(Vector2(x, y));
	touch_event->set_pressed(action == 0);
	Input::get_singleton()->parse_input_event(touch_event);
}

void process_input_text(const char *text) {
	if (!text || text[0] == '\0') return;

	DisplayServerHarmonyOS *ds = static_cast<DisplayServerHarmonyOS *>(DisplayServer::get_singleton());
	if (ds) {
		ds->ime_text(String::utf8(text));
	}

	// Decode the whole UTF-8 string and forward every character as a
	// unicode key event. Iterating raw bytes would drop multi-byte
	// characters (e.g. CJK IME output), since UTF-8 continuation bytes
	// are negative when viewed as signed char.
	String decoded = String::utf8(text);
	if (decoded.is_empty()) {
		return;
	}

	for (int i = 0; i < decoded.length(); i++) {
		char32_t unicode = decoded.unicode_at(i);
		Ref<InputEventKey> key_event;
		key_event.instantiate();
		key_event->set_pressed(true);
		key_event->set_echo(false);
		key_event->set_keycode(Key::NONE);
		key_event->set_physical_keycode(Key::NONE);
		key_event->set_key_label(Key(unicode));
		key_event->set_unicode(unicode);
		Input::get_singleton()->parse_input_event(key_event);
	}
}

} // namespace HarmonyOSInput
