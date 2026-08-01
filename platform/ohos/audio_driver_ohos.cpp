/**************************************************************************/
/*  audio_driver_ohos.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "audio_driver_ohos.h"

#include "core/os/mutex.h"
#include "core/string/print_string.h"
#include "core/templates/vector.h"
#include "servers/audio/audio_driver.h"

#include <ohaudio/native_audiostreambuilder.h>

Error AudioDriverOHOS::init() {
	// 创建 OHAudio 渲染流构建器
	OH_AudioStream_Result res = OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER);
	if (res != AUDIOSTREAM_SUCCESS || !builder) {
		ERR_PRINT(vformat("AudioDriverOHOS: builder create failed (%d)", static_cast<int>(res)));
		return ERR_CANT_OPEN;
	}

	// 采样率 48000 / 双声道 / F32LE（对应 macOS CoreAudio 常用配置）
	OH_AudioStreamBuilder_SetSamplingRate(builder, mix_rate);
	OH_AudioStreamBuilder_SetChannelCount(builder, channels);
	OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_F32LE);
	OH_AudioStreamBuilder_SetLatencyMode(builder, AUDIOSTREAM_LATENCY_MODE_NORMAL);

	// 注册数据回调（OHAudio 独立音频线程调用）
	OH_AudioStreamBuilder_SetRendererWriteDataCallback(builder, on_write_data, this);

	// 生成渲染器
	res = OH_AudioStreamBuilder_GenerateRenderer(builder, &renderer);
	if (res != AUDIOSTREAM_SUCCESS || !renderer) {
		ERR_PRINT(vformat("AudioDriverOHOS: renderer create failed (%d)", static_cast<int>(res)));
		OH_AudioStreamBuilder_Destroy(builder);
		builder = nullptr;
		return ERR_CANT_OPEN;
	}

	// 查询回调帧数（一次回调需填充的帧数）
	OH_AudioRenderer_GetFrameSizeInCallback(renderer, &buffer_frames);
	if (buffer_frames <= 0) {
		buffer_frames = 1024; // 兜底
	}
	mix_buffer.resize(buffer_frames * channels);

	// 构建器用完即销毁（渲染器独立持有配置）
	OH_AudioStreamBuilder_Destroy(builder);
	builder = nullptr;

	print_verbose(vformat("AudioDriverOHOS: init ok, rate=%d ch=%d frames=%d", mix_rate, channels, buffer_frames));
	return OK;
}

void AudioDriverOHOS::start() {
	if (!renderer) {
		return;
	}
	OH_AudioRenderer_Start(renderer);
}

OH_AudioData_Callback_Result AudioDriverOHOS::on_write_data(OH_AudioRenderer *p_renderer, void *p_user_data, void *p_audio_data, int32_t p_audio_data_size) {
	// OHAudio 音频线程回调：填充音频数据
	AudioDriverOHOS *driver = static_cast<AudioDriverOHOS *>(p_user_data);
	driver->lock();

	// 帧数 = 字节数 / (声道数 * sizeof(float))
	int frames = p_audio_data_size / (driver->channels * sizeof(float));
	driver->mix_buffer.resize(frames * driver->channels);
	driver->audio_server_process(frames, driver->mix_buffer.ptrw());

	// int32（Godot 混音）-> float（OHAudio F32LE）
	float *out = static_cast<float *>(p_audio_data);
	const float inv = 1.0f / 2147483648.0f;
	for (int i = 0; i < frames * driver->channels; i++) {
		out[i] = driver->mix_buffer[i] * inv;
	}

	driver->unlock();
	return AUDIO_DATA_CALLBACK_RESULT_VALID;
}

float AudioDriverOHOS::get_latency() {
	// 估算输出延迟：缓冲帧数 / 采样率
	if (mix_rate <= 0) {
		return 0.0f;
	}
	return float(buffer_frames) / float(mix_rate);
}

void AudioDriverOHOS::lock() {
	mutex.lock();
}

void AudioDriverOHOS::unlock() {
	mutex.unlock();
}

void AudioDriverOHOS::finish() {
	if (renderer) {
		OH_AudioRenderer_Stop(renderer);
		OH_AudioRenderer_Release(renderer);
		renderer = nullptr;
	}
}
