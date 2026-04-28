#include "title.h"
#include "gfx.h"
#include "assets.h"
#include <gb/gb.h>

/* "PRESS START" prompt blink: toggle row 13 cols 4..15 (12 tiles) every
 * 30 frames between glyph indices and blank (tile 32 = space).
 *
 * Render/update split: title_update() only mutates state and flips a
 * dirty flag; title_render() does the actual BG writes during the
 * VBlank window scheduled by main(). */

#define PROMPT_ROW 13
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
    DISPLAY_ON;
}

void title_update(void) {
    s_blink++;
    if ((s_blink % 30) == 0) {
        s_visible = !s_visible;
        s_dirty = true;
    }
}

void title_render(void) {
    if (s_dirty) {
        draw_prompt_now(s_visible);
        s_dirty = false;
    }
}
