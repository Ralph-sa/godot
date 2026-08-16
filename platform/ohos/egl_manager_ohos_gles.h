#pragma once

#ifdef OHOS_ENABLED
#ifdef EGL_ENABLED
#ifdef GLES3_ENABLED

#include "drivers/egl/egl_manager.h"

/* EGLManagerOHOSGLES：鸿蒙 GLES3 渲染上下文管理。
 *
 * 鸿蒙仅提供 OpenGL ES（无桌面 GL），对应 Wayland 的 EGLManagerWaylandGLES：
 *  - platform 扩展用鸿蒙专有 EGL_KHR_platform_ohos（EGL_PLATFORM_OHOS_KHR，
 *    native_display 固定为 EGL_DEFAULT_DISPLAY）；
 *  - API 绑定 EGL_OPENGL_ES_API，上下文 ES 3.x；
 *  - 窗口 surface 直接使用 OHNativeWindow*（surfaceId 路径创建）。
 */
class EGLManagerOHOSGLES : public EGLManager {
public:
	virtual const char *_get_platform_extension_name() const override;
	virtual EGLenum _get_platform_extension_enum() const override;
	virtual EGLenum _get_platform_api_enum() const override;
	virtual Vector<EGLAttrib> _get_platform_display_attributes() const override;
	virtual Vector<EGLint> _get_platform_context_attribs() const override;
};

#endif // GLES3_ENABLED
#endif // EGL_ENABLED
#endif // OHOS_ENABLED
