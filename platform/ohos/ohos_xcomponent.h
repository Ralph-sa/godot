/**************************************************************************/
/*  ohos_xcomponent.h                                                     */
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
#include "core/math/rect2.h"
#include "core/math/vector2.h"
#include "core/math/vector2i.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

/* OHOS_XComponent：鸿蒙 XComponent(SURFACE) 原生宿主。
 *
 * 对应 macOS 平台中的 NSView/CALayer 角色（godot_content_view）：
 * macOS 用 NSView 承接绘制与事件，OHOS 用 XComponent 承接 Vulkan Surface
 * 与触摸/鼠标/键盘事件（DispatchTouchEvent / MouseEvent / KeyEvent）。
 *
 * API 26 关键语义：XComponent Surface 延迟到组件首次可见才创建，因此
 * SurfaceCreated/SurfaceDestroyed 回调需与引擎渲染启停解耦（见移植方案 1.1）。
 *
 * 第 2 轮（功能深化）：实现触摸/鼠标/键盘事件回调注册与 InputEvent 队列，
 * 由 DisplayServer::process_events 消费并派发（对应 macOS 的 NSEvent 处理）。
 *
 * 线程模型：事件回调运行在 ArkUI 主线程，引擎线程在 process_events 消费。
 * 因此事件先入队列（互斥锁保护），由引擎线程拉取，避免跨线程直接调用。
 */

typedef struct OH_ArkUI_XComponent OH_ArkUI_XComponent;
typedef struct OH_NativeXComponent OH_NativeXComponent;
typedef struct NativeWindow OHNativeWindow;
typedef struct OH_NativeXComponent_KeyEvent OH_NativeXComponent_KeyEvent;

class OHOS_XComponent {
	// XComponent 的 ArkUI 句柄（从 NAPI 回调获取）
	OH_ArkUI_XComponent *xcomponent = nullptr;

	// 原生 XComponent 句柄（OnSurfaceCreated 时有效）
	OH_NativeXComponent *native_xcomponent = nullptr;

	// 原生窗口句柄（Vulkan Surface 创建用）
	OHNativeWindow *native_window = nullptr;

	// 当前尺寸（单位 px）
	Size2i size;

	// Surface 是否已就绪（决定渲染是否可启动）
	bool surface_ready = false;

	// ---- 输入事件（第 2 轮） ----
	// 输入事件队列：ArkUI 主线程投递，引擎线程 process_events 消费
	Vector<Ref<InputEvent>> input_events;
	Mutex input_events_mutex;

	// 输入队列上限（第 9 轮：防高频触摸/鼠标事件导致内存膨胀）
	// 超出时丢弃最旧事件，引擎侧保持最新状态（对应 macOS 输入事件合并语义）
	static constexpr int MAX_QUEUED_INPUT_EVENTS = 4096;

	// 入队（带上限保护）：超限丢弃最旧事件
	void _enqueue_input_event(const Ref<InputEvent> &p_event);

	// 活动触摸点状态（id -> 位置），用于生成相对位移
	HashMap<int32_t, Vector2> touch_state;

	// 最后一次鼠标位置（编辑器光标查询用）
	Point2i last_mouse_position;

	// ---- C 回调所需静态实例（编辑器主窗口唯一 XComponent） ----
	static OHOS_XComponent *s_instance;

	// ---- C 风格回调包装（OH_NativeXComponent 回调签名） ----
	static void surface_created_cb(OH_NativeXComponent *component, void *window);
	static void surface_changed_cb(OH_NativeXComponent *component, void *window);
	static void surface_destroyed_cb(OH_NativeXComponent *component, void *window);
	static void dispatch_touch_event_cb(OH_NativeXComponent *component, void *window);
	static void dispatch_mouse_event_cb(OH_NativeXComponent *component, void *window);
	static void dispatch_key_event_cb(OH_NativeXComponent *component, void *window);
	static void focus_event_cb(OH_NativeXComponent *component, void *window);

	// 窗口聚焦状态（第 3 轮：由 focus 回调维护）
	bool window_focused = false;

public:
	OHOS_XComponent();
	~OHOS_XComponent();

	// ---- 生命周期回调（由 NAPI 桥注册后触发） ----
	void on_surface_created(OH_NativeXComponent *p_component, OHNativeWindow *p_window);
	void on_surface_changed(int p_width, int p_height);
	void on_surface_destroyed();

	// ---- 事件注册（main_ohos.cpp 调用，绑定 surface/touch/mouse/key 回调） ----
	// 返回 0 成功；非 0 为 ArkUI 注册错误码
	int register_callbacks();

	// 设置 OH_NativeXComponent 句柄（从 ArkTS XComponent onLoad 上下文获取）
	void set_xcomponent(OH_NativeXComponent *p_xc) { native_xcomponent = p_xc; }

	// ---- SurfaceId 路径（第 10 轮修复） ----
	// API 26 上 onLoad 上下文不再携带 nativeXComponent（实测属性为 undefined），
	// 改走 surfaceId：ArkTS 侧 getXComponentSurfaceId() 传字符串，native 侧用
	// OH_NativeWindow_CreateNativeWindowFromSurfaceId 直接创建窗口（Vulkan 只需
	// OHNativeWindow）。输入事件后续经 ArkTS onTouch/onMouse 桥注入。
	void set_native_window_from_surface_id(uint64_t p_surface_id, int p_width, int p_height);

	// ---- 输入事件处理（由静态回调调用，运行在 ArkUI 主线程） ----
	void handle_touch_event(OH_NativeXComponent *p_component, void *p_window);
	void handle_mouse_event(OH_NativeXComponent *p_component, void *p_window);
	void handle_key_event(OH_NativeXComponent *p_component, void *p_window);
	void handle_focus_event(bool p_focused);

	// ---- 访问器 ----
	bool is_window_focused() const { return window_focused; }

	// ---- 事件消费（DisplayServer::process_events 调用，运行在引擎线程） ----
	// 从队列取走全部事件并投递到窗口的 input_event_callback
	void poll_events(const Callable &p_input_event_callback);
	// 清空队列（引擎停止时避免残留事件）
	void clear_input_events();

	// ---- 输入注入（第 8 轮：输入法/触控板，主线程调用） ----
	// 把一次按键入队到输入队列（IME 组合文本/删除/回车；ArkUI 主线程调用，
	// 引擎线程 process_events 消费，保证线程安全）。
	// p_text 非空时作为带文本的按键事件（文本控件直接插入，对应 macOS insertText）；
	// 否则按 p_keycode 生成 pressed/released 键事件。
	void push_input_event(const String &p_text, Key p_keycode, char32_t p_unicode = 0);
	// 把滚轮增量入队（触控板双指滚动由 ArkTS 手势识别后注入，生成 WHEEL 事件）
	void push_wheel_event(const Vector2 &p_delta);

	// ---- 访问器 ----
	OHNativeWindow *get_native_window() const { return native_window; }
	Size2i get_size() const { return size; }
	bool is_surface_ready() const { return surface_ready; }
	Point2i get_last_mouse_position() const { return last_mouse_position; }

	static OHOS_XComponent *get_instance() { return s_instance; }

private:
	// ---- 鼠标按钮位域转换工具（OHOS 位域 <-> Godot 枚举） ----
	static MouseButton _mouse_button_from_flags(int p_flags);
	static MouseButtonMask _mouse_button_mask_from_flags(int p_flags);
};
