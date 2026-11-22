/*
 * Earth And Legend (PS Vita) - missing libc / OpenAL / OpenSSL stubs.
 *
 * The game library imports a number of OpenAL and OpenSSL symbols. Audio is
 * currently not implemented and the game is single player only, so those
 * imports are replaced by safe stubs. A few libc functions that are not
 * present in newlib are implemented here as well.
 */

#include "reimpl/stubs.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reimpl/io.h"
#include "reimpl/openal.h"

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

/*
 * Generic stub.
 */

uintptr_t eal_stub_generic(void) {
    return 0;
}

/*
 * libc.
 */

int eal_scandir(const char * dir, void ** namelist, void * filter,
                void * compar) {
    if (namelist)
        *namelist = NULL;

    if (!dir)
        return -1;

    DIR * d = opendir_soloader((char *) dir);
    if (!d)
        return -1;

    dirent64_bionic ** list = NULL;
    int count = 0;
    int capacity = 0;

    dirent64_bionic * entry;
    while ((entry = readdir_soloader(d)) != NULL) {
        if (filter && !((int (*)(const dirent64_bionic *)) filter)(entry))
            continue;

        if (count == capacity) {
            int new_capacity = capacity ? capacity * 2 : 16;
            dirent64_bionic ** new_list =
                realloc(list, (size_t) new_capacity * sizeof(*list));
            if (!new_list)
                break;
            list = new_list;
            capacity = new_capacity;
        }

        dirent64_bionic * copy = malloc(sizeof(*copy));
        if (!copy)
            break;

        memcpy(copy, entry, sizeof(*copy));
        list[count++] = copy;
    }

    closedir_soloader(d);

    if (compar && count > 1)
        qsort(list, (size_t) count, sizeof(*list),
              (int (*)(const void *, const void *)) compar);

    if (!namelist) {
        for (int i = 0; i < count; i++)
            free(list[i]);
        free(list);
        return count;
    }

    *namelist = list;
    return count;
}

int eal_alphasort(const void * a, const void * b) {
    const dirent64_bionic * da = *(const dirent64_bionic * const *) a;
    const dirent64_bionic * db = *(const dirent64_bionic * const *) b;
    return strcmp(da->d_name, db->d_name);
}

int eal_uname(void * buf) {
    if (!buf)
        return -1;

    char * p = (char *) buf;
    memset(p, 0, 65 * 6);

    strncpy(p + 0 * 65, "PlayStation Vita", 64);
    strncpy(p + 1 * 65, "vita", 64);
    strncpy(p + 2 * 65, "3.65", 64);
    strncpy(p + 3 * 65, "vita", 64);
    strncpy(p + 4 * 65, "armv7l", 64);

    return 0;
}

uint32_t eal_inet_addr(const char * cp) {
    (void) cp;
    return 0xFFFFFFFFu;
}

int eal_vfscanf(FILE * stream, const char * format, va_list ap) {
    return vfscanf(stream, format, ap);
}

float eal_fmaf(float x, float y, float z) {
    return x * y + z;
}

float eal_nextafterf(float x, float y) {
    return (float) nextafter((double) x, (double) y);
}

/*
 * Lookup table.
 */

typedef struct {
    const char * name;
    uintptr_t func;
} StubEntry;

static const StubEntry stub_table[] = {
    // libc
    { "scandir",        (uintptr_t) &eal_scandir },
    { "alphasort",      (uintptr_t) &eal_alphasort },
    { "uname",          (uintptr_t) &eal_uname },
    { "inet_addr",      (uintptr_t) &eal_inet_addr },
    { "vfscanf",        (uintptr_t) &eal_vfscanf },
    { "fmaf",           (uintptr_t) &eal_fmaf },
    { "nextafterf",     (uintptr_t) &eal_nextafterf },

    // OpenAL
    { "alGenBuffers",             (uintptr_t) &eal_alGenBuffers },
    { "alGenSources",             (uintptr_t) &eal_alGenSources },
    { "alDeleteBuffers",          (uintptr_t) &eal_alDeleteBuffers },
    { "alDeleteSources",          (uintptr_t) &eal_alDeleteSources },
    { "alGetError",               (uintptr_t) &eal_alGetError },
    { "alIsBuffer",               (uintptr_t) &eal_alIsBuffer },
    { "alIsExtensionPresent",     (uintptr_t) &eal_alIsExtensionPresent },
    { "alGetString",              (uintptr_t) &eal_alGetString },
    { "alGetBufferi",             (uintptr_t) &eal_alGetBufferi },
    { "alGetSourcei",             (uintptr_t) &eal_alGetSourcei },
    { "alGetProcAddress",         (uintptr_t) &eal_alGetProcAddress },
    { "alBufferData",             (uintptr_t) &eal_alBufferData },
    { "alSourcePlay",             (uintptr_t) &eal_alSourcePlay },
    { "alSourcePause",            (uintptr_t) &eal_alSourcePause },
    { "alSourceStop",             (uintptr_t) &eal_alSourceStop },
    { "alSourceRewind",           (uintptr_t) &eal_alSourceRewind },
    { "alSourceQueueBuffers",     (uintptr_t) &eal_alSourceQueueBuffers },
    { "alSourceUnqueueBuffers",   (uintptr_t) &eal_alSourceUnqueueBuffers },
    { "alSourcei",                (uintptr_t) &eal_alSourcei },
    { "alSourcef",                (uintptr_t) &eal_alSourcef },
    { "alSource3f",               (uintptr_t) &eal_alSource3f },
    { "alSource3i",               (uintptr_t) &eal_alSource3i },
    { "alSourcefv",               (uintptr_t) &eal_alSourcefv },
    { "alListenerf",              (uintptr_t) &eal_alListenerf },
    { "alListener3f",             (uintptr_t) &eal_alListener3f },
    { "alListenerfv",             (uintptr_t) &eal_alListenerfv },
    { "alcOpenDevice",            (uintptr_t) &eal_alcOpenDevice },
    { "alcCloseDevice",           (uintptr_t) &eal_alcCloseDevice },
    { "alcCreateContext",         (uintptr_t) &eal_alcCreateContext },
    { "alcMakeContextCurrent",    (uintptr_t) &eal_alcMakeContextCurrent },
    { "alcGetCurrentContext",     (uintptr_t) &eal_alcGetCurrentContext },
    { "alcGetContextsDevice",     (uintptr_t) &eal_alcGetContextsDevice },
    { "alcDestroyContext",        (uintptr_t) &eal_alcDestroyContext },
    { "alcGetError",              (uintptr_t) &eal_alcGetError },
    { "alcIsExtensionPresent",    (uintptr_t) &eal_alcIsExtensionPresent },
    { "alcGetString",             (uintptr_t) &eal_alcGetString },
    { "alcGetIntegerv",           (uintptr_t) &eal_alcGetIntegerv },
    { "alcProcessContext",        (uintptr_t) &eal_alcProcessContext },
    { "alcSuspendContext",        (uintptr_t) &eal_alcSuspendContext },
    { "alcCaptureOpenDevice",     (uintptr_t) &eal_alcCaptureOpenDevice },
    { "alcCaptureCloseDevice",    (uintptr_t) &eal_alcCaptureCloseDevice },
    { "alcCaptureStart",          (uintptr_t) &eal_alcCaptureStart },
    { "alcCaptureStop",           (uintptr_t) &eal_alcCaptureStop },
    { "alcCaptureSamples",        (uintptr_t) &eal_alcCaptureSamples },
};

uintptr_t eal_stub_for_symbol(const char * name) {
    for (unsigned i = 0; i < sizeof(stub_table) / sizeof(StubEntry); i++) {
        if (strcmp(name, stub_table[i].name) == 0)
            return stub_table[i].func;
    }

    return (uintptr_t) &eal_stub_generic;
}

int eal_has_stub(const char * name) {
    for (unsigned i = 0; i < sizeof(stub_table) / sizeof(StubEntry); i++) {
        if (strcmp(name, stub_table[i].name) == 0)
            return 1;
    }

    return 0;
}
