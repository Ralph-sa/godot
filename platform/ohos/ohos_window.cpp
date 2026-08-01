/**************************************************************************/
/*  ohos_window.cpp                                                       */
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

#include "ohos_window.h"

#include "ohos_xcomponent.h"

#include "core/string/print_string.h"
#include "core/variant/variant.h"

Error OHOS_Window::show() {
	// 骨架期：标记可见；后续轮次通过 ArkUI 侧 NAPI 桥展示窗口
	visible = true;
	if (type == WINDOW_TYPE_MAIN || type == WINDOW_TYPE_SUB) {
		// 主/子窗口依赖 XComponent Surface 就绪后才可真正显示
		ERR_FAIL_COND_V_MSG(xcomponent == nullptr, ERR_UNCONFIGURED, "Window XComponent not configured.");
	}
	print_verbose(vformat("OHOS_Window[%s]: show", type == WINDOW_TYPE_MAIN ? "main" : "sub"));
	return OK;
}

Error OHOS_Window::hide() {
	// 骨架期：标记隐藏
	visible = false;
	print_verbose(vformat("OHOS_Window[%s]: hide", type == WINDOW_TYPE_MAIN ? "main" : "sub"));
	return OK;
}

Error OHOS_Window::resize(const Size2i &p_size) {
	// 骨架期：仅更新几何；后续轮次桥接 ArkUI window.resize
	rect.size = p_size;
	if (xcomponent) {
		xcomponent->on_surface_changed(p_size.width, p_size.height);
	}
	return OK;
}
