/**************************************************************************/
/*  crash_handler_harmonyos.cpp - HarmonyOS Crash Handler                 */
/**************************************************************************/

#include "crash_handler_harmonyos.h"

#ifdef CRASH_HANDLER_ENABLED

#include "core/config/project_settings.h"
#include "core/object/script_language.h"
#include "core/os/main_loop.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "core/version.h"
#include "main/main.h"

#ifdef HARMONYOS_ENABLED
#include <hilog/log.h>
#endif

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

void CrashHandlerHarmonyOS::handle_signal(int sig, siginfo_t *info, void *ctx) {
	// Restore default handlers to prevent re-entrant crashes.
	signal(SIGSEGV, SIG_DFL);
	signal(SIGABRT, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGILL, SIG_DFL);

	if (OS::get_singleton() == nullptr) {
#ifdef HARMONYOS_ENABLED
		OH_LOG_ERROR(LOG_APP, "CrashHandler: OS singleton is null, aborting");
#endif
		abort();
	}

	if (OS::get_singleton()->is_crash_handler_silent()) {
		std::_Exit(0);
	}

	// Log crash signal type and fault address via hilog.
	const char *sig_name = "UNKNOWN";
	switch (sig) {
		case SIGSEGV:
			sig_name = "SIGSEGV";
			break;
		case SIGABRT:
			sig_name = "SIGABRT";
			break;
		case SIGFPE:
			sig_name = "SIGFPE";
			break;
		case SIGILL:
			sig_name = "SIGILL";
			break;
	}

	if (info && info->si_addr) {
#ifdef HARMONYOS_ENABLED
		OH_LOG_ERROR(LOG_APP, "CrashHandler: signal %{public}s (%{public}d), fault addr=%{public}p",
				sig_name, sig, info->si_addr);
#endif
	} else {
#ifdef HARMONYOS_ENABLED
		OH_LOG_ERROR(LOG_APP, "CrashHandler: signal %{public}s (%{public}d)", sig_name, sig);
#endif
	}

	// Collect backtrace using musl libc.
	void *bt_buffer[256];
	int size = backtrace(bt_buffer, 256);

	String msg;
	if (ProjectSettings::get_singleton()) {
		msg = GLOBAL_GET("debug/settings/crash_handler/message");
	}

	// Tell MainLoop about the crash.
	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_CRASH);
	}

	print_error("\n================================================================");
	print_error(vformat("CrashHandler: Program crashed with signal %s (%d)", String(sig_name), sig));

	if (String(GODOT_VERSION_HASH).is_empty()) {
		print_error(vformat("Engine version: %s", GODOT_VERSION_FULL_NAME));
	} else {
		print_error(vformat("Engine version: %s (%s)", GODOT_VERSION_FULL_NAME, GODOT_VERSION_HASH));
	}
	print_error(vformat("Dumping the backtrace. %s", msg));

	// Resolve dladdr for the last frame to get the base load address.
	void *load_addr = nullptr;
	Dl_info dl_info;
	if (size > 0 && dladdr(bt_buffer[size - 1], &dl_info)) {
		load_addr = dl_info.dli_fbase;
	}
	print_error(vformat("Load address: %p", load_addr));

	// Print stack frames.
	for (int i = 1; i < size; i++) {
		uintptr_t pc = (uintptr_t)bt_buffer[i];
		const char *mod_name = "main";
		uintptr_t mod_off = (uintptr_t)load_addr;

		Dl_info frame_info;
		if (dladdr(bt_buffer[i], &frame_info)) {
			mod_off = (uintptr_t)frame_info.dli_fbase;
			if (mod_off != (uintptr_t)load_addr && frame_info.dli_fname && frame_info.dli_fname[0]) {
				const char *slash = strrchr(frame_info.dli_fname, '/');
				mod_name = slash ? slash + 1 : frame_info.dli_fname;
			}

			if (frame_info.dli_sname) {
				char *demangled = nullptr;
				int status = 0;
				if (frame_info.dli_sname[0] == '_') {
					demangled = abi::__cxa_demangle(frame_info.dli_sname, nullptr, nullptr, &status);
				}

				if (status == 0 && demangled) {
					print_error(vformat("[%d] %p (%s+%p) - %s", i, (void *)pc, mod_name, (void *)(pc - mod_off), String(demangled)));
					free(demangled);
				} else {
					print_error(vformat("[%d] %p (%s+%p) - %s", i, (void *)pc, mod_name, (void *)(pc - mod_off), String(frame_info.dli_sname)));
				}
			} else {
				print_error(vformat("[%d] %p (%s+%p) - ???", i, (void *)pc, mod_name, (void *)(pc - mod_off)));
			}
		} else {
			mod_name = "<unknown>";
			print_error(vformat("[%d] %p (%s+%p) - ???", i, (void *)pc, mod_name, (void *)(pc - mod_off)));
		}
	}

	print_error("-- END OF C++ BACKTRACE --");
	print_error("================================================================");

	// Capture and print script backtraces.
	for (const Ref<ScriptBacktrace> &backtrace : ScriptServer::capture_script_backtraces(false)) {
		if (!backtrace->is_empty()) {
			print_error(backtrace->format());
			print_error(vformat("-- END OF %s BACKTRACE --", backtrace->get_language_name().to_upper()));
			print_error("================================================================");
		}
	}

	abort();
}

#endif // CRASH_HANDLER_ENABLED

CrashHandlerHarmonyOS::CrashHandlerHarmonyOS() {
	disabled = false;
}

CrashHandlerHarmonyOS::~CrashHandlerHarmonyOS() {
	disable();
}

void CrashHandlerHarmonyOS::disable() {
	if (disabled) {
		return;
	}

#ifdef CRASH_HANDLER_ENABLED
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	sigaction(SIGSEGV, &sa, nullptr);
	sigaction(SIGABRT, &sa, nullptr);
	sigaction(SIGFPE, &sa, nullptr);
	sigaction(SIGILL, &sa, nullptr);
#endif

	disabled = true;
}

void CrashHandlerHarmonyOS::initialize() {
#ifdef CRASH_HANDLER_ENABLED
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = handle_signal;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;

	sigaction(SIGSEGV, &sa, nullptr);
	sigaction(SIGABRT, &sa, nullptr);
	sigaction(SIGFPE, &sa, nullptr);
	sigaction(SIGILL, &sa, nullptr);

#ifdef HARMONYOS_ENABLED
	OH_LOG_INFO(LOG_APP, "CrashHandler: signal handlers installed");
#endif
#endif
}
