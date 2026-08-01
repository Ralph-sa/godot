/**************************************************************************/
/*  platform_config.h                                                     */
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

/* HarmonyOS 平台配置头。
 *
 * 提供平台相关的编译期配置：
 * 1. 平台线程：OHOS 使用 POSIX 线程（musl libc），无需线程实现覆盖；
 * 2. 栈大小：编辑器默认 8MB 栈（与 Linux/Android 一致）；
 * 3. 路径兼容：OHOS 为 POSIX 文件系统，direct_dir_access 可用。
 *
 * 注意：不要定义 PTHREAD_RENAME_SELF，OHOS 的 musl libc 要求
 * pthread_setname_np(pthread_t, const char*) 双参数版本。
 */

/* OHOS 使用 musl，无 glibc 专属扩展；保留 _GNU_SOURCE 由 detect.py 注入 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

/* 主线程栈大小（编辑器场景，单位字节）：8MB */
#ifndef MAIN_THREAD_STACK_SIZE
#define MAIN_THREAD_STACK_SIZE 8388608
#endif

/* OHOS 支持直接目录访问（POSIX opendir/readdir） */
#define DIRECT_DIR_ACCESS 1
