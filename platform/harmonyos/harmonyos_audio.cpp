/**************************************************************************/
/*  harmonyos_audio.cpp - OHAudio Driver Implementation                   */
/**************************************************************************/

#include "harmonyos_audio.h"

#include <hilog/log.h>
#include <cstring>

// ---- Constants ----
static constexpr int32_t DEFAULT_SAMPLE_RATE = 48000;
static constexpr int32_t DEFAULT_CHANNEL_COUNT = 2;
static constexpr int32_t DEFAULT_BUFFER_FRAMES = 1024;

namespace HarmonyOSAudio {

// ---- Global State ----
static bool g_initialized = false;
static bool g_playing = false;
static float g_master_volume = 1.0f;

static OH_AudioStreamBuilder *g_builder = nullptr;
static OH_AudioRenderer *g_renderer = nullptr;

static int32_t g_sample_rate = DEFAULT_SAMPLE_RATE;
static int32_t g_channel_count = DEFAULT_CHANNEL_COUNT;
static int32_t g_buffer_frames = DEFAULT_BUFFER_FRAMES;

static AudioDataCallback g_data_callback = nullptr;
static void *g_callback_user_data = nullptr;

// ---- OHAudio Callbacks ----

// Called by OHAudio when it needs more PCM data from the engine.
// We write silence initially and the engine fills the buffer later.
static int32_t OnWriteData(OH_AudioRenderer *renderer, void *user_data,
                           void *buffer, int32_t num_frames) {
    if (!g_data_callback || !g_playing) {
        // Fill with silence
        memset(buffer, 0, num_frames * g_channel_count * sizeof(float));
        return 0;
    }

    float *fbuf = static_cast<float *>(buffer);
    int32_t frames_written = g_data_callback(fbuf, num_frames, g_callback_user_data);

    // Fill remaining with silence if callback didn't fill all
    if (frames_written < num_frames) {
        memset(fbuf + (frames_written * g_channel_count), 0,
               (num_frames - frames_written) * g_channel_count * sizeof(float));
    }

    return 0;
}

// Called when an audio stream event occurs (error, underrun, etc.)
static int32_t OnStreamEvent(OH_AudioRenderer *renderer, void *user_data,
                              OH_AudioStream_Event event) {
    switch (event) {
        case AUDIOSTREAM_EVENT_UNDERRUN:
            OH_LOG_WARN(LOG_APP, "OHAudio: Buffer underrun");
            break;
        case AUDIOSTREAM_EVENT_OVERRUN:
            OH_LOG_WARN(LOG_APP, "OHAudio: Buffer overrun");
            break;
        default:
            break;
    }
    return 0;
}

// Called when an audio interruption occurs (e.g. phone call)
static int32_t OnInterruptEvent(OH_AudioRenderer *renderer, void *user_data,
                                OH_AudioInterrupt_ForceType type,
                                OH_AudioInterrupt_Hint hint) {
    OH_LOG_INFO(LOG_APP, "OHAudio: Interrupt, forceType=%{public}d, hint=%{public}d",
                static_cast<int>(type), static_cast<int>(hint));

    switch (type) {
        case AUDIOSTREAM_INTERRUPT_FORCE:
            // Forced stop — pause audio
            OH_AudioRenderer_Pause(g_renderer);
            break;
        case AUDIOSTREAM_INTERRUPT_SHARE:
            // Sharing — may need to lower volume
            break;
    }

    return 0;
}

// ---- Error helper ----
static const char *AudioErrorToString(OH_AudioStream_Result result) {
    switch (result) {
        case AUDIOSTREAM_SUCCESS: return "SUCCESS";
        case AUDIOSTREAM_ERROR_INVALID_PARAM: return "INVALID_PARAM";
        case AUDIOSTREAM_ERROR_ILLEGAL_STATE: return "ILLEGAL_STATE";
        case AUDIOSTREAM_ERROR_SYSTEM: return "SYSTEM_ERROR";
        default: return "UNKNOWN";
    }
}

// ---- Public API ----

bool init_audio() {
    if (g_initialized) return true;

    OH_LOG_INFO(LOG_APP, "OHAudio: Initializing audio driver (sampleRate=%{public}d, channels=%{public}d)...",
                g_sample_rate, g_channel_count);

    // Step 1: Create audio stream builder
    OH_AudioStream_Result result;
    result = OH_AudioStreamBuilder_Create(&g_builder, AUDIOSTREAM_TYPE_RENDERER);
    if (result != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "OHAudio: Failed to create builder: %{public}s",
                     AudioErrorToString(result));
        return false;
    }

    // Step 2: Set stream parameters
    OH_AudioStreamBuilder_SetSamplingRate(g_builder, g_sample_rate);
    OH_AudioStreamBuilder_SetChannelCount(g_builder, g_channel_count);
    OH_AudioStreamBuilder_SetSampleFormat(g_builder, AUDIOSTREAM_SAMPLE_F32LE);
    OH_AudioStreamBuilder_SetEncodingType(g_builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
    OH_AudioStreamBuilder_SetLatencyMode(g_builder, AUDIOSTREAM_LATENCY_MODE_FAST);

    // Step 3: Configure renderer usage for game/editor audio
    OH_AudioStream_Usage usage = AUDIOSTREAM_USAGE_GAME;
    OH_AudioStreamBuilder_SetRendererInfo(g_builder, usage);

    // Step 4: Set callbacks
    OH_AudioRenderer_Callbacks callbacks;
    callbacks.OH_AudioRenderer_OnWriteData = OnWriteData;
    callbacks.OH_AudioRenderer_OnStreamEvent = OnStreamEvent;
    callbacks.OH_AudioRenderer_OnInterruptEvent = OnInterruptEvent;
    callbacks.OH_AudioRenderer_OnError = nullptr;

    result = OH_AudioStreamBuilder_SetRendererCallback(g_builder, callbacks, nullptr);
    if (result != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "OHAudio: Failed to set callbacks: %{public}s",
                     AudioErrorToString(result));
        OH_AudioStreamBuilder_Destroy(g_builder);
        g_builder = nullptr;
        return false;
    }

    // Step 5: Generate renderer
    result = OH_AudioStreamBuilder_GenerateRenderer(g_builder, &g_renderer);
    if (result != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "OHAudio: Failed to generate renderer: %{public}s",
                     AudioErrorToString(result));
        OH_AudioStreamBuilder_Destroy(g_builder);
        g_builder = nullptr;
        return false;
    }

    // Step 6: Get actual buffer size
    uint32_t frame_size = 0;
    OH_AudioRenderer_GetFrameSizeInCallback(g_renderer, &frame_size);
    if (frame_size > 0) {
        g_buffer_frames = static_cast<int32_t>(frame_size);
    }

    g_initialized = true;
    OH_LOG_INFO(LOG_APP, "OHAudio: Audio driver initialized (buffer=%{public}d frames)",
                g_buffer_frames);
    return true;
}

void shutdown_audio() {
    if (!g_initialized) return;

    stop_audio();

    if (g_renderer) {
        OH_AudioRenderer_Release(g_renderer);
        g_renderer = nullptr;
    }

    if (g_builder) {
        OH_AudioStreamBuilder_Destroy(g_builder);
        g_builder = nullptr;
    }

    g_initialized = false;
    g_data_callback = nullptr;
    g_callback_user_data = nullptr;
    OH_LOG_INFO(LOG_APP, "OHAudio: Audio driver shutdown");
}

bool start_audio() {
    if (!g_initialized || !g_renderer) return false;

    if (g_playing) return true;

    OH_AudioStream_Result result = OH_AudioRenderer_Start(g_renderer);
    if (result != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "OHAudio: Failed to start renderer: %{public}s",
                     AudioErrorToString(result));
        return false;
    }

    g_playing = true;
    OH_LOG_INFO(LOG_APP, "OHAudio: Playback started");
    return true;
}

bool stop_audio() {
    if (!g_initialized || !g_renderer) return false;

    if (!g_playing) return true;

    OH_AudioStream_Result result = OH_AudioRenderer_Stop(g_renderer);
    if (result != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "OHAudio: Failed to stop renderer: %{public}s",
                     AudioErrorToString(result));
        return false;
    }

    g_playing = false;
    OH_LOG_INFO(LOG_APP, "OHAudio: Playback stopped");
    return true;
}

bool pause_audio() {
    if (!g_initialized || !g_renderer) return false;

    OH_AudioStream_Result result = OH_AudioRenderer_Pause(g_renderer);
    if (result != AUDIOSTREAM_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "OHAudio: Failed to pause renderer: %{public}s",
                     AudioErrorToString(result));
        return false;
    }

    g_playing = false;
    OH_LOG_INFO(LOG_APP, "OHAudio: Playback paused");
    return true;
}

void set_audio_data_callback(AudioDataCallback callback, void *user_data) {
    g_data_callback = callback;
    g_callback_user_data = user_data;
}

void set_master_volume(float volume) {
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    g_master_volume = volume;

    if (g_renderer) {
        float ohos_volume = volume; // OHAudio volume is 0.0 - 1.0
        OH_AudioRenderer_SetVolume(g_renderer, ohos_volume);
    }
}

float get_master_volume() {
    return g_master_volume;
}

bool is_playing() {
    return g_playing;
}

int32_t get_buffer_size_frames() {
    return g_buffer_frames;
}

int32_t get_sample_rate() {
    return g_sample_rate;
}

int32_t get_channel_count() {
    return g_channel_count;
}

} // namespace HarmonyOSAudio
