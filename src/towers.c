#include "towers.h"
#include "towers_anim.h"
#include "map.h"
#include "economy.h"
#include "enemies.h"
#include "projectiles.h"
#include "audio.h"
#include "cursor.h"
#include "assets.h"
#include "game.h"
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
    u8 type;        /* TOWER_AV | TOWER_FW | TOWER_EMP */
    u8 level;       /* 0 | 1 | 2 */
    u8 spent;       /* cumulative energy spent (cost + any upgrade) */
    u8 idle_phase;  /* iter-3 #21: last-painted LED phase (0 or 1) */
} tower_t;

typedef struct { u8 tx, ty; } clear_tile_t;

static const tower_stats_t s_tower_stats[TOWER_TYPE_COUNT] = {
    /* AV */ {
        .cost = TOWER_AV_COST, .upgrade_cost = TOWER_AV_UPG_COST,
        .upgrade_cost_l2 = TOWER_AV_UPG_COST_L2,
        .cooldown = TOWER_AV_COOLDOWN, .cooldown_l1 = TOWER_AV_COOLDOWN_L1,
        .cooldown_l2 = TOWER_AV_COOLDOWN_L2,
        .damage = TOWER_AV_DAMAGE, .damage_l1 = TOWER_AV_DAMAGE_L1,
        .damage_l2 = TOWER_AV_DAMAGE_L2,
        .range_px = TOWER_AV_RANGE_PX,
        .bg_tile = TILE_TOWER, .bg_tile_alt = TILE_TOWER_B,
        .bg_tile_l1 = TILE_TOWER_L1, .bg_tile_alt_l1 = TILE_TOWER_L1_B,
        .bg_tile_l2 = TILE_TOWER_L2, .bg_tile_alt_l2 = TILE_TOWER_L2_B,
        .hud_letter = 'A', .kind = TKIND_DAMAGE,
        .stun_frames = 0, .stun_frames_l1 = 0, .stun_frames_l2 = 0,
    },
    /* FW */ {
        .cost = TOWER_FW_COST, .upgrade_cost = TOWER_FW_UPG_COST,
        .upgrade_cost_l2 = TOWER_FW_UPG_COST_L2,
        .cooldown = TOWER_FW_COOLDOWN, .cooldown_l1 = TOWER_FW_COOLDOWN_L1,
        .cooldown_l2 = TOWER_FW_COOLDOWN_L2,
        .damage = TOWER_FW_DAMAGE, .damage_l1 = TOWER_FW_DAMAGE_L1,
        .damage_l2 = TOWER_FW_DAMAGE_L2,
        .range_px = TOWER_FW_RANGE_PX,
        .bg_tile = TILE_TOWER_2, .bg_tile_alt = TILE_TOWER_2_B,
        .bg_tile_l1 = TILE_TOWER_2_L1, .bg_tile_alt_l1 = TILE_TOWER_2_L1_B,
        .bg_tile_l2 = TILE_TOWER_2_L2, .bg_tile_alt_l2 = TILE_TOWER_2_L2_B,
        .hud_letter = 'F', .kind = TKIND_DAMAGE,
        .stun_frames = 0, .stun_frames_l1 = 0, .stun_frames_l2 = 0,
    },
    /* EMP */ {
        .cost = TOWER_EMP_COST, .upgrade_cost = TOWER_EMP_UPG_COST,
        .upgrade_cost_l2 = TOWER_EMP_UPG_COST_L2,
        .cooldown = TOWER_EMP_COOLDOWN, .cooldown_l1 = TOWER_EMP_COOLDOWN_L1,
        .cooldown_l2 = TOWER_EMP_COOLDOWN_L2,
        .damage = 0, .damage_l1 = 0, .damage_l2 = 0,
        .range_px = TOWER_EMP_RANGE_PX,
        .bg_tile = TILE_TOWER_3, .bg_tile_alt = TILE_TOWER_3_B,
        .bg_tile_l1 = TILE_TOWER_3_L1, .bg_tile_alt_l1 = TILE_TOWER_3_L1_B,
        .bg_tile_l2 = TILE_TOWER_3_L2, .bg_tile_alt_l2 = TILE_TOWER_3_L2_B,
        .hud_letter = 'E', .kind = TKIND_STUN,
        .stun_frames = TOWER_EMP_STUN, .stun_frames_l1 = TOWER_EMP_STUN_L1,
        .stun_frames_l2 = TOWER_EMP_STUN_L2,
    },
};

static tower_t      s_towers[MAX_TOWERS];
static u16          s_pending_mask;            /* bit i = clear pending */
static clear_tile_t s_pending_tiles[MAX_TOWERS];
static u8           s_idle_scan_idx;           /* iter-3 #21: round-robin cursor */

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
        s_towers[i].idle_phase = 0;
    }
    s_pending_mask = 0;
    s_idle_scan_idx = 0;
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
    /* Iter-3 #18 F3: freshly-placed EMP starts with cooldown=1 (not 0).
     * Reason: SFX_TOWER_PLACE plays this frame on CH1 (prio=2). If the EMP
     * also fires same-frame, SFX_EMP_FIRE (also CH1, prio=2) preempts it
     * via the equal-priority rule, suppressing placement audio. Deferring
     * the first pulse by 1 frame (~16 ms, imperceptible) lets the place
     * SFX play cleanly while preserving "stuns near-immediately on placement". */
    if (s_tower_stats[type].kind == TKIND_STUN)
        s_towers[slot].cooldown = 1;
    s_towers[slot].alive = 1;
    s_towers[slot].dirty = 1;
    s_towers[slot].idle_phase = 0;   /* matches the about-to-be-painted base tile */
    audio_play(SFX_TOWER_PLACE);
    cursor_invalidate_cache();       /* iter-5: tile is now occupied */
    return true;
}

u8 towers_get_type(u8 idx)  { return s_towers[idx].type; }
u8 towers_get_level(u8 idx) { return s_towers[idx].level; }
u8 towers_get_spent(u8 idx) { return s_towers[idx].spent; }

bool towers_can_upgrade(u8 idx) {
    if (!s_towers[idx].alive) return false;
    if (s_towers[idx].level >= 2) return false;
    const tower_stats_t *st = &s_tower_stats[s_towers[idx].type];
    u8 cost = s_towers[idx].level == 0 ? st->upgrade_cost : st->upgrade_cost_l2;
    return economy_get_energy() >= cost;
}

bool towers_upgrade(u8 idx) {
    if (!towers_can_upgrade(idx)) return false;
    const tower_stats_t *st = s_tower_stats + s_towers[idx].type;
    u8 cost = s_towers[idx].level == 0 ? st->upgrade_cost : st->upgrade_cost_l2;
    if (!economy_try_spend(cost)) return false;
    s_towers[idx].level++;
    s_towers[idx].spent += cost;
    s_towers[idx].dirty = 1;         /* iter-4 #26/#27: schedule tile repaint */
    s_towers[idx].idle_phase = 0;    /* matches the about-to-be-painted base tile */
    /* Iter-3 #18 F2 + iter-4 #27: only TKIND_DAMAGE resets cooldown.
     * EMP (TKIND_STUN) preserves its current cooldown across all upgrades
     * (L0→L1 and L1→L2). Resetting on upgrade forces an idle EMP at
     * cooldown=0 into a full dead-window (up to 100 frames at L2),
     * violating D-IT3-18-7 (cooldown set only on successful fire).
     * The L2 cadence (100) differs from L1 (120); the invariant applies
     * regardless of cadence equality. */
    if (st->kind == TKIND_DAMAGE) {
        s_towers[idx].cooldown = s_towers[idx].level == 1
                                 ? st->cooldown_l1 : st->cooldown_l2;
    }
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
    cursor_invalidate_cache();       /* iter-5: tile is now empty */
}

void towers_render(void) {
    u8 i;
    /* Phase 1: at most 1 sell-clear per frame. */
    if (s_pending_mask) {
        for (i = 0; i < MAX_TOWERS; i++) {
            if (s_pending_mask & (1u << i)) {
                set_bkg_tile_xy(s_pending_tiles[i].tx,
                                s_pending_tiles[i].ty + HUD_ROWS,
                                (u8)TILE_GROUND);
                s_pending_mask &= (u16)~(1u << i);
                return;     /* place + idle skipped this frame */
            }
        }
    }
    /* Phase 2: at most 1 placement per frame. */
    for (i = 0; i < MAX_TOWERS; i++) {
        if (s_towers[i].alive && s_towers[i].dirty) {
            const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
            u8 lvl = s_towers[i].level;
            set_bkg_tile_xy(s_towers[i].tx,
                            s_towers[i].ty + HUD_ROWS,
                            lvl >= 2 ? st->bg_tile_l2 : lvl ? st->bg_tile_l1 : st->bg_tile);
            s_towers[i].dirty = 0;
            return;         /* idle skipped this frame */
        }
    }
    /* Phase 3 (iter-3 #21): at most 1 idle-LED toggle per frame.
     * Gated on !game_is_modal_open() so neither pause nor the
     * upgrade/sell menu sees surprise BG writes; consistent with the
     * "no BG writes during modal" convention. Round-robin through the
     * tower pool from the last-touched slot so heavy bursts (16
     * towers all transitioning at the same tick) drain fairly within
     * ~16 frames — invisible at the 32-frame LED half-period. */
    if (game_is_modal_open()) return;
    u8 cur_frame = game_anim_frame();
    u8 k;
    for (k = 0; k < MAX_TOWERS; k++) {
        i = (u8)((s_idle_scan_idx + k) & (MAX_TOWERS - 1)); /* MAX_TOWERS=16 */
        if (!s_towers[i].alive) continue;
        u8 want = towers_idle_phase_for(cur_frame, i);
        if (want == s_towers[i].idle_phase) continue;
        const tower_stats_t *st = &s_tower_stats[s_towers[i].type];
        u8 lvl = s_towers[i].level;
        u8 base = lvl >= 2 ? st->bg_tile_l2     : lvl ? st->bg_tile_l1     : st->bg_tile;
        u8 alt  = lvl >= 2 ? st->bg_tile_alt_l2 : lvl ? st->bg_tile_alt_l1 : st->bg_tile_alt;
        set_bkg_tile_xy(s_towers[i].tx,
                        s_towers[i].ty + HUD_ROWS,
                        want ? alt : base);
        s_towers[i].idle_phase = want;
        s_idle_scan_idx = (u8)((i + 1) & (MAX_TOWERS - 1));
        return;
    }
}

/* Targeting: pick alive enemy with highest wp_idx within range; tie-break
 * lower array index (= scan order). Returns 0xFF if none. */
static u8 acquire_target(u8 cx_px, u8 cy_px, u16 range_sq, u8 range_px) {
    u8 best = 0xFF;
    u8 best_wp = 0;
    u8 i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        if (!enemies_alive(i)) continue;
        i16 dx = (i16)enemies_x_px(i) - (i16)cx_px;
        i16 dy = (i16)enemies_y_px(i) - (i16)cy_px;
        u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
        u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
        /* Iter-5: cheap axis reject before 16-bit multiply. */
        if (adx > (u16)range_px || ady > (u16)range_px) continue;
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

        if (st->kind == TKIND_STUN) {
            /* Iter-3 #18 F1: 3-outcome cooldown rule prevents overlapping-EMP
             * perma-freeze. (a) any_stunned → reset cooldown + SFX.
             * (b) found_target but all already stunned by another EMP →
             * burn cooldown silently (no SFX). Without this, a 2nd EMP
             * polling at cooldown=0 every frame would always grab the
             * unstun frame on its rival's expiry and chain-lock the
             * enemy forever (towers_update runs before enemies_update).
             * (c) no targets in range → keep cooldown=0, retry next frame. */
            bool any_stunned = false;
            bool found_target = false;
            u8 lvl = s_towers[i].level;
            u8 stun_dur = lvl >= 2 ? st->stun_frames_l2
                        : lvl      ? st->stun_frames_l1 : st->stun_frames;
            u8 j;
            for (j = 0; j < MAX_ENEMIES; j++) {
                if (!enemies_alive(j)) continue;
                i16 dx = (i16)enemies_x_px(j) - (i16)cx;
                i16 dy = (i16)enemies_y_px(j) - (i16)cy;
                u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
                u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
                /* Iter-5: cheap axis reject before 16-bit multiply. */
                if (adx > (u16)st->range_px || ady > (u16)st->range_px) continue;
                u16 d2 = adx * adx + ady * ady;
                if (d2 > range_sq) continue;
                found_target = true;
                if (enemies_try_stun(j, stun_dur))
                    any_stunned = true;
            }
            if (any_stunned) {
                audio_play(SFX_EMP_FIRE);
                s_towers[i].cooldown = lvl >= 2 ? st->cooldown_l2
                                     : lvl      ? st->cooldown_l1 : st->cooldown;
            } else if (found_target) {
                /* All targets already stunned — burn cooldown, no SFX. */
                s_towers[i].cooldown = lvl >= 2 ? st->cooldown_l2
                                     : lvl      ? st->cooldown_l1 : st->cooldown;
            }
            /* else: empty range — keep cooldown=0, re-scan next frame. */
        } else {
            /* TKIND_DAMAGE: acquire nearest, fire projectile. */
            u8 t = acquire_target(cx, cy, range_sq, st->range_px);
            if (t == 0xFF) continue;
            u8 lvl = s_towers[i].level;
            u8 dmg = lvl >= 2 ? st->damage_l2 : lvl ? st->damage_l1 : st->damage;
            if (projectiles_fire(FIX8(cx), FIX8(cy), t, dmg)) {
                s_towers[i].cooldown = lvl >= 2 ? st->cooldown_l2
                                     : lvl      ? st->cooldown_l1 : st->cooldown;
            }
        }
    }
}

u8 towers_tile_screen_x(u8 idx) { return s_towers[idx].tx * 8; }
u8 towers_tile_screen_y(u8 idx) { return (s_towers[idx].ty + HUD_ROWS) * 8; }
