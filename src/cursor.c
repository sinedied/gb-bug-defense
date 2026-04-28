#include "cursor.h"
#include "input.h"
#include "map.h"
#include "towers.h"
#include "assets.h"
#include <gb/gb.h>

static u8  s_tx, s_ty;          /* play-field-local */
static u8  s_blink;             /* frame counter */

void cursor_init(void) {
    s_tx = 1;
    s_ty = 0;
    s_blink = 0;
    move_sprite(OAM_CURSOR, 0, 0); /* hidden until first update */
}

bool cursor_on_valid_tile(void) {
    if (map_class_at(s_tx, s_ty) != TC_GROUND) return false;
    if (towers_at(s_tx, s_ty)) return false;
    return true;
}

static void try_move(i8 dx, i8 dy) {
    i8 nx = (i8)s_tx + dx;
    i8 ny = (i8)s_ty + dy;
    if (nx < 0 || nx >= PF_COLS) return;
    if (ny < 0 || ny >= PF_ROWS) return;
    s_tx = (u8)nx;
    s_ty = (u8)ny;
}

void cursor_update(void) {
    if (input_is_repeat(J_LEFT))  try_move(-1,  0);
    if (input_is_repeat(J_RIGHT)) try_move( 1,  0);
    if (input_is_repeat(J_UP))    try_move( 0, -1);
    if (input_is_repeat(J_DOWN))  try_move( 0,  1);

    s_blink++;

    bool valid = cursor_on_valid_tile();
    /* Valid: 1 Hz blink (period 60). Invalid: 2 Hz (period 30). */
    u8 phase = valid ? ((s_blink / 30) & 1) : ((s_blink / 15) & 1);

    u8 tile;
    if (valid) tile = phase ? SPR_CURSOR_B : SPR_CURSOR_A;
    else       tile = phase ? SPR_CURSOR_GB : SPR_CURSOR_GA;

    set_sprite_tile(OAM_CURSOR, tile);
    /* GB sprite coords: x = pixel + 8, y = pixel + 16 */
    u8 sx = s_tx * 8 + 8;
    u8 sy = (s_ty + HUD_ROWS) * 8 + 16;
    move_sprite(OAM_CURSOR, sx, sy);
}

u8 cursor_tx(void) { return s_tx; }
u8 cursor_ty(void) { return s_ty; }
