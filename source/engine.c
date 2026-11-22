/*
 * Earth And Legend (PS Vita) - ShiVa3D engine bindings.
 */

#include "engine.h"

#include <stdlib.h>
#include <string.h>

#include <psp2/kernel/processmgr.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include "utils/logger.h"
#include "utils/trace.h"
#include "java.h"

extern so_module so_mod;

#define THIZ ((jobject) 0x42424242)
#define ENV  ((JNIEnv *) &jni)
#define ENGINE_PREFIX "Java_com_dvidearts_earthandlegend_S3DRenderer_"

typedef struct {
    void     (*setCameraDeviceCount)(JNIEnv *, jobject, jint);
    void     (*setCameraDeviceName)(JNIEnv *, jobject, jint, jstring);
    void     (*setDeviceModel)(JNIEnv *, jobject, jstring);
    void     (*setDeviceName)(JNIEnv *, jobject, jstring);
    void     (*setSystemVersion)(JNIEnv *, jobject, jstring);
    void     (*setSystemLanguage)(JNIEnv *, jobject, jstring);
    void     (*setDirectories)(JNIEnv *, jobject, jstring, jstring, jstring);
    void     (*setPackFileDescriptor)(JNIEnv *, jobject, jobject, jlong, jlong);
    void     (*setLocationSupport)(JNIEnv *, jobject, jboolean);
    void     (*setHeadingSupport)(JNIEnv *, jobject, jboolean);
    void     (*setDeviceIPAddress)(JNIEnv *, jobject, jstring);
    void     (*setDeviceUUID)(JNIEnv *, jobject, jstring);

    void     (*onSurfaceCreated)(JNIEnv *, jobject);
    void     (*onSurfaceChanged)(JNIEnv *, jobject, jint, jint);

    jboolean (*initialize)(JNIEnv *, jobject);
    void     (*shutdown)(JNIEnv *, jobject);
    void     (*pause)(JNIEnv *, jobject, jboolean);

    jboolean (*runOneFrame)(JNIEnv *, jobject);
    jboolean (*getWantSwapBuffers)(JNIEnv *, jobject);
    jboolean (*didPassFirstFrame)(JNIEnv *, jobject);
    jboolean (*getOverlayMovieHasChanged)(JNIEnv *, jobject);
    jstring  (*getOverlayMovie)(JNIEnv *, jobject);
    jboolean (*getCameraDeviceState)(JNIEnv *, jobject);
    void     (*onOverlayMovieStopped)(JNIEnv *, jobject);

    void     (*onTouchesChange)(JNIEnv *, jobject,
                                jint, jfloat, jfloat,
                                jint, jfloat, jfloat,
                                jint, jfloat, jfloat,
                                jint, jfloat, jfloat,
                                jint, jfloat, jfloat);
    void     (*onKeyboardKeyDown)(JNIEnv *, jobject, jint, jint);
    void     (*onKeyboardKeyUp)(JNIEnv *, jobject, jint, jint);
    void     (*onMouseButtonDown)(JNIEnv *, jobject, jfloat, jfloat);
    void     (*onMouseButtonUp)(JNIEnv *, jobject, jfloat, jfloat);
    void     (*onMouseMove)(JNIEnv *, jobject, jfloat, jfloat);
    void     (*onDeviceMove)(JNIEnv *, jobject, jfloat, jfloat, jfloat);
} EngineAPI;

static EngineAPI api;
static bool engine_ready = false;

/* Exported directly by libS3DClient.so, not through the JNI bridge. */
static void (*set_audio_disabled)(int disabled) = NULL;
static void (*set_viewport_rotation)(int rotation) = NULL;
static int  (*get_viewport_rotation)(void) = NULL;

/* 3.0.0 renders asynchronously; these two drive the render thread. */
static void (*android_init_async)(void) = NULL;
static void (*android_flush_async)(void) = NULL;

/* Synchronous 3.0.0 frame entry points (bypass the async wrapper). */
static void (*s3d_run_one_frame)(void) = NULL;
static int  (*s3d_render)(void) = NULL;
static int  (*s3d_load_pack)(const char *name, const char *pack, void *arg) = NULL;
static int  (*s3d_stopped)(void) = NULL;

/* Processor-count plumbing. android_getCpuCount() is the function the engine
 * uses to size its worker pool; iDeviceProcessorCount caches its result. */
static int  (*s3d_get_cpu_count)(void) = NULL;
static int  *s3d_processor_count = NULL;

/* Timing of the two synchronous 3.x frame passes. */
static uint64_t v3_run_us = 0;
static uint64_t v3_render_us = 0;

/* Android sound-device glue exported by the engine. Driver 7 selects the
 * OpenAL backend; driver 1 selects the Java callbacks. */
static int  (*android_set_external_driver)(int use_external) = NULL;
static void (*android_init_sound)(void) = NULL;
static void (*android_shutdown_sound)(void) = NULL;

static char overlay_movie[256];

static jstring make_string(const char * str) {
    if (!str)
        str = "";
    return jni->NewStringUTF(ENV, str);
}

#define RESOLVE(field, name)                                                   \
    do {                                                                       \
        api.field = (void *) so_symbol(&so_mod, ENGINE_PREFIX name);           \
        if (!api.field) {                                                      \
            l_fatal("Engine symbol not found: %s%s", ENGINE_PREFIX, name);     \
            eal_trace("engine: required symbol missing: %s", name);            \
            return false;                                                      \
        }                                                                      \
    } while (0)

/* Some 3.0.0 builds dropped the frame-loop getters. */
#define RESOLVE_OPT(field, name)                                               \
    do {                                                                       \
        api.field = (void *) so_symbol(&so_mod, ENGINE_PREFIX name);           \
        if (!api.field)                                                        \
            eal_trace("engine: optional symbol missing: %s", name);            \
    } while (0)

bool engine_init(void) {
    memset(&api, 0, sizeof(api));

    RESOLVE(setCameraDeviceCount,      "engineSetCameraDeviceCount");
    RESOLVE(setCameraDeviceName,       "engineSetCameraDeviceName");
    RESOLVE(setDeviceModel,            "engineSetDeviceModel");
    RESOLVE(setDeviceName,             "engineSetDeviceName");
    RESOLVE(setSystemVersion,          "engineSetSystemVersion");
    RESOLVE(setSystemLanguage,         "engineSetSystemLanguage");
    RESOLVE(setDirectories,            "engineSetDirectories");
    RESOLVE(setPackFileDescriptor,     "engineSetPackFileDescriptor");
    RESOLVE(setLocationSupport,        "engineSetLocationSupport");
    RESOLVE(setHeadingSupport,         "engineSetHeadingSupport");
    RESOLVE(setDeviceIPAddress,        "engineSetDeviceIPAddress");
    RESOLVE(setDeviceUUID,             "engineSetDeviceUUID");

    RESOLVE(onSurfaceCreated,          "engineOnSurfaceCreated");
    RESOLVE(onSurfaceChanged,          "engineOnSurfaceChanged");

    RESOLVE(initialize,                "engineInitialize");
    RESOLVE(shutdown,                  "engineShutdown");
    RESOLVE(pause,                     "enginePause");

    RESOLVE(runOneFrame,               "engineRunOneFrame");
    RESOLVE_OPT(getWantSwapBuffers,    "engineGetWantSwapBuffers");
    RESOLVE(didPassFirstFrame,         "engineDidPassFirstFrame");
    RESOLVE_OPT(getOverlayMovieHasChanged, "engineGetOverlayMovieHasChanged");
    RESOLVE_OPT(getOverlayMovie,       "engineGetOverlayMovie");
    RESOLVE_OPT(getCameraDeviceState,  "engineGetCameraDeviceState");
    RESOLVE(onOverlayMovieStopped,     "engineOnOverlayMovieStopped");

    RESOLVE(onTouchesChange,           "engineOnTouchesChange");
    RESOLVE(onKeyboardKeyDown,         "engineOnKeyboardKeyDown");
    RESOLVE(onKeyboardKeyUp,           "engineOnKeyboardKeyUp");
    RESOLVE(onMouseButtonDown,         "engineOnMouseButtonDown");
    RESOLVE(onMouseButtonUp,           "engineOnMouseButtonUp");
    RESOLVE(onMouseMove,               "engineOnMouseMove");
    RESOLVE(onDeviceMove,              "engineOnDeviceMove");

    set_audio_disabled = (void *) so_symbol(&so_mod, "S3DClient_SetAudioDisabled");
    if (!set_audio_disabled) {
        l_warn("S3DClient_SetAudioDisabled() is missing; audio may be attempted.");
    }

    android_set_external_driver = (int (*)(int))
        so_symbol(&so_mod, "S3DClient_Android_SetSoundDeviceUseExternalDriver");
    android_init_sound = (void (*)(void))
        so_symbol(&so_mod, "S3DClient_Android_InitializeSoundDevice");
    android_shutdown_sound = (void (*)(void))
        so_symbol(&so_mod, "S3DClient_Android_ShutdownSoundDevice");

    android_init_async = (void (*)(void))
        so_symbol(&so_mod, "S3DClient_Android_InitializeAsyncRenderer");
    android_flush_async = (void (*)(void))
        so_symbol(&so_mod, "S3DClient_Android_FlushAsyncRenderer");

    s3d_run_one_frame = (void (*)(void))
        so_symbol(&so_mod, "S3DClient_RunOneFrame");
    s3d_render = (int (*)(void))
        so_symbol(&so_mod, "S3DClient_Render");

    s3d_load_pack = (int (*)(const char *, const char *, void *))
        so_symbol(&so_mod, "S3DClient_LoadPack");
    s3d_stopped = (int (*)(void))
        so_symbol(&so_mod, "S3DClient_Stopped");

    s3d_get_cpu_count = (int (*)(void))
        so_symbol(&so_mod, "android_getCpuCount");
    s3d_processor_count = (int *)
        so_symbol(&so_mod,
                  "_ZN7Pandora10EngineCore11SystemUtils21iDeviceProcessorCountE");

    if (!s3d_run_one_frame || !s3d_render)
        eal_trace("engine: synchronous 3.x frame symbols missing");

    if (!android_set_external_driver || !android_init_sound) {
        l_warn("Android sound device entry points are missing; "
               "audio may stay disabled.");
    }

    set_viewport_rotation =
        (void *) so_symbol(&so_mod, "S3DClient_iPhone_SetViewportRotation");
    get_viewport_rotation =
        (void *) so_symbol(&so_mod, "S3DClient_iPhone_GetViewportRotation");

    if (!set_viewport_rotation) {
        l_warn("S3DClient_iPhone_SetViewportRotation() is missing.");
    }

    engine_ready = true;
    return true;
}

void engine_set_camera_device_count(int count) {
    if (engine_ready)
        api.setCameraDeviceCount(ENV, THIZ, (jint) count);
}

void engine_set_camera_device_name(int index, const char * name) {
    if (!engine_ready)
        return;
    jstring s = make_string(name);
    api.setCameraDeviceName(ENV, THIZ, (jint) index, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_set_device_model(const char * model) {
    if (!engine_ready)
        return;
    jstring s = make_string(model);
    api.setDeviceModel(ENV, THIZ, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_set_device_name(const char * name) {
    if (!engine_ready)
        return;
    jstring s = make_string(name);
    api.setDeviceName(ENV, THIZ, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_set_system_version(const char * version) {
    if (!engine_ready)
        return;
    jstring s = make_string(version);
    api.setSystemVersion(ENV, THIZ, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_set_system_language(const char * language) {
    if (!engine_ready)
        return;
    jstring s = make_string(language);
    api.setSystemLanguage(ENV, THIZ, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_set_directories(const char * cache_dir,
                            const char * home_dir,
                            const char * pack_dir) {
    if (!engine_ready)
        return;

    jstring cache = make_string(cache_dir);
    jstring home  = make_string(home_dir);
    jstring pack  = make_string(pack_dir);

    api.setDirectories(ENV, THIZ, cache, home, pack);

    jni->DeleteLocalRef(ENV, cache);
    jni->DeleteLocalRef(ENV, home);
    jni->DeleteLocalRef(ENV, pack);
}

void engine_set_pack_file_descriptor(int fd, long offset, long size) {
    if (!engine_ready)
        return;

    java_set_pack_fd(fd);

    // The engine reads the descriptor field from this object and then works
    // with the raw file descriptor, so no real Java object is required.
    api.setPackFileDescriptor(ENV, THIZ, (jobject) 0x42424242,
                              (jlong) offset, (jlong) size);
}

void engine_disable_audio(bool disabled) {
    if (set_audio_disabled)
        set_audio_disabled(disabled ? 1 : 0);
}

void engine_audio_start(void) {
    if (!android_set_external_driver || !android_init_sound) {
        eal_trace("engine: Android sound device symbols missing");
        return;
    }

    /* The engine names this "use external driver", but the mapping is:
     *   false -> SNDDevice driver 1 -> AudioBackend_OpenAL (native OpenAL)
     *   true  -> SNDDevice driver 7 -> AudioBackend_External (Java callbacks)
     * We want the native OpenAL backend, so pass false. */
    int result = android_set_external_driver(0);
    eal_trace("engine: sound driver select -> %d", result);

    android_init_sound();
    eal_trace("engine: sound device init requested");
}

void engine_audio_stop(void) {
    if (android_shutdown_sound)
        android_shutdown_sound();
}

bool engine_is_v3(void) {
    return engine_ready && api.getWantSwapBuffers == NULL;
}

void engine_async_start(void) {
    if (android_init_async) {
        android_init_async();
        eal_trace("engine: async renderer initialised");
    }
}

void engine_async_flush(void) {
    if (android_flush_async)
        android_flush_async();
}

int engine_v3_frame(void) {
    uint64_t t0 = sceKernelGetProcessTimeWide();

    if (s3d_run_one_frame)
        s3d_run_one_frame();

    uint64_t t1 = sceKernelGetProcessTimeWide();
    int result = 0;

    if (s3d_render)
        result = s3d_render();

    uint64_t t2 = sceKernelGetProcessTimeWide();

    v3_run_us += t1 - t0;
    v3_render_us += t2 - t1;

    return result;
}

void engine_v3_get_times(uint64_t *run_us, uint64_t *render_us) {
    if (run_us)
        *run_us = v3_run_us;
    if (render_us)
        *render_us = v3_render_us;
}

int engine_v3_stopped(void) {
    if (s3d_stopped)
        return s3d_stopped();
    return 0;
}

void engine_apply_cpu_count(int force) {
    int detected = -1;
    if (s3d_get_cpu_count)
        detected = s3d_get_cpu_count();

    int cached = s3d_processor_count ? *s3d_processor_count : -1;

    if (s3d_processor_count)
        *s3d_processor_count = force;

    eal_trace("engine: cpu count detected=%d cached=%d forced=%d",
              detected, cached, force);
}

void engine_v3_start(const char *pack_path) {
    if (!s3d_load_pack) {
        eal_trace("engine: S3DClient_LoadPack is missing");
        return;
    }

    int result = s3d_load_pack("", pack_path, NULL);
    eal_trace("engine: v3 LoadPack('%s') -> %d", pack_path, result);
}

void engine_set_viewport_rotation(int rotation) {
    if (!set_viewport_rotation)
        return;

    set_viewport_rotation(rotation);

    eal_trace("engine: viewport rotation <- %d (now %d)",
              rotation, engine_get_viewport_rotation());
}

int engine_get_viewport_rotation(void) {
    if (!get_viewport_rotation)
        return -1;
    return get_viewport_rotation();
}

void engine_set_location_support(bool supported) {
    if (engine_ready)
        api.setLocationSupport(ENV, THIZ, supported ? JNI_TRUE : JNI_FALSE);
}

void engine_set_heading_support(bool supported) {
    if (engine_ready)
        api.setHeadingSupport(ENV, THIZ, supported ? JNI_TRUE : JNI_FALSE);
}

void engine_set_device_ip_address(const char * ip) {
    if (!engine_ready)
        return;
    jstring s = make_string(ip);
    api.setDeviceIPAddress(ENV, THIZ, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_set_device_uuid(const char * uuid) {
    if (!engine_ready)
        return;
    jstring s = make_string(uuid);
    api.setDeviceUUID(ENV, THIZ, s);
    jni->DeleteLocalRef(ENV, s);
}

void engine_on_surface_created(void) {
    if (engine_ready)
        api.onSurfaceCreated(ENV, THIZ);
}

void engine_on_surface_changed(int width, int height) {
    if (engine_ready)
        api.onSurfaceChanged(ENV, THIZ, (jint) width, (jint) height);
}

bool engine_initialize(void) {
    if (!engine_ready)
        return false;
    return api.initialize(ENV, THIZ) != JNI_FALSE;
}

void engine_shutdown(void) {
    if (engine_ready)
        api.shutdown(ENV, THIZ);
}

void engine_pause(bool paused) {
    if (engine_ready)
        api.pause(ENV, THIZ, paused ? JNI_TRUE : JNI_FALSE);
}

bool engine_run_one_frame(void) {
    if (!engine_ready)
        return false;
    return api.runOneFrame(ENV, THIZ) != JNI_FALSE;
}

bool engine_get_want_swap_buffers(void) {
    if (!engine_ready || !api.getWantSwapBuffers)
        return true;
    return api.getWantSwapBuffers(ENV, THIZ) != JNI_FALSE;
}

bool engine_did_pass_first_frame(void) {
    if (!engine_ready)
        return false;
    return api.didPassFirstFrame(ENV, THIZ) != JNI_FALSE;
}

bool engine_get_overlay_movie_has_changed(void) {
    if (!engine_ready || !api.getOverlayMovieHasChanged)
        return false;
    return api.getOverlayMovieHasChanged(ENV, THIZ) != JNI_FALSE;
}

const char * engine_get_overlay_movie(void) {
    if (!engine_ready || !api.getOverlayMovie)
        return "";

    jstring s = api.getOverlayMovie(ENV, THIZ);
    if (!s)
        return "";

    const char * chars = jni->GetStringUTFChars(ENV, s, NULL);
    if (chars) {
        strncpy(overlay_movie, chars, sizeof(overlay_movie) - 1);
        overlay_movie[sizeof(overlay_movie) - 1] = '\0';
        jni->ReleaseStringUTFChars(ENV, s, (char *) chars);
    } else {
        overlay_movie[0] = '\0';
    }

    return overlay_movie;
}

bool engine_get_camera_device_state(void) {
    if (!engine_ready || !api.getCameraDeviceState)
        return false;
    return api.getCameraDeviceState(ENV, THIZ) != JNI_FALSE;
}

void engine_on_overlay_movie_stopped(void) {
    if (engine_ready)
        api.onOverlayMovieStopped(ENV, THIZ);
}

void engine_on_touches_change(const float * active,
                              const float * x,
                              const float * y) {
    if (!engine_ready)
        return;

    api.onTouchesChange(ENV, THIZ,
                        active[0] > 0.5f ? 1 : 0, x[0], y[0],
                        active[1] > 0.5f ? 1 : 0, x[1], y[1],
                        active[2] > 0.5f ? 1 : 0, x[2], y[2],
                        active[3] > 0.5f ? 1 : 0, x[3], y[3],
                        active[4] > 0.5f ? 1 : 0, x[4], y[4]);
}

void engine_on_keyboard_key_down(int keycode, int unicode) {
    if (engine_ready)
        api.onKeyboardKeyDown(ENV, THIZ, (jint) keycode, (jint) unicode);
}

void engine_on_keyboard_key_up(int keycode, int unicode) {
    if (engine_ready)
        api.onKeyboardKeyUp(ENV, THIZ, (jint) keycode, (jint) unicode);
}

void engine_on_mouse_button_down(float x, float y) {
    if (engine_ready)
        api.onMouseButtonDown(ENV, THIZ, x, y);
}

void engine_on_mouse_button_up(float x, float y) {
    if (engine_ready)
        api.onMouseButtonUp(ENV, THIZ, x, y);
}

void engine_on_mouse_move(float x, float y) {
    if (engine_ready)
        api.onMouseMove(ENV, THIZ, x, y);
}

void engine_on_device_move(float x, float y, float z) {
    if (engine_ready)
        api.onDeviceMove(ENV, THIZ, x, y, z);
}
