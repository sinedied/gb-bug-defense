#include "title.h"
#include "gfx.h"
#include "assets.h"
#include "input.h"
#include "game.h"
#include "difficulty_calc.h"
#include <gb/gb.h>

/* "PRESS START" prompt blink: toggle row 13 cols 4..15 (12 tiles) every
 * 30 frames between glyph indices and blank (tile 32 = space).
 *
 * Render/update split: title_update() only mutates state and flips a
 * dirty flag; title_render() does the actual BG writes during the
 * VBlank window scheduled by main().
 *
 * Iter-3 #20 difficulty selector: BG-only `< LABEL >` at row 10 cols
 * 5..14 (10 tiles). Independent dirty flag so cycling does not disturb
 * the PRESS-START blink path. Title screen owns NO game state — the
 * current selection comes from `game_difficulty()`; LEFT/RIGHT writes
 * back via `game_set_difficulty()`. Edge-only input (D9). */

#define PROMPT_ROW 13
#define PROMPT_COL  4
#define PROMPT_LEN 12

#define DIFF_ROW   10
#define DIFF_COL    5
#define DIFF_W     10

static u8   s_blink;
static bool s_visible;
static bool s_dirty;
static u8   s_diff_dirty;
static const char s_prompt[PROMPT_LEN + 1] = "PRESS START ";
static const char *const s_diff_labels[3] = { " EASY ", "NORMAL", " HARD " };

static void draw_prompt_now(bool visible) {
    u8 i;
    for (i = 0; i < PROMPT_LEN; i++) {
        char c = visible ? s_prompt[i] : ' ';
        set_bkg_tile_xy(PROMPT_COL + i, PROMPT_ROW, (u8)c);
    }
}

static void draw_diff_now(void) {
    /* Layout: '<', ' ', label[0..5] (6 chars), ' ', '>'  -> 10 tiles. */
    const char *label = s_diff_labels[game_difficulty()];
    u8 i;
    set_bkg_tile_xy(DIFF_COL + 0, DIFF_ROW, (u8)'<');
    set_bkg_tile_xy(DIFF_COL + 1, DIFF_ROW, (u8)' ');
    for (i = 0; i < 6; i++) {
        set_bkg_tile_xy(DIFF_COL + 2 + i, DIFF_ROW, (u8)label[i]);
    }
    set_bkg_tile_xy(DIFF_COL + 8, DIFF_ROW, (u8)' ');
    set_bkg_tile_xy(DIFF_COL + 9, DIFF_ROW, (u8)'>');
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
    draw_prompt_now(true);
    draw_diff_now();
    s_diff_dirty = 0;
    DISPLAY_ON;
}

void title_update(void) {
    /* Iter-3 #20: edge-only LEFT/RIGHT cycle with wrap. Uses
     * `input_is_pressed` (NOT `input_is_repeat`) — the 3-state cycle
     * is too short for auto-repeat to feel right. See conventions
     * "Iter-3 #20 conventions" → title-selector input rule. */
    if (input_is_pressed(J_LEFT)) {
        game_set_difficulty(difficulty_cycle_left(game_difficulty()));
        s_diff_dirty = 1;
    } else if (input_is_pressed(J_RIGHT)) {
        game_set_difficulty(difficulty_cycle_right(game_difficulty()));
        s_diff_dirty = 1;
    }

    s_blink++;
    if ((s_blink % 30) == 0) {
        s_visible = !s_visible;
        s_dirty = true;
    }
}

void title_render(void) {
    /* VBlank BG-write budget is 16 writes/frame (see conventions
     * iter-2 + iter-3 #21). Servicing both dirty regions in the
     * same frame would emit 10 (selector) + 12 (prompt) = 22 writes
     * and corrupt tiles past the VBlank window on real DMG. Service
     * the user-visible selector first (responds to LEFT/RIGHT this
     * frame) and defer the blink to the next frame — the blink is
     * a 30-frame periodic toggle, so a 1-frame slip is invisible. */
    if (s_diff_dirty) {
        draw_diff_now();
        s_diff_dirty = 0;
        return;
    }
    if (s_dirty) {
        draw_prompt_now(s_visible);
        s_dirty = false;
    }
}

