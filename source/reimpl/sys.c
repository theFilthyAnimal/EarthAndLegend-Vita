/*
 * Copyright (C) 2021      Andy Nguyen
 * Copyright (C) 2022      Rinnegatamante
 * Copyright (C) 2022-2023 Volodymyr Atamanenko
 *
 * This software may be modified and distributed under the terms
 * of the MIT license. See the LICENSE file for details.
 */

#include "reimpl/sys.h"

#include <sys/errno.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <string.h>
#include <stdint.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/rtc.h>
#include <stdlib.h>

#include "utils/utils.h"
#include "utils/logger.h"
#include "utils/settings.h"
#include "utils/trace.h"

#define BIONIC_CLOCK_REALTIME           0
#define BIONIC_CLOCK_MONOTONIC          1
#define BIONIC_CLOCK_PROCESS_CPUTIME_ID 2
#define BIONIC_CLOCK_THREAD_CPUTIME_ID  3
#define BIONIC_CLOCK_MONOTONIC_RAW      4
#define BIONIC_CLOCK_REALTIME_COARSE    5
#define BIONIC_CLOCK_MONOTONIC_COARSE   6
#define BIONIC_CLOCK_BOOTTIME           7
#define BIONIC_CLOCK_REALTIME_ALARM     8
#define BIONIC_CLOCK_BOOTTIME_ALARM     9
#define BIONIC_CLOCK_SGI_CYCLE         10
#define BIONIC_CLOCK_TAI               11

// 1969 years in microseconds, used to adjust SCE tick to UNIX timestamp
#define __epoch 62135587294000000

int clock_gettime_soloader(clockid_t clock_id, struct timespec * tp) {
    switch (clock_id) {
        case BIONIC_CLOCK_MONOTONIC:
        case BIONIC_CLOCK_MONOTONIC_RAW:
        case BIONIC_CLOCK_MONOTONIC_COARSE:
        case BIONIC_CLOCK_BOOTTIME:
        case BIONIC_CLOCK_BOOTTIME_ALARM:
        case BIONIC_CLOCK_SGI_CYCLE:
        case BIONIC_CLOCK_PROCESS_CPUTIME_ID:
        case BIONIC_CLOCK_THREAD_CPUTIME_ID: {
            uint64_t proctime = sceKernelGetProcessTimeWide();

            tp->tv_sec = (proctime / 1000000);
            tp->tv_nsec = ((proctime - (tp->tv_sec * 1000000)) * 1000);
            break;
        }
        case BIONIC_CLOCK_REALTIME:
        case BIONIC_CLOCK_REALTIME_COARSE:
        case BIONIC_CLOCK_REALTIME_ALARM:
        case BIONIC_CLOCK_TAI: {
            SceRtcTick tick;
            sceRtcGetCurrentTick(&tick);
            tick.tick -= __epoch;

            tp->tv_sec = (tick.tick / 1000000);
            tp->tv_nsec = ((tick.tick - (tp->tv_sec * 1000000)) * 1000);
            break;
        }
        default:
            l_error("clock_gettime / unexpected clock id %i", clock_id);
    }

    return 0;
}

int clock_getres_soloader(clockid_t clock_id, struct timespec * res) {
    res->tv_sec = 0;
    res->tv_nsec = 1000;
    return 0;
}

clock_t clock_soloader(void) {
    return sceKernelGetProcessTimeLow();
}

int sigaction(int signum, const struct sigaction * act, struct sigaction * oldact) {
    l_warn("sigaction(%i, ...): not implemented", signum);
    return 0;
}

int __system_property_get_soloader(const char *name, char *value) {
    l_warn("__system_property_get(%s, %p): not implemented", name, value);
    strncpy(value, "psvita", 7);
    return 7;
}

void assert2(const char* f, int l, const char* func, const char* msg) {
    l_fatal("[%s:%i][%s] Assertion failed: %s", f, l, func, msg);
}

void syscall(int c) {
    l_warn("syscall(%i): not implemented", c);
}

void __stack_chk_fail_soloader() {
    l_fatal("Stack collapsed at address %p", __builtin_return_address(0));
}

void abort_soloader() {
    l_fatal("Abort called from address %p", __builtin_return_address(0));
    abort();
}

void exit_soloader(int status) {
    l_fatal("Exit(%i) called from %p", status, __builtin_return_address(0));
    exit(status);
}

int __atomic_dec(volatile int *ptr) {
    return __sync_fetch_and_sub(ptr, 1);
}

int __atomic_inc(volatile int *ptr) {
    return __sync_fetch_and_add(ptr, 1);
}

int __atomic_swap(int new_value, volatile int *ptr) {
    int old_value;
    do {
        old_value = *ptr;
    } while (__sync_val_compare_and_swap(ptr, old_value, new_value) != old_value);
    return old_value;
}

int __atomic_cmpxchg(int old_value, int new_value, volatile int* ptr) {
    /* We must return 0 on success */
    return __sync_val_compare_and_swap(ptr, old_value, new_value) != old_value;
}

char * getenv_soloader(const char * var) {
    l_warn("getenv(\"%s\"): not implemented.", var);
    return NULL;
}

int setenv_soloader(const char * name, const char * value, int overwrite) {
    l_warn("setenv(\"%s\", \"%s\"): not implemented.", name, value);
    return 0;
}

int getpagesize(void) {
    return PAGE_SIZE;
}

/* Bionic's <bits/sysconf.h> numbers. The game's libc++_shared.so was built
 * against bionic, so the integer passed here is a bionic constant even
 * though we are linked with newlib. */
#define BIONIC_SC_CLK_TCK            0x0006
#define BIONIC_SC_OPEN_MAX           0x000b
#define BIONIC_SC_PAGESIZE           0x0027
#define BIONIC_SC_PAGE_SIZE          0x0028
#define BIONIC_SC_THREAD_STACK_MIN   0x004c
#define BIONIC_SC_THREADS            0x004f
#define BIONIC_SC_NPROCESSORS_CONF   0x0060
#define BIONIC_SC_NPROCESSORS_ONLN   0x0061
#define BIONIC_SC_PHYS_PAGES         0x0062
#define BIONIC_SC_AVPHYS_PAGES       0x0063

static int eal_cpu_count(void) {
    if (setting_cpuCount > 0)
        return setting_cpuCount;

    /* Vita exposes three cores to applications; core 3 is reserved for the
     * system. The engine uses this value to size its worker pool. */
    return 3;
}

long eal_sysconf(int name) {
    long result;

    switch (name) {
        case BIONIC_SC_CLK_TCK:
            /* clock() in this port is in microseconds. */
            result = 1000000;
            break;
        case BIONIC_SC_OPEN_MAX:
            result = 256;
            break;
        case BIONIC_SC_PAGESIZE:
        case BIONIC_SC_PAGE_SIZE:
            result = PAGE_SIZE;
            break;
        case BIONIC_SC_THREAD_STACK_MIN:
            result = 16384;
            break;
        case BIONIC_SC_THREADS:
            result = 200809L;
            break;
        case BIONIC_SC_NPROCESSORS_CONF:
            result = 4;
            break;
        case BIONIC_SC_NPROCESSORS_ONLN:
            result = eal_cpu_count();
            break;
        case BIONIC_SC_PHYS_PAGES:
            result = (350868 * 1024) / PAGE_SIZE;
            break;
        case BIONIC_SC_AVPHYS_PAGES: {
            SceKernelFreeMemorySizeInfo info;
            info.size = sizeof(info);
            if (sceKernelGetFreeMemorySize(&info) == 0)
                result = (long) (info.size_user / PAGE_SIZE);
            else
                result = (300080 * 1024) / PAGE_SIZE;
            break;
        }
        default:
            result = -1;
            break;
    }

    /* Log every distinct query once: useful for discovering what the engine
     * (and libc++) expect from this platform. */
    static int seen[24];
    static int seen_count = 0;
    int known = 0;
    for (int i = 0; i < seen_count; i++) {
        if (seen[i] == name) {
            known = 1;
            break;
        }
    }
    if (!known && seen_count < 24) {
        seen[seen_count++] = name;
        eal_trace("sysconf: name=0x%X -> %ld", (unsigned) name, result);
    }

    return result;
}
