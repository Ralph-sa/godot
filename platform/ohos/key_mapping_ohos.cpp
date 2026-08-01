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

// XComponent 键盘事件 KeyCode（OH_NativeXComponent_KeyCode，2000+ 偏移体系）。
// 与上述 multimodalInput KeyCode 不同，见 native_xcomponent_key_event.h。
enum OHOS_XC_KeyCode {
	XC_KEYCODE_HOME = 1,
	XC_KEYCODE_BACK = 2,
	XC_KEYCODE_0 = 2000,
	XC_KEYCODE_1 = 2001,
	XC_KEYCODE_2 = 2002,
	XC_KEYCODE_3 = 2003,
	XC_KEYCODE_4 = 2004,
	XC_KEYCODE_5 = 2005,
	XC_KEYCODE_6 = 2006,
	XC_KEYCODE_7 = 2007,
	XC_KEYCODE_8 = 2008,
	XC_KEYCODE_9 = 2009,
	XC_KEYCODE_DPAD_UP = 2012,
	XC_KEYCODE_DPAD_DOWN = 2013,
	XC_KEYCODE_DPAD_LEFT = 2014,
	XC_KEYCODE_DPAD_RIGHT = 2015,
	XC_KEYCODE_DPAD_CENTER = 2016,
	XC_KEYCODE_A = 2017,
	XC_KEYCODE_B = 2018,
	XC_KEYCODE_C = 2019,
	XC_KEYCODE_D = 2020,
	XC_KEYCODE_E = 2021,
	XC_KEYCODE_F = 2022,
	XC_KEYCODE_G = 2023,
	XC_KEYCODE_H = 2024,
	XC_KEYCODE_I = 2025,
	XC_KEYCODE_J = 2026,
	XC_KEYCODE_K = 2027,
	XC_KEYCODE_L = 2028,
	XC_KEYCODE_M = 2029,
	XC_KEYCODE_N = 2030,
	XC_KEYCODE_O = 2031,
	XC_KEYCODE_P = 2032,
	XC_KEYCODE_Q = 2033,
	XC_KEYCODE_R = 2034,
	XC_KEYCODE_S = 2035,
	XC_KEYCODE_T = 2036,
	XC_KEYCODE_U = 2037,
	XC_KEYCODE_V = 2038,
	XC_KEYCODE_W = 2039,
	XC_KEYCODE_X = 2040,
	XC_KEYCODE_Y = 2041,
	XC_KEYCODE_Z = 2042,
	XC_KEYCODE_COMMA = 2043,
	XC_KEYCODE_PERIOD = 2044,
	XC_KEYCODE_ALT_LEFT = 2045,
	XC_KEYCODE_ALT_RIGHT = 2046,
	XC_KEYCODE_SHIFT_LEFT = 2047,
	XC_KEYCODE_SHIFT_RIGHT = 2048,
	XC_KEYCODE_TAB = 2049,
	XC_KEYCODE_SPACE = 2050,
	XC_KEYCODE_ENTER = 2054,
	XC_KEYCODE_DEL = 2055,
	XC_KEYCODE_GRAVE = 2056,
	XC_KEYCODE_MINUS = 2057,
	XC_KEYCODE_EQUALS = 2058,
	XC_KEYCODE_LEFT_BRACKET = 2059,
	XC_KEYCODE_RIGHT_BRACKET = 2060,
	XC_KEYCODE_BACKSLASH = 2061,
	XC_KEYCODE_SEMICOLON = 2062,
	XC_KEYCODE_APOSTROPHE = 2063,
	XC_KEYCODE_SLASH = 2064,
	XC_KEYCODE_PAGE_UP = 2068,
	XC_KEYCODE_PAGE_DOWN = 2069,
	XC_KEYCODE_ESCAPE = 2070,
	XC_KEYCODE_FORWARD_DEL = 2071,
	XC_KEYCODE_CTRL_LEFT = 2072,
	XC_KEYCODE_CTRL_RIGHT = 2073,
	XC_KEYCODE_CAPS_LOCK = 2074,
	XC_KEYCODE_META_LEFT = 2076,
	XC_KEYCODE_META_RIGHT = 2077,
	XC_KEYCODE_MOVE_HOME = 2081,
	XC_KEYCODE_MOVE_END = 2082,
	XC_KEYCODE_INSERT = 2083,
	XC_KEYCODE_F1 = 2090,
	XC_KEYCODE_F2 = 2091,
	XC_KEYCODE_F3 = 2092,
	XC_KEYCODE_F4 = 2093,
	XC_KEYCODE_F5 = 2094,
	XC_KEYCODE_F6 = 2095,
	XC_KEYCODE_F7 = 2096,
	XC_KEYCODE_F8 = 2097,
	XC_KEYCODE_F9 = 2098,
	XC_KEYCODE_F10 = 2099,
	XC_KEYCODE_F11 = 2100,
	XC_KEYCODE_F12 = 2101,
	XC_KEYCODE_NUM_LOCK = 2102,
	XC_KEYCODE_NUMPAD_0 = 2103,
	XC_KEYCODE_NUMPAD_1 = 2104,
	XC_KEYCODE_NUMPAD_2 = 2105,
	XC_KEYCODE_NUMPAD_3 = 2106,
	XC_KEYCODE_NUMPAD_4 = 2107,
	XC_KEYCODE_NUMPAD_5 = 2108,
	XC_KEYCODE_NUMPAD_6 = 2109,
	XC_KEYCODE_NUMPAD_7 = 2110,
	XC_KEYCODE_NUMPAD_8 = 2111,
	XC_KEYCODE_NUMPAD_9 = 2112,
	XC_KEYCODE_NUMPAD_DIVIDE = 2113,
	XC_KEYCODE_NUMPAD_MULTIPLY = 2114,
	XC_KEYCODE_NUMPAD_SUBTRACT = 2115,
	XC_KEYCODE_NUMPAD_ADD = 2116,
	XC_KEYCODE_NUMPAD_DOT = 2117,
	XC_KEYCODE_NUMPAD_ENTER = 2119,
};

// 静态映射表：鸿蒙 KeyCode -> Godot Key
static HashMap<unsigned int, Key> keysym_map;

// XComponent 键盘事件使用 OH_NativeXComponent_KeyCode（2000+ 偏移体系），
// 与 multimodalInput KeyCode（AOSP 风格）不同，需单独映射。
// 第 2 轮：覆盖 PC/2in1 编辑器所需的字母/数字/功能键/导航键。
static HashMap<unsigned int, Key> xcomponent_key_map;

Key KeyMappingOHOS::translate_key(unsigned int p_key) {
	// 懒加载映射表：先查 XComponent KeyCode（键盘事件来源），再查传统 KeyCode
	if (keysym_map.is_empty()) {
		// ---- 传统 multimodalInput KeyCode（AOSP 风格，值 0-2800） ----
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

		// ---- XComponent KeyCode（OH_NativeXComponent_KeyCode，2000+ 偏移） ----
		xcomponent_key_map[XC_KEYCODE_A] = Key::A;
		xcomponent_key_map[XC_KEYCODE_B] = Key::B;
		xcomponent_key_map[XC_KEYCODE_C] = Key::C;
		xcomponent_key_map[XC_KEYCODE_D] = Key::D;
		xcomponent_key_map[XC_KEYCODE_E] = Key::E;
		xcomponent_key_map[XC_KEYCODE_F] = Key::F;
		xcomponent_key_map[XC_KEYCODE_G] = Key::G;
		xcomponent_key_map[XC_KEYCODE_H] = Key::H;
		xcomponent_key_map[XC_KEYCODE_I] = Key::I;
		xcomponent_key_map[XC_KEYCODE_J] = Key::J;
		xcomponent_key_map[XC_KEYCODE_K] = Key::K;
		xcomponent_key_map[XC_KEYCODE_L] = Key::L;
		xcomponent_key_map[XC_KEYCODE_M] = Key::M;
		xcomponent_key_map[XC_KEYCODE_N] = Key::N;
		xcomponent_key_map[XC_KEYCODE_O] = Key::O;
		xcomponent_key_map[XC_KEYCODE_P] = Key::P;
		xcomponent_key_map[XC_KEYCODE_Q] = Key::Q;
		xcomponent_key_map[XC_KEYCODE_R] = Key::R;
		xcomponent_key_map[XC_KEYCODE_S] = Key::S;
		xcomponent_key_map[XC_KEYCODE_T] = Key::T;
		xcomponent_key_map[XC_KEYCODE_U] = Key::U;
		xcomponent_key_map[XC_KEYCODE_V] = Key::V;
		xcomponent_key_map[XC_KEYCODE_W] = Key::W;
		xcomponent_key_map[XC_KEYCODE_X] = Key::X;
		xcomponent_key_map[XC_KEYCODE_Y] = Key::Y;
		xcomponent_key_map[XC_KEYCODE_Z] = Key::Z;
		xcomponent_key_map[XC_KEYCODE_0] = Key::KEY_0;
		xcomponent_key_map[XC_KEYCODE_1] = Key::KEY_1;
		xcomponent_key_map[XC_KEYCODE_2] = Key::KEY_2;
		xcomponent_key_map[XC_KEYCODE_3] = Key::KEY_3;
		xcomponent_key_map[XC_KEYCODE_4] = Key::KEY_4;
		xcomponent_key_map[XC_KEYCODE_5] = Key::KEY_5;
		xcomponent_key_map[XC_KEYCODE_6] = Key::KEY_6;
		xcomponent_key_map[XC_KEYCODE_7] = Key::KEY_7;
		xcomponent_key_map[XC_KEYCODE_8] = Key::KEY_8;
		xcomponent_key_map[XC_KEYCODE_9] = Key::KEY_9;
		xcomponent_key_map[XC_KEYCODE_ENTER] = Key::ENTER;
		xcomponent_key_map[XC_KEYCODE_ESCAPE] = Key::ESCAPE;
		xcomponent_key_map[XC_KEYCODE_TAB] = Key::TAB;
		xcomponent_key_map[XC_KEYCODE_DEL] = Key::BACKSPACE;
		xcomponent_key_map[XC_KEYCODE_FORWARD_DEL] = Key::KEY_DELETE;
		xcomponent_key_map[XC_KEYCODE_MOVE_HOME] = Key::HOME;
		xcomponent_key_map[XC_KEYCODE_MOVE_END] = Key::END;
		xcomponent_key_map[XC_KEYCODE_PAGE_UP] = Key::PAGEUP;
		xcomponent_key_map[XC_KEYCODE_PAGE_DOWN] = Key::PAGEDOWN;
		xcomponent_key_map[XC_KEYCODE_DPAD_LEFT] = Key::LEFT;
		xcomponent_key_map[XC_KEYCODE_DPAD_UP] = Key::UP;
		xcomponent_key_map[XC_KEYCODE_DPAD_RIGHT] = Key::RIGHT;
		xcomponent_key_map[XC_KEYCODE_DPAD_DOWN] = Key::DOWN;
		xcomponent_key_map[XC_KEYCODE_SHIFT_LEFT] = Key::SHIFT;
		xcomponent_key_map[XC_KEYCODE_SHIFT_RIGHT] = Key::SHIFT;
		xcomponent_key_map[XC_KEYCODE_CTRL_LEFT] = Key::CTRL;
		xcomponent_key_map[XC_KEYCODE_CTRL_RIGHT] = Key::CTRL;
		xcomponent_key_map[XC_KEYCODE_ALT_LEFT] = Key::ALT;
		xcomponent_key_map[XC_KEYCODE_ALT_RIGHT] = Key::ALT;
		xcomponent_key_map[XC_KEYCODE_META_LEFT] = Key::META;
		xcomponent_key_map[XC_KEYCODE_META_RIGHT] = Key::META;
		xcomponent_key_map[XC_KEYCODE_CAPS_LOCK] = Key::CAPSLOCK;
		xcomponent_key_map[XC_KEYCODE_SPACE] = Key::SPACE;
		xcomponent_key_map[XC_KEYCODE_MINUS] = Key::MINUS;
		xcomponent_key_map[XC_KEYCODE_EQUALS] = Key::EQUAL;
		xcomponent_key_map[XC_KEYCODE_LEFT_BRACKET] = Key::BRACKETLEFT;
		xcomponent_key_map[XC_KEYCODE_RIGHT_BRACKET] = Key::BRACKETRIGHT;
		xcomponent_key_map[XC_KEYCODE_BACKSLASH] = Key::BACKSLASH;
		xcomponent_key_map[XC_KEYCODE_SEMICOLON] = Key::SEMICOLON;
		xcomponent_key_map[XC_KEYCODE_APOSTROPHE] = Key::APOSTROPHE;
		xcomponent_key_map[XC_KEYCODE_COMMA] = Key::COMMA;
		xcomponent_key_map[XC_KEYCODE_PERIOD] = Key::PERIOD;
		xcomponent_key_map[XC_KEYCODE_SLASH] = Key::SLASH;
		xcomponent_key_map[XC_KEYCODE_GRAVE] = Key::QUOTELEFT;
		xcomponent_key_map[XC_KEYCODE_INSERT] = Key::INSERT;
		xcomponent_key_map[XC_KEYCODE_F1] = Key::F1;
		xcomponent_key_map[XC_KEYCODE_F2] = Key::F2;
		xcomponent_key_map[XC_KEYCODE_F3] = Key::F3;
		xcomponent_key_map[XC_KEYCODE_F4] = Key::F4;
		xcomponent_key_map[XC_KEYCODE_F5] = Key::F5;
		xcomponent_key_map[XC_KEYCODE_F6] = Key::F6;
		xcomponent_key_map[XC_KEYCODE_F7] = Key::F7;
		xcomponent_key_map[XC_KEYCODE_F8] = Key::F8;
		xcomponent_key_map[XC_KEYCODE_F9] = Key::F9;
		xcomponent_key_map[XC_KEYCODE_F10] = Key::F10;
		xcomponent_key_map[XC_KEYCODE_F11] = Key::F11;
		xcomponent_key_map[XC_KEYCODE_F12] = Key::F12;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_0] = Key::KP_0;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_1] = Key::KP_1;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_2] = Key::KP_2;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_3] = Key::KP_3;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_4] = Key::KP_4;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_5] = Key::KP_5;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_6] = Key::KP_6;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_7] = Key::KP_7;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_8] = Key::KP_8;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_9] = Key::KP_9;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_DIVIDE] = Key::KP_DIVIDE;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_MULTIPLY] = Key::KP_MULTIPLY;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_SUBTRACT] = Key::KP_SUBTRACT;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_ADD] = Key::KP_ADD;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_DOT] = Key::KP_PERIOD;
		xcomponent_key_map[XC_KEYCODE_NUMPAD_ENTER] = Key::KP_ENTER;
		xcomponent_key_map[XC_KEYCODE_NUM_LOCK] = Key::NUMLOCK;
	}

	// 优先匹配 XComponent KeyCode（键盘事件源），其次传统 KeyCode
	const Key *found_xc = xcomponent_key_map.getptr(p_key);
	if (found_xc) {
		return *found_xc;
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
	// 同时支持传统 KeyCode 与 XComponent KeyCode 两种枚举
	switch (p_key) {
		case KEYCODE_SHIFT_LEFT:
		case XC_KEYCODE_SHIFT_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_SHIFT_RIGHT:
		case XC_KEYCODE_SHIFT_RIGHT:
			return KeyLocation::RIGHT;
		case KEYCODE_CTRL_LEFT:
		case XC_KEYCODE_CTRL_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_CTRL_RIGHT:
		case XC_KEYCODE_CTRL_RIGHT:
			return KeyLocation::RIGHT;
		case KEYCODE_ALT_LEFT:
		case XC_KEYCODE_ALT_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_ALT_RIGHT:
		case XC_KEYCODE_ALT_RIGHT:
			return KeyLocation::RIGHT;
		case KEYCODE_META_LEFT:
		case XC_KEYCODE_META_LEFT:
			return KeyLocation::LEFT;
		case KEYCODE_META_RIGHT:
		case XC_KEYCODE_META_RIGHT:
			return KeyLocation::RIGHT;
		default:
			return KeyLocation::UNSPECIFIED;
	}
}
