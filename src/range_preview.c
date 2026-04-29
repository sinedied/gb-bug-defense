#include "range_preview.h"
#include "range_calc.h"
#include "towers.h"
#include "assets.h"
#include <gb/gb.h>

/* Iter-4 #31: Tower range preview — 8 dot sprites on a circle showing
 * the attack radius of the tower under the cursor after a dwell period.
 *
 * OAM slots 1..8 (a subset of menu/pause shared range 1..16). These are
 * idle during normal gameplay. Mutual exclusion is enforced by:
 * - menu_open() and pause_open() call range_preview_hide()
 * - range_preview_update() is only called in the non-modal gameplay path */

static u8 s_dwell;       /* frame counter (0..RANGE_DWELL_FRAMES+) */
static u8 s_last_tx;     /* tile cursor was on last frame (0xFF = invalid) */
static u8 s_last_ty;     /* tile cursor was on last frame (0xFF = invalid) */
static u8 s_visible;     /* 1 if dots are currently shown */

void range_preview_init(void) {
    u8 i;
    s_dwell = 0;
    s_last_tx = 0xFF;
    s_last_ty = 0xFF;
    s_visible = 0;
    for (i = 0; i < OAM_RANGE_COUNT; i++) {
        move_sprite(OAM_RANGE_BASE + i, 0, 0);
    }
}

void range_preview_hide(void) {
    u8 i;
    for (i = 0; i < OAM_RANGE_COUNT; i++) {
        move_sprite(OAM_RANGE_BASE + i, 0, 0);
    }
    s_visible = 0;
    s_dwell = 0;
}

void range_preview_update(u8 tx, u8 ty) {
    i8 idx;

    /* Iter-5: if dots are already displayed and cursor hasn't moved,
     * there is nothing to update — positions are static. */
    if (tx == s_last_tx && ty == s_last_ty && s_visible) return;

    /* Step 1: check if cursor is on a tower. */
    idx = towers_index_at(tx, ty);
    if (idx < 0) {
        if (s_visible) range_preview_hide();
        s_dwell = 0;
        s_last_tx = tx;
        s_last_ty = ty;
        return;
    }

    /* Step 2: moved to a different tile? Reset dwell. */
    if (tx != s_last_tx || ty != s_last_ty) {
        if (s_visible) range_preview_hide();
        s_dwell = 0;
        s_last_tx = tx;
        s_last_ty = ty;
        return;
    }

    /* Step 3: increment dwell if below threshold. */
    if (s_dwell < RANGE_DWELL_FRAMES) {
        s_dwell++;
    }

    /* Step 4: show dots on threshold crossing. */
    if (s_dwell >= RANGE_DWELL_FRAMES && !s_visible) {
        u8 type = towers_get_type((u8)idx);
        u8 range_px = towers_stats(type)->range_px;
        u8 cx = tx * 8 + 4;
        u8 cy = (ty + HUD_ROWS) * 8 + 4;
        u8 i;
        range_dot_t dot;
        for (i = 0; i < OAM_RANGE_COUNT; i++) {
            dot = range_calc_dot(cx, cy, range_px, i);
            if (dot.x == 255 && dot.y == 255) {
                move_sprite(OAM_RANGE_BASE + i, 0, 0);
            } else {
                set_sprite_tile(OAM_RANGE_BASE + i, SPR_PROJ);
                move_sprite(OAM_RANGE_BASE + i, dot.x + 4, dot.y + 12);
            }
        }
        s_visible = 1;
    }

    /* Step 5: record last position (only reached if steps 1/2 did not return). */
    s_last_tx = tx;
    s_last_ty = ty;
}
