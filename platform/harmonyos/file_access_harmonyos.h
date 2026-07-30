/**************************************************************************/
/*  file_access_harmonyos.h - HarmonyOS Sandbox-Aware File Access         */
/**************************************************************************/

#pragma once

#include "drivers/unix/file_access_unix.h"

// FileAccessHarmonyOS: inherits from FileAccessUnix
// Adds sandbox boundary validation for file open operations
class FileAccessHarmonyOS : public FileAccessUnix {
	GDSOFTCLASS(FileAccessHarmonyOS, FileAccessUnix);

public:
	virtual Error open_internal(const String &p_path, int p_mode_flags) override;
	virtual bool file_exists(const String &p_path) override;
};
