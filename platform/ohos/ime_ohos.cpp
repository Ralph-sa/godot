/**************************************************************************/
/*  ime_ohos.cpp                                                          */
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

#include "ime_ohos.h"

#include "display_server_ohos.h"
#include "ohos_bridge.h"
#include "ohos_xcomponent.h"

#include "core/string/print_string.h"

#include <inputmethod/inputmethod_attach_options_capi.h>
#include <inputmethod/inputmethod_controller_capi.h>
#include <inputmethod/inputmethod_cursor_info_capi.h>
#include <inputmethod/inputmethod_inputmethod_proxy_capi.h>
#include <inputmethod/inputmethod_text_config_capi.h>
#include <inputmethod/inputmethod_text_editor_proxy_capi.h>

// 单例（编辑器主窗口唯一输入法实例，由 DisplayServerOHOS 创建）
static IME_OHOS *ime_ohos_singleton = nullptr;

IME_OHOS *IME_OHOS::get_singleton() {
	return ime_ohos_singleton;
}

IME_OHOS::IME_OHOS() {
	ime_ohos_singleton = this;
}

IME_OHOS::~IME_OHOS() {
	// 分离输入法并释放 proxy/options
	detach();
	ime_ohos_singleton = nullptr;
}

Error IME_OHOS::attach() {
	MutexLock lock(mutex);
	if (attached) {
		return OK; // 幂等：重复聚焦不重复附加
	}

	// 1) 创建 TextEditorProxy（输入法 -> 应用回调集）
	proxy = OH_TextEditorProxy_Create();
	if (!proxy) {
		return ERR_CANT_CREATE;
	}

	// 注册编辑回调：插入文本/删除/回车/移动光标/选区/组合预览/文本配置
	OH_TextEditorProxy_SetInsertTextFunc(proxy, &IME_OHOS::on_insert_text);
	OH_TextEditorProxy_SetDeleteBackwardFunc(proxy, &IME_OHOS::on_delete_backward);
	OH_TextEditorProxy_SetDeleteForwardFunc(proxy, &IME_OHOS::on_delete_forward);
	OH_TextEditorProxy_SetSendEnterKeyFunc(proxy, &IME_OHOS::on_send_enter_key);
	OH_TextEditorProxy_SetMoveCursorFunc(proxy, &IME_OHOS::on_move_cursor);
	OH_TextEditorProxy_SetHandleSetSelectionFunc(proxy, &IME_OHOS::on_set_selection);
	OH_TextEditorProxy_SetSetPreviewTextFunc(proxy, &IME_OHOS::on_set_preview_text);
	OH_TextEditorProxy_SetGetTextConfigFunc(proxy, &IME_OHOS::on_get_text_config);
	// 回调回到 ArkUI 主线程：与 XComponent 输入事件同线程，可安全入队
	OH_TextEditorProxy_SetCallbackInMainThread(proxy, true);

	// 2) 附加选项：PC 编辑器使用物理键盘，不主动弹软键盘
	options = OH_AttachOptions_Create(false);
	if (!options) {
		OH_TextEditorProxy_Destroy(proxy);
		proxy = nullptr;
		return ERR_CANT_CREATE;
	}

	// 3) 附加输入法服务，获得 InputMethodProxy（应用 -> 输入法）
	InputMethod_ErrorCode err = OH_InputMethodController_Attach(proxy, options, &input_proxy);
	if (err != IME_ERR_OK) {
		print_verbose(vformat("IME_OHOS: attach failed (code %d)", static_cast<int>(err)));
		OH_AttachOptions_Destroy(options);
		OH_TextEditorProxy_Destroy(proxy);
		options = nullptr;
		proxy = nullptr;
		return ERR_CANT_ACQUIRE_RESOURCE;
	}

	attached = true;
	print_verbose("IME_OHOS: attached to input method service.");
	return OK;
}

void IME_OHOS::detach() {
	MutexLock lock(mutex);
	if (!attached) {
		return;
	}
	if (input_proxy) {
		OH_InputMethodController_Detach(input_proxy);
		input_proxy = nullptr;
	}
	if (proxy) {
		OH_TextEditorProxy_Destroy(proxy);
		proxy = nullptr;
	}
	if (options) {
		OH_AttachOptions_Destroy(options);
		options = nullptr;
	}
	attached = false;
	print_verbose("IME_OHOS: detached from input method service.");
}

void IME_OHOS::show_keyboard() {
	MutexLock lock(mutex);
	if (!attached || !input_proxy) {
		return;
	}
	// 触屏/2in1 场景请求软键盘显示
	OH_InputMethodProxy_ShowKeyboard(input_proxy);
}

void IME_OHOS::hide_keyboard() {
	MutexLock lock(mutex);
	if (!attached || !input_proxy) {
		return;
	}
	OH_InputMethodProxy_HideKeyboard(input_proxy);
}

void IME_OHOS::notify_cursor_rect(int p_x, int p_y, int p_w, int p_h) {
	MutexLock lock(mutex);
	if (!attached || !input_proxy) {
		return;
	}
	// 同步编辑器光标矩形给输入法，候选框据此定位（vp -> px 由引擎侧换算）
	InputMethod_CursorInfo *cursor = OH_CursorInfo_Create(p_x, p_y, p_w, p_h);
	if (!cursor) {
		return;
	}
	OH_InputMethodProxy_NotifyCursorUpdate(input_proxy, cursor);
	OH_CursorInfo_Destroy(cursor);
}

// ---- 输入法 -> 应用回调实现 ----

String IME_OHOS::_to_string(const char16_t *p_text, size_t p_length) {
	// Godot String::utf16 处理 UTF-16 代理对 -> 内部 char32_t
	if (!p_text || p_length == 0) {
		return String();
	}
	return String::utf16(p_text, static_cast<int>(p_length));
}

void IME_OHOS::_push_key_event(Key p_keycode, char32_t p_unicode, const String &p_text) {
	// 按键事件入队（主线程调用）：引擎线程在 process_events 消费，
	// 保证输入法组合文本与场景树操作线程安全（对应 macOS insertText 的 UI 线程投递）。
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (!ds || !ds->get_main_xcomponent()) {
		return;
	}
	ds->get_main_xcomponent()->push_input_event(p_text, p_keycode, p_unicode);
}

void IME_OHOS::on_insert_text(InputMethod_TextEditorProxy *p_proxy, const char16_t *p_text, size_t p_length) {
	// 输入法提交组合文本（中文拼音/手写等上屏）：包装为带文本的按键事件，
	// Godot TextEdit/LineEdit 依据 ev->get_text() 直接插入（对应 macOS insertText）。
	String text = _to_string(p_text, p_length);
	if (text.is_empty()) {
		return;
	}
	// 单字符提交用 unicode，多字符提交用完整文本
	char32_t unicode = text.length() == 1 ? text[0] : 0;
	_push_key_event(Key::NONE, unicode, text);
	print_verbose(vformat("IME_OHOS: commit text \"%s\"", text));
}

void IME_OHOS::on_delete_backward(InputMethod_TextEditorProxy *p_proxy, int32_t p_length) {
	// 输入法请求删除光标前文本（对应 macOS deleteBackward）
	_push_key_event(Key::BACKSPACE, 0, String());
	print_verbose(vformat("IME_OHOS: delete backward x%d", p_length));
}

void IME_OHOS::on_delete_forward(InputMethod_TextEditorProxy *p_proxy, int32_t p_length) {
	// 输入法请求删除光标后文本
	_push_key_event(Key::KEY_DELETE, 0, String());
	print_verbose(vformat("IME_OHOS: delete forward x%d", p_length));
}

void IME_OHOS::on_send_enter_key(InputMethod_TextEditorProxy *p_proxy, InputMethod_EnterKeyType p_enter_key_type) {
	// 输入法发送回车（搜索/完成等场景，对应 macOS insertNewline）
	_push_key_event(Key::ENTER, 0, String());
}

void IME_OHOS::on_move_cursor(InputMethod_TextEditorProxy *p_proxy, InputMethod_Direction p_direction) {
	// 输入法请求移动光标（候选框方向键选择）
	Key key = Key::NONE;
	switch (p_direction) {
		case IME_DIRECTION_UP:
			key = Key::UP;
			break;
		case IME_DIRECTION_DOWN:
			key = Key::DOWN;
			break;
		case IME_DIRECTION_LEFT:
			key = Key::LEFT;
			break;
		case IME_DIRECTION_RIGHT:
			key = Key::RIGHT;
			break;
		default:
			break;
	}
	if (key != Key::NONE) {
		_push_key_event(key, 0, String());
	}
}

void IME_OHOS::on_set_selection(InputMethod_TextEditorProxy *p_proxy, int32_t p_start, int32_t p_end) {
	// 输入法请求设置选区：编辑器场景由候选框文本选择触发。
	// Godot 文本控件选区需 UI 层调用（EditorTextServer），此处记录并忽略，
	// 候选框选择结果已由 on_insert_text 提交。
	print_verbose(vformat("IME_OHOS: set selection [%d,%d]", p_start, p_end));
}

int32_t IME_OHOS::on_set_preview_text(InputMethod_TextEditorProxy *p_proxy, const char16_t p_text[], size_t p_length, int32_t p_start, int32_t p_end) {
	// 组合预览文本（拼音下划线候选）：Godot 4 的 TextServer 无平台预览注入接口，
	// 输入法组合阶段仅需候选框在输入法 UI 中呈现，无需回写编辑器。
	// 返回 0 表示接受预览（避免输入法回退到逐字提交）。
	print_verbose(vformat("IME_OHOS: preview text len=%zu [%d,%d]", p_length, p_start, p_end));
	return 0;
}

void IME_OHOS::on_get_text_config(InputMethod_TextEditorProxy *p_proxy, InputMethod_TextConfig *p_config) {
	// 输入法查询文本配置（输入类型/回车键类型/光标等）。
	// 编辑器为多行文本输入：设置 TEXT_INPUT_TYPE_MULTILINE + 默认回车。
	if (!p_config) {
		return;
	}
	InputMethod_TextInputType input_type = IME_TEXT_INPUT_TYPE_MULTILINE;
	OH_TextConfig_SetInputType(p_config, input_type);
	OH_TextConfig_SetEnterKeyType(p_config, IME_ENTER_KEY_DONE);
}

// ---- NAPI 桥实现（ohos_bridge.h 声明；输入法为 NDK C API，无需 ArkTS 参与） ----

void ohos_ime_attach() {
	// 编辑器文本控件聚焦：附加系统输入法服务
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->ime_attach_for_text_input();
	}
}

void ohos_ime_detach() {
	// 分离输入法（窗口失焦等）
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->ime_detach_on_blur();
	}
}

void ohos_ime_show_keyboard() {
	// 请求显示软键盘（2in1 触屏文本输入）
	IME_OHOS *ime = IME_OHOS::get_singleton();
	if (ime) {
		ime->show_keyboard();
	}
}

void ohos_ime_hide_keyboard() {
	IME_OHOS *ime = IME_OHOS::get_singleton();
	if (ime) {
		ime->hide_keyboard();
	}
}

void ohos_ime_notify_cursor_rect(int p_x, int p_y, int p_w, int p_h) {
	// 同步编辑器文本光标矩形给输入法（候选框定位）
	IME_OHOS *ime = IME_OHOS::get_singleton();
	if (ime) {
		ime->notify_cursor_rect(p_x, p_y, p_w, p_h);
	}
}
