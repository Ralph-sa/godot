#include "egl_manager_ohos_gles.h"

#ifdef OHOS_ENABLED
#ifdef EGL_ENABLED
#ifdef GLES3_ENABLED

const char *EGLManagerOHOSGLES::_get_platform_extension_name() const {
	return "EGL_KHR_platform_ohos";
}

EGLenum EGLManagerOHOSGLES::_get_platform_extension_enum() const {
	return EGL_PLATFORM_OHOS_KHR;
}

EGLenum EGLManagerOHOSGLES::_get_platform_api_enum() const {
	return EGL_OPENGL_ES_API;
}

Vector<EGLAttrib> EGLManagerOHOSGLES::_get_platform_display_attributes() const {
	return Vector<EGLAttrib>();
}

Vector<EGLint> EGLManagerOHOSGLES::_get_platform_context_attribs() const {
	Vector<EGLint> ret;
	ret.push_back(EGL_CONTEXT_MAJOR_VERSION);
	ret.push_back(3);
	ret.push_back(EGL_NONE);

	return ret;
}

#endif // GLES3_ENABLED
#endif // EGL_ENABLED
#endif // OHOS_ENABLED
