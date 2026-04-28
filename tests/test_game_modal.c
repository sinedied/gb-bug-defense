/*
 * Host-side regression tests for src/game_modal.h —
 * the pure modal-precedence / START gating helper used by
 * src/game.c::playing_update().
 *
 * Why a dedicated test: pause/menu isolation tests in test_pause.c
 * never linked playing_update(), so F1 (same-frame menu-close + START
 * leaks into pause_open) went undetected. This test exercises the
 * exact gating predicate that decides whether pause_open() fires.
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_game_modal.c \
 *      -o build/test_game_modal
 *
 * Zero GBDK linkage required: game_modal.h is header-only.
 */
#include <stdio.h>
#include <string.h>

#include "game_modal.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

/* T1 — F1 regression: menu just closed this frame; START must NOT
 * open pause. Without the menu_was_open latch, this returns true. */
static void t1_resume_does_not_open_pause(void) {
    bool got = playing_modal_should_open_pause(
        /* menu_was_open  */ true,
        /* start_pressed  */ true,
        /* menu_now_open  */ false,
        /* pause_now_open */ false);
    CHECK(got == false);
}

/* T2 — Normal case: no modal active, START edge → open pause. */
static void t2_normal_start_opens_pause(void) {
    bool got = playing_modal_should_open_pause(
        /* menu_was_open  */ false,
        /* start_pressed  */ true,
        /* menu_now_open  */ false,
        /* pause_now_open */ false);
    CHECK(got == true);
}

/* T3 — Pause already open: START must not re-open (idempotency gate). */
static void t3_pause_already_open_no_reopen(void) {
    bool got = playing_modal_should_open_pause(
        /* menu_was_open  */ false,
        /* start_pressed  */ true,
        /* menu_now_open  */ false,
        /* pause_now_open */ true);
    CHECK(got == false);
}

/* T4 — No START press: never open pause regardless of other state. */
static void t4_no_start_press_no_open(void) {
    /* Sweep all 8 combinations of the three booleans with start=false. */
    int i;
    for (i = 0; i < 8; i++) {
        bool mwo = (i & 1) != 0;
        bool mno = (i & 2) != 0;
        bool pno = (i & 4) != 0;
        bool got = playing_modal_should_open_pause(mwo, false, mno, pno);
        CHECK(got == false);
    }
}

/* T5 — Menu still open this frame (e.g. selection navigation): START
 * never opens pause. Covers the case menu_was_open=false but a
 * hypothetical same-frame menu_open() raised it. */
static void t5_menu_still_open_no_pause(void) {
    bool got = playing_modal_should_open_pause(
        /* menu_was_open  */ false,
        /* start_pressed  */ true,
        /* menu_now_open  */ true,
        /* pause_now_open */ false);
    CHECK(got == false);
}

int main(void) {
    t1_resume_does_not_open_pause();
    t2_normal_start_opens_pause();
    t3_pause_already_open_no_reopen();
    t4_no_start_press_no_open();
    t5_menu_still_open_no_pause();
    if (failures) {
        fprintf(stderr, "test_game_modal: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_game_modal: all checks passed\n");
    return 0;
}
