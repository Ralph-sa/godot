/**************************************************************************/
/*  harmonyos_audio.h - OHAudio Driver Stub (Phase 5)                     */
/**************************************************************************/

#pragma once

namespace HarmonyOSAudio {

// Initialize audio driver via OHAudio
bool init_audio();

// Cleanup audio resources
void shutdown_audio();

// Set master volume (0.0 - 1.0)
void set_master_volume(float volume);

// Get current master volume
float get_master_volume();

} // namespace HarmonyOSAudio
