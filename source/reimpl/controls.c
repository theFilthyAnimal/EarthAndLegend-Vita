/*
 * Earth And Legend (PS Vita) - physical controls.
 *
 * The game is touch-first, so the physical controls are translated into the
 * input events the ShiVa3D engine understands:
 *
 *   - D-pad / Cross / Circle / Square / L1 / R1 -> Android key codes
 *   - left stick  -> D-pad key codes (digital, 8 directions)
 *   - right stick -> a synthetic touch drag that rotates the camera
 *   - touch screen -> engineOnTouchesChange
 *   - START / SELECT -> a synthetic tap on the game's own HUD buttons
 *
 * START and SELECT do nothing else: they only tap the top-left portrait
 * (inventory) and the bottom-left globe (map). No control is disabled or
 * remapped while a menu is open, exactly like on Android.
 */

#include "reimpl/controls.h"

#include <math.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/touch.h>

#include "utils/settings.h"
#include "utils/trace.h"

static void coord_normalize(float * x, float * y, float deadzone) {
    float magnitude = sqrtf((*x * *x) + (*y * *y));
    if (magnitude < deadzone) {
        *x = 0;
        *y = 0;
        return;
    }

    *x = *x / magnitude;
    *y = *y / magnitude;

    float multiplier = ((magnitude - deadzone) / (1 - deadzone));
    *x = *x * multiplier;
    *y = *y * multiplier;
}

/*
 * HUD shortcut taps.
 *
 * The game has no physical shortcuts for these screens, so the port taps the
 * on-screen buttons instead. Both a touch event and an Android mouse event are
 * synthesized because the game's UI buttons react to mouse events.
 */

typedef struct {
    int active;
    int hold;
    int32_t id;
    float x;
    float y;
} UiTap;

static UiTap inventory_tap = { 0, 0, CONTROLS_POINTER_INVENTORY, 50.0f, 28.0f };
static UiTap map_tap       = { 0, 0, CONTROLS_POINTER_MAP,       50.0f, 485.0f };

/* Send one complete tap: down, a short hold, then up. Returns 1 once the
 * finger has been lifted, so the caller can finish the sequence. */
static int ui_tap_step(UiTap *tap) {
    if (!tap->active) {
        controls_handler_virtual_touch(tap->id, tap->x, tap->y,
                                       CONTROLS_ACTION_DOWN);
        controls_handler_mouse(tap->x, tap->y, CONTROLS_ACTION_DOWN);
        tap->active = 1;
        tap->hold = 3;
        return 0;
    }

    if (tap->hold > 0) {
        tap->hold--;
        return 0;
    }

    controls_handler_virtual_touch(tap->id, tap->x, tap->y,
                                   CONTROLS_ACTION_UP);
    controls_handler_mouse(tap->x, tap->y, CONTROLS_ACTION_UP);
    tap->active = 0;
    return 1;
}

/* Tap sequencers: 0 = idle. */
static int start_seq = 0;
static int select_seq = 0;

void controls_init(void) {
    sceCtrlSetSamplingModeExt(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, 1);

    inventory_tap.x = (float) setting_startTapX;
    inventory_tap.y = (float) setting_startTapY;
    map_tap.x = (float) setting_selectTapX;
    map_tap.y = (float) setting_selectTapY;
}

/*
 * Touch screen: forward real fingers to the engine unchanged.
 */

static SceTouchData touch;
static SceTouchData touch_old;

static void poll_touch(void) {
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    for (int i = 0; i < touch.reportNum; i++) {
        float x = (float) touch.report[i].x * 960.0f / 1920.0f;
        float y = (float) touch.report[i].y * 544.0f / 1088.0f;

        int finger_down = 0;

        for (int j = 0; j < touch_old.reportNum; j++) {
            if (touch.report[i].id == touch_old.report[j].id) {
                finger_down = 1;
                break;
            }
        }

        controls_handler_touch(touch.report[i].id, x, y,
                               finger_down ? CONTROLS_ACTION_MOVE
                                           : CONTROLS_ACTION_DOWN);
    }

    for (int i = 0; i < touch_old.reportNum; i++) {
        int finger_up = 1;

        for (int j = 0; j < touch.reportNum; j++) {
            if (touch.report[j].id == touch_old.report[i].id) {
                finger_up = 0;
                break;
            }
        }

        if (finger_up) {
            float x = (float) touch_old.report[i].x * 960.0f / 1920.0f;
            float y = (float) touch_old.report[i].y * 544.0f / 1088.0f;

            controls_handler_touch(touch_old.report[i].id, x, y,
                                   CONTROLS_ACTION_UP);
        }
    }

    sceClibMemcpy(&touch_old, &touch, sizeof(touch));
}

/*
 * Buttons.
 */

typedef struct {
    uint32_t sce_button;
    int32_t android_button;
} ButtonMapping;

static const ButtonMapping mapping[] = {
    { SCE_CTRL_UP,     AKEYCODE_DPAD_UP },
    { SCE_CTRL_DOWN,   AKEYCODE_DPAD_DOWN },
    { SCE_CTRL_LEFT,   AKEYCODE_DPAD_LEFT },
    { SCE_CTRL_RIGHT,  AKEYCODE_DPAD_RIGHT },
    { SCE_CTRL_CROSS,  AKEYCODE_DPAD_CENTER },
    { SCE_CTRL_CIRCLE, AKEYCODE_BACK },
    { SCE_CTRL_SQUARE, AKEYCODE_BUTTON_X },
    { SCE_CTRL_L1,     AKEYCODE_BUTTON_L1 },
    { SCE_CTRL_R1,     AKEYCODE_BUTTON_R1 },
};

#define MAPPED_MASK (SCE_CTRL_UP | SCE_CTRL_DOWN | SCE_CTRL_LEFT | \
                     SCE_CTRL_RIGHT | SCE_CTRL_CROSS | SCE_CTRL_CIRCLE | \
                     SCE_CTRL_SQUARE | SCE_CTRL_L1 | SCE_CTRL_R1)

static uint32_t current_buttons = 0;
static uint32_t forwarded_buttons = 0;

/*
 * Left stick: D-pad key synthesis.
 */

static uint32_t left_keys_dir = 0;

static void update_left_keys(float x, float y) {
    uint32_t dir = 0;

    if (y < -0.35f) dir |= SCE_CTRL_UP;
    if (y >  0.35f) dir |= SCE_CTRL_DOWN;
    if (x < -0.35f) dir |= SCE_CTRL_LEFT;
    if (x >  0.35f) dir |= SCE_CTRL_RIGHT;

    uint32_t changed = dir ^ left_keys_dir;

    if (changed & SCE_CTRL_UP)
        controls_handler_key(AKEYCODE_DPAD_UP,
                             (dir & SCE_CTRL_UP) ? CONTROLS_ACTION_DOWN
                                                 : CONTROLS_ACTION_UP);
    if (changed & SCE_CTRL_DOWN)
        controls_handler_key(AKEYCODE_DPAD_DOWN,
                             (dir & SCE_CTRL_DOWN) ? CONTROLS_ACTION_DOWN
                                                   : CONTROLS_ACTION_UP);
    if (changed & SCE_CTRL_LEFT)
        controls_handler_key(AKEYCODE_DPAD_LEFT,
                             (dir & SCE_CTRL_LEFT) ? CONTROLS_ACTION_DOWN
                                                   : CONTROLS_ACTION_UP);
    if (changed & SCE_CTRL_RIGHT)
        controls_handler_key(AKEYCODE_DPAD_RIGHT,
                             (dir & SCE_CTRL_RIGHT) ? CONTROLS_ACTION_DOWN
                                                    : CONTROLS_ACTION_UP);

    left_keys_dir = dir;
}

/*
 * Right stick: camera drag.
 *
 * The game rotates the camera by dragging a finger across the screen. When the
 * right stick is tilted we synthesize a virtual finger that starts at a fixed
 * anchor and follows the stick direction. When it reaches the screen edge it
 * is released and pressed again at the anchor, otherwise the engine stops
 * receiving a delta and the camera sticks.
 */

static bool  vstick_right_active = false;
static bool  vstick_right_needs_repress = false;
static float vstick_right_last_x = 0.0f;
static float vstick_right_last_y = 0.0f;

/* Frame-time scale for the right stick. The engine consumes a touch delta per
 * rendered frame, so at a low frame rate the camera would move much slower.
 * Scaling by the real frame duration keeps the rotation speed constant in
 * degrees per second. */
static float frame_scale = 1.0f;

static void release_right_camera(void) {
    vstick_right_needs_repress = false;
    if (!vstick_right_active)
        return;

    controls_handler_virtual_touch(CONTROLS_POINTER_RIGHT_STICK,
                                   vstick_right_last_x,
                                   vstick_right_last_y,
                                   CONTROLS_ACTION_UP);
    vstick_right_active = false;
}

static void update_right_camera(float x, float y) {
    coord_normalize(&x, &y, RIGHT_ANALOG_DEADZONE);

    if (x == 0.0f && y == 0.0f) {
        release_right_camera();
        return;
    }

    if (vstick_right_needs_repress) {
        vstick_right_needs_repress = false;
        vstick_right_last_x = (float) setting_cameraAnchorX;
        vstick_right_last_y = (float) setting_cameraAnchorY;
        controls_handler_virtual_touch(CONTROLS_POINTER_RIGHT_STICK,
                                       vstick_right_last_x,
                                       vstick_right_last_y,
                                       CONTROLS_ACTION_DOWN);
        vstick_right_active = true;
        return;
    }

    if (!vstick_right_active) {
        vstick_right_last_x = (float) setting_cameraAnchorX;
        vstick_right_last_y = (float) setting_cameraAnchorY;
        controls_handler_virtual_touch(CONTROLS_POINTER_RIGHT_STICK,
                                       vstick_right_last_x,
                                       vstick_right_last_y,
                                       CONTROLS_ACTION_DOWN);
        vstick_right_active = true;

        // The engine needs to see the touch start at the anchor before it
        // receives a movement event.
        return;
    }

    float nx = vstick_right_last_x +
               x * (float) setting_cameraSpeed * frame_scale;
    float ny = vstick_right_last_y +
               y * (float) setting_cameraSpeed * frame_scale;

    const float margin = 48.0f;

    if (nx < margin || nx > 960.0f - margin ||
        ny < margin || ny > 544.0f - margin) {
        controls_handler_virtual_touch(CONTROLS_POINTER_RIGHT_STICK,
                                       vstick_right_last_x,
                                       vstick_right_last_y,
                                       CONTROLS_ACTION_UP);
        vstick_right_active = false;
        vstick_right_needs_repress = true;
        return;
    }

    vstick_right_last_x = nx;
    vstick_right_last_y = ny;

    controls_handler_virtual_touch(CONTROLS_POINTER_RIGHT_STICK,
                                   vstick_right_last_x,
                                   vstick_right_last_y,
                                   CONTROLS_ACTION_MOVE);
}

/*
 * Main poll.
 */

static void poll_pad(void) {
    SceCtrlData pad;

    if (sceCtrlPeekBufferPositiveExt2(0, &pad, 1) < 1)
        return;

    uint32_t previous = current_buttons;
    current_buttons = pad.buttons;

    uint32_t pressed = current_buttons & ~previous;

    /* START = inventory (top-left portrait). One tap per press. */
    if (pressed & SCE_CTRL_START) {
        eal_trace("controls: inventory tap (%.0f,%.0f)",
                  inventory_tap.x, inventory_tap.y);
        start_seq = 1;
    }
    if (start_seq == 1 && ui_tap_step(&inventory_tap))
        start_seq = 0;

    /* SELECT = map (bottom-left globe). A plain tap, nothing else. */
    if (pressed & SCE_CTRL_SELECT) {
        eal_trace("controls: map tap (%.0f,%.0f)",
                  map_tap.x, map_tap.y);
        select_seq = 1;
    }
    if (select_seq == 1 && ui_tap_step(&map_tap))
        select_seq = 0;

    float lx = ((float) pad.lx - 128.0f) / 128.0f;
    float ly = ((float) pad.ly - 128.0f) / 128.0f;
    float rx = ((float) pad.rx - 128.0f) / 128.0f;
    float ry = ((float) pad.ry - 128.0f) / 128.0f;

    update_left_keys(lx, ly);
    update_right_camera(rx, ry);

    uint32_t want_forwarded = current_buttons & MAPPED_MASK;

    for (unsigned i = 0; i < sizeof(mapping) / sizeof(ButtonMapping); i++) {
        uint32_t bit = mapping[i].sce_button;
        int32_t android = mapping[i].android_button;

        if ((want_forwarded & bit) && !(forwarded_buttons & bit))
            controls_handler_key(android, CONTROLS_ACTION_DOWN);
        else if (!(want_forwarded & bit) && (forwarded_buttons & bit))
            controls_handler_key(android, CONTROLS_ACTION_UP);
    }

    forwarded_buttons = want_forwarded;
}

void controls_poll(void) {
    static uint64_t last_poll_us = 0;
    uint64_t now = sceKernelGetProcessTimeWide();

    if (last_poll_us != 0) {
        float dt = (float) (now - last_poll_us) / 1000000.0f;
        if (dt < 0.001f)
            dt = 0.001f;
        if (dt > 0.120f)
            dt = 0.120f;
        frame_scale = dt * 60.0f;
    }

    last_poll_us = now;

    poll_touch();
    poll_pad();
}
