#include "title.h"
#include "gfx.h"
#include "assets.h"
#include "input.h"
#include "game.h"
#include "difficulty_calc.h"
#include "map_select.h"
#include "save.h"
#include <gb/gb.h>

#if MAP_SELECT_COUNT != MAP_COUNT
#error "MAP_SELECT_COUNT must equal MAP_COUNT - update src/map_select.h"
#endif

/* "PRESS START" prompt blink: toggle row 13 cols 4..15 (12 tiles) every
 * 30 frames between glyph indices and blank (tile 32 = space).
 *
 * Render/update split: title_update() only mutates state and flips a
 * dirty flag; title_render() does the actual BG writes during the
 * VBlank window scheduled by main().
 *
 * Iter-3 #20 difficulty selector: BG-only label at row 10 cols
 * 7..13 (7 tiles). Independent dirty flag so cycling does not disturb
 * the PRESS-START blink path. Title screen owns NO game state — the
 * current selection comes from `game_difficulty()`; LEFT/RIGHT writes
 * back via `game_set_difficulty()`. Edge-only input (D9).
 *
 * Iter-3 #17: second selector row at row 12 (label only) plus a
 * filled-triangle arrow tile (TILE_ARROW) at column 5 of the focused
 * row. UP/DOWN (edge-only) toggles `s_title_focus`; LEFT/RIGHT cycles
 * the focused selector. Service order in title_render is one-per-frame,
 * priority `s_diff_dirty > s_map_dirty > s_focus_dirty > s_dirty`.
 * Worst case is 12 BG writes/frame (≤ 16 cap). */

#define PROMPT_ROW 13
#define PROMPT_COL  4
#define PROMPT_LEN 12

#define DIFF_ROW   10
#define DIFF_COL    7
#define DIFF_W      7

#define MAP_ROW    12
#define MAP_COL     7
#define MAP_W       7

#define FOCUS_COL   5

/* Iter-3 #19: HI line at row 15 cols 5..13 (9 tiles). Static, recomputed
 * when difficulty or map cycles. Inserted in the priority chain
 * AFTER s_focus_dirty, BEFORE s_dirty (prompt blink). 9 writes/frame. */
#define HI_ROW     15
#define HI_COL      5
#define HI_W        9

static u8   s_blink;
static bool s_visible;
static bool s_dirty;
static u8   s_diff_dirty;
static u8   s_map_dirty;
static u8   s_focus_dirty;
static u8   s_hi_dirty;
static u8   s_title_focus;     /* 0 = difficulty, 1 = map */
static const char s_prompt[PROMPT_LEN + 1] = "PRESS START ";
static const char *const s_diff_labels[3] = { "CASUAL ", "NORMAL ", "VETERAN" };
static const char *const s_map_labels[3]  = { " MAP 1 ", " MAP 2 ", " MAP 3 " };

static u8 focused_row(void) {
    return (s_title_focus == 0) ? DIFF_ROW : MAP_ROW;
}
static u8 unfocused_row(void) {
    return (s_title_focus == 0) ? MAP_ROW : DIFF_ROW;
}

static void draw_prompt_now(bool visible) {
    u8 i;
    for (i = 0; i < PROMPT_LEN; i++) {
        char c = visible ? s_prompt[i] : ' ';
        set_bkg_tile_xy(PROMPT_COL + i, PROMPT_ROW, (u8)c);
    }
}

/* Layout: label[0..6] (7 chars) at cols 7–13. */
static void draw_selector(u8 col, u8 row, const char *label) {
    u8 i;
    for (i = 0; i < 7; i++) {
        set_bkg_tile_xy(col + i, row, (u8)label[i]);
    }
}

static void draw_diff_now(void) {
    draw_selector(DIFF_COL, DIFF_ROW, s_diff_labels[game_difficulty()]);
}

static void draw_map_now(void) {
    draw_selector(MAP_COL, MAP_ROW, s_map_labels[game_active_map()]);
}

static void draw_focus_now(void) {
    /* Two writes: clear the unfocused row's arrow cell, paint TILE_ARROW
     * at the focused row's arrow cell. */
    set_bkg_tile_xy(FOCUS_COL, unfocused_row(), (u8)' ');
    set_bkg_tile_xy(FOCUS_COL, focused_row(),   (u8)TILE_ARROW);
}

/* Iter-3 #19: paint `HI: NNNNN` at row 15 cols 5..13. 9 writes total.
 * Re-reads from save.c on each call; no local cache (single-owner
 * convention — high-score cache lives only in save.c). */
static void draw_hi_now(void) {
    u16 v = save_get_hi(game_active_map(), game_difficulty());
    /* 5-digit zero-padded decimal — manual unrolled to avoid printf. */
    u8  d[5];
    u8  i;
    d[4] = (u8)(v % 10u); v /= 10u;
    d[3] = (u8)(v % 10u); v /= 10u;
    d[2] = (u8)(v % 10u); v /= 10u;
    d[1] = (u8)(v % 10u); v /= 10u;
    d[0] = (u8)(v % 10u);
    set_bkg_tile_xy(HI_COL + 0, HI_ROW, (u8)'H');
    set_bkg_tile_xy(HI_COL + 1, HI_ROW, (u8)'I');
    set_bkg_tile_xy(HI_COL + 2, HI_ROW, (u8)':');
    set_bkg_tile_xy(HI_COL + 3, HI_ROW, (u8)' ');
    for (i = 0; i < 5; i++) {
        set_bkg_tile_xy(HI_COL + 4 + i, HI_ROW, (u8)('0' + d[i]));
    }
}

void title_enter(void) {
    gfx_hide_all_sprites();
    /* Render the title screen tilemap (20x18). 360 BG writes — bracket with
     * DISPLAY_OFF so they bypass active-scan VRAM blocking. */
    DISPLAY_OFF;
    set_bkg_tiles(0, 0, 20, 18, title_tilemap);
    s_blink = 0;
    s_visible = true;
    s_dirty = false;
    s_title_focus = 0;       /* D12: reset focus to difficulty on entry */
    s_diff_dirty = 0;
    s_map_dirty = 0;
    s_focus_dirty = 0;
    s_hi_dirty = 0;
    draw_prompt_now(true);
    draw_diff_now();
    draw_map_now();
    draw_focus_now();
    draw_hi_now();
    DISPLAY_ON;
}

void title_update(void) {
    /* Iter-3 #20: edge-only LEFT/RIGHT cycle with wrap. Uses
     * `input_is_pressed` (NOT `input_is_repeat`) — the 3-state cycle
     * is too short for auto-repeat to feel right.
     *
     * Iter-3 #17: UP/DOWN (edge-only) toggles `s_title_focus`;
     * LEFT/RIGHT routes to whichever selector currently owns focus.
     * Branches are mutually exclusive (single `if/else if` chain) so
     * a single frame never both moves focus and cycles a selector. */
    if (input_is_pressed(J_UP) || input_is_pressed(J_DOWN)) {
        s_title_focus ^= 1u;
        s_focus_dirty = 1;
    } else if (input_is_pressed(J_LEFT)) {
        if (s_title_focus == 0) {
            game_set_difficulty(difficulty_cycle_left(game_difficulty()));
            s_diff_dirty = 1;
            s_hi_dirty = 1;
        } else {
            game_set_active_map(map_cycle_left(game_active_map()));
            s_map_dirty = 1;
            s_hi_dirty = 1;
        }
    } else if (input_is_pressed(J_RIGHT)) {
        if (s_title_focus == 0) {
            game_set_difficulty(difficulty_cycle_right(game_difficulty()));
            s_diff_dirty = 1;
            s_hi_dirty = 1;
        } else {
            game_set_active_map(map_cycle_right(game_active_map()));
            s_map_dirty = 1;
            s_hi_dirty = 1;
        }
    }

    s_blink++;
    if ((s_blink % 30) == 0) {
        s_visible = !s_visible;
        s_dirty = true;
    }
}

void title_render(void) {
    /* VBlank BG-write budget is 16 writes/frame. Service ONE dirty
     * flag per frame in priority order, returning after each so the
     * worst-case write count is the heaviest single branch (12 for
     * the prompt blink; 7 for either selector; 2 for focus). The
     * blink is a 30-frame periodic toggle, so multi-frame slip is
     * invisible. (Iter-3 #20 selector-first, blink-deferred convention
     * extended to four flags.) */
    if (s_diff_dirty) {
        draw_diff_now();
        s_diff_dirty = 0;
        return;
    }
    if (s_map_dirty) {
        draw_map_now();
        s_map_dirty = 0;
        return;
    }
    if (s_focus_dirty) {
        draw_focus_now();
        s_focus_dirty = 0;
        return;
    }
    if (s_hi_dirty) {
        draw_hi_now();
        s_hi_dirty = 0;
        return;
    }
    if (s_dirty) {
        draw_prompt_now(s_visible);
        s_dirty = false;
    }
}

