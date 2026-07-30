/**************************************************************************/
/*  harmonyos_audio.cpp - OHAudio Driver Stub                             */
/**************************************************************************/

#include "harmonyos_audio.h"

#include <hilog/log.h>

// Full OHAudio integration via OH_AudioStreamBuilder API
// will be implemented in a future phase.
// See: https://developer.huawei.com/consumer/en/doc/harmonyos-guides/audio-using-ohaudio

namespace HarmonyOSAudio {

static float g_master_volume = 1.0f;
static bool g_audio_initialized = false;

bool init_audio() {
	if (g_audio_initialized) return true;

	OH_LOG_INFO(LOG_APP, "OHAudio: Initializing audio driver...");

	// Step 1: Create audio stream builder
	// OH_AudioStreamBuilder *builder = nullptr;
	// OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);

	// Step 2: Set audio stream parameters (sample rate, channels, format)
	// OH_AudioStreamBuilder_SetSamplingRate(builder, 48000);
	// OH_AudioStreamBuilder_SetChannelCount(builder, 2);
	// OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_F32LE);
	// etc.

	g_audio_initialized = true;
	OH_LOG_INFO(LOG_APP, "OHAudio: Audio driver initialized (stub)");
	return true;
}

void shutdown_audio() {
	if (!g_audio_initialized) return;

	g_audio_initialized = false;
	OH_LOG_INFO(LOG_APP, "OHAudio: Audio driver shutdown");
}

void set_master_volume(float volume) {
	g_master_volume = volume;
}

float get_master_volume() {
	return g_master_volume;
}

} // namespace HarmonyOSAudio
