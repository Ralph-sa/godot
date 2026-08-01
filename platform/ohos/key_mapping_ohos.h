/**************************************************************************/
/*  key_mapping_ohos.h                                                    */
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

#include "core/os/keyboard.h"

/* KeyMappingOHOS：鸿蒙键盘键码（KeyCode）与 Godot Key 的映射。
 *
 * 对应 macOS 的 KeyMappingMacOS / X11 的 KeyMappingX11。
 * 鸿蒙 NAPI 键盘事件（OnDispatchKeyEvent）提供 KeyCode（见 @ohos.multimodalInput.KeyCode），
 * 本模块将其转换为 Godot 的 Key 枚举与 KeyLocation。
 *
 * 第 1 轮（骨架期）：提供 translate_key 的基本字母/数字/修饰键映射，
 * 完整扫描码映射表在第 7 轮（输入完整期）按鸿蒙 KeyCode 枚举补全。
 */
class KeyMappingOHOS {
	KeyMappingOHOS() {}

public:
	// 将鸿蒙 KeyCode 映射为 Godot Key（未知键返回 Key::NONE）
	static Key translate_key(unsigned int p_key);
	// 将鸿蒙 KeyCode 映射为键位（左右 Ctrl/Shift/Alt 区分）
	static KeyLocation translate_location(unsigned int p_key);
};
