/**************************************************************************/
/*  tts_harmonyos.cpp - TTS Framework Stub for HarmonyOS                  */
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

// OHOS NDK 当前无 TTS API，预留框架供未来对接。
// 当 HarmonyOS SDK 提供原生 TTS 接口后，在此文件中通过 OH_TTS_* 调用
// 实现真实的语音合成功能，并接入 DisplayServer 的 tts_* 虚方法。

#include "tts_harmonyos.h"

TTS_HarmonyOS *TTS_HarmonyOS::singleton = nullptr;

TTS_HarmonyOS *TTS_HarmonyOS::get_singleton() {
	return singleton;
}

bool TTS_HarmonyOS::is_speaking() const {
	return false;
}

bool TTS_HarmonyOS::is_paused() const {
	return false;
}

Array TTS_HarmonyOS::get_voices() const {
	return Array();
}

void TTS_HarmonyOS::speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int64_t p_utterance_id, bool p_interrupt) {
	// Stub: no TTS API available in OHOS NDK yet
}

void TTS_HarmonyOS::pause() {
	// Stub: no TTS API available in OHOS NDK yet
}

void TTS_HarmonyOS::resume() {
	// Stub: no TTS API available in OHOS NDK yet
}

void TTS_HarmonyOS::stop() {
	// Stub: no TTS API available in OHOS NDK yet
}

TTS_HarmonyOS::TTS_HarmonyOS() {
	singleton = this;
}

TTS_HarmonyOS::~TTS_HarmonyOS() {
	singleton = nullptr;
}
