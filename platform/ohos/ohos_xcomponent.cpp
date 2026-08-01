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

#include "key_mapping_ohos.h"

#include "core/input/input_event.h"
#include "core/os/mutex.h"
#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>
#include <native_window/external_window.h>

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
	size = Size2i(p_width, p_height);
	print_verbose(vformat("OHOS_XComponent: surface resized to %dx%d", p_width, p_height));
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
			input_events.push_back(ev);

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
			input_events.push_back(ev);

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
	last_mouse_position = Point2i(static_cast<int>(mouse_event.x), static_cast<int>(mouse_event.y));

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
			input_events.push_back(ev);
			break;
		}
		case OH_NATIVEXCOMPONENT_MOUSE_MOVE: {
			// 鼠标移动 -> InputEventMouseMotion
			Ref<InputEventMouseMotion> ev;
			ev.instantiate();
			ev->set_position(pos);
			ev->set_global_position(pos);
			ev->set_relative(Vector2()); // 无历史信息，轮次后续可增量计算
			ev->set_velocity(Vector2());
			ev->set_button_mask(_mouse_button_mask_from_flags(mouse_event.button));

			MutexLock lock(input_events_mutex);
			input_events.push_back(ev);
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
	input_events.push_back(ev);
}

// ---- 事件消费 ----

void OHOS_XComponent::poll_events(const Callable &p_input_event_callback) {
	// 引擎线程：取出队列全部事件并投递（与 macOS 的 -sendEvent 语义一致）
	if (!p_input_event_callback.is_valid()) {
		// 无回调时丢弃事件，避免队列膨胀
		clear_input_events();
		return;
	}

	Vector<Ref<InputEvent>> events;
	{
		MutexLock lock(input_events_mutex);
		events = input_events;
		input_events.clear();
	}

	for (const Ref<InputEvent> &event : events) {
		p_input_event_callback.call(event);
	}
}

void OHOS_XComponent::clear_input_events() {
	MutexLock lock(input_events_mutex);
	input_events.clear();
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
