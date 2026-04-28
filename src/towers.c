#include "towers.h"
#include "map.h"
#include "economy.h"
#include "enemies.h"
#include "projectiles.h"
#include "audio.h"
#include "assets.h"
#include <gb/gb.h>
#include <string.h>

/* Towers render as BG tiles (TILE_TOWER / TILE_TOWER_2), not sprites — they're
 * stationary and tile-aligned, and rendering as BG side-steps the
 * 10-sprites/scanline limit. The actual BG write is deferred to towers_render()
 * so it lands in the VBlank window safely. */

typedef struct {
    u8 tx, ty;
    u8 cooldown;
    u8 alive;
    u8 dirty;       /* needs BG tile write next render */
    u8 type;        /* TOWER_AV | TOWER_FW */
    u8 level;       /* 0 | 1 */
    u8 spent;       /* cumulative energy spent (cost + any upgrade) */
} tower_t;

typedef struct { u8 tx, ty; } clear_tile_t;

static const tower_stats_t s_tower_stats[TOWER_TYPE_COUNT] = {
    /* AV */ {
        .cost = TOWER_AV_COST, .upgrade_cost = TOWER_AV_UPG_COST,
        .cooldown = TOWER_AV_COOLDOWN, .cooldown_l1 = TOWER_AV_COOLDOWN_L1,
        .damage = TOWER_AV_DAMAGE, .damage_l1 = TOWER_AV_DAMAGE_L1,
        .range_px = TOWER_AV_RANGE_PX,
        .bg_tile = TILE_TOWER, .hud_letter = 'A',
    },
    /* FW */ {
        .cost = TOWER_FW_COST, .upgrade_cost = TOWER_FW_UPG_COST,
        .cooldown = TOWER_FW_COOLDOWN, .cooldown_l1 = TOWER_FW_COOLDOWN_L1,
        .damage = TOWER_FW_DAMAGE, .damage_l1 = TOWER_FW_DAMAGE_L1,
        .range_px = TOWER_FW_RANGE_PX,
        .bg_tile = TILE_TOWER_2, .hud_letter = 'F',
    },
};

static tower_t      s_towers[MAX_TOWERS];
static u16          s_pending_mask;            /* bit i = clear pending */
static clear_tile_t s_pending_tiles[MAX_TOWERS];

const tower_stats_t *towers_stats(u8 type) {
    return &s_tower_stats[type];
}

void towers_init(void) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        s_towers[i].alive = 0;
        s_towers[i].dirty = 0;
        s_towers[i].type = TOWER_AV;
        s_towers[i].level = 0;
        s_towers[i].spent = 0;
    }
    s_pending_mask = 0;
    memset(s_pending_tiles, 0, sizeof s_pending_tiles);
}

bool towers_at(u8 tx, u8 ty) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (s_towers[i].alive && s_towers[i].tx == tx && s_towers[i].ty == ty)
            return true;
    }
    return false;
}

i8 towers_index_at(u8 tx, u8 ty) {
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (s_towers[i].alive && s_towers[i].tx == tx && s_towers[i].ty == ty)
            return (i8)i;
    }
    return -1;
}

bool towers_try_place(u8 tx, u8 ty, u8 type) {
    if (type >= TOWER_TYPE_COUNT) return false;
    if (map_class_at(tx, ty) != TC_GROUND) return false;
    if (towers_at(tx, ty)) return false;
    /* Find free slot first so we don't spend energy if pool is full. */
    u8 slot = MAX_TOWERS;
    u8 i;
    for (i = 0; i < MAX_TOWERS; i++) {
        if (!s_towers[i].alive) { slot = i; break; }
    }
    if (slot == MAX_TOWERS) return false;
    u8 cost = s_tower_stats[type].cost;
    if (!economy_try_spend(cost)) return false;

    /* If a sell-clear is pending for this exact tile, cancel it — the new
     * placement supersedes the clear. */
    for (i = 0; i < MAX_TOWERS; i++) {
        if ((s_pending_mask & (1u << i)) &&
            s_pending_tiles[i].tx == tx &&
            s_pending_tiles[i].ty == ty) {
            s_pending_mask &= (u16)~(1u << i);
        }
    }

    s_towers[slot].tx = tx;
    s_towers[slot].ty = ty;
    s_towers[slot].type = type;
    s_towers[slot].level = 0;
    s_towers[slot].spent = cost;
    s_towers[slot].cooldown = s_tower_stats[type].cooldown;
    s_towers[slot].alive = 1;
    s_towers[slot].dirty = 1;
    audio_play(SFX_TOWER_PLACE);
    return true;
}

u8 towers_get_type(u8 idx)  { return s_towers[idx].type; }
u8 towers_get_level(u8 idx) { return s_towers[idx].level; }
u8 towers_get_spent(u8 idx) { return s_towers[idx].spent; }

bool towers_can_upgrade(u8 idx) {
    if (!s_towers[idx].alive) return false;
    if (s_towers[idx].level != 0) return false;
    u8 cost = s_tower_stats[s_towers[idx].type].upgrade_cost;
    return economy_get_energy() >= cost;
}

bool towers_upgrade(u8 idx) {
    if (!towers_can_upgrade(idx)) return false;
    u8 cost = s_tower_stats[s_towers[idx].type].upgrade_cost;
    if (!economy_try_spend(cost)) return false;
    s_towers[idx].level = 1;
    s_towers[idx].spent += cost;
    /* Reset cooldown to L1 cadence so the change is immediately observable. */
    s_towers[idx].cooldown = s_tower_stats[s_towers[idx].type].cooldown_l1;
    audio_play(SFX_TOWER_PLACE);
    return true;
}

void towers_sell(u8 idx) {
    if (!s_towers[idx].alive) return;
    u8 refund = s_towers[idx].spent / 2;
    economy_award(refund);
    /* Schedule BG tile clear back to TILE_GROUND. */
    s_pending_tiles[idx].tx = s_towers[idx].tx;
    s_pending_tiles[idx].ty = s_towers[idx].ty;
    s_pending_mask |= (u16)(1u << idx);
    s_towers[idx].alive = 0;
    s_towers[idx].dirty = 0;
    audio_play(SFX_TOWER_PLACE);
}

void towers_render(void) {
    u8 i;
    /* Drain phase: at most 1 sell-clear per frame. */
    if (s_pending_mask) {
        for (i = 0; i < MAX_TOWERS; i++) {
            if (s_pending_mask & (1u << i)) {
                set_bkg_tile_xy(s_pending_tiles[i].tx,
                                s_pending_tiles[i].ty + HUD_ROWS,
                                (u8)TILE_GROUND);
                s_pending_mask &= (u16)~(1u << i);
                return;     /* place skipped this frame; ≤ 1 BG write/frame */
            }
        }
    }
    /* Place phase: at most 1 placement per frame. */
    for (i = 0; i < MAX_TOWERS; i++) {
        if (s_towers[i].alive && s_towers[i].dirty) {
            set_bkg_tile_xy(s_towers[i].tx,
                            s_towers[i].ty + HUD_ROWS,
                            s_tower_stats[s_towers[i].type].bg_tile);
            s_towers[i].dirty = 0;
            return;
        }
    }
}

/* Targeting: pick alive enemy with highest wp_idx within range; tie-break
 * lower array index (= scan order). Returns 0xFF if none. */
static u8 acquire_target(u8 cx_px, u8 cy_px, u16 range_sq) {
    u8 best = 0xFF;
    u8 best_wp = 0;
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies_alive(i)) continue;
        i16 dx = (i16)enemies_x_px(i) - (i16)cx_px;
        i16 dy = (i16)enemies_y_px(i) - (i16)cy_px;
        u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
        u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
        u16 d2 = adx * adx + ady * ady;
        if (d2 > range_sq) continue;
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
        const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
        u8 cx = s_towers[i].tx * 8 + 4;
        u8 cy = (s_towers[i].ty + HUD_ROWS) * 8 + 4;
        u16 range_sq = (u16)st->range_px * (u16)st->range_px;
        u8 t = acquire_target(cx, cy, range_sq);
        if (t == 0xFF) continue;
        u8 dmg = s_towers[i].level ? st->damage_l1 : st->damage;
        if (projectiles_fire(FIX8(cx), FIX8(cy), t, dmg)) {
            s_towers[i].cooldown = s_towers[i].level ? st->cooldown_l1 : st->cooldown;
        }
    }
}

u8 towers_tile_screen_x(u8 idx) { return s_towers[idx].tx * 8; }
u8 towers_tile_screen_y(u8 idx) { return (s_towers[idx].ty + HUD_ROWS) * 8; }
