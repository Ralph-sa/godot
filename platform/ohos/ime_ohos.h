/**************************************************************************/
/*  ime_ohos.h                                                            */
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

#include "core/input/input_event.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"

#include <inputmethod/inputmethod_text_config_capi.h>
#include <inputmethod/inputmethod_types_capi.h>

/* IME_OHOS：鸿蒙系统输入法（中文/日文/韩文等 IME 组合文本）接入。
 *
 * 对应 macOS 的 NSTextInputClient（insertText / doCommandBySelector）与
 * Windows 的 IMM32 消息处理：编辑器文本控件聚焦时附加系统输入法服务，
 * 输入法组合文本经回调注入引擎（Godot TextEdit/LineEdit 显示组合结果）。
 *
 * 实现基于 OHOS NDK inputmethod C API（@library libohinputmethod.so）：
 * - OH_TextEditorProxy_Create + 注册回调 -> TextEditorProxy（输入法 -> 应用）
 * - OH_InputMethodController_Attach(options) -> InputMethodProxy（应用 -> 输入法）
 *
 * 线程模型：回调默认运行在输入法服务线程；通过 SetCallbackInMainThread(true)
 * 让回调回到 ArkUI 主线程，与 XComponent 输入事件一致地入队到引擎输入队列，
 * 由引擎线程 process_events 消费，避免跨线程直接触碰场景树。
 */
class IME_OHOS {
	// TextEditorProxy：输入法 -> 应用的编辑回调集合
	struct InputMethod_TextEditorProxy *proxy = nullptr;
	// InputMethodProxy：应用 -> 输入法的操作句柄（Attach 成功后有效）
	struct InputMethod_InputMethodProxy *input_proxy = nullptr;
	// 附加选项（是否显示软键盘）
	struct InputMethod_AttachOptions *options = nullptr;

	// 附加状态（幂等保护）
	bool attached = false;

	// 回调跨线程保护（主线程 attach/回调，引擎线程 detach）
	Mutex mutex;

	// ---- 输入法 -> 应用回调（静态，转为成员处理） ----
	// 输入法提交组合文本（如中文拼音上屏）
	static void on_insert_text(InputMethod_TextEditorProxy *p_proxy, const char16_t *p_text, size_t p_length);
	// 输入法请求删除光标前文本
	static void on_delete_backward(InputMethod_TextEditorProxy *p_proxy, int32_t p_length);
	// 输入法请求删除光标后文本
	static void on_delete_forward(InputMethod_TextEditorProxy *p_proxy, int32_t p_length);
	// 输入法发送回车键（搜索/确认输入）
	static void on_send_enter_key(InputMethod_TextEditorProxy *p_proxy, InputMethod_EnterKeyType p_enter_key_type);
	// 输入法请求移动光标
	static void on_move_cursor(InputMethod_TextEditorProxy *p_proxy, InputMethod_Direction p_direction);
	// 输入法请求设置选区
	static void on_set_selection(InputMethod_TextEditorProxy *p_proxy, int32_t p_start, int32_t p_end);
	// 输入法设置组合预览文本（下划线高亮候选串，中文输入法拼音阶段）
	static int32_t on_set_preview_text(InputMethod_TextEditorProxy *p_proxy, const char16_t p_text[], size_t p_length, int32_t p_start, int32_t p_end);
	// 输入法请求获取文本配置（输入类型等，编辑器场景返回默认文本类型）
	static void on_get_text_config(InputMethod_TextEditorProxy *p_proxy, InputMethod_TextConfig *p_config);

	// ---- 内部工具 ----
	// 把 char16_t 数组转为 Godot String（含代理对处理，由 String::utf16 完成）
	static String _to_string(const char16_t *p_text, size_t p_length);
	// 把一次按键入队到引擎输入队列（主线程调用，引擎线程消费）
	static void _push_key_event(Key p_keycode, char32_t p_unicode, const String &p_text);

public:
	IME_OHOS();
	~IME_OHOS();

	// 附加输入法服务（编辑器文本控件聚焦；幂等）
	Error attach();
	// 分离输入法服务（文本控件失焦/窗口失焦）
	void detach();
	// 显示/隐藏软键盘（2in1 触屏场景）
	void show_keyboard();
	void hide_keyboard();
	// 同步光标矩形（vp）给输入法，用于候选框/预览定位
	void notify_cursor_rect(int p_x, int p_y, int p_w, int p_h);

	// 全局实例（编辑器主窗口单 IME）
	static IME_OHOS *get_singleton();

	bool is_attached() const { return attached; }
};
