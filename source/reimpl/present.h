/*
 * Earth And Legend 3.0.0 (PS Vita) - presentation.
 *
 * 3.0.0 renders in native landscape, so the presentation helpers are no-ops.
 * They are kept because the loader and the GL import wrappers call them.
 */

#ifndef EAL_PRESENT_H
#define EAL_PRESENT_H

#ifdef __cplusplus
extern "C" {
#endif

void present_init(void);
int present_active(void);
int present_gpu_active(void);
unsigned int present_get_fbo(void);
int present_engine_width(void);
int present_engine_height(void);
void present_begin_frame(void);
void present_frame(void);
void present_transform_touch(float *x, float *y);
void present_note_draw_call(void);
unsigned present_take_draw_calls(void);

#ifdef __cplusplus
}
#endif

#endif // EAL_PRESENT_H
