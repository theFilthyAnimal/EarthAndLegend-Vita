/*
 * Earth And Legend (PS Vita) - lightweight file trace logger.
 *
 * The regular logger writes to the debug TTY, which is awkward to collect on
 * a retail console. This helper appends important events to a file in the
 * data directory so that crashes can be diagnosed from the last lines.
 */

#ifndef EAL_TRACE_H
#define EAL_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

void eal_trace(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif // EAL_TRACE_H
