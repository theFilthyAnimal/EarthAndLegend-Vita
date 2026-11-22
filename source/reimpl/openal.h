/*
 * Earth And Legend (PS Vita) - minimal OpenAL implementation.
 */

#ifndef EAL_OPENAL_H
#define EAL_OPENAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Performs a clean shutdown of the audio output (optional). */
void eal_openal_shutdown(void);

/* Returns 1 once the audio output port and mixer thread are running. */
int eal_openal_active(void);

/* AL entry points used by the engine (resolved through the import table). */
void eal_alGenBuffers(int32_t n, uint32_t *names);
void eal_alGenSources(int32_t n, uint32_t *names);
void eal_alDeleteBuffers(int32_t n, const uint32_t *names);
void eal_alDeleteSources(int32_t n, const uint32_t *names);
int32_t eal_alGetError(void);
uint8_t eal_alIsBuffer(uint32_t name);
uint8_t eal_alIsExtensionPresent(const char *name);
const char *eal_alGetString(int32_t param);
void eal_alGetBufferi(uint32_t name, int32_t param, int32_t *value);
void eal_alGetSourcei(uint32_t name, int32_t param, int32_t *value);
void *eal_alGetProcAddress(const char *name);
void eal_alBufferData(uint32_t name, int32_t format, const void *data,
                      int32_t size, int32_t freq);
void eal_alSourcePlay(uint32_t name);
void eal_alSourcePause(uint32_t name);
void eal_alSourceStop(uint32_t name);
void eal_alSourceRewind(uint32_t name);
void eal_alSourceQueueBuffers(uint32_t name, int32_t n,
                              const uint32_t *names);
void eal_alSourceUnqueueBuffers(uint32_t name, int32_t n, uint32_t *names);
void eal_alSourcei(uint32_t name, int32_t param, int32_t value);
void eal_alSourcef(uint32_t name, int32_t param, float value);
void eal_alSource3f(uint32_t name, int32_t param, float v1, float v2,
                    float v3);
void eal_alSource3i(uint32_t name, int32_t param, int32_t v1, int32_t v2,
                    int32_t v3);
void eal_alSourcefv(uint32_t name, int32_t param, const float *values);
void eal_alListenerf(int32_t param, float value);
void eal_alListener3f(int32_t param, float v1, float v2, float v3);
void eal_alListenerfv(int32_t param, const float *values);

/* ALC entry points used by the engine. */
void *eal_alcOpenDevice(const char *name);
uint8_t eal_alcCloseDevice(void *device);
void *eal_alcCreateContext(void *device, const int32_t *attrlist);
uint8_t eal_alcMakeContextCurrent(void *context);
void *eal_alcGetCurrentContext(void);
void *eal_alcGetContextsDevice(void *context);
void eal_alcDestroyContext(void *context);
int32_t eal_alcGetError(void *device);
uint8_t eal_alcIsExtensionPresent(void *device, const char *name);
const char *eal_alcGetString(void *device, int32_t param);
void eal_alcGetIntegerv(void *device, int32_t param, int32_t size,
                        int32_t *values);
void eal_alcProcessContext(void *context);
void eal_alcSuspendContext(void *context);
void *eal_alcCaptureOpenDevice(const char *name, uint32_t frequency,
                               int32_t format, int32_t size);
uint8_t eal_alcCaptureCloseDevice(void *device);
void eal_alcCaptureStart(void *device);
void eal_alcCaptureStop(void *device);
void eal_alcCaptureSamples(void *device, void *buffer, int32_t samples);

#ifdef __cplusplus
}
#endif

#endif // EAL_OPENAL_H
