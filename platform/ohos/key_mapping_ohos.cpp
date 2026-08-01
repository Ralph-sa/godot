/**************************************************************************/
/*  key_mapping_ohos.cpp                                                  */
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

#include "key_mapping_ohos.h"

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"

// 鸿蒙 KeyCode 枚举值（与 @ohos.multimodalInput.KeyCode 一致，体系沿袭 AOSP）。
// 完整枚举见 DevEco SDK：apis/@ohos.multimodalInput.d.ts。
// 第 7 轮（输入完整期）将以 SDK 实际枚举逐项核对补全。
enum OHOS_KeyCode {
	KEYCODE_UNKNOWN = -1,
	KEYCODE_0 = 7,
	KEYCODE_1 = 8,
	KEYCODE_2 = 9,
	KEYCODE_3 = 10,
	KEYCODE_4 = 11,
	KEYCODE_5 = 12,
	KEYCODE_6 = 13,
	KEYCODE_7 = 14,
	KEYCODE_8 = 15,
	KEYCODE_9 = 16,
	KEYCODE_DPAD_UP = 19,
	KEYCODE_DPAD_DOWN = 20,
	KEYCODE_DPAD_LEFT = 21,
	KEYCODE_DPAD_RIGHT = 22,
	KEYCODE_A = 29,
	KEYCODE_B = 30,
	KEYCODE_C = 31,
	KEYCODE_D = 32,
	KEYCODE_E = 33,
	KEYCODE_F = 34,
	KEYCODE_G = 35,
	KEYCODE_H = 36,
	KEYCODE_I = 37,
	KEYCODE_J = 38,
	KEYCODE_K = 39,
	KEYCODE_L = 40,
	KEYCODE_M = 41,
	KEYCODE_N = 42,
	KEYCODE_O = 43,
	KEYCODE_P = 44,
	KEYCODE_Q = 45,
	KEYCODE_R = 46,
	KEYCODE_S = 47,
	KEYCODE_T = 48,
	KEYCODE_U = 49,
	KEYCODE_V = 50,
	KEYCODE_W = 51,
	KEYCODE_X = 52,
	KEYCODE_Y = 53,
	KEYCODE_Z = 54,
	KEYCODE_COMMA = 55,
	KEYCODE_PERIOD = 56,
	KEYCODE_ALT_LEFT = 57,
	KEYCODE_ALT_RIGHT = 58,
	KEYCODE_SHIFT_LEFT = 59,
	KEYCODE_SHIFT_RIGHT = 60,
	KEYCODE_TAB = 61,
	KEYCODE_SPACE = 62,
	KEYCODE_ENTER = 66,
	KEYCODE_BACKSPACE = 67,
	KEYCODE_BACKTICK = 68,
	KEYCODE_MINUS = 69,
	KEYCODE_EQUALS = 70,
	KEYCODE_LEFT_BRACKET = 71,
	KEYCODE_RIGHT_BRACKET = 72,
	KEYCODE_BACKSLASH = 73,
	KEYCODE_SEMICOLON = 74,
	KEYCODE_APOSTROPHE = 75,
	KEYCODE_SLASH = 76,
	KEYCODE_PAGE_UP = 92,
	KEYCODE_PAGE_DOWN = 93,
	KEYCODE_ESCAPE = 111,
	KEYCODE_DELETE = 112,
	KEYCODE_CTRL_LEFT = 113,
	KEYCODE_CTRL_RIGHT = 114,
	KEYCODE_CAPS_LOCK = 115,
	KEYCODE_META_LEFT = 117,
	KEYCODE_META_RIGHT = 118,
	KEYCODE_MOVE_HOME = 122,
	KEYCODE_MOVE_END = 123,
	KEYCODE_F1 = 131,
	KEYCODE_F2 = 132,
	KEYCODE_F3 = 133,
	KEYCODE_F4 = 134,
	KEYCODE_F5 = 135,
	KEYCODE_F6 = 136,
	KEYCODE_F7 = 137,
	KEYCODE_F8 = 138,
	KEYCODE_F9 = 139,
	KEYCODE_F10 = 140,
	KEYCODE_F11 = 141,
	KEYCODE_F12 = 142,
};

// 静态映射表：鸿蒙 KeyCode -> Godot Key
static HashMap<unsigned int, Key> keysym_map;

Key KeyMappingOHOS::translate_key(unsigned int p_key) {
	// 懒加载映射表（骨架期先做常用键映射，第 7 轮按完整枚举表替换）
	if (keysym_map.is_empty()) {
		keysym_map[KEYCODE_A] = Key::A;
		keysym_map[KEYCODE_B] = Key::B;
		keysym_map[KEYCODE_C] = Key::C;
		keysym_map[KEYCODE_D] = Key::D;
		keysym_map[KEYCODE_E] = Key::E;
		keysym_map[KEYCODE_F] = Key::F;
		keysym_map[KEYCODE_G] = Key::G;
		keysym_map[KEYCODE_H] = Key::H;
		keysym_map[KEYCODE_I] = Key::I;
		keysym_map[KEYCODE_J] = Key::J;
		keysym_map[KEYCODE_K] = Key::K;
		keysym_map[KEYCODE_L] = Key::L;
		keysym_map[KEYCODE_M] = Key::M;
		keysym_map[KEYCODE_N] = Key::N;
		keysym_map[KEYCODE_O] = Key::O;
		keysym_map[KEYCODE_P] = Key::P;
		keysym_map[KEYCODE_Q] = Key::Q;
		keysym_map[KEYCODE_R] = Key::R;
		keysym_map[KEYCODE_S] = Key::S;
		keysym_map[KEYCODE_T] = Key::T;
		keysym_map[KEYCODE_U] = Key::U;
		keysym_map[KEYCODE_V] = Key::V;
		keysym_map[KEYCODE_W] = Key::W;
		keysym_map[KEYCODE_X] = Key::X;
		keysym_map[KEYCODE_Y] = Key::Y;
		keysym_map[KEYCODE_Z] = Key::Z;
		keysym_map[KEYCODE_0] = Key::KEY_0;
		keysym_map[KEYCODE_1] = Key::KEY_1;
		keysym_map[KEYCODE_2] = Key::KEY_2;
		keysym_map[KEYCODE_3] = Key::KEY_3;
		keysym_map[KEYCODE_4] = Key::KEY_4;
		keysym_map[KEYCODE_5] = Key::KEY_5;
		keysym_map[KEYCODE_6] = Key::KEY_6;
		keysym_map[KEYCODE_7] = Key::KEY_7;
		keysym_map[KEYCODE_8] = Key::KEY_8;
		keysym_map[KEYCODE_9] = Key::KEY_9;
		keysym_map[KEYCODE_ENTER] = Key::ENTER;
		keysym_map[KEYCODE_ESCAPE] = Key::ESCAPE;
		keysym_map[KEYCODE_TAB] = Key::TAB;
		keysym_map[KEYCODE_BACKSPACE] = Key::BACKSPACE;
		keysym_map[KEYCODE_DELETE] = Key::KEY_DELETE;
		keysym_map[KEYCODE_MOVE_HOME] = Key::HOME;
		keysym_map[KEYCODE_MOVE_END] = Key::END;
		keysym_map[KEYCODE_PAGE_UP] = Key::PAGEUP;
		keysym_map[KEYCODE_PAGE_DOWN] = Key::PAGEDOWN;
		keysym_map[KEYCODE_DPAD_LEFT] = Key::LEFT;
		keysym_map[KEYCODE_DPAD_UP] = Key::UP;
		keysym_map[KEYCODE_DPAD_RIGHT] = Key::RIGHT;
		keysym_map[KEYCODE_DPAD_DOWN] = Key::DOWN;
		keysym_map[KEYCODE_SHIFT_LEFT] = Key::SHIFT;
		keysym_map[KEYCODE_SHIFT_RIGHT] = Key::SHIFT;
		keysym_map[KEYCODE_CTRL_LEFT] = Key::CTRL;
		keysym_map[KEYCODE_CTRL_RIGHT] = Key::CTRL;
		keysym_map[KEYCODE_ALT_LEFT] = Key::ALT;
		keysym_map[KEYCODE_ALT_RIGHT] = Key::ALT;
		keysym_map[KEYCODE_META_LEFT] = Key::META;
		keysym_map[KEYCODE_META_RIGHT] = Key::META;
		keysym_map[KEYCODE_CAPS_LOCK] = Key::CAPSLOCK;
		keysym_map[KEYCODE_SPACE] = Key::SPACE;
		keysym_map[KEYCODE_MINUS] = Key::MINUS;
		keysym_map[KEYCODE_EQUALS] = Key::EQUAL;
		keysym_map[KEYCODE_LEFT_BRACKET] = Key::BRACKETLEFT;
		keysym_map[KEYCODE_RIGHT_BRACKET] = Key::BRACKETRIGHT;
		keysym_map[KEYCODE_BACKSLASH] = Key::BACKSLASH;
		keysym_map[KEYCODE_SEMICOLON] = Key::SEMICOLON;
		keysym_map[KEYCODE_APOSTROPHE] = Key::APOSTROPHE;
		keysym_map[KEYCODE_COMMA] = Key::COMMA;
		keysym_map[KEYCODE_PERIOD] = Key::PERIOD;
		keysym_map[KEYCODE_SLASH] = Key::SLASH;
		keysym_map[KEYCODE_BACKTICK] = Key::QUOTELEFT;
		keysym_map[KEYCODE_F1] = Key::F1;
		keysym_map[KEYCODE_F2] = Key::F2;
		keysym_map[KEYCODE_F3] = Key::F3;
		keysym_map[KEYCODE_F4] = Key::F4;
		keysym_map[KEYCODE_F5] = Key::F5;
		keysym_map[KEYCODE_F6] = Key::F6;
		keysym_map[KEYCODE_F7] = Key::F7;
		keysym_map[KEYCODE_F8] = Key::F8;
		keysym_map[KEYCODE_F9] = Key::F9;
		keysym_map[KEYCODE_F10] = Key::F10;
		keysym_map[KEYCODE_F11] = Key::F11;
		keysym_map[KEYCODE_F12] = Key::F12;
	}

	// 查询映射表；未命中返回 Key::NONE（调用方可回退到 unicode 输入法事件）
	const Key *found = keysym_map.getptr(p_key);
	if (found) {
		return *found;
	}
	return Key::NONE;
}

KeyLocation KeyMappingOHOS::translate_location(unsigned int p_key) {
	// 区分左右修饰键（编辑器快捷键需要区分 Ctrl_L/Ctrl_R 等）
	switch (p_key) {
		case KEYCODE_SHIFT_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_SHIFT_RIGHT:
			return KeyLocation::RIGHT;
		case KEYCODE_CTRL_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_CTRL_RIGHT:
			return KeyLocation::RIGHT;
		case KEYCODE_ALT_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_ALT_RIGHT:
			return KeyLocation::RIGHT;
		case KEYCODE_META_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_META_RIGHT:
			return KeyLocation::RIGHT;
		default:
			return KeyLocation::UNSPECIFIED;
	}
}
