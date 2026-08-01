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

#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>

OHOS_XComponent::~OHOS_XComponent() {
	// Surface 销毁时释放原生窗口引用
	if (native_window) {
		OH_NativeWindow_DestroyNativeWindow(native_window);
		native_window = nullptr;
	}
	native_xcomponent = nullptr;
	xcomponent = nullptr;
}

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
