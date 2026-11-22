/*
 * Earth And Legend 3.0.0 (PS Vita) - presentation.
 *
 * The 3.0.0 engine renders in native landscape straight into the Vita's
 * 960x544 surface, so no rotation, scaling or touch transformation is needed.
 * This file only keeps the small API used by the loader and the draw-call
 * counter used by the periodic performance report.
 */

#include "reimpl/present.h"

#define SCREEN_WIDTH  960
#define SCREEN_HEIGHT 544

static unsigned draw_calls = 0;

int present_active(void) {
    return 0;
}

int present_gpu_active(void) {
    return 0;
}

unsigned int present_get_fbo(void) {
    return 0;
}

int present_engine_width(void) {
    return SCREEN_WIDTH;
}

int present_engine_height(void) {
    return SCREEN_HEIGHT;
}

void present_init(void) {
}

void present_begin_frame(void) {
}

void present_frame(void) {
}

void present_transform_touch(float *x, float *y) {
    (void) x;
    (void) y;
}

/* Incremented by the import wrappers in dynlib.c. */
void present_note_draw_call(void) {
    draw_calls++;
}

unsigned present_take_draw_calls(void) {
    unsigned value = draw_calls;
    draw_calls = 0;
    return value;
}
