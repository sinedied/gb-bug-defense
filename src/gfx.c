#include "gfx.h"
#include "assets.h"
#include <gb/gb.h>

void gfx_init(void) {
    /* DMG palette: 0=white .. 3=black */
    BGP_REG  = 0xE4;
    OBP0_REG = 0xE4;
    OBP1_REG = 0xE4;

    /* Load BG font (tiles 0..127) and map tiles (tiles 128..). */
    set_bkg_data(0, 128, font_tiles);
    set_bkg_data(MAP_TILE_BASE, MAP_TILE_COUNT, map_tile_data);

    /* Sprite tiles */
    set_sprite_data(0, SPRITE_TILE_COUNT, sprite_tile_data);
}

void gfx_hide_all_sprites(void) {
    u8 i;
    for (i = 0; i < OAM_TOTAL; i++) {
        move_sprite(i, 0, 0);
    }
}

void gfx_clear_bg(void) {
    u8 row[20];
    u8 y;
    for (y = 0; y < 20; y++) row[y] = 32; /* space = blank glyph */
    for (y = 0; y < 18; y++) {
        set_bkg_tiles(0, y, 20, 1, row);
    }
}
