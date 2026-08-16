#pragma once

// OHOS 平台的 EGL 头（对应 platform/android/platform_egl.h）。
// drivers/egl 以 <platform_egl.h> 引入平台 EGL 声明；OHOS 无桌面 GL，
// 仅提供 EGL + OpenGL ES（EGL_KHR_platform_ohos 扩展，见 eglext.h）。

// IWYU pragma: begin_exports.
#include <EGL/egl.h>
#include <EGL/eglext.h>
// IWYU pragma: end_exports.
