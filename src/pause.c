#include "pause.h"
#include "input.h"
#include "menu.h"
#include "enemies.h"
#include "projectiles.h"
#include "cursor.h"
#include "assets.h"
#include <gb/gb.h>

/* Iter-3 #22 — Pause menu (sprite overlay).
 *
 * Layout (3 rows × widget; 16 OAM slots in OAM_PAUSE_BASE..+15):
 *   row 0 (y=80):                P A U S E
 *   row 1 (y=88):           >    R E S U M E
 *   row 2 (y=96):                Q U I T
 *
 * The chevron (OAM_PAUSE_BASE+5) snaps between y=88 and y=96 on
 * UP/DOWN. RESUME is the default selection. A on RESUME closes; A on
 * QUIT raises the one-shot quit_requested flag for game.c to consume.
 * B and START close without quitting (resume).
 *
 * Anchor is FIXED at screen (48, 64) — no contextual placement.
 * Mutual exclusion with menu.c is enforced by game.c; pause_open() is
 * also a defensive no-op if menu_is_open() (review R1). */

static bool s_open;
static u8   s_sel;        /* 0 = RESUME, 1 = QUIT */
static bool s_quit_req;   /* one-shot quit flag */

/* Per-cell layout table (16 entries). x/y are GB OAM screen coords
 * (pixel + 8 / pixel + 16). Cell index 5 (the chevron) has its y
 * patched at render time based on s_sel. */
typedef struct {
    u8 x;
    u8 y;
    u8 tile;
} pause_cell_t;

static const pause_cell_t s_layout[OAM_PAUSE_COUNT] = {
    /*  0 P */ { 72,  80, SPR_GLYPH_P },
    /*  1 A */ { 80,  80, SPR_GLYPH_A },
    /*  2 U */ { 88,  80, SPR_GLYPH_U },
    /*  3 S */ { 96,  80, SPR_GLYPH_S },
    /*  4 E */ {104,  80, SPR_GLYPH_E },
    /*  5 > */ { 56,  88, SPR_GLYPH_GT },   /* y patched: 88 (RESUME) | 96 (QUIT) */
    /*  6 R */ { 72,  88, SPR_GLYPH_R },
    /*  7 E */ { 80,  88, SPR_GLYPH_E },
    /*  8 S */ { 88,  88, SPR_GLYPH_S },
    /*  9 U */ { 96,  88, SPR_GLYPH_U },
    /* 10 M */ {104,  88, SPR_GLYPH_M },
    /* 11 E */ {112,  88, SPR_GLYPH_E },
    /* 12 Q */ { 72,  96, SPR_GLYPH_Q },
    /* 13 U */ { 80,  96, SPR_GLYPH_U },
    /* 14 I */ { 88,  96, SPR_GLYPH_I },
    /* 15 T */ { 96,  96, SPR_GLYPH_T },
};

#define CHEVRON_CELL 5

bool pause_is_open(void) { return s_open; }

static void hide_pause_oam(void) {
    u8 i;
    for (i = 0; i < OAM_PAUSE_COUNT; i++) {
        move_sprite(OAM_PAUSE_BASE + i, 0, 0);
    }
}

void pause_init(void) {
    s_open = false;
    s_sel = 0;
    s_quit_req = false;
    hide_pause_oam();
}

void pause_open(void) {
    /* Defensive: caller (game.c) is responsible for mutual exclusion,
     * but if both modal handlers ever raced this prevents the overlap. */
    if (menu_is_open()) return;
    if (s_open) return;
    s_open = true;
    s_sel = 0;
    s_quit_req = false;
    enemies_hide_all();
    projectiles_hide_all();
    cursor_blink_pause(true);
    /* Hide the placement cursor entirely: it has no function while
     * paused and would burn an extra per-scanline sprite. */
    move_sprite(OAM_CURSOR, 0, 0);
}

void pause_close(void) {
    s_open = false;
    s_sel = 0;
    /* NOTE: do NOT clear s_quit_req here — game.c calls pause_close()
     * BEFORE consuming the flag via pause_quit_requested(). */
    hide_pause_oam();
    cursor_blink_pause(false);
}

void pause_update(void) {
    if (!s_open) return;

    /* Selection navigation (wraps; only 2 items so up/down are symmetric). */
    if (input_is_repeat(J_UP))   s_sel = (s_sel == 0) ? 1 : 0;
    if (input_is_repeat(J_DOWN)) s_sel = (s_sel == 1) ? 0 : 1;

    if (input_is_pressed(J_B) || input_is_pressed(J_START)) {
        pause_close();
        return;
    }
    if (input_is_pressed(J_A)) {
        if (s_sel == 0) {
            /* RESUME */
            pause_close();
            return;
        }
        /* QUIT — signal the caller; do NOT close here so game.c sees
         * `pause_is_open() == true` together with quit_requested and
         * can sequence pause_close → audio_reset → enter_title. */
        s_quit_req = true;
        return;
    }
}

void pause_render(void) {
    if (!s_open) return;
    u8 i;
    for (i = 0; i < OAM_PAUSE_COUNT; i++) {
        u8 y = s_layout[i].y;
        if (i == CHEVRON_CELL) {
            y = (s_sel == 0) ? 88 : 96;
        }
        set_sprite_tile(OAM_PAUSE_BASE + i, s_layout[i].tile);
        move_sprite(OAM_PAUSE_BASE + i, s_layout[i].x, y);
    }
}

bool pause_quit_requested(void) {
    if (s_quit_req) {
        s_quit_req = false;
        return true;
    }
    return false;
}
