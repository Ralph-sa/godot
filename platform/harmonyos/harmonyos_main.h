/**************************************************************************/
/*  harmonyos_main.h - Godot Engine Lifecycle Interface                   */
/**************************************************************************/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Engine lifecycle
int harmonyos_godot_init(const char *project_path, const char *files_dir, const char *cache_dir,
                         const char *temp_dir, const char *surface_id, int surface_width, int surface_height,
                         unsigned long long surface_generation, void *native_xcomponent);
void harmonyos_godot_start();
void harmonyos_godot_cleanup();

// Surface lifecycle (called from NAPI bridge)
int harmonyos_godot_surface_created(const char *surface_id, int surface_width, int surface_height,
                                    unsigned long long surface_generation);
int harmonyos_godot_surface_destroy(const char *surface_id, unsigned long long surface_generation);
int harmonyos_godot_set_xcomponent(void *native_xcomponent);
void harmonyos_godot_set_destroyed_surface_generation(unsigned long long surface_generation);

// Input events (forwarded from ArkTS)
void harmonyos_godot_key_event(int key_code, int event_type, const char *key_text);
void harmonyos_godot_mouse_event(int button, int action, double x, double y,
                                  double offset_x, double offset_y);
void harmonyos_godot_touch_event(int touch_id, int action, double x, double y);
void harmonyos_godot_input_text(const char *text);
void harmonyos_godot_ime_update(const char *text, int selection_start, int selection_length);

// Application lifecycle
void harmonyos_godot_on_pause();
void harmonyos_godot_on_resume();
void harmonyos_godot_on_back_press();

// Terminate
void harmonyos_godot_terminate(int exit_code);

#ifdef __cplusplus
}
#endif
