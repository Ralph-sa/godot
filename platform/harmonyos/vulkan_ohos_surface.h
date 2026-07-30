/**************************************************************************/
/*  vulkan_ohos_surface.h - Minimal OHOS Vulkan Surface API               */
/**************************************************************************/

#pragma once

// This file provides minimal Vulkan OHOS surface API declarations for use
// when Godot's volk loader sets VK_NO_PROTOTYPES (which disables platform
// extension prototypes from the system Vulkan headers).
//
// The OHNativeWindow type is expected to already be defined by the OHOS SDK
// headers (native_interface_xcomponent.h → external_window.h) before this
// file is included. We do NOT redefine it to avoid type conflicts.
// However, if this file is included before the SDK headers, a guarded
// forward declaration is provided to make it parseable.
#ifndef OH_NATIVE_WINDOW
#define OH_NATIVE_WINDOW
struct NativeWindow;
typedef struct NativeWindow OHNativeWindow;
#endif

#define VK_OHOS_SURFACE_SPEC_VERSION 1
#define VK_OHOS_SURFACE_EXTENSION_NAME "VK_OHOS_surface"

#ifndef VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS
#define VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS ((VkStructureType)1000403000)
#endif

#ifndef VK_OHOS_SURFACE_CREATE_INFO_DEFINED
#define VK_OHOS_SURFACE_CREATE_INFO_DEFINED

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

#endif // VK_OHOS_SURFACE_CREATE_INFO_DEFINED
