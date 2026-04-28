#ifndef GAME_MODAL_H
#define GAME_MODAL_H

/* Pure helper extracted from playing_update() so the modal-precedence /
 * START gating logic can be unit-tested without GBDK linkage.
 *
 * Question: should playing_update() open the pause menu this frame?
 *
 * Inputs (sampled by the caller in playing_update):
 *   menu_was_open   — value of menu_is_open() LATCHED at the top of the
 *                     frame, BEFORE menu_update() ran. Required to suppress
 *                     a same-frame "A/B closes upgrade menu, START opens
 *                     pause" misfire (F1 regression).
 *   start_pressed   — input_is_pressed(J_START) edge for this frame.
 *   menu_now_open   — menu_is_open() AFTER menu_update() ran.
 *   pause_now_open  — pause_is_open() right before this check.
 *
 * Returns true iff the caller should invoke pause_open() this frame.
 *
 * Header-inline (not a separate .c) so test_game_modal links with zero
 * GBDK collaborators — see tests/test_game_modal.c.
 */

/* Match gtypes.h's bool definition without forcing it as a dependency,
 * so test code can include this header standalone. */
#ifndef bool
#define bool unsigned char
#define true 1
#define false 0
#endif

static inline bool playing_modal_should_open_pause(bool menu_was_open,
                                                   bool start_pressed,
                                                   bool menu_now_open,
                                                   bool pause_now_open) {
    return start_pressed
        && !menu_was_open
        && !menu_now_open
        && !pause_now_open;
}

#endif /* GAME_MODAL_H */
