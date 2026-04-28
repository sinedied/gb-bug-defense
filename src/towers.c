#include "towers.h"
#include "map.h"
#include "economy.h"
#include "enemies.h"
#include "projectiles.h"
#include "assets.h"
#include <gb/gb.h>

/* Towers render as BG tiles (TILE_TOWER), not sprites — they're stationary
 * and tile-aligned, and rendering as BG side-steps the 10-sprites/scanline
 * limit. The actual BG write is deferred to towers_render() so it lands in
 * the VBlank window safely. */

typedef struct {
    u8 tx, ty;
    u8 cooldown;
    u8 alive;
    u8 dirty;       /* needs BG tile write next render */
} tower_t;

static tower_t s_towers[MAX_TOWERS];

void towers_init(void) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        s_towers[i].alive = 0;
        s_towers[i].dirty = 0;
        /* OAM slots OAM_TOWERS_BASE..OAM_TOWERS_BASE+15 are now reserved
         * unused (see memory/conventions.md). gfx_hide_all_sprites() at
         * boot/state-change still hides them defensively. */
    }
}

bool towers_at(u8 tx, u8 ty) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (s_towers[i].alive && s_towers[i].tx == tx && s_towers[i].ty == ty)
            return true;
    }
    return false;
}

bool towers_try_place(u8 tx, u8 ty) {
    if (map_class_at(tx, ty) != TC_GROUND) return false;
    if (towers_at(tx, ty)) return false;
    /* Find free slot first so we don't spend energy if pool is full. */
    u8 slot = MAX_TOWERS;
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (!s_towers[i].alive) { slot = i; break; }
    }
    if (slot == MAX_TOWERS) return false;
    if (!economy_try_spend(TOWER_COST)) return false;

    s_towers[slot].tx = tx;
    s_towers[slot].ty = ty;
    s_towers[slot].cooldown = TOWER_COOLDOWN;
    s_towers[slot].alive = 1;
    s_towers[slot].dirty = 1;       /* BG write happens in towers_render */
    return true;
}

void towers_render(void) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (s_towers[i].alive && s_towers[i].dirty) {
            set_bkg_tile_xy(s_towers[i].tx,
                            s_towers[i].ty + HUD_ROWS,
                            (u8)TILE_TOWER);
            s_towers[i].dirty = 0;
        }
    }
}

/* Targeting: pick alive enemy with highest wp_idx within range; tie-break
 * lower array index (= scan order). Returns 0xFF if none. */
static u8 acquire_target(u8 cx_px, u8 cy_px) {
    u8 best = 0xFF;
    u8 best_wp = 0;
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies_alive(i)) continue;
        i16 dx = (i16)enemies_x_px(i) - (i16)cx_px;
        i16 dy = (i16)enemies_y_px(i) - (i16)cy_px;
        /* Unsigned squared distance: i16 product overflows past ~181px. */
        u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
        u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
        u16 d2 = adx * adx + ady * ady;
        if (d2 > (u16)TOWER_RANGE_SQ) continue;
        u8 wp = enemies_wp_idx(i);
        if (best == 0xFF || wp > best_wp) {
            best = i;
            best_wp = wp;
        }
    }
    return best;
}

void towers_update(void) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (!s_towers[i].alive) continue;
        if (s_towers[i].cooldown) {
            s_towers[i].cooldown--;
            continue;
        }
        u8 cx = s_towers[i].tx * 8 + 4;
        u8 cy = (s_towers[i].ty + HUD_ROWS) * 8 + 4;
        u8 t = acquire_target(cx, cy);
        if (t == 0xFF) continue;
        if (projectiles_fire(FIX8(cx), FIX8(cy), t)) {
            s_towers[i].cooldown = TOWER_COOLDOWN;
        }
    }
}
