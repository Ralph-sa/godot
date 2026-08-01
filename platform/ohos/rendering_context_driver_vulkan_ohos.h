/**************************************************************************/
/*  rendering_context_driver_vulkan_ohos.h                                */
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

#ifdef VULKAN_ENABLED

#include "drivers/vulkan/rendering_context_driver_vulkan.h"

struct NativeWindow;

/* RenderingContextDriverVulkanOHOS：鸿蒙 Vulkan 渲染上下文驱动。
 *
 * 对应 macOS 的 RenderingContextDriverVulkanMacOS（VK_EXT_metal_surface）
 * 与 Android 的 RenderingContextDriverVulkanAndroid（VK_KHR_android_surface）。
 * OHOS 使用 VK_OHOS_surface 扩展创建 Vulkan Surface，
 * 平台数据为 XComponent 的 OHNativeWindow 句柄。
 *
 * 第 1 轮（骨架期）：实现 surface_create 的 Surface 创建路径；
 * swapchain/队列等由基类 RenderingContextDriverVulkan 处理。
 */
class RenderingContextDriverVulkanOHOS : public RenderingContextDriverVulkan {
private:
	virtual const char *_get_platform_surface_extension() const override final;

protected:
	SurfaceID surface_create(const void *p_platform_data) override final;
	bool _use_validation_layers() const override final;

public:
	// 平台数据：XComponent 提供的原生窗口句柄
	struct WindowPlatformData {
		NativeWindow *window;
	};

	RenderingContextDriverVulkanOHOS() = default;
	~RenderingContextDriverVulkanOHOS() override = default;
};

#endif // VULKAN_ENABLED
