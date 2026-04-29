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

/* Iter-4 #25: BG save/restore state for menu background rectangle. */
static u8 s_bg_col;              /* screen tile column of BG rect left edge */
static u8 s_bg_row;              /* screen tile row of BG rect top edge */
static u8 s_menu_bg_buf[12];     /* saved BG tiles (6 cols × 2 rows, row-major) */
static u8 s_bg_save_pending;     /* 1 = save + clear on next render */
static u8 s_bg_restore_pending;  /* 1 = restore on next render */

/* Cell offsets within the 14-slot OAM range. */
#define CELL(row, col)  ((row) * 7 + (col))

/* Compute the pixel anchor (mx, my) for the menu overlay from s_tower_idx.
 * Used by both menu_open (for BG rect position) and menu_render (for sprites). */
static void compute_anchor(u8 *out_mx, u8 *out_my) {
    u8 tx_px = towers_tile_screen_x(s_tower_idx);
    u8 ty_px = towers_tile_screen_y(s_tower_idx);

    i16 mx_raw = (i16)tx_px - 24;
    if (mx_raw < 0) mx_raw = 0;
    if (mx_raw > 160 - 56) mx_raw = 160 - 56;
    *out_mx = (u8)mx_raw;

    u8 pf_row = (ty_px / 8) - HUD_ROWS;
    if (pf_row <= 1) *out_my = ty_px + 16;   /* below */
    else             *out_my = ty_px - 16;   /* above */
}

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
    /* Iter-4 #25: clear BG save/restore state. */
    s_bg_col = 0;
    s_bg_row = 0;
    s_bg_save_pending = 0;
    s_bg_restore_pending = 0;
    {
        u8 i;
        for (i = 0; i < 12; i++) s_menu_bg_buf[i] = 0;
    }
    hide_menu_oam();
}

void menu_open(u8 idx) {
    u8 mx, my;
    s_open = true;
    s_tower_idx = idx;
    s_sel = 0;
    /* Iter-4 #25: compute BG rect position and arm save+clear. */
    compute_anchor(&mx, &my);
    s_bg_col = (mx / 8) + 1;   /* skip cursor column */
    s_bg_row = my / 8;
    s_bg_save_pending = 1;
    enemies_hide_all();
    projectiles_hide_all();
    cursor_blink_pause(true);
    set_sprite_tile(OAM_CURSOR, SPR_CURSOR_A);
}

void menu_close(void) {
    s_open = false;
    s_bg_restore_pending = 1;   /* iter-4 #25: arm BG restore */
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
            if (towers_get_level(s_tower_idx) >= 2) return;
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
    u8 mx, my;

    /* Iter-4 #25: deferred BG save+clear (fires once after menu_open). */
    if (s_bg_save_pending) {
        u8 r, c;
        for (r = 0; r < 2; r++) {
            for (c = 0; c < 6; c++) {
                s_menu_bg_buf[r * 6 + c] = get_bkg_tile_xy(s_bg_col + c, s_bg_row + r);
                set_bkg_tile_xy(s_bg_col + c, s_bg_row + r, (u8)' ');
            }
        }
        s_bg_save_pending = 0;
    }

    /* Iter-4 #25: deferred BG restore (fires once after menu_close). */
    if (s_bg_restore_pending) {
        u8 r, c;
        for (r = 0; r < 2; r++) {
            for (c = 0; c < 6; c++) {
                set_bkg_tile_xy(s_bg_col + c, s_bg_row + r, s_menu_bg_buf[r * 6 + c]);
            }
        }
        s_bg_restore_pending = 0;
        return;   /* menu is closed — no sprite rendering */
    }

    if (!s_open) return;

    compute_anchor(&mx, &my);

    /* Row 0: > U P G : Nh Nl   (cursor only when s_sel == 0) */
    /* Row 1:   S E L : Nh Nl   (cursor only when s_sel == 1) */
    u8 type  = towers_get_type(s_tower_idx);
    u8 level = towers_get_level(s_tower_idx);
    const tower_stats_t *st = towers_stats(type);
    u8 upg_cost = level == 0 ? st->upgrade_cost : st->upgrade_cost_l2;
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
    if (level < 2) {
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
