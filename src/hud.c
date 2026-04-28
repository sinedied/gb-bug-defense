#include "hud.h"
#include "economy.h"
#include "waves.h"
#include <gb/gb.h>

/* HUD format on row 0:  HP:05  E:030  W:1/3  + 3 trailing blanks. */

static u8 s_dirty_hp, s_dirty_e, s_dirty_w;

static void put_char(u8 x, u8 y, char c) {
    set_bkg_tile_xy(x, y, (u8)c);
}

static void put_dec2(u8 x, u8 y, u8 v) {
    if (v > 99) v = 99;
    put_char(x,     y, '0' + (v / 10));
    put_char(x + 1, y, '0' + (v % 10));
}

static void put_dec3(u8 x, u8 y, u8 v) {
    /* v in 0..255; render as 3 digits */
    put_char(x,     y, '0' + (v / 100));
    put_char(x + 1, y, '0' + ((v / 10) % 10));
    put_char(x + 2, y, '0' + (v % 10));
}

void hud_init(void) {
    /* Static labels (cols 0..16 then 17..19 blank). */
    put_char(0,  0, 'H'); put_char(1,  0, 'P'); put_char(2,  0, ':');
    put_char(5,  0, ' '); put_char(11, 0, ' ');
    put_char(6,  0, 'E'); put_char(7,  0, ':');
    put_char(12, 0, 'W'); put_char(13, 0, ':');
    put_char(15, 0, '/');
    put_char(17, 0, ' ');
    put_char(18, 0, ' ');
    put_char(19, 0, ' ');

    s_dirty_hp = s_dirty_e = s_dirty_w = 1;
    hud_update();
}

void hud_mark_hp_dirty(void) { s_dirty_hp = 1; }
void hud_mark_e_dirty(void)  { s_dirty_e  = 1; }
void hud_mark_w_dirty(void)  { s_dirty_w  = 1; }

void hud_update(void) {
    if (s_dirty_hp) {
        put_dec2(3, 0, economy_get_hp());
        s_dirty_hp = 0;
    }
    if (s_dirty_e) {
        put_dec3(8, 0, economy_get_energy());
        s_dirty_e = 0;
    }
    if (s_dirty_w) {
        put_char(14, 0, '0' + waves_get_current());
        put_char(16, 0, '3');
        s_dirty_w = 0;
    }
}
