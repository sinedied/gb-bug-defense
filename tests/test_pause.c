/*
 * Host-side regression tests for the iter-3 #22 pause module
 * (src/pause.c). Compiles pause.c against host stubs of <gb/gb.h>
 * and <gb/hardware.h>; collaborator dependencies are stubbed
 * inline below so the test exercises pause behavior in isolation.
 *
 * NOT compiled into this binary:
 *   - src/input.c (we stub input_is_pressed/is_repeat)
 *   - src/menu.c, src/enemies.c, src/projectiles.c, src/cursor.c
 *     (no-op shims; menu_is_open() is test-time injectable for T2)
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
 *      tests/test_pause.c src/pause.c -o build/test_pause
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "gtypes.h"
#include "input.h"
#include "menu.h"
#include "cursor.h"
#include "enemies.h"
#include "projectiles.h"
#include "pause.h"
#include "assets.h"

/* ---------------------------------------------------------------- */
/* Stubs for collaborators.                                          */
/* ---------------------------------------------------------------- */

/* OAM tracking. Sized to cover the highest slot pause.c touches
 * (OAM_PAUSE_BASE + 15 = 16) plus OAM_CURSOR (0). */
#define OAM_TRACK 64

typedef struct {
    int   move_count;
    int   tile_count;
    u8    last_x, last_y;
    u8    last_tile;
} oam_slot_t;

static oam_slot_t s_oam[OAM_TRACK];

void move_sprite(unsigned char nb, unsigned char x, unsigned char y) {
    if (nb >= OAM_TRACK) return;
    s_oam[nb].move_count++;
    s_oam[nb].last_x = x;
    s_oam[nb].last_y = y;
}

void set_sprite_tile(unsigned char nb, unsigned char tile) {
    if (nb >= OAM_TRACK) return;
    s_oam[nb].tile_count++;
    s_oam[nb].last_tile = tile;
}

/* Input stubs — test scripts presses one frame at a time. */
static u8 s_pressed_mask;
static u8 s_repeat_mask;

bool input_is_held(u8 mask)    { (void)mask; return false; }
bool input_is_pressed(u8 mask) { return (s_pressed_mask & mask) != 0; }
bool input_is_repeat(u8 mask)  { return (s_repeat_mask & mask) != 0; }

/* Test-time injectable menu state (default: closed). */
static bool s_menu_open_stub;
bool menu_is_open(void) { return s_menu_open_stub; }

/* Cursor / enemies / projectiles — no-op shims (pause.c calls them
 * for side effects we don't need to assert here; T1 verifies pause
 * OAM hiding directly). */
static int s_blink_pause_calls;
static int s_enemies_hide_calls;
static int s_proj_hide_calls;
void cursor_blink_pause(bool paused)  { (void)paused; s_blink_pause_calls++; }
void enemies_hide_all(void)           { s_enemies_hide_calls++; }
void projectiles_hide_all(void)       { s_proj_hide_calls++; }

/* music_duck stub — pause.c calls music_duck(1) on open and music_duck(0)
 * on close (D-MUS-4). The actual duck (NR50 write) is exercised in
 * test_music; here we just record the calls so a future test could
 * assert the open/close pairing if needed. */
static int s_duck_calls;
void music_duck(unsigned char on)      { (void)on; s_duck_calls++; }

/* ---------------------------------------------------------------- */
/* Test framework                                                    */
/* ---------------------------------------------------------------- */

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void reset_all(void) {
    memset(s_oam, 0, sizeof s_oam);
    s_pressed_mask = 0;
    s_repeat_mask = 0;
    s_menu_open_stub = false;
    s_blink_pause_calls = 0;
    s_enemies_hide_calls = 0;
    s_proj_hide_calls = 0;
    pause_init();
    /* Reset OAM tracking AFTER pause_init (which itself fires 16 hide
     * moves we don't want polluting subsequent assertions). */
    memset(s_oam, 0, sizeof s_oam);
}

/* Press a button for one frame, run pause_update, then clear input. */
static void press_one_frame(u8 mask) {
    s_pressed_mask = mask;
    s_repeat_mask = mask;     /* covers UP/DOWN repeat path too */
    pause_update();
    s_pressed_mask = 0;
    s_repeat_mask = 0;
}

/* ---------------------------------------------------------------- */
/* T1 — pause_init hides all 16 OAM slots                            */
/* ---------------------------------------------------------------- */
static void t1_init_hides_all_oam(void) {
    memset(s_oam, 0, sizeof s_oam);
    pause_init();
    u8 i;
    for (i = 0; i < OAM_PAUSE_COUNT; i++) {
        u8 slot = OAM_PAUSE_BASE + i;
        CHECK(s_oam[slot].move_count >= 1);
        CHECK(s_oam[slot].last_x == 0);
        CHECK(s_oam[slot].last_y == 0);
    }
    CHECK(pause_is_open() == false);
    CHECK(pause_quit_requested() == false);
}

/* ---------------------------------------------------------------- */
/* T2 — open/close idempotency + defensive no-op when menu open      */
/* ---------------------------------------------------------------- */
static void t2_open_close_idempotent_and_defensive(void) {
    reset_all();
    pause_open();
    CHECK(pause_is_open() == true);
    pause_close();
    pause_close();    /* second close must not crash or flip state */
    CHECK(pause_is_open() == false);

    /* With menu open, pause_open() must be a defensive no-op. */
    s_menu_open_stub = true;
    pause_open();
    CHECK(pause_is_open() == false);
    s_menu_open_stub = false;
}

/* ---------------------------------------------------------------- */
/* T3 — default selection is RESUME                                  */
/* ---------------------------------------------------------------- */
static void t3_default_selection_resume(void) {
    reset_all();
    pause_open();
    /* Quit flag must be false right after open. */
    CHECK(pause_quit_requested() == false);
    /* A press with default selection (RESUME) must close, NOT quit. */
    press_one_frame(J_A);
    CHECK(pause_is_open() == false);
    CHECK(pause_quit_requested() == false);
}

/* ---------------------------------------------------------------- */
/* T4 — DOWN selects QUIT, A confirms QUIT (one-shot flag)           */
/* ---------------------------------------------------------------- */
static void t4_down_then_a_quits(void) {
    reset_all();
    pause_open();
    press_one_frame(J_DOWN);
    press_one_frame(J_A);
    /* Pause stays open until the caller (game.c) sequences close. */
    CHECK(pause_is_open() == true);
    CHECK(pause_quit_requested() == true);
    /* One-shot: second read returns false. */
    CHECK(pause_quit_requested() == false);
}

/* Helper: render and return the chevron cell's last y coordinate. */
static u8 render_and_get_chevron_y(void) {
    u8 chev_slot = OAM_PAUSE_BASE + 5;
    s_oam[chev_slot].last_y = 0xFF;
    pause_render();
    return s_oam[chev_slot].last_y;
}

/* ---------------------------------------------------------------- */
/* T5 — UP from RESUME wraps to QUIT                                 */
/* ---------------------------------------------------------------- */
static void t5_up_wraps_to_quit(void) {
    reset_all();
    pause_open();
    press_one_frame(J_UP);
    CHECK(render_and_get_chevron_y() == 96);   /* QUIT row */
}

/* ---------------------------------------------------------------- */
/* T6 — DOWN from QUIT wraps to RESUME                               */
/* ---------------------------------------------------------------- */
static void t6_down_from_quit_wraps_to_resume(void) {
    reset_all();
    pause_open();
    press_one_frame(J_DOWN);    /* RESUME → QUIT */
    press_one_frame(J_DOWN);    /* QUIT → RESUME (wrap) */
    CHECK(render_and_get_chevron_y() == 88);   /* RESUME row */
}

/* ---------------------------------------------------------------- */
/* T7 — B closes without quit                                        */
/* ---------------------------------------------------------------- */
static void t7_b_closes_no_quit(void) {
    reset_all();
    pause_open();
    press_one_frame(J_B);
    CHECK(pause_is_open() == false);
    CHECK(pause_quit_requested() == false);
}

/* ---------------------------------------------------------------- */
/* T8 — START closes without quit                                    */
/* ---------------------------------------------------------------- */
static void t8_start_closes_no_quit(void) {
    reset_all();
    pause_open();
    press_one_frame(J_START);
    CHECK(pause_is_open() == false);
    CHECK(pause_quit_requested() == false);
}

/* ---------------------------------------------------------------- */
/* T9 — A on RESUME closes without quit                              */
/* ---------------------------------------------------------------- */
static void t9_a_on_resume_closes(void) {
    reset_all();
    pause_open();
    press_one_frame(J_A);
    CHECK(pause_is_open() == false);
    CHECK(pause_quit_requested() == false);
}

/* ---------------------------------------------------------------- */
/* T10 — render writes 16 OAM tiles with the correct glyph IDs       */
/* ---------------------------------------------------------------- */
static void t10_render_writes_correct_tiles(void) {
    reset_all();
    pause_open();
    pause_render();

    /* Per spec §4 (slot N = OAM_PAUSE_BASE + (N-1)). */
    static const u8 expected_tiles[OAM_PAUSE_COUNT] = {
        SPR_GLYPH_P,     /*  1 */
        SPR_GLYPH_A,     /*  2 */
        SPR_GLYPH_U,     /*  3 */
        SPR_GLYPH_S,     /*  4 */
        SPR_GLYPH_E,     /*  5 */
        SPR_GLYPH_GT,    /*  6 chevron */
        SPR_GLYPH_R,     /*  7 */
        SPR_GLYPH_E,     /*  8 */
        SPR_GLYPH_S,     /*  9 */
        SPR_GLYPH_U,     /* 10 */
        SPR_GLYPH_M,     /* 11 */
        SPR_GLYPH_E,     /* 12 */
        SPR_GLYPH_Q,     /* 13 */
        SPR_GLYPH_U,     /* 14 */
        SPR_GLYPH_I,     /* 15 */
        SPR_GLYPH_T,     /* 16 */
    };
    static const u8 expected_x[OAM_PAUSE_COUNT] = {
        72, 80, 88, 96, 104,
        56,
        72, 80, 88, 96, 104, 112,
        72, 80, 88, 96
    };
    static const u8 expected_y[OAM_PAUSE_COUNT] = {
        80, 80, 80, 80, 80,
        88,    /* chevron at RESUME */
        88, 88, 88, 88, 88, 88,
        96, 96, 96, 96
    };
    u8 i;
    for (i = 0; i < OAM_PAUSE_COUNT; i++) {
        u8 slot = OAM_PAUSE_BASE + i;
        CHECK(s_oam[slot].tile_count >= 1);
        CHECK(s_oam[slot].last_tile == expected_tiles[i]);
        CHECK(s_oam[slot].last_x == expected_x[i]);
        CHECK(s_oam[slot].last_y == expected_y[i]);
    }
}

/* ---------------------------------------------------------------- */
/* T11 — render before open does nothing                             */
/* ---------------------------------------------------------------- */
static void t11_render_when_closed_is_noop(void) {
    reset_all();
    /* pause is closed (default after init). */
    pause_render();
    u8 i;
    int total_tile_writes = 0;
    for (i = 0; i < OAM_TRACK; i++) total_tile_writes += s_oam[i].tile_count;
    CHECK(total_tile_writes == 0);
}

/* ---------------------------------------------------------------- */

int main(void) {
    t1_init_hides_all_oam();
    t2_open_close_idempotent_and_defensive();
    t3_default_selection_resume();
    t4_down_then_a_quits();
    t5_up_wraps_to_quit();
    t6_down_from_quit_wraps_to_resume();
    t7_b_closes_no_quit();
    t8_start_closes_no_quit();
    t9_a_on_resume_closes();
    t10_render_writes_correct_tiles();
    t11_render_when_closed_is_noop();
    if (failures) {
        fprintf(stderr, "test_pause: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_pause: all checks passed\n");
    return 0;
}
