/**************************************************************************/
/*  audio_driver_ohos.h - OHAudio AudioDriver for HarmonyOS               */
/**************************************************************************/

#pragma once

#include "core/os/mutex.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "servers/audio/audio_driver.h"

class AudioDriverOHAudio : public AudioDriver {
	Mutex mutex;

	// Engine mix buffer (int32 interleaved, written by audio_server_process).
	Vector<int32_t> samples_in;

	unsigned int buffer_frames = 0;
	unsigned int mix_rate = 0;
	int channels = 0;
	SpeakerMode speaker_mode = SPEAKER_MODE_STEREO;

	// Called on the OHAudio render thread when it needs PCM data.
	static int32_t _audio_data_callback(float *buffer, int32_t num_frames, void *user_data);
	int32_t mix_to_buffer(float *buffer, int32_t num_frames);

public:
	virtual const char *get_name() const override {
		return "OHAudio";
	}

	virtual Error init() override;
	virtual void start() override;
	virtual int get_mix_rate() const override { return (int)mix_rate; }
	virtual SpeakerMode get_speaker_mode() const override { return speaker_mode; }
	virtual float get_latency() override { return 0.0f; }

	virtual void lock() override;
	virtual void unlock() override;
	virtual void finish() override;

	AudioDriverOHAudio() {}
	virtual ~AudioDriverOHAudio() {}
};
