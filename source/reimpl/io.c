/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2024 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/io.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdarg.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/mutex.h>

#ifdef USE_SCELIBC_IO
#include <libc_bridge/libc_bridge.h>
#endif

#include "utils/logger.h"
#include "utils/settings.h"
#include "utils/trace.h"
#include "utils/utils.h"

static uint64_t eal_read_bytes = 0;
static uint64_t eal_read_time_us = 0;
static unsigned eal_pack_misses = 0;
static uint64_t eal_pack_time_us = 0;

void eal_io_take_read_stats(uint64_t *bytes, uint64_t *time_us,
                            unsigned *pack_misses, uint64_t *pack_time_us) {
    if (bytes)
        *bytes = eal_read_bytes;
    if (time_us)
        *time_us = eal_read_time_us;
    if (pack_misses)
        *pack_misses = eal_pack_misses;
    if (pack_time_us)
        *pack_time_us = eal_pack_time_us;
    eal_read_bytes = 0;
    eal_read_time_us = 0;
    eal_pack_misses = 0;
    eal_pack_time_us = 0;
}

/* The game pack is read through many small fread()/fseek() calls. On the
 * Vita every such call goes through SceLibcBridge and often hits the memory
 * card, which makes scene loading several times slower than the raw read
 * speed. Keep one small read-only block cache shared by all open pack
 * streams and serve the engine's small reads from RAM.
 *
 * The buffer is deliberately small (256 KB): during scene changes and the
 * engine's shutdown the Vita only has a few megabytes of free user memory,
 * so per-stream caches used to make the shutdown path run out of memory. */
#define PACK_MIN_BLOCK    (16 * 1024)
#define PACK_MAX_BLOCK    (256 * 1024)
#define PACK_STREAM_SLOTS 4

typedef struct {
    FILE *stream;
    int64_t logical;
    int64_t size;
} pack_stream;

static pack_stream pack_streams[PACK_STREAM_SLOTS];

static uint8_t *pack_cache = NULL;
static FILE *pack_cache_owner = NULL;
static int64_t pack_cache_base = -1;
static size_t pack_cache_size = 0;
static size_t pack_block_size = PACK_MIN_BLOCK;
static int pack_cache_disabled = 0;

/* The engine reads pack resources from more than one thread. The cache is
 * shared between streams, so protect the stream state and the buffer. */
static SceUID eal_pack_mutex = -1;

void eal_io_init(void) {
    if (eal_pack_mutex < 0)
        eal_pack_mutex = sceKernelCreateMutex("eal_pack", 0, 0, NULL);
}

static void pack_lock(void) {
    if (eal_pack_mutex >= 0)
        sceKernelLockMutex(eal_pack_mutex, 1, NULL);
}

static void pack_unlock(void) {
    if (eal_pack_mutex >= 0)
        sceKernelUnlockMutex(eal_pack_mutex, 1);
}

void eal_pack_cache_disable(void) {
    pack_lock();
    pack_cache_disabled = 1;
    if (pack_cache) {
        free(pack_cache);
        pack_cache = NULL;
        eal_trace("io: pack cache disabled for shutdown");
    }
    pack_cache_owner = NULL;
    pack_cache_base = -1;
    pack_cache_size = 0;
    pack_unlock();
}

static pack_stream * pack_find(FILE *stream) {
    if (!stream)
        return NULL;

    for (int i = 0; i < PACK_STREAM_SLOTS; i++) {
        if (pack_streams[i].stream == stream)
            return &pack_streams[i];
    }
    return NULL;
}

static void pack_remove(FILE *stream) {
    pack_lock();
    pack_stream *p = pack_find(stream);
    if (!p) {
        pack_unlock();
        return;
    }

    if (pack_cache_owner == stream) {
        pack_cache_owner = NULL;
        pack_cache_base = -1;
        pack_cache_size = 0;
    }
    memset(p, 0, sizeof(*p));
    pack_unlock();
}

static pack_stream * pack_register(FILE *stream) {
    if (eal_pack_mutex < 0)
        eal_io_init();

    pack_lock();
    pack_stream *found = pack_find(stream);
    if (found) {
        pack_unlock();
        return found;
    }

    for (int i = 0; i < PACK_STREAM_SLOTS; i++) {
        if (pack_streams[i].stream == NULL) {
            pack_stream *p = &pack_streams[i];
            memset(p, 0, sizeof(*p));
            p->stream = stream;

#ifdef USE_SCELIBC_IO
            sceLibcBridge_fseek(stream, 0, SEEK_END);
            p->size = (int64_t) sceLibcBridge_ftell(stream);
            sceLibcBridge_fseek(stream, 0, SEEK_SET);
#else
            fseek(stream, 0, SEEK_END);
            p->size = (int64_t) ftell(stream);
            fseek(stream, 0, SEEK_SET);
#endif
            if (p->size <= 0) {
                eal_trace("io: pack cache disabled (size %lld)",
                          (long long) p->size);
                memset(p, 0, sizeof(*p));
                pack_unlock();
                return NULL;
            }

            eal_trace("io: pack cache stream %p registered (size %lld)",
                      (void *) stream, (long long) p->size);
            pack_unlock();
            return p;
        }
    }

    pack_unlock();
    return NULL;
}

static int pack_memory_available(size_t need) {
    SceKernelFreeMemorySizeInfo info;
    info.size = sizeof(info);
    if (sceKernelGetFreeMemorySize(&info) != 0)
        return 1;
    return info.size_user > need;
}

static size_t pack_fill(pack_stream *p, int64_t offset) {
    if (pack_cache_disabled)
        return 0;

    if (pack_cache_owner == p->stream &&
        offset >= pack_cache_base &&
        offset < pack_cache_base + (int64_t) pack_cache_size)
        return pack_cache_size;

    /* The engine re-initializes Lua and scripting during shutdown, when free
     * user memory can drop below a megabyte. Drop the cache in that case so
     * the engine's own allocations succeed; reads then go to the file. */
    if (pack_cache && !pack_memory_available(1024 * 1024)) {
        free(pack_cache);
        pack_cache = NULL;
        pack_cache_owner = NULL;
        pack_cache_base = -1;
        pack_cache_size = 0;
        return 0;
    }

    if (!pack_cache) {
        if (!pack_memory_available(2 * 1024 * 1024))
            return 0;

        pack_cache = malloc(PACK_MAX_BLOCK);
        if (!pack_cache) {
            eal_trace("io: pack cache allocation failed");
            return 0;
        }
    }

    /* Sequential reads (scene loading) grow the block up to 256 KB; random
     * reads (resource streaming while walking) stay at 16 KB so the port
     * does not over-read megabytes for a few tiny requests. */
    int sequential = (pack_cache_owner == p->stream &&
                      pack_cache_size > 0 &&
                      offset == pack_cache_base + (int64_t) pack_cache_size);
    if (sequential) {
        if (pack_block_size < PACK_MAX_BLOCK)
            pack_block_size <<= 1;
    } else {
        pack_block_size = PACK_MIN_BLOCK;
    }

    int64_t base = offset - (offset % (int64_t) pack_block_size);

    uint64_t t0 = sceKernelGetProcessTimeWide();

#ifdef USE_SCELIBC_IO
    if (sceLibcBridge_fseek(p->stream, (long) base, SEEK_SET) != 0) {
        pack_cache_owner = NULL;
        pack_cache_base = -1;
        pack_cache_size = 0;
        return 0;
    }
    size_t got = sceLibcBridge_fread(pack_cache, 1, pack_block_size,
                                     p->stream);
#else
    if (fseek(p->stream, (long) base, SEEK_SET) != 0) {
        pack_cache_owner = NULL;
        pack_cache_base = -1;
        pack_cache_size = 0;
        return 0;
    }
    size_t got = fread(pack_cache, 1, pack_block_size, p->stream);
#endif

    uint64_t t1 = sceKernelGetProcessTimeWide();
    eal_pack_time_us += t1 - t0;
    eal_read_time_us += t1 - t0;
    eal_pack_misses++;

    pack_cache_owner = p->stream;
    pack_cache_base = base;
    pack_cache_size = got;
    return got;
}

/* Relative paths from the Android game must be resolved against the data
 * directory: the SceLibcBridge file calls do not honour our chdir(). */
const char *eal_io_absolute(const char *path, char *buf, size_t size) {
    if (!path || path[0] == '\0')
        return path;

    if (path[0] == '/' || strchr(path, ':') != NULL)
        return path;

    snprintf(buf, size, "%s%s", DATA_PATH, path);
    return buf;
}

// Includes the following inline utilities:
// int oflags_musl_to_newlib(int flags);
// dirent64_bionic * dirent_newlib_to_bionic(struct dirent* dirent_newlib);
// void stat_newlib_to_bionic(struct stat * src, stat64_bionic * dst);
#include "reimpl/bits/_struct_converters.c"

// The ShiVa engine looks for fonts in /system/fonts, which does not exist on
// the Vita. Redirect those paths to our own data directory.
static const char * redirect_system_path(const char * path, char * buffer,
                                         size_t buffer_size) {
    static const char prefix[] = "/system/fonts/";
    const size_t prefix_len = sizeof(prefix) - 1;

    if (strncmp(path, prefix, prefix_len) == 0) {
        snprintf(buffer, buffer_size, DATA_PATH "fonts/%s",
                 path + prefix_len);
        return buffer;
    }

    return path;
}

FILE * fopen_soloader(const char * filename, const char * mode) {
    char redirected[512];
    char absolute[512];
    filename = redirect_system_path(filename, redirected,
                                    sizeof(redirected));
    filename = eal_io_absolute(filename, absolute, sizeof(absolute));

    /* The engine writes its internal log to a relative path. Keep it in a
     * known writable location so it can always be dumped into eal.log. */
    if (filename && strstr(filename, "S3DClient.log") != NULL)
        filename = DATA_PATH "S3DClient.log";

    if (strcmp(filename, "/proc/cpuinfo") == 0) {
        return fopen_soloader("app0:/cpuinfo", mode);
    } else if (strcmp(filename, "/sys/devices/system/cpu/present") == 0) {
        return fopen_soloader("app0:/cpu_present", mode);
    } else if (strcmp(filename, "/sys/devices/system/cpu/possible") == 0) {
        return fopen_soloader("app0:/cpu_possible", mode);
    } else if (strcmp(filename, "/proc/meminfo") == 0) {
        return fopen_soloader("app0:/meminfo", mode);
    }

#ifdef USE_SCELIBC_IO
    FILE* ret = sceLibcBridge_fopen(filename, mode);
#else
    FILE* ret = fopen(filename, mode);
#endif

    static int io_log_count = 0;
    int io_errno = errno;
    int important = filename && (strstr(filename, "Saves") ||
                                 strstr(filename, "aves") ||
                                 strstr(filename, "Shaders"));
    if (important || io_log_count < 64) {
        eal_trace("io: fopen('%s','%s') -> %p errno=%d",
                  filename ? filename : "?", mode ? mode : "?",
                  (void *) ret, io_errno);
        if (!important)
            io_log_count++;
    }

    if (ret)
        l_debug("fopen(%s, %s): %p", filename, mode, ret);
    else
        l_warn("fopen(%s, %s): %p", filename, mode, ret);

    /* SceLibc's default stdio buffer is small. Ordinary read-only files get
     * a larger buffer; the game pack is served by pack_fill() instead. */
    if (ret && mode && mode[0] == 'r' && strchr(mode, '+') == NULL) {
        if (filename && strstr(filename, ".stk") != NULL) {
            if (setting_packCache)
                pack_register(ret);
        } else {
            size_t buffer_size = 16 * 1024;
#ifdef USE_SCELIBC_IO
            int vb = sceLibcBridge_setvbuf(ret, NULL, _IOFBF, buffer_size);
            (void) vb;
#else
            setvbuf(ret, NULL, _IOFBF, buffer_size);
#endif
        }
    }

    return ret;
}

int open_soloader(const char * path, int oflag, ...) {
    char redirected[512];
    char absolute[512];
    path = redirect_system_path(path, redirected, sizeof(redirected));
    path = eal_io_absolute(path, absolute, sizeof(absolute));

    if (strcmp(path, "/proc/cpuinfo") == 0) {
        return open_soloader("app0:/cpuinfo", oflag);
    } else if (strcmp(path, "/sys/devices/system/cpu/present") == 0) {
        return open_soloader("app0:/cpu_present", oflag);
    } else if (strcmp(path, "/sys/devices/system/cpu/possible") == 0) {
        return open_soloader("app0:/cpu_possible", oflag);
    } else if (strcmp(path, "/proc/meminfo") == 0) {
        return open_soloader("app0:/meminfo", oflag);
    } else if (strcmp(path, "/dev/urandom") == 0) {
        return open_soloader("app0:/urandom", oflag);
    }

    mode_t mode = 0666;
    if (((oflag & BIONIC_O_CREAT) == BIONIC_O_CREAT) ||
        ((oflag & BIONIC_O_TMPFILE) == BIONIC_O_TMPFILE)) {
        va_list args;
        va_start(args, oflag);
        mode = (mode_t)(va_arg(args, int));
        va_end(args);
    }

    oflag = oflags_bionic_to_newlib(oflag);
    int ret = open(path, oflag, mode);

    static int open_log_count = 0;
    if (open_log_count < 64) {
        eal_trace("io: open('%s',0x%x) -> %d", path ? path : "?", oflag, ret);
        open_log_count++;
    }

    if (ret >= 0)
        l_debug("open(%s, %x): %i", path, oflag, ret);
    else
        l_warn("open(%s, %x): %i", path, oflag, ret);
    return ret;
}

int fstat_soloader(int fd, stat64_bionic * buf) {
    struct stat st;
    int res = fstat(fd, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    l_debug("fstat(%i): %i", fd, res);
    return res;
}

int stat_soloader(const char * path, stat64_bionic * buf) {
    char redirected[512];
    char absolute[512];
    path = redirect_system_path(path, redirected, sizeof(redirected));
    path = eal_io_absolute(path, absolute, sizeof(absolute));

    if (strcmp(path, "/system/lib/libOpenSLES.so") == 0) {
        l_debug("stat(%s): returning 0 in case this is a check for OpenSLES support", path);
        return 0;
    }

    struct stat st;
    int res = stat(path, &st);

    if (res == 0)
        stat_newlib_to_bionic(&st, buf);

    static int stat_log_count = 0;
    if (stat_log_count < 64) {
        eal_trace("io: stat('%s') -> %d", path ? path : "?", res);
        stat_log_count++;
    }

    l_debug("stat(%s): %i", path, res);
    return res;
}

int eal_mkdir(const char *path, mode_t mode) {
    char absolute[512];
    const char *p = eal_io_absolute(path, absolute, sizeof(absolute));
    int r = mkdir(p, mode);
    eal_trace("io: mkdir('%s') -> %d", p ? p : "?", r);
    return r;
}

int eal_remove(const char *path) {
    char absolute[512];
    const char *p = eal_io_absolute(path, absolute, sizeof(absolute));
    int r = remove(p);
    eal_trace("io: remove('%s') -> %d", p ? p : "?", r);
    return r;
}

int eal_rename(const char *old_path, const char *new_path) {
    char old_abs[512];
    char new_abs[512];
    const char *o = eal_io_absolute(old_path, old_abs, sizeof(old_abs));
    const char *n = eal_io_absolute(new_path, new_abs, sizeof(new_abs));
    int r = rename(o, n);
    eal_trace("io: rename('%s','%s') -> %d", o ? o : "?", n ? n : "?", r);
    return r;
}

int fclose_soloader(FILE * f) {
    pack_remove(f);

#ifdef USE_SCELIBC_IO
    int ret = sceLibcBridge_fclose(f);
#else
    int ret = fclose(f);
#endif

    l_debug("fclose(%p): %i", f, ret);
    return ret;
}

size_t fread_soloader(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!ptr || size == 0 || nmemb == 0)
        return 0;

    pack_lock();
    pack_stream *p = pack_find(stream);
    if (p) {
        size_t want = size * nmemb;
        size_t done = 0;

        while (done < want) {
            if (p->logical < 0 || p->logical >= p->size)
                break;

            size_t cached = pack_fill(p, p->logical);
            if (cached == 0) {
                if (!pack_cache) {
                    /* Cache allocation failed: serve directly from the file. */
#ifdef USE_SCELIBC_IO
                    sceLibcBridge_fseek(stream, (long) p->logical, SEEK_SET);
                    size_t direct = sceLibcBridge_fread(
                        (uint8_t *) ptr + done, 1, want - done, stream);
#else
                    fseek(stream, (long) p->logical, SEEK_SET);
                    size_t direct = fread(
                        (uint8_t *) ptr + done, 1, want - done, stream);
#endif
                    p->logical += (int64_t) direct;
                    done += direct;
                }
                break;
            }

            int64_t rel = p->logical - pack_cache_base;
            if (rel < 0 || rel >= (int64_t) cached)
                break;

            size_t available = (size_t) ((int64_t) cached - rel);
            size_t chunk = want - done;
            if (chunk > available)
                chunk = available;

            memcpy((uint8_t *) ptr + done, pack_cache + rel, chunk);
            p->logical += (int64_t) chunk;
            done += chunk;
        }

        eal_read_bytes += done;
        pack_unlock();
        return done / size;
    }
    pack_unlock();

    uint64_t t0 = sceKernelGetProcessTimeWide();
#ifdef USE_SCELIBC_IO
    size_t ret = sceLibcBridge_fread(ptr, size, nmemb, stream);
#else
    size_t ret = fread(ptr, size, nmemb, stream);
#endif
    uint64_t t1 = sceKernelGetProcessTimeWide();

    eal_read_bytes += (uint64_t) ret * size;
    eal_read_time_us += t1 - t0;

    return ret;
}

int fseek_soloader(FILE *stream, long offset, int whence) {
    pack_lock();
    pack_stream *p = pack_find(stream);
    if (p) {
        int64_t target;

        switch (whence) {
            case SEEK_SET: target = offset; break;
            case SEEK_CUR: target = p->logical + offset; break;
            case SEEK_END: target = p->size + offset; break;
            default:
                pack_unlock();
                return -1;
        }

        if (target < 0) {
            pack_unlock();
            return -1;
        }

        p->logical = target;
        pack_unlock();
        return 0;
    }
    pack_unlock();

#ifdef USE_SCELIBC_IO
    return sceLibcBridge_fseek(stream, offset, whence);
#else
    return fseek(stream, offset, whence);
#endif
}

long ftell_soloader(FILE *stream) {
    pack_lock();
    pack_stream *p = pack_find(stream);
    if (p) {
        long pos = (long) p->logical;
        pack_unlock();
        return pos;
    }
    pack_unlock();

#ifdef USE_SCELIBC_IO
    return sceLibcBridge_ftell(stream);
#else
    return ftell(stream);
#endif
}

int fgetc_soloader(FILE *stream) {
    pack_stream *p = pack_find(stream);
    if (p) {
        unsigned char c;
        if (fread_soloader(&c, 1, 1, stream) == 1)
            return c;
        return -1;
    }

#ifdef USE_SCELIBC_IO
    return sceLibcBridge_fgetc(stream);
#else
    return fgetc(stream);
#endif
}

int ungetc_soloader(int c, FILE *stream) {
    pack_lock();
    pack_stream *p = pack_find(stream);
    if (p) {
        if (p->logical > 0) {
            p->logical--;
            pack_unlock();
            return c;
        }
        pack_unlock();
        return -1;
    }
    pack_unlock();

#ifdef USE_SCELIBC_IO
    return sceLibcBridge_ungetc(c, stream);
#else
    return ungetc(c, stream);
#endif
}

int close_soloader(int fd) {
    int ret = close(fd);
    l_debug("close(%i): %i", fd, ret);
    return ret;
}

DIR* opendir_soloader(char* _pathname) {
    char redirected[512];
    char absolute[512];
    const char * pathname = redirect_system_path(_pathname, redirected,
                                                 sizeof(redirected));
    pathname = eal_io_absolute(pathname, absolute, sizeof(absolute));

    DIR* ret = opendir(pathname);
    l_debug("opendir(\"%s\"): %p", pathname, ret);
    return ret;
}

struct dirent64_bionic * readdir_soloader(DIR * dir) {
    static struct dirent64_bionic dirent_tmp;

    struct dirent* ret = readdir(dir);
    l_debug("readdir(%p): %p", dir, ret);

    if (ret) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(ret);
        memcpy(&dirent_tmp, entry_tmp, sizeof(dirent64_bionic));
        free(entry_tmp);
        return &dirent_tmp;
    }

    return NULL;
}

int readdir_r_soloader(DIR * dirp, dirent64_bionic * entry,
                       dirent64_bionic ** result) {
    struct dirent dirent_tmp;
    struct dirent * pdirent_tmp;

    int ret = readdir_r(dirp, &dirent_tmp, &pdirent_tmp);

    if (ret == 0) {
        dirent64_bionic* entry_tmp = dirent_newlib_to_bionic(&dirent_tmp);
        memcpy(entry, entry_tmp, sizeof(dirent64_bionic));
        *result = (pdirent_tmp != NULL) ? entry : NULL;
        free(entry_tmp);
    }

    l_debug("readdir_r(%p, %p, %p): %i", dirp, entry, result, ret);
    return ret;
}

int closedir_soloader(DIR * dir) {
    int ret = closedir(dir);
    l_debug("closedir(%p): %i", dir, ret);
    return ret;
}

int fcntl_soloader(int fd, int cmd, ...) {
    l_warn("fcntl(%i, %i, ...): not implemented", fd, cmd);
    return 0;
}

int ioctl_soloader(int fd, int request, ...) {
    l_warn("ioctl(%i, %i, ...): not implemented", fd, request);
    return 0;
}

int fsync_soloader(int fd) {
    int ret = fsync(fd);
    l_debug("fsync(%i): %i", fd, ret);
    return ret;
}
