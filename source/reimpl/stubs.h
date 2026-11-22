/*
 * Earth And Legend (PS Vita) - missing libc / OpenAL / OpenSSL stubs.
 */

#ifndef EAL_STUBS_H
#define EAL_STUBS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Return the function pointer that should be used for an unresolved import.
 * Known symbols get a dedicated (safe) implementation, everything else gets
 * a generic "return 0" stub.
 */
uintptr_t eal_stub_for_symbol(const char * name);

/** @return 1 if the symbol has a dedicated stub, 0 otherwise. */
int eal_has_stub(const char * name);

#ifdef __cplusplus
}
#endif

#endif // EAL_STUBS_H
