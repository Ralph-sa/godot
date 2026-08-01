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

#include "core/math/rect2.h"
#include "core/math/vector2i.h"

/* OHOS_XComponent：鸿蒙 XComponent(SURFACE) 原生宿主。
 *
 * 对应 macOS 平台中的 NSView/CALayer 角色（godot_content_view）：
 * macOS 用 NSView 承接绘制与事件，OHOS 用 XComponent 承接 Vulkan Surface
 * 与触摸/笔事件（OnDispatchTouchEvent）。
 *
 * API 26 关键语义：XComponent Surface 延迟到组件首次可见才创建，因此
 * SurfaceCreated/SurfaceDestroyed 回调需与引擎渲染启停解耦（见移植方案 1.1）。
 * 第 1 轮（骨架期）：定义生命周期与触摸回调接口，具体实现第 3 轮补全。
 */

typedef struct OH_ArkUI_XComponent OH_ArkUI_XComponent;
typedef struct OH_NativeXComponent OH_NativeXComponent;
typedef struct NativeWindow OHNativeWindow;

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

public:
	OHOS_XComponent() = default;
	~OHOS_XComponent();

	// ---- 生命周期回调（由 NAPI 桥注册后触发） ----
	void on_surface_created(OH_NativeXComponent *p_component, OHNativeWindow *p_window);
	void on_surface_changed(int p_width, int p_height);
	void on_surface_destroyed();

	// ---- 触摸/笔事件分发（OnDispatchTouchEvent） ----
	// 骨架期仅声明，第 4 轮实现具体 InputEventScreenTouch/Drag 注入

	// ---- 访问器 ----
	OHNativeWindow *get_native_window() const { return native_window; }
	Size2i get_size() const { return size; }
	bool is_surface_ready() const { return surface_ready; }
};
