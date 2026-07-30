/**************************************************************************/
/*  harmonyos_input.cpp - HarmonyOS Input Event Translation               */
/**************************************************************************/

#include "harmonyos_input.h"

#include "core/input/input.h"
#include "core/input/input_event.h"

#include <cstring>
#include <hilog/log.h>

namespace HarmonyOSInput {

// OHOS key codes (partial list - expand as needed)
// See: https://developer.huawei.com/consumer/en/doc/harmonyos-references/input-interfaces
static const int OHOS_KEYCODE_SHIFT_LEFT = 2045;
static const int OHOS_KEYCODE_SHIFT_RIGHT = 2046;
static const int OHOS_KEYCODE_CTRL_LEFT = 2072;
static const int OHOS_KEYCODE_CTRL_RIGHT = 2073;
static const int OHOS_KEYCODE_ALT_LEFT = 2047;
static const int OHOS_KEYCODE_ALT_RIGHT = 2048;
static const int OHOS_KEYCODE_META_LEFT = 2049; // Windows/Meta key

static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool meta_pressed = false;

Key ohos_key_to_godot(int ohos_keycode) {
	// OHOS Key Mappings → Godot Key enum
	// Reference keycodes from HarmonyOS documentation
	
	// Modifier keys
	if (ohos_keycode == OHOS_KEYCODE_SHIFT_LEFT) return Key::SHIFT;
	if (ohos_keycode == OHOS_KEYCODE_SHIFT_RIGHT) return Key::SHIFT;
	if (ohos_keycode == OHOS_KEYCODE_CTRL_LEFT) return Key::CTRL;
	if (ohos_keycode == OHOS_KEYCODE_CTRL_RIGHT) return Key::CTRL;
	if (ohos_keycode == OHOS_KEYCODE_ALT_LEFT) return Key::ALT;
	if (ohos_keycode == OHOS_KEYCODE_ALT_RIGHT) return Key::ALT;
	if (ohos_keycode == OHOS_KEYCODE_META_LEFT) return Key::META;

	// Navigation keys
	if (ohos_keycode == 2014) return Key::ESCAPE;
	if (ohos_keycode == 2015) return Key::ENTER;
	if (ohos_keycode == 2054) return Key::BACKSPACE;
	if (ohos_keycode == 2055) return Key::TAB;
	if (ohos_keycode == 2056) return Key::SPACE;
	if (ohos_keycode == 2057) return Key::MINUS;
	if (ohos_keycode == 2058) return Key::EQUAL;
	if (ohos_keycode == 2059) return Key::BRACKETLEFT;
	if (ohos_keycode == 2060) return Key::BRACKETRIGHT;
	if (ohos_keycode == 2061) return Key::BACKSLASH;
	if (ohos_keycode == 2062) return Key::SEMICOLON;
	if (ohos_keycode == 2063) return Key::APOSTROPHE;
	if (ohos_keycode == 2064) return Key::COMMA;
	if (ohos_keycode == 2065) return Key::PERIOD;
	if (ohos_keycode == 2066) return Key::SLASH;
	if (ohos_keycode == 2067) return Key::QUOTELEFT;

	// Arrow keys
	if (ohos_keycode == 2017) return Key::UP;
	if (ohos_keycode == 2018) return Key::DOWN;
	if (ohos_keycode == 2019) return Key::LEFT;
	if (ohos_keycode == 2020) return Key::RIGHT;

	// Function keys
	if (ohos_keycode >= 2082 && ohos_keycode <= 2093) {
		return Key(Key::F1 + (ohos_keycode - 2082));
	}

	// Number keys (main keyboard)
	if (ohos_keycode >= 2001 && ohos_keycode <= 2010) {
		// 2001='1' → 2010='0'
		if (ohos_keycode == 2010) return Key::KEY_0;
		return Key(Key::KEY_1 + (ohos_keycode - 2001));
	}

	// Letter keys (A-Z)
	if (ohos_keycode >= 2011 && ohos_keycode <= 2036) {
		return Key(Key::A + (ohos_keycode - 2011));
	}

	// Caps Lock
	if (ohos_keycode == 2050) return Key::CAPSLOCK;

	// Home, End, PageUp, PageDown
	if (ohos_keycode == 2021) return Key::HOME;
	if (ohos_keycode == 2022) return Key::END;
	if (ohos_keycode == 2023) return Key::PAGEUP;
	if (ohos_keycode == 2024) return Key::PAGEDOWN;

	// Insert, Delete
	if (ohos_keycode == 2074) return Key::INSERT;
	if (ohos_keycode == 2052) return Key::KEY_DELETE;

	// Numpad
	if (ohos_keycode == 2094) return Key::KP_MULTIPLY;
	if (ohos_keycode == 2095) return Key::KP_SUBTRACT;
	if (ohos_keycode == 2096) return Key::KP_ADD;
	if (ohos_keycode == 2079) return Key::KP_ENTER;
	if (ohos_keycode == 2098) return Key::KP_PERIOD;
	if (ohos_keycode == 2097) return Key::KP_DIVIDE;
	if (ohos_keycode >= 2075 && ohos_keycode <= 2078) {
		// Numpad 7-9 (missing 0-6 mapping - extend as needed)
		return Key(Key::KP_7 + (ohos_keycode - 2075));
	}

	// Print Screen, Scroll Lock, Pause
	if (ohos_keycode == 2068) return Key::PRINT;
	if (ohos_keycode == 2069) return Key::SCROLLLOCK;
	if (ohos_keycode == 2070) return Key::PAUSE;

	return Key::NONE;
}

void process_key_event(int key_code, int event_type, const char *key_text) {
	if (key_code == 0) return;

	Key godot_key = ohos_key_to_godot(key_code);
	if (godot_key == Key::NONE) return;

	// Track modifier state
	if (key_code == OHOS_KEYCODE_SHIFT_LEFT || key_code == OHOS_KEYCODE_SHIFT_RIGHT) {
		shift_pressed = (event_type == 0);
	}
	if (key_code == OHOS_KEYCODE_CTRL_LEFT || key_code == OHOS_KEYCODE_CTRL_RIGHT) {
		ctrl_pressed = (event_type == 0);
	}
	if (key_code == OHOS_KEYCODE_ALT_LEFT || key_code == OHOS_KEYCODE_ALT_RIGHT) {
		alt_pressed = (event_type == 0);
	}
	if (key_code == OHOS_KEYCODE_META_LEFT) {
		meta_pressed = (event_type == 0);
	}

	Ref<InputEventKey> key_event;
	key_event.instantiate();

	if (event_type == 0) {
		// Key pressed
		key_event->set_pressed(true);
		key_event->set_echo(false);
	} else if (event_type == 1) {
		// Key released
		key_event->set_pressed(false);
		key_event->set_echo(false);
	} else if (event_type == 2) {
		// Key repeat
		key_event->set_pressed(true);
		key_event->set_echo(true);
	}

	key_event->set_keycode(godot_key);
	key_event->set_physical_keycode(godot_key);
	key_event->set_key_label(Key::NONE);
	key_event->set_unicode(0);

	if (key_text && std::strlen(key_text) > 0) {
		key_event->set_key_label(static_cast<Key>(static_cast<uint32_t>(godot_key)));
		key_event->set_unicode(key_text[0]);
	}

	// Set modifiers
	key_event->set_shift_pressed(shift_pressed);
	key_event->set_ctrl_pressed(ctrl_pressed);
	key_event->set_alt_pressed(alt_pressed);
	key_event->set_meta_pressed(meta_pressed);

	Input::get_singleton()->parse_input_event(key_event);
}

void process_mouse_event(int button, int action, double x, double y,
                          double offset_x, double offset_y) {
	Ref<InputEventMouseButton> mouse_event;
	mouse_event.instantiate();

	mouse_event->set_position(Vector2(x, y));

	// Map action: 0=press, 1=release, 2=move
	if (action == 2) {
		// Motion event
		Ref<InputEventMouseMotion> motion_event;
		motion_event.instantiate();
		motion_event->set_position(Vector2(x, y));
		motion_event->set_relative(Vector2(offset_x, offset_y));
		motion_event->set_button_mask(
			MouseButtonMask(MouseButtonMask::LEFT | MouseButtonMask::MIDDLE | MouseButtonMask::RIGHT));
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

	mouse_event->set_button_index(mouse_button);
	mouse_event->set_pressed(action == 0);

	MouseButtonMask mask = mouse_button_to_mask(mouse_button);
	mouse_event->set_button_mask(mask);

	Input::get_singleton()->parse_input_event(mouse_event);
}

void process_touch_event(int touch_id, int action, double x, double y) {
	Ref<InputEventScreenTouch> touch_event;
	touch_event.instantiate();

	touch_event->set_index(touch_id);
	touch_event->set_position(Vector2(x, y));

	// 0=down, 1=up, 2=move (move uses InputEventScreenDrag)
	if (action == 2) {
		Ref<InputEventScreenDrag> drag_event;
		drag_event.instantiate();
		drag_event->set_index(touch_id);
		drag_event->set_position(Vector2(x, y));
		Input::get_singleton()->parse_input_event(drag_event);
		return;
	}

	touch_event->set_pressed(action == 0);
	Input::get_singleton()->parse_input_event(touch_event);
}

void process_input_text(const char *text) {
	if (!text || text[0] == '\0') return;

	Ref<InputEventKey> key_event;
	key_event.instantiate();
	key_event->set_pressed(true);
	key_event->set_keycode(Key::NONE);
	key_event->set_physical_keycode(Key::NONE);
	key_event->set_key_label(Key::NONE);
	key_event->set_unicode(text[0]);

	for (const char *c = text; *c != '\0'; c++) {
		if (*c > 0) {
			key_event->set_unicode(static_cast<char32_t>(*c));
			Input::get_singleton()->parse_input_event(key_event);
		}
	}
}

} // namespace HarmonyOSInput
