#include "menu.h"
#include "input.h"
#include "towers.h"
#include "enemies.h"
#include "projectiles.h"
#include "cursor.h"
#include "audio.h"
#include "economy.h"
#include "assets.h"
#include <gb/gb.h>

/* Upgrade / sell menu — sprite overlay. See specs/iter2.md §5 and
 * specs/iter2-design.md §5.
 *
 * Layout (2 rows × 7 cols, 14 OAM slots starting at OAM_MENU_BASE):
 *   row 0:  > U P G : N N
 *   row 1:    S E L : N N
 *
 * The widget anchors near the selected tower; vertical flip rule per the
 * design spec keeps it on screen for top rows. The menu is modal — while
 * open, game.c gates enemy/projectile/wave updates and we hide their OAM
 * so frozen sprites don't share scanlines with menu glyphs. */

static bool s_open;
static u8   s_tower_idx;
static u8   s_sel;        /* 0 = UPG, 1 = SEL */

/* Cell offsets within the 14-slot OAM range. */
#define CELL(row, col)  ((row) * 7 + (col))

bool menu_is_open(void) { return s_open; }

static void hide_menu_oam(void) {
    u8 i;
    for (i = 0; i < OAM_MENU_COUNT; i++) {
        move_sprite(OAM_MENU_BASE + i, 0, 0);
    }
}

void menu_init(void) {
    s_open = false;
    s_tower_idx = 0;
    s_sel = 0;
    hide_menu_oam();
}

void menu_open(u8 idx) {
    s_open = true;
    s_tower_idx = idx;
    s_sel = 0;
    enemies_hide_all();
    projectiles_hide_all();
    cursor_blink_pause(true);
    /* cursor_update() is gated off while the menu is open, so the paused-
     * blink phase set above never reaches OAM. Force the steady on-phase
     * tile here so the cursor stays visible for the entire menu session
     * (otherwise it freezes at whatever blink phase it had on this frame
     * — half the time the transparent SPR_CURSOR_GB tile -> invisible). */
    set_sprite_tile(OAM_CURSOR, SPR_CURSOR_A);
}

void menu_close(void) {
    s_open = false;
    hide_menu_oam();
    cursor_blink_pause(false);
}

void menu_update(void) {
    if (!s_open) return;
    if (input_is_repeat(J_UP))   s_sel = 0;
    if (input_is_repeat(J_DOWN)) s_sel = 1;

    if (input_is_pressed(J_B)) {
        menu_close();
        return;
    }
    if (input_is_pressed(J_A)) {
        if (s_sel == 0) {
            /* UPG: silently ignore at max level. */
            if (towers_get_level(s_tower_idx) != 0) return;
            if (towers_upgrade(s_tower_idx)) {
                menu_close();
            }
            /* If upgrade failed (e.g. insufficient energy) menu stays open. */
            return;
        }
        /* SEL */
        towers_sell(s_tower_idx);
        menu_close();
        return;
    }
}

/* Map a 0..9 digit to its sprite-bank glyph tile. */
static u8 digit_glyph(u8 d) {
    switch (d) {
        case 0: return SPR_GLYPH_0;
        case 1: return SPR_GLYPH_1;
        case 2: return SPR_GLYPH_2;
        case 3: return SPR_GLYPH_3;
        case 4: return SPR_GLYPH_4;
        case 5: return SPR_GLYPH_5;
        case 6: return SPR_GLYPH_6;
        case 7: return SPR_GLYPH_7;
        case 8: return SPR_GLYPH_8;
        default: return SPR_GLYPH_9;
    }
}

static void place_cell(u8 cell, u8 anchor_x, u8 anchor_y, u8 tile) {
    u8 row = cell / 7;
    u8 col = cell % 7;
    /* GB sprite OAM origin: x = pixel + 8, y = pixel + 16. */
    u8 sx = anchor_x + col * 8 + 8;
    u8 sy = anchor_y + row * 8 + 16;
    set_sprite_tile(OAM_MENU_BASE + cell, tile);
    move_sprite(OAM_MENU_BASE + cell, sx, sy);
}

static void hide_cell(u8 cell) {
    move_sprite(OAM_MENU_BASE + cell, 0, 0);
}

void menu_render(void) {
    if (!s_open) return;

    /* Anchor: 56-px-wide widget centered on tower, clamped to screen.
     * Vertical: pf_row >= 2 -> float above tower; else float below. */
    u8 tx_px = towers_tile_screen_x(s_tower_idx);   /* tile top-left X */
    u8 ty_px = towers_tile_screen_y(s_tower_idx);   /* tile top-left Y */

    /* Centered: tx_px + 4 - 28 = tx_px - 24 */
    i16 mx_raw = (i16)tx_px - 24;
    if (mx_raw < 0) mx_raw = 0;
    if (mx_raw > 160 - 56) mx_raw = 160 - 56;
    u8 mx = (u8)mx_raw;

    /* The tower's play-field row drives vertical placement. The on-screen
     * tile-row equals (ty_px / 8); subtract HUD_ROWS to get pf-row. */
    u8 pf_row = (ty_px / 8) - HUD_ROWS;
    u8 my;
    if (pf_row <= 1) my = ty_px + 16;   /* below */
    else             my = ty_px - 16;   /* above */

    /* Row 0: > U P G : Nh Nl   (cursor only when s_sel == 0) */
    /* Row 1:   S E L : Nh Nl   (cursor only when s_sel == 1) */
    u8 type  = towers_get_type(s_tower_idx);
    u8 level = towers_get_level(s_tower_idx);
    const tower_stats_t *st = towers_stats(type);
    u8 upg_cost = st->upgrade_cost;
    u8 refund   = towers_get_spent(s_tower_idx) / 2;

    /* Cursor cells */
    if (s_sel == 0) { place_cell(CELL(0,0), mx, my, SPR_GLYPH_GT); hide_cell(CELL(1,0)); }
    else            { place_cell(CELL(1,0), mx, my, SPR_GLYPH_GT); hide_cell(CELL(0,0)); }

    /* Static glyphs row 0: U P G : */
    place_cell(CELL(0,1), mx, my, SPR_GLYPH_U);
    place_cell(CELL(0,2), mx, my, SPR_GLYPH_P);
    place_cell(CELL(0,3), mx, my, SPR_GLYPH_G);
    place_cell(CELL(0,4), mx, my, SPR_GLYPH_COLON);
    /* Digits row 0 */
    if (level == 0) {
        place_cell(CELL(0,5), mx, my, digit_glyph(upg_cost / 10));
        place_cell(CELL(0,6), mx, my, digit_glyph(upg_cost % 10));
    } else {
        /* Max level: show "--" */
        place_cell(CELL(0,5), mx, my, SPR_GLYPH_DASH);
        place_cell(CELL(0,6), mx, my, SPR_GLYPH_DASH);
    }

    /* Static glyphs row 1: S E L : */
    place_cell(CELL(1,1), mx, my, SPR_GLYPH_S);
    place_cell(CELL(1,2), mx, my, SPR_GLYPH_E);
    place_cell(CELL(1,3), mx, my, SPR_GLYPH_L);
    place_cell(CELL(1,4), mx, my, SPR_GLYPH_COLON);
    place_cell(CELL(1,5), mx, my, digit_glyph(refund / 10));
    place_cell(CELL(1,6), mx, my, digit_glyph(refund % 10));
}
