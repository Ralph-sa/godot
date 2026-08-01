/**************************************************************************/
/*  crash_handler_ohos.h                                                  */
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

/* CrashHandlerOHOS：鸿蒙崩溃处理器。
 *
 * 对应 macOS 的 CrashHandler（安装 SIGABRT/SIGSEGV 信号处理器）。
 * 鸿蒙上同样通过 sigaction 拦截崩溃信号，将栈回溯写入 hilog（系统日志），
 * 便于通过 hdc log 拉取编辑器/游戏崩溃现场。
 *
 * 第 1 轮（骨架期）：实现信号注册与 hilog 输出；
 * 后续轮次可扩展写崩溃文件（应用沙盒内 crash/ 目录）。
 */
class CrashHandlerOHOS {
	bool disabled = false;

public:
	void initialize();
	void disable();
	bool is_disabled() const { return disabled; }

	CrashHandlerOHOS();
	~CrashHandlerOHOS();
};
