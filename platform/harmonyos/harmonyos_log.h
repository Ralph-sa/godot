/**************************************************************************/
/*  harmonyos_log.h - HiLog configuration for the HarmonyOS port          */
/**************************************************************************/

#ifndef HARMONYOS_LOG_H
#define HARMONYOS_LOG_H

// hilog/log.h defaults LOG_TAG to NULL and LOG_DOMAIN to 0 when the including
// translation unit has not set them:
//
//   #ifndef LOG_TAG
//   #define LOG_TAG NULL
//   #endif
//
//   #define OH_LOG_INFO(type, ...) \
//       ((void)OH_LOG_Print((type), LOG_INFO, LOG_DOMAIN, LOG_TAG, __VA_ARGS__))
//
// OH_LOG_Print silently drops records whose tag is NULL, so every OH_LOG_*
// call in a file that includes <hilog/log.h> directly is discarded — the logs
// never reach the hilog buffer and the port looks silent at runtime.
//
// Defining both macros here, before the SDK header, keeps engine output
// visible. Include this header instead of <hilog/log.h> anywhere in the
// HarmonyOS platform layer; including the SDK header first would let its
// fallback definitions win.

#ifndef LOG_DOMAIN
// Matches the domain used by the ArkTS layer so a single hilog filter covers
// the whole app.
#define LOG_DOMAIN 0x0000
#endif

#ifndef LOG_TAG
#define LOG_TAG "Godot"
#endif

#include <hilog/log.h>

#endif // HARMONYOS_LOG_H
