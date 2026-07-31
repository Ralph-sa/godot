/**************************************************************************/
/*  harmonyos_engine_logger.h - Godot engine logger for HarmonyOS         */
/**************************************************************************/

#ifndef HARMONYOS_ENGINE_LOGGER_H
#define HARMONYOS_ENGINE_LOGGER_H

#include "core/io/logger.h"
#include "core/os/mutex.h"

// Logger registered on the HarmonyOS platform that mirrors Godot's
// print/printerr/print_error output into both hilog and an append-only
// file inside the app sandbox. The default StdLogger writes to stdout,
// which is not observable on HarmonyOS, making engine errors (e.g. Vulkan
// swap chain or shader failures) invisible at runtime.
class HarmonyOSEngineLogger : public Logger {
private:
	mutable Mutex mutex;
	static const char *log_file_path();

public:
	virtual void logv(const char *p_format, va_list p_list, bool p_err) override _PRINTF_FORMAT_ATTRIBUTE_2_0;
	virtual void log_error(const char *p_function, const char *p_file, int p_line, const char *p_code,
			const char *p_rationale, bool p_editor_notify = false, ErrorType p_type = ERR_ERROR,
			const Vector<Ref<ScriptBacktrace>> &p_script_backtraces = {}) override;

	virtual ~HarmonyOSEngineLogger() {}
};

#endif // HARMONYOS_ENGINE_LOGGER_H
