#include "hud.h"
#include "economy.h"
#include "waves.h"
#include "towers.h"
#include "game.h"
#include <gb/gb.h>

/* Iter-2 HUD layout (20 chars on row 0):
 *   0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
 *   H P : N _ E : N N N _  W  :  N  N  /  N  N  _  X
 * X = 'A' or 'F' (selected tower). */

static u8 s_dirty_hp, s_dirty_e, s_dirty_w, s_dirty_t;

static void put_char(u8 x, u8 y, char c) {
    set_bkg_tile_xy(x, y, (u8)c);
}

static void put_dec1(u8 x, u8 y, u8 v) {
    if (v > 9) v = 9;
    put_char(x, y, '0' + v);
}

static void put_dec2_zero(u8 x, u8 y, u8 v) {
    if (v > 99) v = 99;
    put_char(x,     y, '0' + (v / 10));
    put_char(x + 1, y, '0' + (v % 10));
}

static void put_dec3(u8 x, u8 y, u8 v) {
    put_char(x,     y, '0' + (v / 100));
    put_char(x + 1, y, '0' + ((v / 10) % 10));
    put_char(x + 2, y, '0' + (v % 10));
}

void hud_init(void) {
    /* Static labels. */
    put_char(0, 0, 'H'); put_char(1, 0, 'P'); put_char(2, 0, ':');
    put_char(4, 0, ' ');
    put_char(5, 0, 'E'); put_char(6, 0, ':');
    put_char(10, 0, ' ');
    put_char(11, 0, 'W'); put_char(12, 0, ':');
    put_char(15, 0, '/');
    put_char(18, 0, ' ');

    s_dirty_hp = s_dirty_e = s_dirty_w = s_dirty_t = 1;
    hud_update();
}

void hud_mark_hp_dirty(void) { s_dirty_hp = 1; }
void hud_mark_e_dirty(void)  { s_dirty_e  = 1; }
void hud_mark_w_dirty(void)  { s_dirty_w  = 1; }
void hud_mark_t_dirty(void)  { s_dirty_t  = 1; }

void hud_update(void) {
    if (s_dirty_hp) {
        put_dec1(3, 0, economy_get_hp());
        s_dirty_hp = 0;
    }
    if (s_dirty_e) {
        put_dec3(7, 0, economy_get_energy());
        s_dirty_e = 0;
    }
    if (s_dirty_w) {
        put_dec2_zero(13, 0, waves_get_current());
        put_dec2_zero(16, 0, waves_get_total());
        s_dirty_w = 0;
    }
    if (s_dirty_t) {
        u8 type = game_get_selected_tower_type();
        const tower_stats_t *st = towers_stats(type);
        put_char(19, 0, (char)st->hud_letter);
        s_dirty_t = 0;
    }
}
