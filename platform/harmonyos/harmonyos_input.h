/**************************************************************************/
/*  harmonyos_input.h - HarmonyOS Input Event Translation                 */
/**************************************************************************/

#pragma once

#include "core/input/input_enums.h"
#include "core/input/input_event.h"

namespace HarmonyOSInput {

// Translate OHOS key code to Godot Key enum.
::Key ohos_key_to_godot(int ohos_keycode);

// Side of the keyboard a modifier key sits on; UNSPECIFIED for all other keys.
::KeyLocation ohos_key_location(int ohos_keycode);

// Process keyboard event.
void process_key_event(int key_code, int event_type, const char *key_text);

// Process mouse button/motion event.
// button: 0=left, 1=middle, 2=right, 3=xbutton1, 4=xbutton2
// action: 0=press, 1=release, 2=move
void process_mouse_event(int button, int action, double x, double y,
                         double offset_x, double offset_y);

// Process mouse wheel event. Offsets are in wheel notches; positive
// offset_y scrolls up, positive offset_x scrolls right.
void process_mouse_scroll_event(double x, double y, double offset_x, double offset_y);

// Process touch event.
// action: 0=down, 1=up, 2=move
void process_touch_event(int touch_id, int action, double x, double y);

// Process IME text input.
void process_input_text(const char *text);

// Current held mouse button bitmask (MouseButtonMask).
MouseButtonMask get_mouse_button_mask();

// Last known pointer position, updated by process_mouse_event().
Point2i get_mouse_position();

} // namespace HarmonyOSInput
