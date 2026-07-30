/**************************************************************************/
/*  harmonyos_input.h - HarmonyOS Input Event Translation                 */
/**************************************************************************/

#pragma once

#include "core/input/input.h"

// Entry point for Phase 5 input system integration
namespace HarmonyOSInput {

// Process a key event from ArkTS/NAPI bridge
void process_key_event(int key_code, int event_type, const char *key_text);

// Process mouse event from ArkTS/NAPI bridge
void process_mouse_event(int button, int action, double x, double y,
                         double offset_x, double offset_y);

// Process touch event from ArkTS/NAPI bridge
void process_touch_event(int touch_id, int action, double x, double y);

// Process text input from virtual keyboard
void process_input_text(const char *text);

// Convert OHOS keycode to Godot Key enum
Key ohos_key_to_godot(int ohos_keycode);

} // namespace HarmonyOSInput
