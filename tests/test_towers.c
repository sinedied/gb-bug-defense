/*
 * Host-side tests for iter-3 #18 EMP tower logic: kind discriminator,
 * AoE stun scan, cooldown-on-success, freshly-placed cooldown=0.
 *
 * Compiles src/towers.c against host stubs; records enemies_try_stun
 * calls to verify tower behavior without linking enemies.c.
 *
 * Compile (via justfile `test` recipe):
 *   cc -std=c99 -Wall -Wextra -O2 -Itests/stubs -Isrc -Ires \
 *      tests/test_towers.c src/towers.c -o build/test_towers
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "gtypes.h"
#include "towers.h"
#include "audio.h"
#include "assets.h"

/* ---------------------------------------------------------------- */
/* Stubs                                                             */
/* ---------------------------------------------------------------- */

/* gb/gb.h stubs */
void move_sprite(unsigned char nb, unsigned char x, unsigned char y) {
    (void)nb; (void)x; (void)y;
}
void set_sprite_tile(unsigned char nb, unsigned char tile) {
    (void)nb; (void)tile;
}

static u8 s_bkg_tiles[20][18];
void set_bkg_tile_xy(unsigned char x, unsigned char y, unsigned char tile) {
    if (x < 20 && y < 18) s_bkg_tiles[x][y] = tile;
}

/* map.h stubs */
#include "map.h"
u8 map_class_at(u8 tx, u8 ty) {
    (void)tx; (void)ty;
    return TC_GROUND;  /* all tiles buildable */
}

/* economy.h stubs */
static u8 s_energy = 255;
u8 economy_get_energy(void) { return s_energy; }
bool economy_try_spend(u8 cost) {
    if (cost > s_energy) return false;
    s_energy -= cost;
    return true;
}
void economy_award(u8 amount) { s_energy += amount; }

/* enemies.h stubs — record try_stun calls */
#define MAX_STUB_ENEMIES 14
typedef struct {
    u8 alive;
    u8 x, y;
    u8 wp_idx;
    u8 gen;
    u8 bounty;
    u8 stunned;
} stub_enemy_t;
static stub_enemy_t s_stub_enemies[MAX_STUB_ENEMIES];

/* Record of try_stun calls */
#define MAX_STUN_CALLS 64
typedef struct { u8 idx; u8 frames; bool result; } stun_call_t;
static stun_call_t s_stun_calls[MAX_STUN_CALLS];
static int s_stun_call_count;

bool enemies_alive(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].alive : false;
}
u8 enemies_x_px(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].x : 0;
}
u8 enemies_y_px(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].y : 0;
}
u8 enemies_wp_idx(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].wp_idx : 0;
}
u8 enemies_gen(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].gen : 0;
}
u8 enemies_bounty(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].bounty : 0;
}
bool enemies_try_stun(u8 idx, u8 frames) {
    bool result = false;
    if (idx < MAX_STUB_ENEMIES && s_stub_enemies[idx].alive &&
        !s_stub_enemies[idx].stunned) {
        s_stub_enemies[idx].stunned = 1;
        result = true;
    }
    if (s_stun_call_count < MAX_STUN_CALLS) {
        s_stun_calls[s_stun_call_count].idx = idx;
        s_stun_calls[s_stun_call_count].frames = frames;
        s_stun_calls[s_stun_call_count].result = result;
        s_stun_call_count++;
    }
    return result;
}
bool enemies_is_stunned(u8 idx) {
    return (idx < MAX_STUB_ENEMIES) ? s_stub_enemies[idx].stunned : false;
}
bool enemies_apply_damage(u8 idx, u8 dmg) {
    (void)idx; (void)dmg; return false;
}
void enemies_set_flash(u8 idx) { (void)idx; }

/* projectiles.h stubs */
static u8 s_last_proj_damage;
static int s_proj_fire_count;

bool projectiles_fire(fix8 x, fix8 y, u8 target, u8 damage) {
    (void)x; (void)y; (void)target;
    s_last_proj_damage = damage;
    s_proj_fire_count++;
    return true;
}

/* audio.h stubs — record SFX plays */
static u8 s_last_sfx;
static int s_sfx_count;
void audio_play(u8 sfx_id) { s_last_sfx = sfx_id; s_sfx_count++; }

/* game.h stubs */
u8 game_anim_frame(void) { return 0; }
bool game_is_modal_open(void) { return false; }

/* cursor.h stubs — iter-5: towers.c now calls cursor_invalidate_cache(). */
void cursor_blink_pause(bool paused) { (void)paused; }
void cursor_invalidate_cache(void) {}

/* ---------------------------------------------------------------- */
/* Test harness                                                      */
/* ---------------------------------------------------------------- */
static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)
#define CHECK_EQ(a, b) do { \
    unsigned _a = (unsigned)(a), _b = (unsigned)(b); \
    if (_a != _b) { \
        fprintf(stderr, "FAIL %s:%d: %s == %s (got %u, expected %u)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        failures++; \
    } \
} while (0)

static void reset_all(void) {
    memset(s_stub_enemies, 0, sizeof s_stub_enemies);
    memset(s_stun_calls, 0, sizeof s_stun_calls);
    s_stun_call_count = 0;
    s_sfx_count = 0;
    s_last_sfx = 0;
    s_proj_fire_count = 0;
    s_last_proj_damage = 0;
    s_energy = 255;
    towers_init();
}

/* Helper: place a stub enemy at given pixel position. */
static void place_enemy(u8 idx, u8 x, u8 y) {
    s_stub_enemies[idx].alive = 1;
    s_stub_enemies[idx].x = x;
    s_stub_enemies[idx].y = y;
    s_stub_enemies[idx].gen = 1;
    s_stub_enemies[idx].bounty = 3;
    s_stub_enemies[idx].stunned = 0;
}

/* ---------------------------------------------------------------- */
/* Tests                                                             */
/* ---------------------------------------------------------------- */

/* T1: freshly-placed EMP defers first pulse by 1 frame (F3), then fires. */
static void test_emp_freshly_placed_cooldown_zero(void) {
    reset_all();
    /* Place EMP at tile (5, 5). Center pixel = 5*8+4 = 44, (5+1)*8+4 = 52. */
    CHECK(towers_try_place(5, 5, TOWER_EMP));

    /* Place enemy within 16 px of tower center (44, 52). */
    place_enemy(0, 44, 52);

    /* Frame 0: cooldown 1 → 0, no fire (F3 — avoids same-frame SFX collision). */
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();
    CHECK_EQ(s_stun_call_count, 0);

    /* Frame 1: cooldown=0, EMP fires. */
    towers_update();
    CHECK(s_stun_call_count > 0);
    int found = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].idx == 0 && s_stun_calls[k].result)
            found = 1;
    }
    CHECK(found);
    CHECK_EQ(s_last_sfx, SFX_EMP_FIRE);
}

/* T2: EMP scans all alive enemies in range. */
static void test_emp_scan_all_in_range(void) {
    reset_all();
    /* EMP at tile (5, 5): center = (44, 52). Range = 16 px. */
    CHECK(towers_try_place(5, 5, TOWER_EMP));

    /* Place 3 enemies: 2 in range, 1 out. */
    place_enemy(0, 44, 52);   /* distance 0 — in range */
    place_enemy(1, 50, 52);   /* distance 6 — in range */
    place_enemy(2, 100, 52);  /* distance 56 — out of range */

    /* Warm-up: fresh-place cooldown=1 (F3), so first update only decrements. */
    towers_update();
    s_stun_call_count = 0;
    towers_update();

    /* Check that try_stun was called for enemies 0 and 1, but not 2
     * (or called with no success for 2). */
    int stun_0 = 0, stun_1 = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].idx == 0 && s_stun_calls[k].result) stun_0 = 1;
        if (s_stun_calls[k].idx == 1 && s_stun_calls[k].result) stun_1 = 1;
    }
    CHECK(stun_0);
    CHECK(stun_1);
    CHECK(s_sfx_count > 0);  /* SFX played once (at least one stun) */
}

/* T3: cooldown NOT reset when zero stuns succeeded. */
static void test_emp_cooldown_not_reset_on_empty_scan(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_EMP));

    /* No enemies in range. */
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();

    /* No stun calls should have returned true. */
    int any_success = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result) any_success = 1;
    }
    CHECK(!any_success);
    CHECK_EQ(s_sfx_count, 0);  /* no SFX on empty scan */

    /* Cooldown should still be 0 — place an enemy next frame. */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();

    /* Now it should stun. */
    int found = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result) found = 1;
    }
    CHECK(found);
    CHECK_EQ(s_last_sfx, SFX_EMP_FIRE);
}

/* T4: cooldown IS reset to 120 after a successful pulse. */
static void test_emp_cooldown_reset_on_success(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    place_enemy(0, 44, 52);

    /* Warm-up frame to drain F3 fresh-place cooldown=1. */
    towers_update();
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();  /* should stun, set cooldown = 120 */
    CHECK(s_sfx_count > 0);

    /* Clear the enemy's stunned flag so the next try_stun would succeed
     * if cooldown were 0. */
    s_stub_enemies[0].stunned = 0;
    s_stun_call_count = 0;
    s_sfx_count = 0;

    /* Run 120 updates — cooldown counts from 120 down to 0. */
    for (int f = 0; f < 120; f++) {
        towers_update();
    }
    CHECK_EQ(s_stun_call_count, 0);  /* just reached 0, fires on NEXT tick */

    /* 121st update — cooldown is now 0, tower scans and fires. */
    towers_update();
    CHECK(s_stun_call_count > 0);
}

/* T5: EMP stun uses level 1 duration when upgraded. */
static void test_emp_upgrade_stun_duration(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);

    /* Upgrade to level 1. */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 1);

    /* Place enemy in range. */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;

    /* Need to wait for cooldown to expire after upgrade.
     * towers_upgrade sets cooldown to cooldown_l1 = 120. Takes 120 ticks
     * to decrement to 0, then one more tick to actually fire. */
    for (int f = 0; f < 121; f++) towers_update();

    CHECK(s_stun_call_count > 0);
    /* Check that the stun duration used was TOWER_EMP_STUN_L1 (90). */
    int found_l1 = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result && s_stun_calls[k].frames == TOWER_EMP_STUN_L1)
            found_l1 = 1;
    }
    CHECK(found_l1);
}

/* T6: SFX queued only on success, not on empty scan. */
static void test_emp_sfx_only_on_success(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_EMP));

    /* Scan with no enemies. */
    s_sfx_count = 0;
    towers_update();
    CHECK_EQ(s_sfx_count, 0);

    /* Scan with enemy in range. */
    place_enemy(0, 44, 52);
    towers_update();
    CHECK(s_sfx_count > 0);
}

/* T7: tower stats have correct kind and bg_tile_alt. */
static void test_tower_stats_kind(void) {
    const tower_stats_t *av = towers_stats(TOWER_AV);
    const tower_stats_t *fw = towers_stats(TOWER_FW);
    const tower_stats_t *emp = towers_stats(TOWER_EMP);

    CHECK_EQ(av->kind, TKIND_DAMAGE);
    CHECK_EQ(fw->kind, TKIND_DAMAGE);
    CHECK_EQ(emp->kind, TKIND_STUN);

    CHECK_EQ(av->bg_tile_alt, TILE_TOWER_B);
    CHECK_EQ(fw->bg_tile_alt, TILE_TOWER_2_B);
    CHECK_EQ(emp->bg_tile_alt, TILE_TOWER_3_B);

    CHECK_EQ(emp->hud_letter, 'E');
    CHECK_EQ(emp->cost, TOWER_EMP_COST);
    CHECK_EQ(emp->stun_frames, TOWER_EMP_STUN);
    CHECK_EQ(emp->stun_frames_l1, TOWER_EMP_STUN_L1);
}

/* F1 regression: overlapping EMPs must NOT chain-lock an enemy.
 *
 * Without F1: a 2nd EMP whose target is already stunned by the 1st EMP
 * keeps cooldown=0 (no successful stun → no reset). It then polls every
 * frame and grabs the unstun frame on E1's expiry, perma-freezing the
 * enemy. Spec (specs/iter3-18-third-enemy-tower.md L269-276) explicitly
 * rejects perma-freeze: stacking EMPs must be wasteful.
 *
 * With F1: when found_target && !any_stunned, the 2nd EMP burns its
 * cooldown silently. Both EMPs are now phase-locked to roughly the same
 * cycle and a 60-frame post-stun window opens during which the enemy
 * walks normally. */
static void test_overlapping_emp_no_perma_freeze(void) {
    reset_all();
    /* Two EMPs whose ranges overlap (range_px=16 each, centers 8 px apart). */
    CHECK(towers_try_place(5, 5, TOWER_EMP));  /* slot 0; center (44, 52) */
    CHECK(towers_try_place(6, 5, TOWER_EMP));  /* slot 1; center (52, 52) */
    place_enemy(0, 48, 52);  /* distance 4 from each — in range of both */

    /* Frame 0: F3 fresh-place cooldown=1 → both EMPs decrement to 0; no fire. */
    towers_update();
    CHECK_EQ(s_stun_call_count, 0);

    /* Frame 1: E1 (slot 0, lower idx) processes first — fires & stuns enemy.
     * E2 (slot 1) finds enemy already stunned → F1 third outcome:
     * cooldown reset to 120 (NOT held at 0). */
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();
    int succ = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result) succ++;
    }
    CHECK_EQ(succ, 1);                /* exactly one stun (from E1) */
    CHECK(s_stub_enemies[0].stunned);
    CHECK_EQ(s_sfx_count, 1);         /* SFX_EMP_FIRE only from E1, none for E2 */

    /* Run 60 more frames so both cooldowns count down to ~60.
     * Without F1: E2.cooldown stays at 0 throughout (kept polling, never
     * succeeded). With F1: E2.cooldown ≈ 60 now (was 120 minus 60 ticks). */
    for (int f = 0; f < 60; f++) towers_update();

    /* Simulate enemy stun expiring (the real enemies_update would clear it). */
    s_stub_enemies[0].stunned = 0;
    s_stun_call_count = 0;
    s_sfx_count = 0;

    /* Next frame: with F1, both EMPs are still cooling (~60 left) — no fire.
     * Without F1, E2 (cooldown=0) would re-stun immediately → chain-lock. */
    towers_update();
    int re_succ = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result) re_succ++;
    }
    CHECK_EQ(re_succ, 0);
    CHECK(!s_stub_enemies[0].stunned);
    CHECK_EQ(s_sfx_count, 0);
}

/* F2 regression: upgrading an EMP must not bump its cooldown.
 *
 * Without F2: towers_upgrade() unconditionally sets cooldown to L1 cadence
 * (120). For EMP (whose L0=L1=120), an idle EMP at cooldown=0 is forced
 * into a 120-frame dead window, violating the cooldown-on-success
 * invariant (D-IT3-18-7).
 *
 * With F2: TKIND_STUN preserves its cooldown across upgrade — the EMP
 * fires on its next eligible frame, applying the new L1 stun duration. */
static void test_emp_upgrade_preserves_cooldown(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);

    /* Drain F3 fresh-place cooldown (1 → 0) without firing. */
    towers_update();

    /* Upgrade the (now idle, cooldown=0) EMP. */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 1);

    /* Place an enemy. Next update: with F2, cooldown is still 0 → fires
     * immediately with L1 stun. Without F2, cooldown jumped to 120 → no fire. */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();
    int succ = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result && s_stun_calls[k].frames == TOWER_EMP_STUN_L1)
            succ++;
    }
    CHECK_EQ(succ, 1);
    CHECK_EQ(s_sfx_count, 1);
}

/* F3 regression: freshly-placed EMP defers first pulse by one frame so
 * SFX_TOWER_PLACE isn't preempted same-frame by SFX_EMP_FIRE (both CH1
 * priority 2, equal-priority preempt allowed). */
static void test_emp_fresh_placement_defers_one_frame(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    place_enemy(0, 44, 52);

    /* Frame 0: cooldown 1 → 0, no fire (no SFX_EMP_FIRE). */
    s_stun_call_count = 0;
    s_sfx_count = 0;
    towers_update();
    CHECK_EQ(s_stun_call_count, 0);
    CHECK_EQ(s_sfx_count, 0);

    /* Frame 1: cooldown=0 → fires. */
    towers_update();
    int succ = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result) succ++;
    }
    CHECK(succ > 0);
    CHECK_EQ(s_last_sfx, SFX_EMP_FIRE);
}

/* T_L2_1: L0→L1→L2 upgrade path, and can_upgrade gate at L2. */
static void test_l2_can_upgrade_levels(void) {
    reset_all();
    CHECK(towers_try_place(5, 5, TOWER_AV));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK(towers_can_upgrade((u8)idx));        /* L0: can */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 1);
    CHECK(towers_can_upgrade((u8)idx));        /* L1: can */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 2);
    CHECK(!towers_can_upgrade((u8)idx));       /* L2: cannot */
    CHECK(!towers_upgrade((u8)idx));           /* upgrade fails */
}

/* T_L2_2: spent accumulates all 3 costs; sell refund = spent/2. */
static void test_l2_spent_and_refund(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_FW));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK_EQ(towers_get_spent((u8)idx), TOWER_FW_COST);
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_spent((u8)idx), TOWER_FW_COST + TOWER_FW_UPG_COST);
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_spent((u8)idx),
             TOWER_FW_COST + TOWER_FW_UPG_COST + TOWER_FW_UPG_COST_L2);
    /* 15 + 20 + 30 = 65. Refund = 65/2 = 32. */
    u8 energy_before = economy_get_energy();
    towers_sell((u8)idx);
    CHECK_EQ((u8)(economy_get_energy() - energy_before), 32);
}

/* T_L2_3: L2 AV fires with damage_l2. */
static void test_l2_av_damage(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_AV));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK(towers_upgrade((u8)idx));  /* L1: cooldown set to 40 */
    CHECK(towers_upgrade((u8)idx));  /* L2: cooldown set to 30 */
    /* Place enemy in range: AV center = (44, 52), range = 24 px. */
    place_enemy(0, 44, 52);
    s_proj_fire_count = 0;
    /* L2 AV cooldown=30. 30 frames to decrement to 0, then fires on 31st. */
    for (int f = 0; f < 31; f++) towers_update();
    CHECK(s_proj_fire_count > 0);
    CHECK_EQ(s_last_proj_damage, TOWER_AV_DAMAGE_L2);
}

/* T_L2_4: L2 EMP stuns with stun_frames_l2. */
static void test_l2_emp_stun_duration(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    CHECK(towers_upgrade((u8)idx));  /* L1: cooldown preserved (F2) */
    CHECK(towers_upgrade((u8)idx));  /* L2: cooldown preserved (F2) */
    CHECK_EQ(towers_get_level((u8)idx), 2);
    /* Place enemy in range. */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;
    /* Fresh-place cooldown=1, preserved across both upgrades.
     * Frame 1: cooldown 1→0. Frame 2: cooldown=0 → fire. */
    towers_update();
    towers_update();
    CHECK(s_stun_call_count > 0);
    int found_l2 = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result && s_stun_calls[k].frames == TOWER_EMP_STUN_L2)
            found_l2 = 1;
    }
    CHECK(found_l2);
}

/* T_L2_5: L1→L2 EMP upgrade preserves cooldown (F2 regression). */
static void test_l2_emp_upgrade_preserves_cooldown(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_EMP));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    /* Drain F3 fresh-place cooldown (1→0). */
    towers_update();
    /* Upgrade L0→L1. F2: cooldown preserved (still 0). */
    CHECK(towers_upgrade((u8)idx));
    /* Upgrade L1→L2. F2: cooldown preserved (still 0). */
    CHECK(towers_upgrade((u8)idx));
    CHECK_EQ(towers_get_level((u8)idx), 2);
    /* Place enemy. Tower should fire immediately (cooldown=0). */
    place_enemy(0, 44, 52);
    s_stun_call_count = 0;
    towers_update();
    int succ = 0;
    for (int k = 0; k < s_stun_call_count; k++) {
        if (s_stun_calls[k].result && s_stun_calls[k].frames == TOWER_EMP_STUN_L2)
            succ++;
    }
    CHECK_EQ(succ, 1);
}

/* T_L2_6: tower stats have correct L2 constants. */
static void test_l2_tower_stats(void) {
    const tower_stats_t *av = towers_stats(TOWER_AV);
    const tower_stats_t *fw = towers_stats(TOWER_FW);
    const tower_stats_t *emp = towers_stats(TOWER_EMP);
    CHECK_EQ(av->damage_l2, TOWER_AV_DAMAGE_L2);
    CHECK_EQ(av->cooldown_l2, TOWER_AV_COOLDOWN_L2);
    CHECK_EQ(av->upgrade_cost_l2, TOWER_AV_UPG_COST_L2);
    CHECK_EQ(fw->damage_l2, TOWER_FW_DAMAGE_L2);
    CHECK_EQ(fw->cooldown_l2, TOWER_FW_COOLDOWN_L2);
    CHECK_EQ(fw->upgrade_cost_l2, TOWER_FW_UPG_COST_L2);
    CHECK_EQ(emp->stun_frames_l2, TOWER_EMP_STUN_L2);
    CHECK_EQ(emp->cooldown_l2, TOWER_EMP_COOLDOWN_L2);
    CHECK_EQ(emp->upgrade_cost_l2, TOWER_EMP_UPG_COST_L2);
}

/* T_L2_7: L2 towers render with L2 BG tiles. */
static void test_l2_render_tiles(void) {
    reset_all();
    s_energy = 255;
    CHECK(towers_try_place(5, 5, TOWER_AV));
    i8 idx = towers_index_at(5, 5);
    CHECK(idx >= 0);
    /* Render the L0 placement (Phase 2). */
    towers_render();
    CHECK_EQ(s_bkg_tiles[5][5 + HUD_ROWS], TILE_TOWER);
    /* Upgrade to L1, render. */
    CHECK(towers_upgrade((u8)idx));
    towers_render();
    CHECK_EQ(s_bkg_tiles[5][5 + HUD_ROWS], TILE_TOWER_L1);
    /* Upgrade to L2, render. */
    CHECK(towers_upgrade((u8)idx));
    towers_render();
    CHECK_EQ(s_bkg_tiles[5][5 + HUD_ROWS], TILE_TOWER_L2);
}

int main(void) {
    test_emp_freshly_placed_cooldown_zero();
    test_emp_scan_all_in_range();
    test_emp_cooldown_not_reset_on_empty_scan();
    test_emp_cooldown_reset_on_success();
    test_emp_upgrade_stun_duration();
    test_emp_sfx_only_on_success();
    test_tower_stats_kind();
    test_overlapping_emp_no_perma_freeze();
    test_emp_upgrade_preserves_cooldown();
    test_emp_fresh_placement_defers_one_frame();
    test_l2_can_upgrade_levels();
    test_l2_spent_and_refund();
    test_l2_av_damage();
    test_l2_emp_stun_duration();
    test_l2_emp_upgrade_preserves_cooldown();
    test_l2_tower_stats();
    test_l2_render_tiles();

    if (failures) {
        fprintf(stderr, "test_towers: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_towers: OK\n");
    return 0;
}
