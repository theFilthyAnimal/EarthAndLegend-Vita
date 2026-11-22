/*
 * Earth And Legend (PS Vita) - ShiVa3D engine bindings.
 *
 * This file wraps the Java_com_dvidearts_earthandlegend_S3DRenderer_* JNI
 * entry points exported by libS3DClient.so so that they can be driven from
 * native code instead of from the original Java S3DRenderer class.
 */

#ifndef EAL_ENGINE_H
#define EAL_ENGINE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Resolve all engine entry points from the loaded .so.
 *
 * @return true if every required entry point was found.
 */
bool engine_init(void);

void engine_set_camera_device_count(int count);
void engine_set_camera_device_name(int index, const char *name);
void engine_set_device_model(const char *model);
void engine_set_device_name(const char *name);
void engine_set_system_version(const char *version);
void engine_set_system_language(const char *language);

void engine_set_directories(const char *cache_dir,
                            const char *home_dir,
                            const char *pack_dir);

/**
 * Provide the main data pack through a real file descriptor.
 *
 * The engine reads the "descriptor" int field of the supplied
 * java.io.FileDescriptor object; the value is provided by our FalsoJNI
 * implementation (see java.c).
 */
void engine_set_pack_file_descriptor(int fd, long offset, long size);

/**
 * Globally enable or disable the engine's audio subsystem.
 *
 * Audio is not implemented in this port, so the engine is told to skip all
 * OpenAL initialisation and sound playback.
 */
void engine_disable_audio(bool disabled);
void engine_audio_start(void);
void engine_audio_stop(void);
bool engine_is_v3(void);
void engine_async_start(void);
void engine_async_flush(void);
int engine_v3_frame(void);
void engine_v3_start(const char *pack_path);
void engine_v3_get_times(uint64_t *run_us, uint64_t *render_us);
int engine_v3_stopped(void);
void engine_apply_cpu_count(int force);

/**
 * Set the engine viewport rotation.
 *
 * 0 = 0 degrees, 1 = -90 degrees, 2 = 180 degrees, 3 = +90 degrees.
 * Has no effect if the entry point is missing.
 */
void engine_set_viewport_rotation(int rotation);

/** Return the current engine viewport rotation (or -1 if unavailable). */
int engine_get_viewport_rotation(void);

void engine_set_location_support(bool supported);
void engine_set_heading_support(bool supported);
void engine_set_device_ip_address(const char *ip);
void engine_set_device_uuid(const char *uuid);

void engine_on_surface_created(void);
void engine_on_surface_changed(int width, int height);

bool engine_initialize(void);
void engine_shutdown(void);
void engine_pause(bool paused);

/**
 * @return false when the engine wants the application to terminate.
 */
bool engine_run_one_frame(void);
bool engine_get_want_swap_buffers(void);
bool engine_did_pass_first_frame(void);
bool engine_get_overlay_movie_has_changed(void);
bool engine_get_camera_device_state(void);
void engine_on_overlay_movie_stopped(void);

/**
 * @return a pointer to an internal buffer holding the current overlay movie
 *         name, or an empty string.
 */
const char *engine_get_overlay_movie(void);

/**
 * Feed up to five touch points to the engine. `active`, `x` and `y` are
 * expected to point to 5-element arrays; `active` values are 1.0f or 0.0f
 * and coordinates are in pixels (the engine is initialized with 960x544).
 */
void engine_on_touches_change(const float *active,
                              const float *x,
                              const float *y);

void engine_on_keyboard_key_down(int keycode, int unicode);
void engine_on_keyboard_key_up(int keycode, int unicode);

void engine_on_mouse_button_down(float x, float y);
void engine_on_mouse_button_up(float x, float y);
void engine_on_mouse_move(float x, float y);

void engine_on_device_move(float x, float y, float z);

#ifdef __cplusplus
}
#endif

#endif // EAL_ENGINE_H
