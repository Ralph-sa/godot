/**************************************************************************/
/*  ohos_xcomponent.cpp                                                   */
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

#include "ohos_xcomponent.h"
#include <hilog/log.h>

#include "display_server_ohos.h"
#include "key_mapping_ohos.h"

#include "core/input/input_event.h"
#include "core/math/math_funcs.h"
#include "core/os/mutex.h"
#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>
#include <native_window/external_window.h>
#include <native_buffer/buffer_common.h>

// 静态实例指针（编辑器主窗口唯一 XComponent）
OHOS_XComponent *OHOS_XComponent::s_instance = nullptr;

OHOS_XComponent::OHOS_XComponent() {
	s_instance = this;
}

OHOS_XComponent::~OHOS_XComponent() {
	if (s_instance == this) {
		s_instance = nullptr;
	}
	// Surface 销毁时释放原生窗口引用
	if (native_window) {
		OH_NativeWindow_DestroyNativeWindow(native_window);
		native_window = nullptr;
	}
	native_xcomponent = nullptr;
	xcomponent = nullptr;
}

// ---- C 风格回调包装 ----

void OHOS_XComponent::surface_created_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：Surface 创建
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		xc->on_surface_created(component, static_cast<OHNativeWindow *>(window));
	}
}

void OHOS_XComponent::surface_changed_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：Surface 尺寸变化
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		uint64_t width = 0;
		uint64_t height = 0;
		if (OH_NativeXComponent_GetXComponentSize(component, window, &width, &height) == 0) {
			xc->on_surface_changed(static_cast<int>(width), static_cast<int>(height));
		}
	}
}

void OHOS_XComponent::surface_destroyed_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：Surface 销毁
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		xc->on_surface_destroyed();
	}
}

void OHOS_XComponent::dispatch_touch_event_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：触摸事件
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		xc->handle_touch_event(component, window);
	}
}

void OHOS_XComponent::dispatch_mouse_event_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：鼠标事件（PC/2in1）
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		xc->handle_mouse_event(component, window);
	}
}

void OHOS_XComponent::dispatch_key_event_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：键盘事件（PC/2in1）
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		xc->handle_key_event(component, window);
	}
}

void OHOS_XComponent::focus_event_cb(OH_NativeXComponent *component, void *window) {
	// ArkUI 主线程回调：窗口聚焦/失焦（RegisterFocusEventCallback）
	OHOS_XComponent *xc = get_instance();
	if (xc) {
		xc->handle_focus_event(true);
	}
}

// ---- Surface 变换（第 11 轮真机修复：GLES Y 轴翻转） ----

void OHOS_XComponent::set_surface_transform(int p_transform) {
	if (!native_window) {
		return;
	}
	OH_NativeWindow_NativeWindowHandleOpt(native_window, SET_TRANSFORM, p_transform);
}

// ---- SurfaceId 路径（第 10 轮修复：API 26 无 nativeXComponent 上下文） ----

void OHOS_XComponent::set_native_window_from_surface_id(uint64_t p_surface_id, int p_width, int p_height) {
	// ArkTS getXComponentSurfaceId() 返回 surfaceId 字符串，native 侧直接创建
	// OHNativeWindow（对应 OnSurfaceCreated 回调里的 window 参数），绕过
	// OH_NativeXComponent 上下文传递（API 26 已不提供 nativeXComponent 属性）。
	OHNativeWindow *win = nullptr;
	int32_t ret = OH_NativeWindow_CreateNativeWindowFromSurfaceId(p_surface_id, &win);
	if (ret == 0 && win) {
		// 手动创建的 OHNativeWindow 未初始化 buffer geometry：
		// Vulkan vkGetPhysicalDeviceSurfaceCapabilitiesKHR/vkCreateSwapchainKHR
		// 需要窗口具备有效宽高，否则交换链创建失败（屏幕无交换链导致
		// screen_prepare_for_drawing 报错、渲染路径空指针崩溃）。
		// 格式由系统为 surface 窗口默认设置（鸿蒙 SDK 不导出 pixel format
		// 枚举，SET_FORMAT 不可用；EGL 侧用 RGBA8 config 匹配系统默认格式）。
		OH_NativeWindow_NativeWindowHandleOpt(win, SET_BUFFER_GEOMETRY, p_width, p_height);
	}
	// 结果写诊断文件（引擎打印链未注册前 print_verbose 行为不确定，
	// 且诊断文件在崩溃后仍可 hdc file recv 读取）
	{
		FILE *f = fopen("/data/app/el2/100/base/com.godot.editor/haps/entry/cache/godot_xc_diag.log", "a");
		if (f) {
			fprintf(f, "CreateNativeWindowFromSurfaceId(%llu) -> %d, win=%p\n",
					(unsigned long long)p_surface_id, ret, (void *)win);
			fclose(f);
		}
	}
	if (ret == 0 && win) {
		native_window = win;
		size = Size2i(p_width, p_height);
		surface_ready = true;
	} else {
		// 创建失败：保持 surface_ready=false，引擎线程按未就绪节流并重试
		surface_ready = false;
	}
}

// ---- Surface 生命周期 ----

void OHOS_XComponent::on_surface_created(OH_NativeXComponent *p_component, OHNativeWindow *p_window) {
	// 鸿蒙 XComponent 原生渲染 Surface 创建回调。
	// 注意：此回调运行在 ArkUI 主线程，需将窗口句柄记录后，
	// 通过主循环队列/互斥通知引擎线程创建 Vulkan Surface（线程模型见方案 1.1）。
	native_xcomponent = p_component;
	native_window = p_window;

	// 读取 Surface 实际尺寸
	if (native_xcomponent) {
		uint64_t width = 0;
		uint64_t height = 0;
		if (OH_NativeXComponent_GetXComponentSize(native_xcomponent, native_window, &width, &height) == 0) {
			size = Size2i(static_cast<int>(width), static_cast<int>(height));
		}
	}

	surface_ready = true;
	print_verbose("OHOS_XComponent: surface created");
}

void OHOS_XComponent::on_surface_changed(int p_width, int p_height) {
	// Surface 尺寸变化回调（旋转/窗口缩放触发）
	// 通知 DisplayServer 同步窗口尺寸并触发 rect_changed 回调（第 3 轮）
	Size2i new_size(p_width, p_height);
	if (new_size == size) {
		return; // 尺寸未变，忽略
	}
	size = new_size;
	print_verbose(vformat("OHOS_XComponent: surface resized to %dx%d", p_width, p_height));
	// 第 11 轮修复：窗口拉伸后更新 OHNativeWindow buffer 几何，否则
	// EGL swap 的 buffer 保持启动尺寸，渲染内容不跟随窗口变化。
	if (native_window) {
		OH_NativeWindow_NativeWindowHandleOpt(native_window, SET_BUFFER_GEOMETRY, p_width, p_height);
	}

	// 通知 DisplayServer：引擎侧同步窗口尺寸、触发 rect_changed/window 事件
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->notify_main_surface_resized();
	}
}

void OHOS_XComponent::on_surface_destroyed() {
	// Surface 销毁回调：渲染必须暂停，等待下次 SurfaceCreated
	surface_ready = false;
	if (native_window) {
		OH_NativeWindow_DestroyNativeWindow(native_window);
		native_window = nullptr;
	}
	print_verbose("OHOS_XComponent: surface destroyed");
}

// ---- 事件注册 ----

int OHOS_XComponent::register_callbacks() {
	// 注册 surface/touch 回调（第 1 轮骨架：生命周期）
	OH_NativeXComponent_Callback callbacks = {
		.OnSurfaceCreated = surface_created_cb,
		.OnSurfaceChanged = surface_changed_cb,
		.OnSurfaceDestroyed = surface_destroyed_cb,
		.DispatchTouchEvent = dispatch_touch_event_cb,
	};
	int ret = OH_NativeXComponent_RegisterCallback(native_xcomponent, &callbacks);

	// 注册鼠标回调（PC/2in1 键鼠，since 9）
	OH_NativeXComponent_MouseEvent_Callback mouse_callbacks = {
		.DispatchMouseEvent = dispatch_mouse_event_cb,
		.DispatchHoverEvent = nullptr, // 悬停事件第 8 轮实现
	};
	ret |= OH_NativeXComponent_RegisterMouseEventCallback(native_xcomponent, &mouse_callbacks);

	// 注册键盘回调（PC/2in1 键盘，since 10）
	ret |= OH_NativeXComponent_RegisterKeyEventCallback(native_xcomponent, dispatch_key_event_cb);

	// 注册聚焦回调（窗口焦点变化，since 10）
	// 注：失焦时系统不回调本函数（ArkUI 仅回调获得焦点），
	// 失焦事件由 ArkTS 侧 onBlur 通知（第 4 轮接入）。
	ret |= OH_NativeXComponent_RegisterFocusEventCallback(native_xcomponent, focus_event_cb);

	return ret;
}

// ---- 触摸事件处理 ----

void OHOS_XComponent::handle_touch_event(OH_NativeXComponent *p_component, void *p_window) {
	// 触摸事件在 ArkUI 主线程回调，转换为 InputEventScreenTouch/Drag 后入队，
	// 由引擎线程 process_events 消费（与 Android input 队列模型一致）。
	OH_NativeXComponent_TouchEvent touch_event;
	if (OH_NativeXComponent_GetTouchEvent(p_component, p_window, &touch_event) != 0) {
		return;
	}

	// 处理每个触点：DOWN/UP 生成 ScreenTouch，MOVE 生成 ScreenDrag
	for (uint32_t i = 0; i < touch_event.numPoints; i++) {
		const OH_NativeXComponent_TouchPoint &point = touch_event.touchPoints[i];
		Vector2 pos(point.x, point.y);

		if (point.type == OH_NATIVEXCOMPONENT_DOWN || point.type == OH_NATIVEXCOMPONENT_UP) {
			// 触摸按下/抬起 -> InputEventScreenTouch
			Ref<InputEventScreenTouch> ev;
			ev.instantiate();
			ev->set_index(point.id);
			ev->set_position(pos);
			ev->set_pressed(point.type == OH_NATIVEXCOMPONENT_DOWN);

			MutexLock lock(input_events_mutex);
			_enqueue_input_event(ev);

			if (point.type == OH_NATIVEXCOMPONENT_UP) {
				touch_state.erase(point.id);
			} else {
				touch_state[point.id] = pos;
			}
		} else if (point.type == OH_NATIVEXCOMPONENT_MOVE) {
			// 触摸移动 -> InputEventScreenDrag
			Ref<InputEventScreenDrag> ev;
			ev.instantiate();
			ev->set_index(point.id);
			ev->set_position(pos);

			// 计算相对位移（无历史点时用零）
			Vector2 rel;
			if (touch_state.has(point.id)) {
				rel = pos - touch_state[point.id];
			}
			ev->set_relative(rel);

			MutexLock lock(input_events_mutex);
			_enqueue_input_event(ev);

			touch_state[point.id] = pos;
		}
		// CANCEL/UNKNOWN 忽略（或后续轮次处理为取消事件）
	}
}

// ---- 鼠标事件处理 ----

void OHOS_XComponent::handle_mouse_event(OH_NativeXComponent *p_component, void *p_window) {
	// 鼠标事件：PC/2in1 上鼠标操作编辑器
	OH_NativeXComponent_MouseEvent mouse_event;
	if (OH_NativeXComponent_GetMouseEvent(p_component, p_window, &mouse_event) != 0) {
		return;
	}

	Vector2 pos(mouse_event.x, mouse_event.y);

	switch (mouse_event.action) {
		case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
		case OH_NATIVEXCOMPONENT_MOUSE_RELEASE: {
			// 鼠标按键 -> InputEventMouseButton
			Ref<InputEventMouseButton> ev;
			ev.instantiate();
			ev->set_position(pos);
			ev->set_global_position(pos);
			ev->set_pressed(mouse_event.action == OH_NATIVEXCOMPONENT_MOUSE_PRESS);
			// 鼠标按钮位域转 Godot MouseButton
			ev->set_button_index(_mouse_button_from_flags(mouse_event.button));
			ev->set_button_mask(_mouse_button_mask_from_flags(mouse_event.button));

			MutexLock lock(input_events_mutex);
			_enqueue_input_event(ev);
			break;
		}
		case OH_NATIVEXCOMPONENT_MOUSE_MOVE: {
			// 鼠标移动 -> InputEventMouseMotion（相对位移增量计算，第 8 轮）
			// 编辑器 3D 视口旋转/拖拽平移依赖 relative（鼠标捕获模式），
			// 与 macOS 的 deltaX/deltaY（NSEvent mouseDelta）对应。
			Vector2 relative = pos - Vector2(last_mouse_position);
			last_mouse_position = Point2i(static_cast<int>(mouse_event.x), static_cast<int>(mouse_event.y));

			Ref<InputEventMouseMotion> ev;
			ev.instantiate();
			ev->set_position(pos);
			ev->set_global_position(pos);
			ev->set_relative(relative);
			ev->set_velocity(Vector2());
			ev->set_button_mask(_mouse_button_mask_from_flags(mouse_event.button));

			MutexLock lock(input_events_mutex);
			_enqueue_input_event(ev);
			break;
		}
		default:
			break;
	}
}

// ---- 键盘事件处理 ----

void OHOS_XComponent::handle_key_event(OH_NativeXComponent *p_component, void *p_window) {
	// 键盘事件：PC/2in1 编辑器快捷键
	OH_NativeXComponent_KeyEvent *key_event = nullptr;
	if (OH_NativeXComponent_GetKeyEvent(p_component, &key_event) != 0 || !key_event) {
		return;
	}

	OH_NativeXComponent_KeyAction action = OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN;
	OH_NativeXComponent_KeyCode code = KEY_UNKNOWN;
	OH_NativeXComponent_EventSourceType source = OH_NATIVEXCOMPONENT_SOURCE_TYPE_UNKNOWN;
	OH_NativeXComponent_GetKeyEventAction(key_event, &action);
	OH_NativeXComponent_GetKeyEventCode(key_event, &code);
	OH_NativeXComponent_GetKeyEventSourceType(key_event, &source);

	// 只处理键盘来源（过滤触屏软键盘）
	if (source != OH_NATIVEXCOMPONENT_SOURCE_TYPE_KEYBOARD) {
		return;
	}

	// 鸿蒙 keycode -> Godot Key（KeyMappingOHOS 支持 XComponent keycode 系列）
	Key key = KeyMappingOHOS::translate_key(static_cast<unsigned int>(code));
	if (key == Key::NONE) {
		return;
	}

	Ref<InputEventKey> ev;
	ev.instantiate();
	ev->set_pressed(action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN);
	ev->set_keycode(key);
	ev->set_physical_keycode(key);
	ev->set_location(KeyMappingOHOS::translate_location(static_cast<unsigned int>(code)));
	ev->set_echo(false);

	MutexLock lock(input_events_mutex);
	_enqueue_input_event(ev);
}

void OHOS_XComponent::handle_focus_event(bool p_focused) {
	// 窗口聚焦状态更新：通知 DisplayServer 触发 WINDOW_EVENT_FOCUS_IN/OUT
	// （对应 macOS 的 windowDidBecomeMain / windowDidResignMain）
	window_focused = p_focused;
	DisplayServerOHOS *ds = DisplayServerOHOS::get_singleton_ohos();
	if (ds) {
		ds->notify_main_surface_focus(p_focused);
	}
}

// ---- 事件消费 ----

void OHOS_XComponent::poll_events(const Callable &p_input_event_callback) {
	// 引擎线程：取出队列全部事件并投递（与 macOS 的 -sendEvent 语义一致）
	if (!p_input_event_callback.is_valid()) {
		// 无回调时丢弃事件，避免队列膨胀
		static int no_cb_count = 0;
		no_cb_count++;
		if (no_cb_count <= 3) {
			OH_LOG_Print(LOG_APP, LOG_INFO, 0xD001, "GodotOHOS", "poll_events: NO VALID CALLBACK n=%{public}d", no_cb_count);
		}
		clear_input_events();
		return;
	}

	Vector<Ref<InputEvent>> events;
	{
		MutexLock lock(input_events_mutex);
		events = input_events;
		input_events.clear();
	}
	// 消费诊断（第 11 轮）
	if (!events.is_empty()) {
		static int poll_nonempty = 0;
		poll_nonempty++;
		if (poll_nonempty <= 5 || poll_nonempty % 100 == 0) {
			OH_LOG_Print(LOG_APP, LOG_INFO, 0xD001, "GodotOHOS", "poll_events: consumed n=%{public}d count=%{public}d", poll_nonempty, (int)events.size());
			FILE *df = fopen("/data/app/el2/100/base/com.godot.editor/haps/entry/cache/godot_input_diag.log", "a");
			if (df) {
				fprintf(df, "poll_events: n=%d consumed=%d", poll_nonempty, (int)events.size());
				fputc('\n', df);
				fclose(df);
			}
		}
	}

	for (const Ref<InputEvent> &event : events) {
		p_input_event_callback.call(event);
	}
}

void OHOS_XComponent::clear_input_events() {
	MutexLock lock(input_events_mutex);
	input_events.clear();
}

void OHOS_XComponent::_enqueue_input_event(const Ref<InputEvent> &p_event) {
	// 带上限保护的入队（调用方持有 input_events_mutex）：
	// 队列满时丢弃最旧事件，防止高频触摸/鼠标/滚轮导致内存膨胀
	if (input_events.size() >= MAX_QUEUED_INPUT_EVENTS) {
		input_events.remove_at(0);
	}
	input_events.push_back(p_event);
}

// ---- 输入注入（第 10 轮修复：ArkTS 事件桥，主线程调用） ----

void OHOS_XComponent::push_mouse_event(int p_action, int p_button, const Vector2 &p_pos) {
	// ArkTS onMouse 注入：action 1=按下 2=抬起 3=移动；button 0=左 1=右 2=中。
	// 生成 InputEventMouseButton/Motion 入队（对应 OH_NativeXComponent
	// DispatchMouseEvent 回调，但由 ArkTS 通用事件驱动）。
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	ev->set_position(p_pos);
	ev->set_global_position(p_pos);
	MouseButton mb = MouseButton::NONE;
	switch (p_button) {
		case 0:
			mb = MouseButton::LEFT;
			break;
		case 1:
			mb = MouseButton::RIGHT;
			break;
		case 2:
			mb = MouseButton::MIDDLE;
			break;
		default:
			mb = MouseButton::NONE;
			break;
	}
	MouseButtonMask mask = MouseButtonMask::NONE;
	if (mb == MouseButton::LEFT) {
		mask = MouseButtonMask::LEFT;
	} else if (mb == MouseButton::RIGHT) {
		mask = MouseButtonMask::RIGHT;
	} else if (mb == MouseButton::MIDDLE) {
		mask = MouseButtonMask::MIDDLE;
	}
	if (p_action == 3) {
		// 移动：转为 InputEventMouseMotion（相对位移用上次位置增量）
		Ref<InputEventMouseMotion> mev;
		mev.instantiate();
		mev->set_position(p_pos);
		mev->set_global_position(p_pos);
		mev->set_relative(Vector2(p_pos) - Vector2(last_mouse_position));
		mev->set_button_mask(mask);
		last_mouse_position = Point2i((int)p_pos.x, (int)p_pos.y);
		MutexLock lock(input_events_mutex);
		_enqueue_input_event(mev);
	} else {
		ev->set_button_index(mb);
		ev->set_button_mask(mask);
		ev->set_pressed(p_action == 1);
		last_mouse_position = Point2i((int)p_pos.x, (int)p_pos.y);
		MutexLock lock(input_events_mutex);
		_enqueue_input_event(ev);
	}
}

void OHOS_XComponent::push_key_event(int p_ohos_keycode, bool p_pressed) {
	// ArkTS onKeyEvent 注入：OHOS KeyCode -> Godot Key（复用双枚举映射表）。
	Key key = KeyMappingOHOS::translate_key(static_cast<unsigned int>(p_ohos_keycode));
	if (key == Key::NONE) {
		return;
	}
	Ref<InputEventKey> ev;
	ev.instantiate();
	ev->set_pressed(p_pressed);
	ev->set_keycode(key);
	ev->set_physical_keycode(key);
	ev->set_location(KeyMappingOHOS::translate_location(static_cast<unsigned int>(p_ohos_keycode)));
	ev->set_echo(false);
	MutexLock lock(input_events_mutex);
	_enqueue_input_event(ev);
}

void OHOS_XComponent::push_touch_event(int p_type, int p_id, const Vector2 &p_pos) {
	// 输入链诊断（第 11 轮）
	static int touch_push_count = 0;
	touch_push_count++;
	if (touch_push_count <= 5 || touch_push_count % 100 == 0) {
		OH_LOG_Print(LOG_APP, LOG_INFO, 0xD001, "GodotOHOS", "push_touch: n=%{public}d type=%{public}d pos=(%{public}.0f,%{public}.0f)", touch_push_count, p_type, p_pos.x, p_pos.y);
		FILE *df = fopen("/data/app/el2/100/base/com.godot.editor/haps/entry/cache/godot_input_diag.log", "a");
		if (df) {
			fprintf(df, "push_touch: n=%d type=%d id=%d pos=(%.0f,%.0f) queued=%d", touch_push_count, p_type, p_id, p_pos.x, p_pos.y, (int)input_events.size());
			fputc('\n', df);
			fclose(df);
		}
	}
	// ArkTS onTouch 注入（第 11 轮修订）：编辑器 GUI 为桌面鼠标语义，
	// InputEventScreenTouch 不会触发按钮/菜单（Godot 桌面 UI 需鼠标事件或
	// emulate_mouse_from_touch），改为注入鼠标事件：按下=左键按下、
	// 移动=鼠标移动、抬起=左键释放。
	if (p_type == 0 || p_type == 2) {
		Ref<InputEventMouseButton> mb;
		mb.instantiate();
		mb->set_button_index(MouseButton::LEFT);
		mb->set_pressed(p_type == 0);
		mb->set_position(p_pos);
		mb->set_global_position(p_pos);
		mb->set_factor(1.0f);
		MutexLock lock(input_events_mutex);
		_enqueue_input_event(mb);
		if (p_type == 0) {
			touch_state[p_id] = p_pos;
			last_mouse_position = Point2i((int)p_pos.x, (int)p_pos.y);
		} else {
			touch_state.erase(p_id);
		}
	} else if (p_type == 1) {
		// 移动：注入鼠标移动事件（第 11 轮修订，鼠标语义）
		Ref<InputEventMouseMotion> mm;
		mm.instantiate();
		Vector2 rel;
		if (touch_state.has(p_id)) {
			rel = p_pos - touch_state[p_id];
		}
		mm->set_position(p_pos);
		mm->set_global_position(p_pos);
		mm->set_relative(rel);
		mm->set_button_mask(MouseButtonMask::LEFT);
		touch_state[p_id] = p_pos;
		last_mouse_position = Point2i((int)p_pos.x, (int)p_pos.y);
		MutexLock lock(input_events_mutex);
		_enqueue_input_event(mm);
	} else if (false && p_type == 1) { // 原 ScreenDrag 路径保留（死代码，后续需要触摸语义时恢复）
		Ref<InputEventScreenDrag> ev;
		ev.instantiate();
		ev->set_index(p_id);
		ev->set_position(p_pos);
		Vector2 rel2;
		if (touch_state.has(p_id)) {
			rel2 = p_pos - touch_state[p_id];
		}
		ev->set_relative(rel2);
		MutexLock lock(input_events_mutex);
		_enqueue_input_event(ev);
		touch_state[p_id] = p_pos;
	}
	// 取消：忽略（引擎按抬起处理即可）
}

// ---- 输入注入（第 8 轮：输入法/触控板，主线程调用） ----

void OHOS_XComponent::push_input_event(const String &p_text, Key p_keycode, char32_t p_unicode) {
	// 按键事件入队（IME 组合文本/删除/回车；ArkUI 主线程，引擎线程消费）。
	// Godot InputEventKey 无文本字段，多字符文本拆为多个 unicode 按键事件
	//（对应 macOS insertText 逐个字符插入 + keyDown/keyUp）。
	auto push_pair = [&](bool p_pressed, char32_t p_cp) {
		Ref<InputEventKey> ev;
		ev.instantiate();
		ev->set_pressed(p_pressed);
		ev->set_keycode(p_keycode);
		ev->set_physical_keycode(p_keycode);
		ev->set_key_label(p_keycode);
		ev->set_unicode(p_cp);

		MutexLock lock(input_events_mutex);
		_enqueue_input_event(ev);
	};

	if (p_keycode == Key::NONE) {
		// 输入法提交文本：逐字符拆分（中文等多字节字符每个字符一个事件）
		if (!p_text.is_empty()) {
			for (int i = 0; i < p_text.length(); i++) {
				push_pair(true, p_text[i]);
			}
		} else if (p_unicode != 0) {
			push_pair(true, p_unicode);
		}
	} else {
		push_pair(true, p_unicode);
		push_pair(false, p_unicode);
	}
}

void OHOS_XComponent::push_wheel_event(const Vector2 &p_delta) {
	// 滚轮增量入队：触控板双指滚动由 ArkTS 手势识别（onTouch 双指滑动）后
	// 经 engine_inject_wheel NAPI 注入；XComponent 原生鼠标事件不携带滚轮。
	// 与 macOS scrollWheel 的 scrollingDeltaY 语义一致（向下为正）。
	Ref<InputEventMouseButton> ev;
	ev.instantiate();
	Vector2 pos(last_mouse_position);
	ev->set_position(pos);
	ev->set_global_position(pos);

	// 垂直滚动优先（Godot 编辑器滚轮滚动/缩放）
	bool is_vertical = Math::abs(p_delta.y) >= Math::abs(p_delta.x);
	MouseButton button = is_vertical ? MouseButton::WHEEL_DOWN : MouseButton::WHEEL_RIGHT;
	float delta = is_vertical ? p_delta.y : p_delta.x;
	if (delta < 0.0f) {
		// 负增量（向上/向左滚动）转换为相反的滚轮按钮
		button = is_vertical ? MouseButton::WHEEL_UP : MouseButton::WHEEL_LEFT;
		delta = -delta;
	}

	// 每次滚动事件按 1 格处理（多格累加由引擎 factor 处理）
	ev->set_button_index(button);
	ev->set_button_mask(MouseButtonMask::NONE);
	ev->set_factor(CLAMP(delta, 1.0f, 100.0f));

	{
		MutexLock lock(input_events_mutex);
		ev->set_pressed(true);
		_enqueue_input_event(ev);
		Ref<InputEventMouseButton> ev_up;
		ev_up.instantiate();
		ev_up->set_position(pos);
		ev_up->set_global_position(pos);
		ev_up->set_button_index(button);
		ev_up->set_button_mask(MouseButtonMask::NONE);
		ev_up->set_pressed(false);
		_enqueue_input_event(ev_up);
	}
}

// ---- 静态工具 ----

MouseButton OHOS_XComponent::_mouse_button_from_flags(int p_flags) {
	// 鼠标按钮位域 -> Godot MouseButton（一次取一个主按钮）
	if (p_flags & OH_NATIVEXCOMPONENT_LEFT_BUTTON) {
		return MouseButton::LEFT;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_RIGHT_BUTTON) {
		return MouseButton::RIGHT;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_MIDDLE_BUTTON) {
		return MouseButton::MIDDLE;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_BACK_BUTTON) {
		return MouseButton::MB_XBUTTON1;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_FORWARD_BUTTON) {
		return MouseButton::MB_XBUTTON2;
	}
	return MouseButton::NONE;
}

MouseButtonMask OHOS_XComponent::_mouse_button_mask_from_flags(int p_flags) {
	// 鼠标按钮位域 -> Godot MouseButtonMask
	MouseButtonMask mask = MouseButtonMask::NONE;
	if (p_flags & OH_NATIVEXCOMPONENT_LEFT_BUTTON) {
		mask |= MouseButtonMask::LEFT;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_RIGHT_BUTTON) {
		mask |= MouseButtonMask::RIGHT;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_MIDDLE_BUTTON) {
		mask |= MouseButtonMask::MIDDLE;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_BACK_BUTTON) {
		mask |= MouseButtonMask::MB_XBUTTON1;
	}
	if (p_flags & OH_NATIVEXCOMPONENT_FORWARD_BUTTON) {
		mask |= MouseButtonMask::MB_XBUTTON2;
	}
	return mask;
}
