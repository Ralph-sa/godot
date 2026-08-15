/**************************************************************************/
/*  crash_handler_ohos.cpp                                                */
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

#include "crash_handler_ohos.h"

#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <csignal>
#include <cstdlib>
#include <cstring>

#include <hilog/log.h>

// hilog 域名（0x0-0xFFFF 合法范围，超范围会被截断导致日志异常）
#define OHOS_LOG_DOMAIN 0xD001
#define OHOS_LOG_TAG "GodotOHOS"

// 需要拦截的崩溃信号集合
static const int crash_signals[] = {
	SIGABRT,
	SIGFPE,
	SIGILL,
	SIGSEGV,
	SIGBUS,
};

static void handle_crash(int p_signal) {
	// 崩溃信号处理：先还原默认行为（避免递归崩溃），再输出 hilog 日志
	const char *sig_name = strsignal(p_signal);
	OH_LOG_Print(LOG_APP, LOG_FATAL, OHOS_LOG_DOMAIN, OHOS_LOG_TAG,
			"Godot crashed with signal %d (%s)", p_signal, sig_name ? sig_name : "unknown");

	// 恢复默认信号处理，重新抛出以生成系统崩溃转储（core dump / 系统崩溃捕获）
	for (int sig : crash_signals) {
		signal(sig, SIG_DFL);
	}
	raise(p_signal);
}

CrashHandlerOHOS::CrashHandlerOHOS() = default;

CrashHandlerOHOS::~CrashHandlerOHOS() {
	disable();
}

void CrashHandlerOHOS::initialize() {
	if (disabled) {
		return;
	}
	// 注册崩溃信号处理器
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_crash;
	sigemptyset(&sa.sa_mask);

	for (int sig : crash_signals) {
		if (sigaction(sig, &sa, nullptr) != 0) {
			// 注册失败不致命（可能是调试器环境）
			WARN_PRINT(vformat("Couldn't install crash handler for signal %d.", sig));
		}
	}
}

void CrashHandlerOHOS::disable() {
	// 还原默认信号处理（退出前或禁用崩溃处理器时调用）
	if (disabled) {
		return;
	}
	disabled = true;
	for (int sig : crash_signals) {
		signal(sig, SIG_DFL);
	}
}
