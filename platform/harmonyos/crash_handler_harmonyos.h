/**************************************************************************/
/*  crash_handler_harmonyos.h - HarmonyOS Crash Handler                   */
/**************************************************************************/

#pragma once

#ifdef DEBUG_ENABLED
#define CRASH_HANDLER_ENABLED 1
#endif

#include <csignal>

class CrashHandlerHarmonyOS {
	bool disabled = false;

	static void handle_signal(int sig, siginfo_t *info, void *ctx);

public:
	void initialize();
	void disable();
	bool is_disabled() const { return disabled; }

	CrashHandlerHarmonyOS();
	~CrashHandlerHarmonyOS();
};
