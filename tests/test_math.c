/*
 * Host-side regression tests for the squared-distance arithmetic used by
 * tower target acquisition and projectile hit detection (F2).
 *
 * The on-device code uses i16 for the per-axis pixel deltas, then computes
 * d2 = adx*adx + ady*ady as u16 (was i16, which silently overflows past
 * sqrt(32767) ≈ 181 px — a pair of corner positions on the 160x144 DMG
 * screen exceeds that). These tests reproduce the failing pre-fix
 * behaviour and lock in the post-fix correctness.
 *
 * Constants are pulled from src/tuning.h (host-compilable header — no
 * GBDK dependency) so any drift between gameplay and tests fails the
 * build/tests instead of silently mirroring stale values. See
 * memory/decisions.md "Iter-2 F4 fix: extract tuning.h".
 *
 * Compile: cc -std=c99 -Wall -Wextra -O2 -Isrc tests/test_math.c -o build/test_math
 */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef int16_t  i16;
typedef uint16_t u16;
typedef uint8_t  u8;

#include "tuning.h"
#include "tower_select.h"

/* Iter-3 #18: these live in gtypes.h which pulls <gb/gb.h>. Replicate
 * them here for host tests; the test_tower_select_cycle assertion below
 * locks the expected value so drift is caught. */
enum { ENEMY_BUG = 0, ENEMY_ROBOT = 1, ENEMY_ARMORED = 2, ENEMY_TYPE_COUNT };
enum { TOWER_AV  = 0, TOWER_FW    = 1, TOWER_EMP = 2, TOWER_TYPE_COUNT };

/* Pre-fix (BUGGY) implementation, kept for regression demonstration. */
static i16 d2_signed_buggy(u8 ax, u8 ay, u8 bx, u8 by) {
    i16 dx = (i16)ax - (i16)bx;
    i16 dy = (i16)ay - (i16)by;
    return (i16)(dx*dx + dy*dy);
}

/* Post-fix implementation, matching towers.c::acquire_target and
 * projectiles.c::step_proj. */
static u16 d2_unsigned(u8 ax, u8 ay, u8 bx, u8 by) {
    i16 dx = (i16)ax - (i16)bx;
    i16 dy = (i16)ay - (i16)by;
    u16 adx = dx < 0 ? (u16)-dx : (u16)dx;
    u16 ady = dy < 0 ? (u16)-dy : (u16)dy;
    return (u16)(adx*adx + ady*ady);
}

static int failures = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_short_distance_unaffected(void) {
    /* In-range tower targeting: TOWER_RANGE_SQ = 24*24 = 576. */
    EXPECT(d2_unsigned(100, 100, 100, 100) == 0);
    EXPECT(d2_unsigned(100, 100, 110, 100) == 100);
    EXPECT(d2_unsigned(100, 100, 110, 110) == 200);
    EXPECT(d2_unsigned(100, 100, 124, 100) == 576); /* exactly at range  */
    EXPECT(d2_unsigned(100, 100, 125, 100) == 625); /* just out of range */

    /* Projectile hit: PROJ_HIT_SQ = 4*4 = 16. */
    EXPECT(d2_unsigned( 80, 72,  80, 72) == 0);
    EXPECT(d2_unsigned( 80, 72,  84, 72) == 16);
    EXPECT(d2_unsigned( 80, 72,  84, 73) == 17);
}

static void test_overflow_corner_cases(void) {
    /* Worst case: opposite-corner pixels of the 160x144 screen.
     * dx = 160, dy = 144 -> d2 = 25600 + 20736 = 46336 (> i16 max 32767). */
    u16 d2 = d2_unsigned(0, 0, 160, 144);
    EXPECT(d2 == 46336);

    /* The signed-i16 implementation overflows here: 46336 doesn't fit in
     * i16 (max 32767). The bit pattern survives in u16 (same 16 bits)
     * but the SIGNED comparison used by the original code (`if (d2 >
     * TOWER_RANGE_SQ)` with d2 typed as i16) sees a negative value and
     * accepts it as "in range" — that's the surface bug we lock in below. */
    i16 buggy = d2_signed_buggy(0, 0, 160, 144);
    EXPECT(buggy < 0); /* signed overflow → negative — the real bug surface */

    /* Single-axis 160 -> 25600, fits in i16 (32767), still correct both ways. */
    EXPECT(d2_unsigned(0, 0, 160, 0) == 25600);
    EXPECT((u16)d2_signed_buggy(0, 0, 160, 0) == 25600);
}

static void test_signed_cast_paths(void) {
    /* Negative deltas (target left/above source): abs cast must work. */
    EXPECT(d2_unsigned(160, 144,   0,   0) == 46336);

    /* Past u16 too: 255²+255² = 130050, mod 65536 = 64514. We aren't
     * required to handle this distance (sprite coords cap at 255, play
     * field tops out near 160x144), but the wrap behaviour is defined. */
    EXPECT(d2_unsigned(255, 255,   0,   0) == (u16)64514);
    EXPECT(d2_unsigned(  0,   0, 255, 255) == (u16)64514);
}

static void test_in_vs_out_of_range_decision(void) {
    /* The fix matters because the buggy code, on overflow, can produce a
     * negative i16 that compares <= TOWER_RANGE_SQ and falsely accepts an
     * obviously-out-of-range enemy. Reproduce one such case. */
    const i16 TR_SQ = TOWER_RANGE_SQ;  /* 576 */
    /* dx=180, dy=180 -> true d2=64800 (out of range). */
    i16 buggy = d2_signed_buggy(0, 0, 180, 180);
    /* Demonstrates the bug: signed-overflow result is small/negative and
     * would erroneously be accepted as "in range". */
    EXPECT(buggy <= TR_SQ);
    /* Post-fix: correct unsigned arithmetic rejects it. */
    EXPECT(d2_unsigned(0, 0, 180, 180) > (u16)TR_SQ);
}

/* ============================================================================
 * Iter-2 regression tests — see specs/iter2.md §14.
 * Mirrors of on-device logic; we don't link against game source on the host.
 * ============================================================================ */

/* §14 #1: sell refund formula = spent / 2 (integer division).
 * Cost numbers come from tuning.h so any tuning drift is caught here. */
static u8 sell_refund(u8 spent) { return spent / 2; }

static void test_sell_refund(void) {
    EXPECT(sell_refund(TOWER_AV_COST)                       == 5);
    EXPECT(sell_refund(TOWER_FW_COST)                       == 7);
    EXPECT(sell_refund(TOWER_AV_COST + TOWER_AV_UPG_COST)   == 12); /* 25/2 */
    EXPECT(sell_refund(TOWER_FW_COST + TOWER_FW_UPG_COST)   == 17); /* 35/2 */
}

/* §14 #2: passive income tick — +1 every PASSIVE_INCOME_PERIOD frames.
 * PASSIVE_INCOME_PERIOD comes from tuning.h. */
#define PASSIVE_INCOME_PERIOD 180

typedef struct { u16 timer; u16 awarded; } passive_t;
static void passive_tick(passive_t *p) {
    p->timer++;
    if (p->timer >= PASSIVE_INCOME_PERIOD) { p->timer = 0; p->awarded++; }
}

static void test_passive_income_tick(void) {
    passive_t p = { 0, 0 };
    int i;
    for (i = 0; i < PASSIVE_INCOME_PERIOD; i++) passive_tick(&p);
    EXPECT(p.awarded == 1);
    for (i = PASSIVE_INCOME_PERIOD; i < 2 * PASSIVE_INCOME_PERIOD - 1; i++) passive_tick(&p);
    EXPECT(p.awarded == 1);
    passive_tick(&p);  /* tick to 2*period */
    EXPECT(p.awarded == 2);
    /* Run total to 10*period frames -> 10 awards. */
    p.timer = 0; p.awarded = 0;
    for (i = 0; i < 10 * PASSIVE_INCOME_PERIOD; i++) passive_tick(&p);
    EXPECT(p.awarded == 10);
}

/* §14 #3: wave event-list interpreter, mirror of waves.c state machine. */
enum { WS_DELAY = 0, WS_SPAWNING = 1, WS_DONE = 2 };
typedef struct { u8 type; u8 delay; } spawn_event_t;
typedef struct { const spawn_event_t *events; u8 count; u16 inter_delay; } wave_t;

static u8  ws_state, ws_cur, ws_event_idx;
static u16 ws_timer;
static const wave_t *ws_waves;
static u8           ws_total;
/* Test spawn: succeeds always for deterministic indexing. */
static void ws_init(const wave_t *waves, u8 total) {
    ws_waves = waves; ws_total = total;
    ws_state = WS_DELAY; ws_cur = 0; ws_event_idx = 0; ws_timer = 1;
}
static int spawns_per_wave[8];

static void ws_step(void) {
    if (ws_state == WS_DONE) return;
    if (ws_state == WS_DELAY) {
        if (ws_timer) ws_timer--;
        if (ws_timer == 0) { ws_state = WS_SPAWNING; ws_event_idx = 0; ws_timer = 0; }
        return;
    }
    if (ws_timer) { ws_timer--; return; }
    /* spawn event */
    spawns_per_wave[ws_cur]++;
    ws_event_idx++;
    if (ws_event_idx >= ws_waves[ws_cur].count) {
        u16 nd = ws_waves[ws_cur].inter_delay;
        ws_cur++;
        if (ws_cur >= ws_total) ws_state = WS_DONE;
        else { ws_state = WS_DELAY; ws_timer = nd; }
    } else {
        ws_timer = ws_waves[ws_cur].events[ws_event_idx].delay;
    }
}

static void test_wave_event_index_advance(void) {
    static const spawn_event_t W1[] = { {0,1},{1,2},{0,2},{1,2} };
    static const spawn_event_t W2[] = { {0,1},{1,1} };
    static const wave_t WV[2] = { {W1, 4, 5}, {W2, 2, 0} };
    spawns_per_wave[0] = spawns_per_wave[1] = 0;
    ws_init(WV, 2);
    int safety = 200;
    while (ws_state != WS_DONE && safety-- > 0) ws_step();
    EXPECT(spawns_per_wave[0] == 4);
    EXPECT(spawns_per_wave[1] == 2);
    EXPECT(ws_state == WS_DONE);
}

/* §14 #4: tower-stats lookup. Every value is pulled from tuning.h so
 * gameplay drift triggers a real failure here. The struct shape mirrors
 * src/towers.h::tower_stats_t but uses pure-data fields only. */
typedef struct {
    u8 cost, upgrade_cost;
    u8 cooldown, cooldown_l1;
    u8 damage, damage_l1;
    u8 range_px;
    char hud_letter;
} tower_stats_t;
static const tower_stats_t TS[2] = {
    { TOWER_AV_COST, TOWER_AV_UPG_COST,
      TOWER_AV_COOLDOWN, TOWER_AV_COOLDOWN_L1,
      TOWER_AV_DAMAGE, TOWER_AV_DAMAGE_L1,
      TOWER_AV_RANGE_PX, 'A' },
    { TOWER_FW_COST, TOWER_FW_UPG_COST,
      TOWER_FW_COOLDOWN, TOWER_FW_COOLDOWN_L1,
      TOWER_FW_DAMAGE, TOWER_FW_DAMAGE_L1,
      TOWER_FW_RANGE_PX, 'F' },
};
static void test_tower_stats_lookup(void) {
    /* Anchor the spec's published numbers. If anyone retunes these,
     * either the test fails (bad change) or both spec + this test get
     * updated (good change). */
    EXPECT(TS[0].cost == 10);
    EXPECT(TS[0].damage == 1);
    EXPECT(TS[0].cooldown == 60);
    EXPECT(TS[0].cooldown_l1 == 40);
    EXPECT(TS[0].range_px == 24);
    EXPECT(TS[0].upgrade_cost == 15);
    EXPECT(TS[1].cost == 15);
    EXPECT(TS[1].damage == 3);
    EXPECT(TS[1].damage_l1 == 4);
    EXPECT(TS[1].cooldown == 120);
    EXPECT(TS[1].cooldown_l1 == 90);
    EXPECT(TS[1].range_px == 40);
    EXPECT(TS[1].upgrade_cost == 20);
}

/* §14 #5: enemy bounty lookup. Pulled from tuning.h. */
static const u8 ENEMY_BOUNTY[3] = { BUG_BOUNTY, ROBOT_BOUNTY, ARMORED_BOUNTY };
static void test_enemy_bounty_lookup(void) {
    EXPECT(ENEMY_BOUNTY[0] == 3);
    EXPECT(ENEMY_BOUNTY[1] == 5);
    EXPECT(ENEMY_BOUNTY[2] == 8);
}

/* §14 #6: sell-then-place same tile within one frame. Mirrors the
 * pending-mask de-dup in towers.c. MAX_TOWERS comes from tuning.h. */
typedef struct { u8 tx, ty; } clear_tile_t;
static u16          pending_mask;
static clear_tile_t pending_tiles[MAX_TOWERS];
static u8           occupied[MAX_TOWERS];
static u8           occ_tx[MAX_TOWERS], occ_ty[MAX_TOWERS];

static void t_sell(u8 idx) {
    pending_tiles[idx].tx = occ_tx[idx];
    pending_tiles[idx].ty = occ_ty[idx];
    pending_mask |= (u16)(1u << idx);
    occupied[idx] = 0;
}
static int t_place(u8 tx, u8 ty) {
    /* Cancel any pending clear at (tx,ty). */
    for (u8 i = 0; i < MAX_TOWERS; i++) {
        if ((pending_mask & (1u << i)) &&
            pending_tiles[i].tx == tx && pending_tiles[i].ty == ty) {
            pending_mask &= (u16)~(1u << i);
        }
    }
    /* Find free slot. */
    for (u8 i = 0; i < MAX_TOWERS; i++) {
        if (!occupied[i]) {
            occupied[i] = 1;
            occ_tx[i] = tx; occ_ty[i] = ty;
            return i;
        }
    }
    return -1;
}
/* Drain phase: at most 1 clear per frame, returns count of clears performed. */
static int t_drain(void) {
    if (!pending_mask) return 0;
    for (u8 i = 0; i < MAX_TOWERS; i++) {
        if (pending_mask & (1u << i)) {
            pending_mask &= (u16)~(1u << i);
            return 1;
        }
    }
    return 0;
}

static void test_sell_then_place_same_tile(void) {
    pending_mask = 0;
    for (u8 i = 0; i < MAX_TOWERS; i++) occupied[i] = 0;
    /* Place tower at (5,5). */
    int idx = t_place(5, 5);
    EXPECT(idx == 0);
    /* Sell it. */
    t_sell((u8)idx);
    EXPECT(pending_mask == 0x0001);
    /* Place again at the same tile in the same frame. The de-dup must
     * cancel the pending clear. */
    int idx2 = t_place(5, 5);
    EXPECT(idx2 == 0);          /* slot 0 reused */
    EXPECT(pending_mask == 0);  /* clear was cancelled */
    /* If the drain phase ran later this frame, it would do nothing. */
    EXPECT(t_drain() == 0);
    EXPECT(occupied[0] == 1);
}

/* §14 #7: dist-squared at the new firewall range — pulled from tuning.h. */
static void test_dist_squared_extended_range(void) {
    const u16 FW_RSQ = TOWER_FW_RANGE_SQ;  /* = 1600 */
    EXPECT(FW_RSQ == 1600);
    EXPECT(d2_unsigned(0, 0, TOWER_FW_RANGE_PX, 0)     <= FW_RSQ);  /* exact */
    EXPECT(d2_unsigned(0, 0, TOWER_FW_RANGE_PX + 1, 0) >  FW_RSQ);  /* out  */
    EXPECT(d2_unsigned(0, 0, 24, 32) == 1600);    /* 24²+32² = 576+1024 */
    EXPECT(d2_unsigned(0, 0, 24, 32) <= FW_RSQ);
    EXPECT(d2_unsigned(0, 0, 25, 33) >  FW_RSQ);  /* 625+1089 = 1714 */
}

/* §F1 regression: audio state must reset on new gameplay session so the
 * priority-3 win/lose jingle on ch1 does not leak across sessions and
 * block all subsequent ch1 SFX (e.g. SFX_TOWER_PLACE).
 *
 * The on-device implementation in audio.c::audio_reset() zeros each
 * channel's state struct. We mirror just the state-reset slice here
 * (the hardware-register writes can't be tested without a GB CPU). */
typedef struct { u8 prio; u8 frames_left; u8 note_idx; u8 notes_left; void *cur; } audio_chan_t;
static void audio_state_reset(audio_chan_t s_ch[3]) {
    u8 i;
    for (i = 0; i < 3; i++) {
        s_ch[i].cur = NULL;
        s_ch[i].prio = 0;
        s_ch[i].frames_left = 0;
        s_ch[i].note_idx = 0;
        s_ch[i].notes_left = 0;
    }
}
static void test_audio_state_reset_clears_priority(void) {
    audio_chan_t ch[3];
    /* Simulate the leak scenario: ch1 (idx 0) is playing the win jingle
     * at priority 3, mid-note. */
    ch[0].cur = (void*)0x1234; ch[0].prio = 3;
    ch[0].frames_left = 7; ch[0].note_idx = 1; ch[0].notes_left = 4;
    ch[1].cur = NULL;          ch[1].prio = 0;
    ch[1].frames_left = 0; ch[1].note_idx = 0; ch[1].notes_left = 0;
    ch[2].cur = (void*)0x5678; ch[2].prio = 1;
    ch[2].frames_left = 2; ch[2].note_idx = 0; ch[2].notes_left = 1;

    audio_state_reset(ch);

    EXPECT(ch[0].prio == 0);
    EXPECT(ch[0].cur == NULL);
    EXPECT(ch[0].frames_left == 0);
    EXPECT(ch[1].prio == 0);
    EXPECT(ch[2].prio == 0);
    EXPECT(ch[2].cur == NULL);

    /* Post-reset, a priority-2 SFX (e.g. SFX_TOWER_PLACE on ch1) can
     * preempt because the leaked priority-3 marker is gone. The on-device
     * preempt rule is `prio == 0 || new_prio >= cur_prio`. */
    EXPECT(ch[0].prio == 0 || 2 >= ch[0].prio);
}

/* Iter-3 #18: tower-select cycle via pure header src/tower_select.h. */
static void test_tower_select_cycle(void) {
    EXPECT(TOWER_TYPE_COUNT == 3);
    EXPECT(ENEMY_TYPE_COUNT == 3);
    EXPECT(tower_select_next(0, 3) == 1);   /* AV -> FW */
    EXPECT(tower_select_next(1, 3) == 2);   /* FW -> EMP */
    EXPECT(tower_select_next(2, 3) == 0);   /* EMP -> AV (wrap) */
    /* Backwards-compat: modulo gives same result as XOR for count=2. */
    EXPECT(tower_select_next(0, 2) == 1);
    EXPECT(tower_select_next(1, 2) == 0);
}

/* Iter-3 #18: EMP sell refund. */
static void test_emp_sell_refund(void) {
    EXPECT(sell_refund(TOWER_EMP_COST) == 9);
    EXPECT(sell_refund(TOWER_EMP_COST + TOWER_EMP_UPG_COST) == 15);  /* 30/2 */
}

int main(void) {
    test_short_distance_unaffected();
    test_overflow_corner_cases();
    test_signed_cast_paths();
    test_in_vs_out_of_range_decision();
    test_sell_refund();
    test_passive_income_tick();
    test_wave_event_index_advance();
    test_tower_stats_lookup();
    test_enemy_bounty_lookup();
    test_sell_then_place_same_tile();
    test_dist_squared_extended_range();
    test_audio_state_reset_clears_priority();
    test_tower_select_cycle();
    test_emp_sell_refund();

    if (failures) {
        fprintf(stderr, "%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("test_math: all assertions passed\n");
    return 0;
}
