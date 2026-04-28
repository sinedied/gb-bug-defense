#include "gameover.h"
#include "gfx.h"
#include "assets.h"
#include <gb/gb.h>

#define PROMPT_ROW 15
#define PROMPT_COL  4
#define PROMPT_LEN 12

static u8   s_blink;
static bool s_visible;
static bool s_dirty;
static const char s_prompt[PROMPT_LEN + 1] = "PRESS START ";

static void draw_prompt_now(bool visible) {
    u8 i;
    for (i = 0; i < PROMPT_LEN; i++) {
        char c = visible ? s_prompt[i] : ' ';
        set_bkg_tile_xy(PROMPT_COL + i, PROMPT_ROW, (u8)c);
    }
}

void gameover_enter(bool win) {
    gfx_hide_all_sprites();
    DISPLAY_OFF;
    set_bkg_tiles(0, 0, 20, 18, win ? win_tilemap : lose_tilemap);
    s_blink = 0;
    s_visible = true;
    s_dirty = false;
    draw_prompt_now(true);
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
