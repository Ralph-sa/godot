/**************************************************************************/
/*  harmonyos_engine_logger.cpp - Godot engine logger for HarmonyOS       */
/**************************************************************************/

#include "harmonyos_engine_logger.h"

#include "harmonyos_log.h"

#include <atomic>
#include <dlfcn.h>
#include <execinfo.h>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

static void _log_first_unexpected_nul_backtrace() {
	static std::atomic<bool> logged(false);
	bool expected = false;
	if (!logged.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
		return;
	}

	void *frames[64];
	int frame_count = backtrace(frames, 64);
	OH_LOG_ERROR(LOG_APP, "[UnicodeNUL] first occurrence, native frames=%{public}d", frame_count);
	for (int i = 1; i < frame_count; i++) {
		Dl_info info = {};
		if (dladdr(frames[i], &info) != 0 && info.dli_fbase) {
			const char *module = info.dli_fname ? strrchr(info.dli_fname, '/') : nullptr;
			module = module ? module + 1 : (info.dli_fname ? info.dli_fname : "<unknown>");
			uintptr_t offset = reinterpret_cast<uintptr_t>(frames[i]) - reinterpret_cast<uintptr_t>(info.dli_fbase);
			OH_LOG_ERROR(LOG_APP, "[UnicodeNUL] #%{public}d %{public}s+0x%{public}llx %{public}s",
					i, module, static_cast<unsigned long long>(offset),
					info.dli_sname ? info.dli_sname : "<no-symbol>");
		} else {
			OH_LOG_ERROR(LOG_APP, "[UnicodeNUL] #%{public}d pc=%{public}p", i, frames[i]);
		}
	}
}

const char *HarmonyOSEngineLogger::log_file_path() {
	// The virtual sandbox path is resolvable from inside the process; the
	// real path is used as a fallback (e.g. when the sandbox mount is not
	// yet ready during early startup).
	static const char *candidates[] = {
		"/data/storage/el2/base/haps/entry/files/godot_engine.log",
		"/data/app/el2/100/base/com.godotengine.editor/haps/entry/files/godot_engine.log",
	};

	for (const char *path : candidates) {
		FILE *f = fopen(path, "a");
		if (f) {
			fclose(f);
			return path;
		}
	}

	// None writable yet: try to create the parent directory of the virtual
	// path and return it anyway; subsequent logv calls will retry.
	mkdir("/data/storage/el2/base/haps/entry/files", 0770);
	return candidates[0];
}

void HarmonyOSEngineLogger::logv(const char *p_format, va_list p_list, bool p_err) {
	// Only keep the engine's errors in hilog to avoid flooding the logcat;
	// plain prints still go to the file for post-mortem analysis.
	char buf[4096];
	vsnprintf(buf, sizeof(buf), p_format, p_list);

	MutexLock lock(mutex);

	if (p_err) {
		OH_LOG_ERROR(LOG_APP, "[GodotEngine] %{public}s", buf);
	}

	FILE *f = fopen(log_file_path(), "a");
	if (f) {
		fprintf(f, "%s\n", buf);
		if (strstr(buf, "Unexpected NUL character") != nullptr) {
			_log_first_unexpected_nul_backtrace();
		}
		fclose(f);
	}
}

void HarmonyOSEngineLogger::log_error(const char *p_function, const char *p_file, int p_line,
		const char *p_code, const char *p_rationale, bool p_editor_notify, ErrorType p_type,
		const Vector<Ref<ScriptBacktrace>> &p_script_backtraces) {
	MutexLock lock(mutex);

	const char *type_str = error_type_string(p_type);
	char buf[4096];
	snprintf(buf, sizeof(buf), "[%s] %s:%d - %s() %s %s", type_str, p_file, p_line, p_function,
			p_code, p_rationale ? p_rationale : "");

	OH_LOG_ERROR(LOG_APP, "%{public}s", buf);

	FILE *f = fopen(log_file_path(), "a");
	if (f) {
		fprintf(f, "%s\n", buf);
		fclose(f);
	}
}
