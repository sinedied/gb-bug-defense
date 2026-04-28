#include "input.h"
#include <gb/gb.h>

#define INITIAL_DELAY 12
#define REPEAT_DELAY   6

static u8 s_held;
static u8 s_prev;
static u8 s_pressed;
static u8 s_repeat;
static u8 s_hold_frames;   /* frames the current direction has been held */
static u8 s_prev_dpad;     /* d-pad bits last poll, for direction-change reset */

#define DPAD_MASK (J_UP | J_DOWN | J_LEFT | J_RIGHT)

void input_init(void) {
    s_held = s_prev = s_pressed = s_repeat = 0;
    s_hold_frames = 0;
    s_prev_dpad = 0;
}

void input_poll(void) {
    s_prev = s_held;
    s_held = joypad();
    s_pressed = s_held & ~s_prev;
    s_repeat = s_pressed;

    /* Auto-repeat for D-pad: 12-frame initial then 6-frame interval.
     * Reset the hold counter whenever the d-pad bitmask changes, so e.g.
     * holding RIGHT then pressing UP doesn't inherit RIGHT's accumulated
     * hold time and immediately auto-repeat UP. */
    u8 cur_dpad = s_held & DPAD_MASK;
    if (cur_dpad != s_prev_dpad) {
        s_hold_frames = 0;
    }
    if (cur_dpad) {
        if (s_hold_frames < 255) s_hold_frames++;
        if (s_hold_frames > INITIAL_DELAY) {
            u8 t = s_hold_frames - INITIAL_DELAY;
            if ((t % REPEAT_DELAY) == 0) {
                s_repeat |= cur_dpad;
            }
        }
    } else {
        s_hold_frames = 0;
    }
    s_prev_dpad = cur_dpad;
}

bool input_is_held(u8 mask)    { return (s_held & mask) ? true : false; }
bool input_is_pressed(u8 mask) { return (s_pressed & mask) ? true : false; }
bool input_is_repeat(u8 mask)  { return (s_repeat & mask) ? true : false; }
