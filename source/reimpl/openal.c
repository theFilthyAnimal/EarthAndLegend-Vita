/*
 * Earth And Legend (PS Vita) - minimal OpenAL implementation.
 *
 * The game engine ships its own OpenAL-based audio backend and calls the
 * standard AL/ALC entry points directly. This file provides the subset it
 * uses, mixing into a stereo 44.1 kHz stream played through sceAudioOut.
 *
 * Audio is optional: when setting_openal is 0 every entry point behaves like
 * the old stubs, so the engine disables its audio backend and the rest of the
 * port is unaffected.
 */

#include "reimpl/openal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>

#include "utils/settings.h"
#include "utils/trace.h"

/* ------------------------------------------------------------------ */
/* OpenAL constants                                                     */
/* ------------------------------------------------------------------ */

#define AL_FALSE 0
#define AL_TRUE  1

#define AL_SOURCE_RELATIVE     0x0202
#define AL_CONE_INNER_ANGLE    0x1001
#define AL_CONE_OUTER_ANGLE    0x1002
#define AL_PITCH               0x1003
#define AL_POSITION            0x1004
#define AL_DIRECTION           0x1005
#define AL_VELOCITY            0x1006
#define AL_LOOPING             0x1007
#define AL_BUFFER              0x1009
#define AL_GAIN                0x100A
#define AL_MIN_GAIN            0x100D
#define AL_MAX_GAIN            0x100E
#define AL_ORIENTATION         0x100F
#define AL_SOURCE_STATE        0x1010
#define AL_INITIAL             0x1011
#define AL_PLAYING             0x1012
#define AL_PAUSED              0x1013
#define AL_STOPPED             0x1014
#define AL_BUFFERS_QUEUED      0x1015
#define AL_BUFFERS_PROCESSED   0x1016
#define AL_REFERENCE_DISTANCE  0x1020
#define AL_ROLLOFF_FACTOR      0x1021
#define AL_MAX_DISTANCE        0x1023
#define AL_FREQUENCY           0x2001
#define AL_BITS                0x2002
#define AL_CHANNELS            0x2003
#define AL_SIZE                0x2004

#define AL_FORMAT_MONO8        0x1100
#define AL_FORMAT_MONO16       0x1101
#define AL_FORMAT_STEREO8      0x1102
#define AL_FORMAT_STEREO16     0x1103

#define AL_NO_ERROR            0x0000
#define AL_INVALID_NAME        0xA001
#define AL_INVALID_ENUM        0xA002
#define AL_INVALID_VALUE       0xA003
#define AL_INVALID_OPERATION   0xA004
#define AL_OUT_OF_MEMORY       0xA005

#define AL_VENDOR              0x0B00
#define AL_VERSION             0x0B01
#define AL_RENDERER            0x0B02
#define AL_EXTENSIONS          0x0B03

#define ALC_MAJOR_VERSION             0x1000
#define ALC_MINOR_VERSION             0x1001
#define ALC_ATTRIBUTES_SIZE           0x1002
#define ALC_ALL_ATTRIBUTES            0x1003
#define ALC_DEFAULT_DEVICE_SPECIFIER  0x1004
#define ALC_DEVICE_SPECIFIER          0x1005
#define ALC_EXTENSIONS                0x1006
#define ALC_FREQUENCY                 0x1007

#define ALC_NO_ERROR        0x0000
#define ALC_INVALID_DEVICE  0xA001
#define ALC_INVALID_CONTEXT 0xA002

/* ------------------------------------------------------------------ */
/* Configuration                                                        */
/* ------------------------------------------------------------------ */

#define AL_MAX_BUFFERS 512
#define AL_MAX_SOURCES 96
#define AL_MAX_QUEUE   32

/* The Vita MAIN output port only accepts 48000 Hz. Source buffers keep their
 * own sample rate and are resampled by the mixer. A 1024-frame grain gives
 * the mixer ~21 ms of slack per call, which prevents underruns when the game
 * thread is busy. */
#define MIX_RATE   48000
#define MIX_FRAMES 1024

/* Overall headroom before the final clip. The engine sums music, ambience
 * and effects, all with gains up to 1.0; without headroom the mix clips
 * constantly and sounds like crackling. */
#define MIX_HEADROOM 0.75f

typedef struct {
    int in_use;
    int format;
    int freq;
    int channels;
    int bps;
    int frames;
    int32_t size;
    uint8_t *data;
} al_buffer_t;

typedef struct {
    int in_use;
    int state;

    float gain;
    float pitch;
    int looping;
    int relative;

    float pos[3];
    float vel[3];
    float dir[3];

    float ref_dist;
    float max_dist;
    float rolloff;
    float inner_angle;
    float outer_angle;
    float outer_gain;

    int pending[AL_MAX_QUEUE];
    int pending_count;
    int done[AL_MAX_QUEUE];
    int done_count;

    double cursor;

    /* Smoothed stereo gains, ramped over one mixer chunk to avoid zipper
     * noise when the engine moves or fades a source. */
    float last_gl;
    float last_gr;
} al_source_t;

static al_buffer_t buffers[AL_MAX_BUFFERS];
static al_source_t sources[AL_MAX_SOURCES];

static float listener_gain = 1.0f;
static float listener_pos[3] = { 0.0f, 0.0f, 0.0f };
static float listener_at[3]  = { 0.0f, 0.0f, -1.0f };
static float listener_up[3]  = { 0.0f, 1.0f, 0.0f };

static int32_t al_error = AL_NO_ERROR;

static int buffer_log_count = 0;
static int play_log_count = 0;
static int queue_log_count = 0;

static SceUID al_mutex = -1;
static int al_mutex_ready = 0;

static SceUID audio_port = -1;
static SceUID audio_thread = -1;
static volatile int audio_running = 0;
static int audio_started = 0;

/* Output buffers. The audio hardware reads them by DMA, so they live in an
 * uncached block and are cycled (never rewritten while still being played). */
#define AUDIO_OUT_BUFFERS 4

static SceUID audio_mem_uid = -1;
static int16_t *audio_out = NULL;
static int audio_out_index = 0;
static int16_t audio_fallback[AUDIO_OUT_BUFFERS][MIX_FRAMES * 2]
    __attribute__((aligned(64)));

static float mix_l[MIX_FRAMES];
static float mix_r[MIX_FRAMES];

/* Mixer diagnostics: logged once every few seconds. */
static unsigned mix_chunks = 0;
static unsigned mix_underruns = 0;
static unsigned mix_clicks = 0;
static float mix_peak = 0.0f;
static float last_out_l = 0.0f;
static float last_out_r = 0.0f;
static int gain_log_count = 0;
static int pitch_log_count = 0;
static int pos_log_count = 0;

/* Minimal device/context objects: the engine only passes pointers around. */
typedef struct { int dummy; } al_device_t;
typedef struct { int dummy; } al_context_t;

static al_device_t the_device;
static al_context_t the_context;
static al_context_t *current_context = NULL;

static int openal_available(void) {
    return setting_openal != 0;
}

int eal_openal_active(void) {
    return audio_started;
}

static void al_lock(void) {
    if (al_mutex_ready)
        sceKernelLockMutex(al_mutex, 1, NULL);
}

static void al_unlock(void) {
    if (al_mutex_ready)
        sceKernelUnlockMutex(al_mutex, 1);
}

static void al_set_error(int32_t error) {
    al_error = error;
}

/* ------------------------------------------------------------------ */
/* Mixer                                                                */
/* ------------------------------------------------------------------ */

static int source_next_sample(al_source_t *s, float *out_l, float *out_r) {
    for (;;) {
        if (s->pending_count <= 0)
            return 0;

        int bi = s->pending[0];
        if (bi <= 0 || bi > AL_MAX_BUFFERS)
            return 0;

        al_buffer_t *b = &buffers[bi - 1];
        if (!b->in_use || b->frames <= 0 || !b->data) {
            /* Drop the bad buffer and continue with the next one. */
            memmove(&s->pending[0], &s->pending[1],
                    (size_t) (s->pending_count - 1) * sizeof(int));
            s->pending_count--;
            s->cursor = 0.0;
            continue;
        }

        if (s->cursor >= (double) b->frames) {
            if (s->looping) {
                if (s->pending_count > 1) {
                    int first = s->pending[0];
                    memmove(&s->pending[0], &s->pending[1],
                            (size_t) (s->pending_count - 1) * sizeof(int));
                    s->pending[s->pending_count - 1] = first;
                }
            } else {
                if (s->done_count < AL_MAX_QUEUE)
                    s->done[s->done_count++] = s->pending[0];
                memmove(&s->pending[0], &s->pending[1],
                        (size_t) (s->pending_count - 1) * sizeof(int));
                s->pending_count--;
            }
            s->cursor = 0.0;

            if (s->pending_count <= 0) {
                s->state = AL_STOPPED;
                return 0;
            }
            continue;
        }

        uint32_t i0 = (uint32_t) s->cursor;
        uint32_t i1 = i0 + 1;
        if (i1 >= (uint32_t) b->frames)
            i1 = i0;

        float t = (float) (s->cursor - (double) i0);
        float s0l, s0r, s1l, s1r;

        if (b->channels == 2) {
            if (b->bps == 2) {
                const int16_t *p = (const int16_t *) b->data;
                s0l = p[i0 * 2] / 32768.0f;
                s0r = p[i0 * 2 + 1] / 32768.0f;
                s1l = p[i1 * 2] / 32768.0f;
                s1r = p[i1 * 2 + 1] / 32768.0f;
            } else {
                const uint8_t *p = b->data;
                s0l = (p[i0 * 2] - 128) / 128.0f;
                s0r = (p[i0 * 2 + 1] - 128) / 128.0f;
                s1l = (p[i1 * 2] - 128) / 128.0f;
                s1r = (p[i1 * 2 + 1] - 128) / 128.0f;
            }
        } else {
            if (b->bps == 2) {
                const int16_t *p = (const int16_t *) b->data;
                s0l = s0r = p[i0] / 32768.0f;
                s1l = s1r = p[i1] / 32768.0f;
            } else {
                const uint8_t *p = b->data;
                s0l = s0r = (p[i0] - 128) / 128.0f;
                s1l = s1r = (p[i1] - 128) / 128.0f;
            }
        }

        *out_l = s0l + (s1l - s0l) * t;
        *out_r = s0r + (s1r - s0r) * t;

        double step = (double) b->freq * (double) s->pitch / (double) MIX_RATE;
        if (step < 0.0001)
            step = 0.0001;
        s->cursor += step;

        return 1;
    }
}

static void source_gains(const al_source_t *s, float *gl, float *gr) {
    float gain = s->gain * listener_gain;
    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 4.0f)
        gain = 4.0f;

    float att = 1.0f;
    float pan = 0.0f;

    if (!s->relative) {
        float dx = s->pos[0] - listener_pos[0];
        float dy = s->pos[1] - listener_pos[1];
        float dz = s->pos[2] - listener_pos[2];
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);

        float ref = s->ref_dist > 0.0f ? s->ref_dist : 1.0f;
        float maxd = s->max_dist > 0.0f ? s->max_dist : 1.0e9f;
        float d = dist;
        if (d < ref) d = ref;
        if (d > maxd) d = maxd;

        att = ref / (ref + s->rolloff * (d - ref));
        if (att < 0.0f) att = 0.0f;
        if (att > 1.0f) att = 1.0f;

        if (dist > 0.0001f) {
            float rx = listener_at[1] * listener_up[2] -
                       listener_at[2] * listener_up[1];
            float ry = listener_at[2] * listener_up[0] -
                       listener_at[0] * listener_up[2];
            float rz = listener_at[0] * listener_up[1] -
                       listener_at[1] * listener_up[0];
            pan = (dx * rx + dy * ry + dz * rz) / dist;
        }
    }

    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    float l = sqrtf(0.5f * (1.0f - pan));
    float r = sqrtf(0.5f * (1.0f + pan));

    int bi = s->pending_count > 0 ? s->pending[0] : 0;
    int stereo = 0;
    if (bi > 0 && bi <= AL_MAX_BUFFERS && buffers[bi - 1].in_use)
        stereo = buffers[bi - 1].channels == 2;

    if (stereo) {
        *gl = gain * att;
        *gr = gain * att;
    } else {
        *gl = gain * att * l;
        *gr = gain * att * r;
    }
}

static void mix_sources(int frames) {
    memset(mix_l, 0, (size_t) frames * sizeof(float));
    memset(mix_r, 0, (size_t) frames * sizeof(float));

    al_lock();

    for (int i = 0; i < AL_MAX_SOURCES; i++) {
        al_source_t *s = &sources[i];
        if (!s->in_use || s->state != AL_PLAYING)
            continue;

        if (s->pending_count <= 0) {
            /* Playing source with no data: the engine has not refilled the
             * stream in time. Counted for the diagnostics line. */
            mix_underruns++;
            continue;
        }

        float gl1, gr1;
        source_gains(s, &gl1, &gr1);

        float gl0 = s->last_gl;
        float gr0 = s->last_gr;
        float inv = 1.0f / (float) frames;

        for (int f = 0; f < frames; f++) {
            float sl, sr;
            if (!source_next_sample(s, &sl, &sr))
                break;
            float t = (float) (f + 1) * inv;
            float gl = gl0 + (gl1 - gl0) * t;
            float gr = gr0 + (gr1 - gr0) * t;
            mix_l[f] += sl * gl;
            mix_r[f] += sr * gr;
        }

        s->last_gl = gl1;
        s->last_gr = gr1;
    }

    al_unlock();

    /* Track the peak for the diagnostics report. */
    for (int f = 0; f < frames; f++) {
        float al = mix_l[f] < 0.0f ? -mix_l[f] : mix_l[f];
        float ar = mix_r[f] < 0.0f ? -mix_r[f] : mix_r[f];
        if (al > mix_peak) mix_peak = al;
        if (ar > mix_peak) mix_peak = ar;
    }
}

static int audio_thread_main(SceSize args, void *argp) {
    (void) args;
    (void) argp;

    while (audio_running) {
        mix_sources(MIX_FRAMES);

        int16_t *out = audio_out + (audio_out_index * MIX_FRAMES * 2);
        audio_out_index = (audio_out_index + 1) % AUDIO_OUT_BUFFERS;

        float prev_l = last_out_l;
        float prev_r = last_out_r;

        for (int i = 0; i < MIX_FRAMES; i++) {
            float l = mix_l[i] * MIX_HEADROOM;
            float r = mix_r[i] * MIX_HEADROOM;
            if (l > 1.0f) l = 1.0f;
            if (l < -1.0f) l = -1.0f;
            if (r > 1.0f) r = 1.0f;
            if (r < -1.0f) r = -1.0f;

            float dl = l - prev_l;
            float dr = r - prev_r;
            if (dl < 0.0f) dl = -dl;
            if (dr < 0.0f) dr = -dr;
            if (dl > 0.5f || dr > 0.5f)
                mix_clicks++;

            prev_l = l;
            prev_r = r;

            out[i * 2] = (int16_t) (l * 32767.0f);
            out[i * 2 + 1] = (int16_t) (r * 32767.0f);
        }

        last_out_l = prev_l;
        last_out_r = prev_r;

        if (audio_port >= 0) {
            if (sceAudioOutOutput(audio_port, out) < 0)
                sceKernelDelayThread(4000);
        }

        if (++mix_chunks >= 400) {
            eal_trace("openal: mix peak=%.2f underruns=%u clicks=%u",
                      mix_peak, mix_underruns, mix_clicks);
            mix_chunks = 0;
            mix_underruns = 0;
            mix_clicks = 0;
            mix_peak = 0.0f;
        }
    }

    return 0;
}

static int audio_start(void) {
    if (audio_started)
        return 1;

    if (!audio_out) {
        int bytes = AUDIO_OUT_BUFFERS * MIX_FRAMES * 2 *
                    (int) sizeof(int16_t);

        audio_mem_uid = sceKernelAllocMemBlock("eal_audio_out",
                                               SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                               bytes, NULL);
        if (audio_mem_uid >= 0) {
            void *base = NULL;
            if (sceKernelGetMemBlockBase(audio_mem_uid, &base) == 0 && base)
                audio_out = (int16_t *) base;
        }

        if (!audio_out) {
            audio_out = &audio_fallback[0][0];
            eal_trace("openal: using cached fallback output buffer");
        } else {
            eal_trace("openal: uncached output buffer allocated");
        }
    }

    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,
                                     MIX_FRAMES, MIX_RATE,
                                     SCE_AUDIO_OUT_MODE_STEREO);
    if (audio_port < 0) {
        eal_trace("openal: sceAudioOutOpenPort failed (0x%08X)",
                  (unsigned) audio_port);
        audio_port = -1;
        return 0;
    }

    int volumes[2] = { 32768, 32768 };
    sceAudioOutSetVolume(audio_port,
                         SCE_AUDIO_VOLUME_FLAG_L_CH |
                         SCE_AUDIO_VOLUME_FLAG_R_CH,
                         volumes);

    audio_running = 1;
    audio_thread = sceKernelCreateThread("eal_audio", audio_thread_main,
                                         0x10000100, 0x8000, 0,
                                         SCE_KERNEL_CPU_MASK_USER_ALL, NULL);
    if (audio_thread < 0) {
        eal_trace("openal: could not create audio thread (0x%08X)",
                  (unsigned) audio_thread);
        audio_running = 0;
        sceAudioOutReleasePort(audio_port);
        audio_port = -1;
        return 0;
    }

    sceKernelStartThread(audio_thread, 0, NULL);
    audio_started = 1;
    eal_trace("openal: output started (%d Hz, %d frames)",
              MIX_RATE, MIX_FRAMES);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Buffer management                                                    */
/* ------------------------------------------------------------------ */

static al_buffer_t *buffer_get(uint32_t name) {
    if (name == 0 || name > AL_MAX_BUFFERS)
        return NULL;
    al_buffer_t *b = &buffers[name - 1];
    return b->in_use ? b : NULL;
}

static al_source_t *source_get(uint32_t name) {
    if (name == 0 || name > AL_MAX_SOURCES)
        return NULL;
    al_source_t *s = &sources[name - 1];
    return s->in_use ? s : NULL;
}

static int openal_ready(void) {
    if (!openal_available())
        return 0;

    if (!al_mutex_ready) {
        al_mutex = sceKernelCreateMutex("eal_al_mutex", 0, 0, NULL);
        al_mutex_ready = 1;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* AL implementation                                                    */
/* ------------------------------------------------------------------ */

void eal_alGenBuffers(int32_t n, uint32_t *names) {
    if (!names || n < 0)
        return;

    if (!openal_ready()) {
        for (int32_t i = 0; i < n; i++)
            names[i] = 0;
        return;
    }

    al_lock();
    int32_t made = 0;
    for (int i = 0; i < AL_MAX_BUFFERS && made < n; i++) {
        if (!buffers[i].in_use) {
            memset(&buffers[i], 0, sizeof(buffers[i]));
            buffers[i].in_use = 1;
            names[made++] = (uint32_t) (i + 1);
        }
    }
    al_unlock();

    if (made < n)
        al_set_error(AL_OUT_OF_MEMORY);
}

void eal_alDeleteBuffers(int32_t n, const uint32_t *names) {
    if (!names || n <= 0 || !al_mutex_ready)
        return;

    al_lock();
    for (int32_t i = 0; i < n; i++) {
        al_buffer_t *b = buffer_get(names[i]);
        if (!b)
            continue;
        free(b->data);
        memset(b, 0, sizeof(*b));
    }
    al_unlock();
}

void eal_alGenSources(int32_t n, uint32_t *names) {
    if (!names || n < 0)
        return;

    if (!openal_ready()) {
        for (int32_t i = 0; i < n; i++)
            names[i] = 0;
        return;
    }

    al_lock();
    int32_t made = 0;
    for (int i = 0; i < AL_MAX_SOURCES && made < n; i++) {
        if (!sources[i].in_use) {
            memset(&sources[i], 0, sizeof(sources[i]));
            sources[i].in_use = 1;
            sources[i].state = AL_INITIAL;
            sources[i].gain = 1.0f;
            sources[i].pitch = 1.0f;
            sources[i].ref_dist = 1.0f;
            sources[i].max_dist = 1.0e9f;
            sources[i].rolloff = 1.0f;
            sources[i].inner_angle = 360.0f;
            sources[i].outer_angle = 360.0f;
            sources[i].outer_gain = 1.0f;
            names[made++] = (uint32_t) (i + 1);
        }
    }
    al_unlock();

    if (made < n)
        al_set_error(AL_OUT_OF_MEMORY);
}

void eal_alDeleteSources(int32_t n, const uint32_t *names) {
    if (!names || n <= 0 || !al_mutex_ready)
        return;

    al_lock();
    for (int32_t i = 0; i < n; i++) {
        al_source_t *s = source_get(names[i]);
        if (s)
            memset(s, 0, sizeof(*s));
    }
    al_unlock();
}

int32_t eal_alGetError(void) {
    int32_t value = al_error;
    al_error = AL_NO_ERROR;
    return value;
}

uint8_t eal_alIsBuffer(uint32_t name) {
    if (!al_mutex_ready)
        return 0;
    al_lock();
    int ok = buffer_get(name) != NULL;
    al_unlock();
    return ok ? AL_TRUE : AL_FALSE;
}

uint8_t eal_alIsExtensionPresent(const char *name) {
    (void) name;
    return AL_FALSE;
}

const char *eal_alGetString(int32_t param) {
    switch (param) {
        case AL_VENDOR:     return "Earth And Legend Vita";
        case AL_VERSION:    return "1.1";
        case AL_RENDERER:   return "sceAudioOut";
        case AL_EXTENSIONS: return "";
        default:            return "";
    }
}

void eal_alGetBufferi(uint32_t name, int32_t param, int32_t *value) {
    if (!value)
        return;

    *value = 0;
    if (!al_mutex_ready)
        return;

    al_lock();
    al_buffer_t *b = buffer_get(name);
    if (b) {
        switch (param) {
            case AL_FREQUENCY: *value = b->freq; break;
            case AL_BITS:      *value = b->bps * 8; break;
            case AL_CHANNELS:  *value = b->channels; break;
            case AL_SIZE:      *value = b->size; break;
            default: break;
        }
    }
    al_unlock();
}

void eal_alGetSourcei(uint32_t name, int32_t param, int32_t *value) {
    if (!value)
        return;

    *value = 0;
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s) {
        switch (param) {
            case AL_SOURCE_STATE:     *value = s->state; break;
            case AL_BUFFERS_QUEUED:   *value = s->pending_count + s->done_count; break;
            case AL_BUFFERS_PROCESSED:*value = s->done_count; break;
            case AL_LOOPING:          *value = s->looping; break;
            case AL_SOURCE_RELATIVE:  *value = s->relative; break;
            case AL_BUFFER:
                *value = s->pending_count > 0 ? s->pending[0] : 0;
                break;
            default: break;
        }
    }
    al_unlock();
}

void *eal_alGetProcAddress(const char *name) {
    (void) name;
    return NULL;
}

void eal_alBufferData(uint32_t name, int32_t format, const void *data,
                      int32_t size, int32_t freq) {
    if (!openal_ready())
        return;

    if (freq <= 0)
        freq = 44100;

    int channels;
    int bps;

    switch (format) {
        case AL_FORMAT_MONO8:    channels = 1; bps = 1; break;
        case AL_FORMAT_MONO16:   channels = 1; bps = 2; break;
        case AL_FORMAT_STEREO8:  channels = 2; bps = 1; break;
        case AL_FORMAT_STEREO16: channels = 2; bps = 2; break;
        default:
            al_set_error(AL_INVALID_ENUM);
            return;
    }

    if (!data || size <= 0) {
        al_lock();
        al_buffer_t *b = buffer_get(name);
        if (b) {
            free(b->data);
            b->data = NULL;
            b->size = 0;
            b->frames = 0;
        }
        al_unlock();
        return;
    }

    uint8_t *copy = malloc((size_t) size);
    if (!copy) {
        al_set_error(AL_OUT_OF_MEMORY);
        return;
    }
    memcpy(copy, data, (size_t) size);

    al_lock();
    al_buffer_t *b = buffer_get(name);
    if (!b) {
        al_unlock();
        free(copy);
        al_set_error(AL_INVALID_NAME);
        return;
    }

    free(b->data);
    b->data = copy;
    b->format = format;
    b->freq = freq;
    b->channels = channels;
    b->bps = bps;
    b->size = size;
    b->frames = size / (channels * bps);
    al_unlock();

    if (buffer_log_count < 8) {
        eal_trace("openal: buffer %u data fmt=%d %d Hz %d bytes",
                  (unsigned) name, format, freq, size);
        buffer_log_count++;
    }
}

void eal_alSourcePlay(uint32_t name) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s) {
        if (s->pending_count > 0) {
            if (s->state != AL_PLAYING) {
                if (s->state == AL_STOPPED || s->state == AL_INITIAL) {
                    s->cursor = 0.0;
                }
                s->state = AL_PLAYING;
            }
        } else {
            s->state = AL_STOPPED;
        }
    }
    al_unlock();

    if (play_log_count < 4) {
        eal_trace("openal: play source %u", (unsigned) name);
        play_log_count++;
    }
}

void eal_alSourcePause(uint32_t name) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s && s->state == AL_PLAYING)
        s->state = AL_PAUSED;
    al_unlock();
}

void eal_alSourceStop(uint32_t name) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s) {
        s->state = AL_STOPPED;
        s->cursor = 0.0;
    }
    al_unlock();
}

void eal_alSourceRewind(uint32_t name) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s) {
        s->state = AL_INITIAL;
        s->cursor = 0.0;
    }
    al_unlock();
}

void eal_alSourceQueueBuffers(uint32_t name, int32_t n,
                              const uint32_t *names) {
    if (!al_mutex_ready || !names || n <= 0)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s) {
        for (int32_t i = 0; i < n; i++) {
            if (s->pending_count >= AL_MAX_QUEUE) {
                al_set_error(AL_OUT_OF_MEMORY);
                break;
            }
            al_buffer_t *b = buffer_get(names[i]);
            if (!b) {
                al_set_error(AL_INVALID_NAME);
                break;
            }
            s->pending[s->pending_count++] = (int) names[i];
        }
    } else {
        al_set_error(AL_INVALID_NAME);
    }
    al_unlock();

    if (queue_log_count < 4) {
        eal_trace("openal: queue %d buffer(s) on source %u",
                  (int) n, (unsigned) name);
        queue_log_count++;
    }
}

void eal_alSourceUnqueueBuffers(uint32_t name, int32_t n, uint32_t *names) {
    if (!al_mutex_ready || !names || n <= 0)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (!s) {
        al_set_error(AL_INVALID_NAME);
    } else if (s->done_count < n) {
        al_set_error(AL_INVALID_VALUE);
    } else {
        for (int32_t i = 0; i < n; i++)
            names[i] = (uint32_t) s->done[i];
        memmove(&s->done[0], &s->done[n],
                (size_t) (s->done_count - n) * sizeof(int));
        s->done_count -= n;
    }
    al_unlock();
}

void eal_alSourcei(uint32_t name, int32_t param, int32_t value) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (!s) {
        al_set_error(AL_INVALID_NAME);
        al_unlock();
        return;
    }

    switch (param) {
        case AL_BUFFER:
            if (value == 0) {
                s->pending_count = 0;
                s->done_count = 0;
                s->cursor = 0.0;
            } else if (buffer_get((uint32_t) value)) {
                s->pending_count = 0;
                s->done_count = 0;
                s->pending[s->pending_count++] = value;
                s->cursor = 0.0;
            } else {
                al_set_error(AL_INVALID_NAME);
            }
            break;
        case AL_LOOPING:
            s->looping = value != 0;
            break;
        case AL_SOURCE_RELATIVE:
            s->relative = value != 0;
            break;
        default:
            /* Ignore unsupported parameters instead of setting an error:
             * the engine probes several optional ones during init. */
            break;
    }
    al_unlock();
}

void eal_alSourcef(uint32_t name, int32_t param, float value) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (!s) {
        al_set_error(AL_INVALID_NAME);
        al_unlock();
        return;
    }

    switch (param) {
        case AL_GAIN:
            s->gain = value;
            if (gain_log_count < 8) {
                eal_trace("openal: source %u gain=%.3f",
                          (unsigned) name, value);
                gain_log_count++;
            }
            break;
        case AL_PITCH:
            s->pitch = value;
            if (pitch_log_count < 8) {
                eal_trace("openal: source %u pitch=%.3f",
                          (unsigned) name, value);
                pitch_log_count++;
            }
            break;
        case AL_REFERENCE_DISTANCE: s->ref_dist = value; break;
        case AL_MAX_DISTANCE:       s->max_dist = value; break;
        case AL_ROLLOFF_FACTOR:     s->rolloff = value; break;
        case AL_CONE_INNER_ANGLE:   s->inner_angle = value; break;
        case AL_CONE_OUTER_ANGLE:   s->outer_angle = value; break;
        case AL_MIN_GAIN:           break;
        case AL_MAX_GAIN:           break;
        default: break;
    }
    al_unlock();
}

void eal_alSource3f(uint32_t name, int32_t param, float v1, float v2,
                    float v3) {
    if (!al_mutex_ready)
        return;

    al_lock();
    al_source_t *s = source_get(name);
    if (s) {
        switch (param) {
            case AL_POSITION:
                s->pos[0] = v1; s->pos[1] = v2; s->pos[2] = v3;
                if (pos_log_count < 8) {
                    eal_trace("openal: source %u pos=(%.1f,%.1f,%.1f)",
                              (unsigned) name, v1, v2, v3);
                    pos_log_count++;
                }
                break;
            case AL_VELOCITY:  s->vel[0] = v1; s->vel[1] = v2; s->vel[2] = v3; break;
            case AL_DIRECTION: s->dir[0] = v1; s->dir[1] = v2; s->dir[2] = v3; break;
            default: break;
        }
    }
    al_unlock();
}

void eal_alSource3i(uint32_t name, int32_t param, int32_t v1, int32_t v2,
                    int32_t v3) {
    eal_alSource3f(name, param, (float) v1, (float) v2, (float) v3);
}

void eal_alSourcefv(uint32_t name, int32_t param, const float *values) {
    if (!values)
        return;
    eal_alSource3f(name, param, values[0], values[1], values[2]);
}

void eal_alListenerf(int32_t param, float value) {
    if (!al_mutex_ready)
        return;

    al_lock();
    if (param == AL_GAIN) {
        listener_gain = value;
        static int listener_logged = 0;
        if (!listener_logged) {
            eal_trace("openal: listener gain=%.3f", value);
            listener_logged = 1;
        }
    }
    al_unlock();
}

void eal_alListener3f(int32_t param, float v1, float v2, float v3) {
    if (!al_mutex_ready)
        return;

    al_lock();
    if (param == AL_POSITION) {
        listener_pos[0] = v1;
        listener_pos[1] = v2;
        listener_pos[2] = v3;
    }
    al_unlock();
}

void eal_alListenerfv(int32_t param, const float *values) {
    if (!values || !al_mutex_ready)
        return;

    al_lock();
    if (param == AL_POSITION) {
        listener_pos[0] = values[0];
        listener_pos[1] = values[1];
        listener_pos[2] = values[2];
    } else if (param == AL_ORIENTATION) {
        listener_at[0] = values[0];
        listener_at[1] = values[1];
        listener_at[2] = values[2];
        listener_up[0] = values[3];
        listener_up[1] = values[4];
        listener_up[2] = values[5];
    }
    al_unlock();
}

/* ------------------------------------------------------------------ */
/* ALC implementation                                                   */
/* ------------------------------------------------------------------ */

void *eal_alcOpenDevice(const char *name) {
    (void) name;

    if (!openal_available()) {
        eal_trace("openal: disabled by config");
        return NULL;
    }

    if (!openal_ready())
        return NULL;

    if (!audio_start())
        return NULL;

    eal_trace("openal: device opened");
    return &the_device;
}

uint8_t eal_alcCloseDevice(void *device) {
    (void) device;

    if (audio_started) {
        audio_running = 0;
        if (audio_thread >= 0) {
            sceKernelWaitThreadEnd(audio_thread, NULL, NULL);
            audio_thread = -1;
        }
        audio_started = 0;
        if (audio_port >= 0) {
            sceAudioOutReleasePort(audio_port);
            audio_port = -1;
        }
        if (audio_mem_uid >= 0) {
            sceKernelFreeMemBlock(audio_mem_uid);
            audio_mem_uid = -1;
        }
        audio_out = NULL;
        eal_trace("openal: device closed");
    }

    return AL_TRUE;
}

void *eal_alcCreateContext(void *device, const int32_t *attrlist) {
    (void) attrlist;

    if (!device || !openal_available())
        return NULL;

    eal_trace("openal: context created");
    return &the_context;
}

uint8_t eal_alcMakeContextCurrent(void *context) {
    current_context = (al_context_t *) context;
    if (context)
        eal_trace("openal: context current");
    return AL_TRUE;
}

void *eal_alcGetCurrentContext(void) {
    return current_context;
}

void *eal_alcGetContextsDevice(void *context) {
    (void) context;
    return &the_device;
}

void eal_alcDestroyContext(void *context) {
    (void) context;
    if (current_context == context)
        current_context = NULL;
}

int32_t eal_alcGetError(void *device) {
    (void) device;
    return ALC_NO_ERROR;
}

uint8_t eal_alcIsExtensionPresent(void *device, const char *name) {
    (void) device;
    (void) name;
    return AL_FALSE;
}

const char *eal_alcGetString(void *device, int32_t param) {
    (void) device;

    switch (param) {
        case ALC_DEFAULT_DEVICE_SPECIFIER: return "Vita Audio";
        case ALC_DEVICE_SPECIFIER:         return "Vita Audio";
        case ALC_EXTENSIONS:               return "";
        default:                           return "";
    }
}

void eal_alcGetIntegerv(void *device, int32_t param, int32_t size,
                        int32_t *values) {
    (void) device;

    if (!values || size <= 0)
        return;

    switch (param) {
        case ALC_MAJOR_VERSION:
            values[0] = 1;
            break;
        case ALC_MINOR_VERSION:
            values[0] = 1;
            break;
        case ALC_ATTRIBUTES_SIZE:
            values[0] = 3;
            break;
        case ALC_ALL_ATTRIBUTES:
            if (size >= 2) {
                values[0] = ALC_FREQUENCY;
                values[1] = MIX_RATE;
                if (size >= 3)
                    values[2] = 0;
            }
            break;
        default:
            values[0] = 0;
            break;
    }
}

void eal_alcProcessContext(void *context) {
    (void) context;
}

void eal_alcSuspendContext(void *context) {
    (void) context;
}

/* Capture is not used by this game; report failure so the engine skips it. */
void *eal_alcCaptureOpenDevice(const char *name, uint32_t frequency,
                               int32_t format, int32_t size) {
    (void) name;
    (void) frequency;
    (void) format;
    (void) size;
    return NULL;
}

uint8_t eal_alcCaptureCloseDevice(void *device) {
    (void) device;
    return AL_FALSE;
}

void eal_alcCaptureStart(void *device) {
    (void) device;
}

void eal_alcCaptureStop(void *device) {
    (void) device;
}

void eal_alcCaptureSamples(void *device, void *buffer, int32_t samples) {
    (void) device;
    (void) buffer;
    (void) samples;
}

/* ------------------------------------------------------------------ */
/* Shutdown                                                             */
/* ------------------------------------------------------------------ */

void eal_openal_shutdown(void) {
    eal_alcCloseDevice(&the_device);
}
