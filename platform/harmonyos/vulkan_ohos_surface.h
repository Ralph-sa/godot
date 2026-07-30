/**************************************************************************/
/*  vulkan_ohos_surface.h - Minimal OHOS Vulkan Surface API               */
/**************************************************************************/

#pragma once

// Forward declare OHNativeWindow (defined in <native_window/external_window.h>)
typedef struct NativeWindow OHNativeWindow;

#define VK_OHOS_SURFACE_SPEC_VERSION 1
#define VK_OHOS_SURFACE_EXTENSION_NAME "VK_OHOS_surface"

#ifndef VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS
#define VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS ((VkStructureType)1000403000)
#endif

typedef VkFlags VkSurfaceCreateFlagsOHOS;

typedef struct VkSurfaceCreateInfoOHOS {
	VkStructureType sType;
	const void *pNext;
	VkSurfaceCreateFlagsOHOS flags;
	OHNativeWindow *window;
} VkSurfaceCreateInfoOHOS;

typedef VkResult(VKAPI_PTR *PFN_vkCreateSurfaceOHOS)(
	VkInstance instance,
	const VkSurfaceCreateInfoOHOS *pCreateInfo,
	const VkAllocationCallbacks *pAllocator,
	VkSurfaceKHR *pSurface);
