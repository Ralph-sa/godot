/**************************************************************************/
/*  ohos_window.h                                                         */
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

#include "core/math/rect2.h"
#include "core/math/rect2i.h"
#include "core/math/vector2i.h"
#include "core/object/object_id.h"
#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/variant/callable.h"
#include "servers/display/display_server_enums.h"

class OHOS_XComponent;

/* OHOS_Window：鸿蒙窗口抽象（对应 macOS godot_window）。
 *
 * 鸿蒙 PC/2in1 上编辑器需要多窗口：
 * - 主窗口（主界面编辑器，XComponent 承载） —— 第 2~3 轮实现
 * - 子窗口（弹出属性/浮窗，XComponent 承载 Vulkan 渲染） —— 第 5 轮实现
 *   （子窗口能否承载 Vulkan Surface 需提前验证，见移植方案风险 R7）
 * - 纯 ArkUI 浮层（工具面板，不渲染 3D） —— 第 6 轮实现
 *
 * 第 1 轮（骨架期）：仅定义窗口索引/状态与生命周期接口，用于 DisplayServer 骨架。
 */
class OHOS_Window {
public:
	// 窗口类型
	enum Type {
		WINDOW_TYPE_MAIN,       // 主窗口
		WINDOW_TYPE_SUB,        // 子窗口（Vulkan 承载）
		WINDOW_TYPE_PANEL,      // ArkUI 浮层面板
	};

private:
	Type type = WINDOW_TYPE_MAIN;

	// 窗口状态
	bool visible = false;
	bool minimized = false;

	// 窗口标题
	String title;

	// 窗口几何（逻辑像素，相对主窗口）
	Rect2i rect;

	// 窗口尺寸限制
	Size2i min_size;
	Size2i max_size;

	// 关联的引擎 Window 实例（window_attach_instance_id）
	ObjectID instance_id;

	// 窗口回调（尺寸变化/窗口事件/输入/文本/文件拖放）
	Callable rect_changed_callback;
	Callable window_event_callback;
	Callable input_event_callback;
	Callable input_text_callback;
	Callable drop_files_callback;

	// 窗口模式（第 3 轮：全屏/最大化经 NAPI 请求 ArkUI 窗口）
	DisplayServerEnums::WindowMode window_mode = DisplayServerEnums::WINDOW_MODE_WINDOWED;

	// XComponent 宿主（仅 MAIN/SUB 类型有效）
	OHOS_XComponent *xcomponent = nullptr;

public:
	OHOS_Window(Type p_type = WINDOW_TYPE_MAIN) : type(p_type) {}

	// 访问器
	Type get_type() const { return type; }
	bool is_visible() const { return visible; }
	void set_visible(bool p_visible) { visible = p_visible; }
	bool is_minimized() const { return minimized; }
	void set_minimized(bool p_minimized) { minimized = p_minimized; }
	const String &get_title() const { return title; }
	void set_title(const String &p_title) { title = p_title; }
	Rect2i get_rect() const { return rect; }
	void set_rect(const Rect2i &p_rect) { rect = p_rect; }
	Size2i get_min_size() const { return min_size; }
	void set_min_size(const Size2i &p_size) { min_size = p_size; }
	Size2i get_max_size() const { return max_size; }
	void set_max_size(const Size2i &p_size) { max_size = p_size; }
	ObjectID get_instance_id() const { return instance_id; }
	void set_instance_id(ObjectID p_instance_id) { instance_id = p_instance_id; }

	const Callable &get_rect_changed_callback() const { return rect_changed_callback; }
	void set_rect_changed_callback(const Callable &p_callable) { rect_changed_callback = p_callable; }
	const Callable &get_window_event_callback() const { return window_event_callback; }
	void set_window_event_callback(const Callable &p_callable) { window_event_callback = p_callable; }
	const Callable &get_input_event_callback() const { return input_event_callback; }
	void set_input_event_callback(const Callable &p_callable) { input_event_callback = p_callable; }
	const Callable &get_input_text_callback() const { return input_text_callback; }
	void set_input_text_callback(const Callable &p_callable) { input_text_callback = p_callable; }
	const Callable &get_drop_files_callback() const { return drop_files_callback; }
	void set_drop_files_callback(const Callable &p_callable) { drop_files_callback = p_callable; }

	OHOS_XComponent *get_xcomponent() const { return xcomponent; }
	void set_xcomponent(OHOS_XComponent *p_xc) { xcomponent = p_xc; }

	DisplayServerEnums::WindowMode get_window_mode() const { return window_mode; }
	void set_window_mode(DisplayServerEnums::WindowMode p_mode) { window_mode = p_mode; }

	// 窗口生命周期（骨架期：由 DisplayServer 调用；后续轮次桥接 ArkUI Window API）
	Error show();
	Error hide();
	Error resize(const Size2i &p_size);
};
