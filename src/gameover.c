#include "gameover.h"
#include "gfx.h"
#include "assets.h"
#include <gb/gb.h>

/* PRESS START prompt blink (existing). Iter-3 #19 adds two STATIC
 * lines painted once inside the DISPLAY_OFF bracket of gameover_enter:
 *   row 14 cols 4..15  → `SCORE: NNNNN`        (12 tiles)
 *   row 13 cols 2..16  → `NEW HIGH SCORE!`     (15 tiles, only if new_hi)
 * Neither line redraws after the initial paint — only the prompt blink
 * runs in gameover_render. */

#define PROMPT_ROW 15
#define PROMPT_COL  4
#define PROMPT_LEN 12

#define SCORE_ROW  14
#define SCORE_COL   4

#define BANNER_ROW 13
#define BANNER_COL  2

static u8   s_blink;
static bool s_visible;
static bool s_dirty;
static const char s_prompt[PROMPT_LEN + 1] = "PRESS START ";
/* `NEW HIGH SCORE!` = 15 chars, painted statically. */
static const char s_banner[16] = "NEW HIGH SCORE!";

static void draw_prompt_now(bool visible) {
    u8 i;
    for (i = 0; i < PROMPT_LEN; i++) {
        char c = visible ? s_prompt[i] : ' ';
        set_bkg_tile_xy(PROMPT_COL + i, PROMPT_ROW, (u8)c);
    }
}

static void draw_score_line(u16 v) {
    /* `SCORE: NNNNN` — 12 tiles. 5-digit zero-padded decimal. */
    u8 d[5];
    u8 i;
    d[4] = (u8)(v % 10u); v /= 10u;
    d[3] = (u8)(v % 10u); v /= 10u;
    d[2] = (u8)(v % 10u); v /= 10u;
    d[1] = (u8)(v % 10u); v /= 10u;
    d[0] = (u8)(v % 10u);
    set_bkg_tile_xy(SCORE_COL + 0, SCORE_ROW, (u8)'S');
    set_bkg_tile_xy(SCORE_COL + 1, SCORE_ROW, (u8)'C');
    set_bkg_tile_xy(SCORE_COL + 2, SCORE_ROW, (u8)'O');
    set_bkg_tile_xy(SCORE_COL + 3, SCORE_ROW, (u8)'R');
    set_bkg_tile_xy(SCORE_COL + 4, SCORE_ROW, (u8)'E');
    set_bkg_tile_xy(SCORE_COL + 5, SCORE_ROW, (u8)':');
    set_bkg_tile_xy(SCORE_COL + 6, SCORE_ROW, (u8)' ');
    for (i = 0; i < 5; i++) {
        set_bkg_tile_xy(SCORE_COL + 7 + i, SCORE_ROW, (u8)('0' + d[i]));
    }
}

static void draw_banner_line(void) {
    u8 i;
    for (i = 0; i < 15; i++) {
        set_bkg_tile_xy(BANNER_COL + i, BANNER_ROW, (u8)s_banner[i]);
    }
}

void gameover_enter(bool win, u16 final_score, bool new_hi) {
    gfx_hide_all_sprites();
    DISPLAY_OFF;
    set_bkg_tiles(0, 0, 20, 18, win ? win_tilemap : lose_tilemap);
    if (new_hi) draw_banner_line();   /* row 13 — above score */
    draw_score_line(final_score);     /* row 14 */
    s_blink = 0;
    s_visible = true;
    s_dirty = false;
    draw_prompt_now(true);            /* row 15 */
    DISPLAY_ON;
}

void gameover_update(void) {
    s_blink++;
    if ((s_blink % 30) == 0) {
        s_visible = !s_visible;
        s_dirty = true;
    }
}

void gameover_render(void) {
    if (s_dirty) {
        draw_prompt_now(s_visible);
        s_dirty = false;
    }
}
