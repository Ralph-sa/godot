/**************************************************************************/
/*  rendering_context_driver_vulkan_ohos.cpp                              */
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

#include "rendering_context_driver_vulkan_ohos.h"

#ifdef VULKAN_ENABLED

#include "core/variant/variant.h"

#include <drivers/vulkan/godot_vulkan.h>
#include <vulkan/vulkan_ohos.h>

const char *RenderingContextDriverVulkanOHOS::_get_platform_surface_extension() const {
	// 鸿蒙平台 Vulkan Surface 扩展：VK_OHOS_surface
	return VK_OHOS_SURFACE_EXTENSION_NAME;
}

RenderingContextDriver::SurfaceID RenderingContextDriverVulkanOHOS::surface_create(const void *p_platform_data) {
	// 平台数据：XComponent 的 OHNativeWindow 句柄（见 ohos_xcomponent.h）
	const WindowPlatformData *wpd = (const WindowPlatformData *)(p_platform_data);
	ERR_FAIL_NULL_V(wpd, SurfaceID());
	ERR_FAIL_NULL_V(wpd->window, SurfaceID());

	VkSurfaceCreateInfoOHOS create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
	create_info.window = wpd->window;

	VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
	VkResult err = vkCreateSurfaceOHOS(instance_get(), &create_info, get_allocation_callbacks(VK_OBJECT_TYPE_SURFACE_KHR), &vk_surface);
	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, SurfaceID(), vformat("Couldn't create OHOS Surface (VkResult error %d).", err));

	Surface *surface = memnew(Surface);
	surface->vk_surface = vk_surface;
	return SurfaceID(surface);
}

bool RenderingContextDriverVulkanOHOS::_use_validation_layers() const {
	TightLocalVector<const char *> layer_names;
	Error err = _find_validation_layers(layer_names);

	// 鸿蒙上与 Android 策略一致：显式链接了验证层时才启用
	return (err == OK) && !layer_names.is_empty();
}

#endif // VULKAN_ENABLED
