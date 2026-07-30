/**************************************************************************/
/*  harmonyos_main.h - Godot Engine Lifecycle Interface                   */
/**************************************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Engine lifecycle
int harmonyos_godot_init();
void harmonyos_godot_cleanup();

// Surface lifecycle (called from NAPI bridge)
int harmonyos_godot_surface_created(const char *surface_id);
int harmonyos_godot_surface_destroy();

// Input events (forwarded from ArkTS)
void harmonyos_godot_key_event(int key_code, int event_type, const char *key_text);
void harmonyos_godot_mouse_event(int button, int action, double x, double y,
                                  double offset_x, double offset_y);
void harmonyos_godot_touch_event(int touch_id, int action, double x, double y);
void harmonyos_godot_input_text(const char *text);

// Application lifecycle
void harmonyos_godot_on_pause();
void harmonyos_godot_on_resume();
void harmonyos_godot_on_back_press();

// Terminate
void harmonyos_godot_terminate(int exit_code);

#ifdef __cplusplus
}
#endif
