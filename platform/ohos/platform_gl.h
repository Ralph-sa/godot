#pragma once

// OHOS 平台的 GL 头（对应 platform/linuxbsd/platform_gl.h 的 GLES 子集）。
// 鸿蒙仅提供 OpenGL ES（无桌面 GL）：GLES_API_ENABLED + GLAD_GLES2 使
// gles3 驱动走纯 GLES 代码路径，全部 GL 函数声明由 glad 提供
// （libGLESv3.so 静态链接 + glad 运行时加载，对应 Linux opengl3_es 模式）。

#ifndef GLES_API_ENABLED
#define GLES_API_ENABLED // Allow using GLES.
#endif

#ifndef GLAD_GLES2
#define GLAD_GLES2
#endif

#include <thirdparty/glad/glad/gl.h> // IWYU pragma: export.
