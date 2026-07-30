/**************************************************************************/
/*  harmonyos_audio.h - OHAudio Driver                                    */
/**************************************************************************/

#pragma once

#include <atomic>
#include <ohaudio/native_audiostreambuilder.h>
#include <ohaudio/native_audiorenderer.h>

namespace HarmonyOSAudio {

// Callback type for audio data requests from the engine
// Returns number of frames written. Called when OHAudio needs more PCM data.
using AudioDataCallback = int32_t (*)(float *buffer, int32_t num_frames, void *user_data);

// Initialize audio driver via OHAudio
// Returns true on success, false on failure.
bool init_audio();

// Cleanup audio resources
void shutdown_audio();

// Start playback
bool start_audio();

// Stop playback
bool stop_audio();

// Pause playback
bool pause_audio();

// Set the audio data callback
void set_audio_data_callback(AudioDataCallback callback, void *user_data);

// Set master volume (0.0 - 1.0)
void set_master_volume(float volume);

// Get current master volume
float get_master_volume();

// Check if audio is currently playing
bool is_playing();

// Get recommended buffer size in frames
int32_t get_buffer_size_frames();

// Get sample rate
int32_t get_sample_rate();

// Get channel count
int32_t get_channel_count();

} // namespace HarmonyOSAudio
