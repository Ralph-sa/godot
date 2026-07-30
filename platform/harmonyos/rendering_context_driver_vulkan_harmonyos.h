/**************************************************************************/
/*  rendering_context_driver_vulkan_harmonyos.h                           */
/**************************************************************************/

#pragma once

#ifdef VULKAN_ENABLED

#include "drivers/vulkan/rendering_context_driver_vulkan.h"

class RenderingContextDriverVulkanHarmonyOS : public RenderingContextDriverVulkan {
private:
	virtual const char *_get_platform_surface_extension() const override;

protected:
	virtual SurfaceID surface_create(const void *p_platform_data) override;
	virtual bool _use_validation_layers() const override;

public:
	struct WindowPlatformData {
		void *native_window; // OHNativeWindow*
	};

	RenderingContextDriverVulkanHarmonyOS() = default;
	~RenderingContextDriverVulkanHarmonyOS() override = default;
};

#endif // VULKAN_ENABLED
