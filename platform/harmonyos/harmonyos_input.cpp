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
#include <hilog/log.h>
#include <atomic>

namespace HarmonyOSInput {

// ==== OHOS Key Code Constants ====
// Reference: https://developer.huawei.com/consumer/en/doc/harmonyos-references/input-interfaces

// Number row
static const int OHOS_KEY_0 = 2000;
static const int OHOS_KEY_1 = 2001;
static const int OHOS_KEY_2 = 2002;
static const int OHOS_KEY_3 = 2003;
static const int OHOS_KEY_4 = 2004;
static const int OHOS_KEY_5 = 2005;
static const int OHOS_KEY_6 = 2006;
static const int OHOS_KEY_7 = 2007;
static const int OHOS_KEY_8 = 2008;
static const int OHOS_KEY_9 = 2009;

// Letters
static const int OHOS_KEY_A = 2011;
static const int OHOS_KEY_Z = 2036;

// Navigation
static const int OHOS_KEY_ESCAPE = 2014;
static const int OHOS_KEY_ENTER = 2015;
static const int OHOS_KEY_UP = 2017;
static const int OHOS_KEY_DOWN = 2018;
static const int OHOS_KEY_LEFT = 2019;
static const int OHOS_KEY_RIGHT = 2020;
static const int OHOS_KEY_HOME = 2021;
static const int OHOS_KEY_END = 2022;
static const int OHOS_KEY_PAGE_UP = 2023;
static const int OHOS_KEY_PAGE_DOWN = 2024;

// Modifiers
static const int OHOS_KEY_SHIFT_LEFT = 2045;
static const int OHOS_KEY_SHIFT_RIGHT = 2046;
static const int OHOS_KEY_ALT_LEFT = 2047;
static const int OHOS_KEY_ALT_RIGHT = 2048;
static const int OHOS_KEY_META_LEFT = 2049;
static const int OHOS_KEY_META_RIGHT = 2050;
static const int OHOS_KEY_CAPS_LOCK = 2051;
static const int OHOS_KEY_NUM_LOCK = 2080;
static const int OHOS_KEY_SCROLL_LOCK = 2069;

// Whitespace / punctuation
static const int OHOS_KEY_BACKSPACE = 2054;
static const int OHOS_KEY_TAB = 2055;
static const int OHOS_KEY_SPACE = 2056;
static const int OHOS_KEY_MINUS = 2057;
static const int OHOS_KEY_EQUAL = 2058;
static const int OHOS_KEY_BRACKET_LEFT = 2059;
static const int OHOS_KEY_BRACKET_RIGHT = 2060;
static const int OHOS_KEY_BACKSLASH = 2061;
static const int OHOS_KEY_SEMICOLON = 2062;
static const int OHOS_KEY_APOSTROPHE = 2063;
static const int OHOS_KEY_GRAVE = 2067;

// Special
static const int OHOS_KEY_DELETE = 2052;
static const int OHOS_KEY_INSERT = 2074;
static const int OHOS_KEY_PRINT_SCREEN = 2068;
static const int OHOS_KEY_PAUSE = 2070;

// Ctrl / Context Menu
static const int OHOS_KEY_CTRL_LEFT = 2072;
static const int OHOS_KEY_CTRL_RIGHT = 2073;

// Function keys
static const int OHOS_KEY_F1 = 2082;
static const int OHOS_KEY_F12 = 2093;

// Numpad
static const int OHOS_KEY_NUMPAD_0 = 2066;  // shared with PERIOD in some OHOS versions
static const int OHOS_KEY_NUMPAD_1 = 2075;
static const int OHOS_KEY_NUMPAD_2 = 2076;
static const int OHOS_KEY_NUMPAD_3 = 2077;
static const int OHOS_KEY_NUMPAD_4 = 2078;
static const int OHOS_KEY_NUMPAD_5 = 2079;
static const int OHOS_KEY_NUMPAD_6 = 2080;
static const int OHOS_KEY_NUMPAD_7 = 2081;
static const int OHOS_KEY_NUMPAD_8 = 2082;
static const int OHOS_KEY_NUMPAD_9 = 2083;
static const int OHOS_KEY_NUMPAD_DIVIDE = 2097;
static const int OHOS_KEY_NUMPAD_MULTIPLY = 2094;
static const int OHOS_KEY_NUMPAD_SUBTRACT = 2095;
static const int OHOS_KEY_NUMPAD_ADD = 2096;
static const int OHOS_KEY_NUMPAD_DOT = 2098;
static const int OHOS_KEY_NUMPAD_ENTER = 2099;

// Media keys
static const int OHOS_KEY_MEDIA_PLAY_PAUSE = 2100;
static const int OHOS_KEY_MEDIA_STOP = 2101;
static const int OHOS_KEY_MEDIA_NEXT = 2102;
static const int OHOS_KEY_MEDIA_PREV = 2103;
static const int OHOS_KEY_MEDIA_VOLUME_UP = 2104;
static const int OHOS_KEY_MEDIA_VOLUME_DOWN = 2105;
static const int OHOS_KEY_MEDIA_VOLUME_MUTE = 2106;

// Modifier tracking
static std::atomic<bool> shift_pressed(false);
static std::atomic<bool> ctrl_pressed(false);
static std::atomic<bool> alt_pressed(false);
static std::atomic<bool> meta_pressed(false);

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
		case OHOS_KEY_MEDIA_PLAY_PAUSE:  return Key::MEDIAPLAY;
		case OHOS_KEY_MEDIA_STOP:        return Key::MEDIASTOP;
		case OHOS_KEY_MEDIA_NEXT:        return Key::MEDIANEXT;
		case OHOS_KEY_MEDIA_PREV:        return Key::MEDIAPREVIOUS;
		case OHOS_KEY_MEDIA_VOLUME_UP:   return Key::VOLUMEUP;
		case OHOS_KEY_MEDIA_VOLUME_DOWN: return Key::VOLUMEDOWN;
		case OHOS_KEY_MEDIA_VOLUME_MUTE: return Key::VOLUMEMUTE;
	}

	return Key::NONE;
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

	if (key_text && std::strlen(key_text) > 0) {
		key_event->set_key_label(godot_key);
		key_event->set_unicode(static_cast<char32_t>(key_text[0]));
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
	if (action == 2) {
		// Motion event
		Ref<InputEventMouseMotion> motion_event;
		motion_event.instantiate();
		motion_event->set_position(Vector2(x, y));
		motion_event->set_relative(Vector2(offset_x, offset_y));

		MouseButtonMask mask = MouseButtonMask(
			MouseButtonMask::LEFT | MouseButtonMask::MIDDLE | MouseButtonMask::RIGHT);
		motion_event->set_button_mask(mask);

		Input::get_singleton()->parse_input_event(motion_event);
		return;
	}

	Ref<InputEventMouseButton> mouse_event;
	mouse_event.instantiate();
	mouse_event->set_position(Vector2(x, y));

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
	mouse_event->set_button_mask(mouse_button_to_mask(mouse_button));

	Input::get_singleton()->parse_input_event(mouse_event);
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

	Ref<InputEventKey> key_event;
	key_event.instantiate();
	key_event->set_pressed(true);
	key_event->set_keycode(Key::NONE);
	key_event->set_physical_keycode(Key::NONE);
	key_event->set_key_label(Key::NONE);

	for (const char *c = text; *c != '\0'; c++) {
		if (*c > 0) {
			key_event->set_unicode(static_cast<char32_t>(*c));
			Input::get_singleton()->parse_input_event(key_event);
		}
	}
}

} // namespace HarmonyOSInput
