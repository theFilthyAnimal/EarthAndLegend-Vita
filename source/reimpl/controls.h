/*
 * Earth And Legend (PS Vita) - physical controls.
 *
 * The Android game is touch-first, so the physical controls are translated
 * into the input events the ShiVa3D engine understands:
 *   - buttons     -> Android key codes (engineOnKeyboardKeyDown/Up)
 *   - left stick  -> D-pad key codes
 *   - right stick -> a virtual touch drag (engineOnTouchesChange)
 *   - touch screen -> engineOnTouchesChange
 *   - START / SELECT -> a virtual tap on the game's HUD buttons
 */

#ifndef EAL_CONTROLS_H
#define EAL_CONTROLS_H

#include <stdbool.h>
#include <stdint.h>

#define LEFT_ANALOG_DEADZONE  0.16f
#define RIGHT_ANALOG_DEADZONE 0.16f

typedef enum ControlsAction {
    CONTROLS_ACTION_UP = 0,
    CONTROLS_ACTION_DOWN = 1,
    CONTROLS_ACTION_MOVE = 2
} ControlsAction;

/* Implemented in main.c. */
extern void controls_handler_key(int32_t keycode, ControlsAction action);
extern void controls_handler_touch(int32_t id, float x, float y, ControlsAction action);
extern void controls_handler_virtual_touch(int32_t id, float x, float y, ControlsAction action);
extern void controls_handler_mouse(float x, float y, ControlsAction action);

/* Virtual pointer IDs used for the synthesized touch events. */
#define CONTROLS_POINTER_RIGHT_STICK   (-1)
#define CONTROLS_POINTER_INVENTORY     (-2)
#define CONTROLS_POINTER_MAP           (-3)

/* Android key codes used by the game. */
enum {
    AKEYCODE_BACK = 4,
    AKEYCODE_DPAD_UP = 19,
    AKEYCODE_DPAD_DOWN = 20,
    AKEYCODE_DPAD_LEFT = 21,
    AKEYCODE_DPAD_RIGHT = 22,
    AKEYCODE_DPAD_CENTER = 23,
    AKEYCODE_BUTTON_X = 99,
    AKEYCODE_BUTTON_L1 = 102,
    AKEYCODE_BUTTON_R1 = 103,
};

void controls_init(void);
void controls_poll(void);

#endif // EAL_CONTROLS_H
