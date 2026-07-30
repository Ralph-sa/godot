/**************************************************************************/
/*  rendering_context_driver_vulkan_harmonyos.cpp                         */
/**************************************************************************/

#include "rendering_context_driver_vulkan_harmonyos.h"

#ifdef VULKAN_ENABLED

#include "core/variant/variant.h"

#include <drivers/vulkan/godot_vulkan.h>

// Use minimal OHOS surface API header to avoid conflicts
// with Godot's bundled vulkan headers (VK_NO_PROTOTYPES is set by volk).
#include "vulkan_ohos_surface.h"
#include "harmonyos_native_window.h"

const char *RenderingContextDriverVulkanHarmonyOS::_get_platform_surface_extension() const {
	return VK_OHOS_SURFACE_EXTENSION_NAME;
}

RenderingContextDriver::SurfaceID RenderingContextDriverVulkanHarmonyOS::surface_create(const void *p_platform_data) {
	const WindowPlatformData *wpd = reinterpret_cast<const WindowPlatformData *>(p_platform_data);
	ERR_FAIL_NULL_V(wpd, SurfaceID());

	// Load vkCreateSurfaceOHOS dynamically (VK_NO_PROTOTYPES is set by volk)
	PFN_vkCreateSurfaceOHOS vkCreateSurfaceOHOS_func =
		reinterpret_cast<PFN_vkCreateSurfaceOHOS>(
			vkGetInstanceProcAddr(instance_get(), "vkCreateSurfaceOHOS"));
	ERR_FAIL_NULL_V_MSG(vkCreateSurfaceOHOS_func, SurfaceID(),
		"vkCreateSurfaceOHOS not available");

	VkSurfaceCreateInfoOHOS create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.window = reinterpret_cast<OHNativeWindow *>(wpd->native_window);

	VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
	VkResult err = vkCreateSurfaceOHOS_func(instance_get(), &create_info,
		get_allocation_callbacks(VK_OBJECT_TYPE_SURFACE_KHR), &vk_surface);
	ERR_FAIL_COND_V_MSG(err != VK_SUCCESS, SurfaceID(),
		vformat("Couldn't create OHOS Surface (VkResult error %d).", err));

	Surface *surface = memnew(Surface);
	surface->vk_surface = vk_surface;
	return SurfaceID(surface);
}

bool RenderingContextDriverVulkanHarmonyOS::_use_validation_layers() const {
	TightLocalVector<const char *> layer_names;
	Error err = _find_validation_layers(layer_names);
	return (err == OK) && !layer_names.is_empty();
}

#endif // VULKAN_ENABLED
