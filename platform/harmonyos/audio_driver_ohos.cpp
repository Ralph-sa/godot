/**************************************************************************/
/*  audio_driver_ohos.cpp - OHAudio AudioDriver for HarmonyOS             */
/**************************************************************************/

#include "audio_driver_ohos.h"

#include "harmonyos_audio.h"

#include "core/math/math_funcs.h"
#include "core/os/os.h"

#ifdef HARMONYOS_ENABLED
#include <hilog/log.h>
#endif

Error AudioDriverOHAudio::init() {
	// Query the negotiated stream parameters from the OHAudio layer.
	mix_rate = (unsigned int)HarmonyOSAudio::get_sample_rate();
	channels = HarmonyOSAudio::get_channel_count();
	buffer_frames = (unsigned int)HarmonyOSAudio::get_buffer_size_frames();
	speaker_mode = SPEAKER_MODE_STEREO;

	if (buffer_frames == 0) {
		buffer_frames = 1024;
	}
	if (mix_rate == 0) {
		mix_rate = 48000;
	}
	if (channels == 0) {
		channels = 2;
	}

	samples_in.resize(buffer_frames * channels);

	// Route the OHAudio write callback back into Godot's mixer.
	HarmonyOSAudio::set_audio_data_callback(_audio_data_callback, this);

	if (!HarmonyOSAudio::init_audio()) {
#ifdef HARMONYOS_ENABLED
		OH_LOG_ERROR(LOG_APP, "AudioDriverOHAudio: failed to initialize OHAudio stream");
#endif
		return ERR_CANT_CREATE;
	}

#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "AudioDriverOHAudio: initialized (rate=%{public}d, ch=%{public}d, frames=%{public}d)",
			(int)mix_rate, channels, (int)buffer_frames);
#endif
	return OK;
}

void AudioDriverOHAudio::start() {
	if (!HarmonyOSAudio::start_audio()) {
#ifdef HARMONYOS_ENABLED
		OH_LOG_WARN(LOG_APP, "AudioDriverOHAudio: failed to start playback");
#endif
	}
}

void AudioDriverOHAudio::finish() {
	HarmonyOSAudio::set_audio_data_callback(nullptr, nullptr);
	HarmonyOSAudio::shutdown_audio();
}

void AudioDriverOHAudio::lock() {
	mutex.lock();
}

void AudioDriverOHAudio::unlock() {
	mutex.unlock();
}

int32_t AudioDriverOHAudio::_audio_data_callback(float *buffer, int32_t num_frames, void *user_data) {
	AudioDriverOHAudio *ad = static_cast<AudioDriverOHAudio *>(user_data);
	if (!ad) {
		return 0;
	}
	return ad->mix_to_buffer(buffer, num_frames);
}

int32_t AudioDriverOHAudio::mix_to_buffer(float *buffer, int32_t num_frames) {
	lock();

	int32_t frames = MIN(num_frames, (int32_t)buffer_frames);

	// Mix the engine's audio graph into the int32 interleaved buffer.
	audio_server_process(frames, samples_in.ptrw());

	// Convert int32 interleaved samples to the F32LE format OHAudio expects.
	const float inv = 1.0f / 2147483648.0f;
	for (int32_t i = 0; i < frames * channels; i++) {
		buffer[i] = (float)samples_in[i] * inv;
	}

	unlock();

	return frames;
}
