/**************************************************************************/
/*  audio_driver_ohos.h                                                   */
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

#pragma once

#include "core/os/mutex.h"
#include "servers/audio/audio_driver.h"

#include <ohaudio/native_audiorenderer.h>

/* AudioDriverOHOS：基于 OHOS NDK OHAudio 的音频输出驱动。
 *
 * 对应 macOS 的 AudioDriverCoreAudio（AudioQueue/AudioUnit）。
 * 采用 OHAudio 渲染流（AUDIOSTREAM_TYPE_RENDERER）：
 *   - 采样率 48000Hz、双声道、F32LE；
 *   - OHAudio 独立音频线程回调 on_write_data，填充 Godot 混音输出；
 *   - Godot 混音为 int32 样本，需转换为 float（÷2^31）写入 OHAudio 缓冲。
 *
 * 第 6 轮（音频/显示/物理存储完整期）。
 */
class AudioDriverOHOS : public AudioDriver {
private:
	OH_AudioStreamBuilder *builder = nullptr;
	OH_AudioRenderer *renderer = nullptr;

	// 配置（init 时固定）
	int mix_rate = 48000;
	int channels = 2;
	int buffer_frames = 0; // OHAudio 每回调帧数

	// 混音中间缓冲（Godot int32 样本，转 float 后写入 OHAudio）
	Vector<int32_t> mix_buffer;

	// 线程锁：OHAudio 回调线程 vs 引擎主线程
	Mutex mutex;

	// OHAudio 回调：请求填充音频数据（运行于 OHAudio 音频线程）
	static OH_AudioData_Callback_Result on_write_data(OH_AudioRenderer *p_renderer, void *p_user_data, void *p_audio_data, int32_t p_audio_data_size);

public:
	const char *get_name() const override { return "OHAudio"; }

	Error init() override;
	void start() override;
	int get_mix_rate() const override { return mix_rate; }
	SpeakerMode get_speaker_mode() const override { return SPEAKER_MODE_STEREO; }
	float get_latency() override;

	void lock() override;
	void unlock() override;
	void finish() override;
};
