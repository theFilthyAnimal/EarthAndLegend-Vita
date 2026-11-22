#include "utils/trace.h"

#include <stdarg.h>
#include <stdio.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#define TRACE_FOPEN  sceLibcBridge_fopen
#define TRACE_FWRITE sceLibcBridge_fwrite
#define TRACE_FFLUSH sceLibcBridge_fflush
#else
#define TRACE_FOPEN  fopen
#define TRACE_FWRITE fwrite
#define TRACE_FFLUSH fflush
#endif

#define TRACE_PATH DATA_PATH "eal.log"

static FILE *trace_file = NULL;
static int trace_attempted = 0;

void eal_trace(const char *fmt, ...) {
    if (!trace_attempted) {
        trace_file = TRACE_FOPEN(TRACE_PATH, "a");
        trace_attempted = 1;
    }

    if (!trace_file)
        return;

    char buffer[768];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    size_t len = 0;
    while (buffer[len] != '\0' && len < sizeof(buffer) - 1)
        len++;

    buffer[len++] = '\n';

    TRACE_FWRITE(buffer, 1, len, trace_file);
    TRACE_FFLUSH(trace_file);
}
