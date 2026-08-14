/*
 * kernel/abi.h
 *
 * JARVIS OS — public kernel ABI (FastAPI bridge <-> C kernel via ctypes).
 * Every object crossing this boundary is a JSON string. The kernel owns
 * all returned pointers; the bridge must never free them.
 */

#ifndef JARVIS_ABI_H
#define JARVIS_ABI_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(JARVIS_KERNEL_BUILD)
#  define JVK_API __declspec(dllexport)
#elif defined(_WIN32)
#  define JVK_API __declspec(dllimport)
#else
#  define JVK_API
#endif

/* Boot the kernel with a JSON config string. Returns "ok" or an error. */
JVK_API const char* jvk_init(const char* config_json);

/* Execute one action. Input: {"action":"...", ...}; returns result_json.
   A failing command still returns {"ok":false,"error":...}. */
JVK_API const char* jvk_command(const char* action_json);

/* Advance the virtual clock, fire the timer interrupt, run the scheduler. */
JVK_API void jvk_tick(void);

/* Full system state JSON for the UI. */
JVK_API const char* jvk_snapshot(void);

/* Incremental kernel logs, starting from log index `since`. */
JVK_API const char* jvk_logs(int since);

/* Graceful shutdown. */
JVK_API void jvk_shutdown(void);

/* Copy the last error message (never NULL) into buf. Returns length. */
JVK_API int jvk_last_error(char* buf, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* JARVIS_ABI_H */