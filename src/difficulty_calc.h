#ifndef GBTD_DIFFICULTY_CALC_H
#define GBTD_DIFFICULTY_CALC_H

/* Iter-3 #20 — Difficulty modes (CASUAL / NORMAL / VETERAN).
 *
 * Pure helper header — `<stdint.h>` only, no GBDK pull-in. Joins the
 * `tuning.h` / `game_modal.h` / `*_anim.h` family so host tests can
 * include this header directly without `gtypes.h` / `<gb/gb.h>`. The
 * difficulty enum lives here (NOT in `game.h`) so the tests can reach
 * it; `game.h` re-exports via `#include "difficulty_calc.h"`. */

#include <stdint.h>
#include "tuning.h"

enum { DIFF_CASUAL = 0, DIFF_NORMAL = 1, DIFF_VETERAN = 2, DIFF_COUNT = 3 };

/* Per-(difficulty, enemy-type) HP table — 9 bytes ROM. Indexed
 * [difficulty][enemy_type]; order matches DIFF_* and ENEMY_*. The NORMAL
 * row uses BUG_HP / ROBOT_HP / ARMORED_HP symbolically so any future
 * drift in `tuning.h` produces a compile-time identity (T1 in
 * test_difficulty keeps it as defense in depth). */
static const uint8_t DIFF_ENEMY_HP[3][3] = {
    /* CASUAL  */ { 1, 3, 6 },
    /* NORMAL  */ { BUG_HP, ROBOT_HP, ARMORED_HP },
    /* VETERAN */ { 4, 7, 14 },
};

/* Spawn-interval scaler: result = max(floor, (base * num) >> 3).
 * NORMAL is identity (num=8). 30-frame engine floor (supersedes the
 * iter-2 D12 50-frame floor — see memory/conventions.md iter-3 #20). */
#define DIFF_SPAWN_NUM_CASUAL   14u  /* x1.75 — slower */
#define DIFF_SPAWN_NUM_NORMAL   8u  /* x1.0 */
#define DIFF_SPAWN_NUM_VETERAN  6u  /* x0.75 — faster */
#define DIFF_SPAWN_FLOOR       30u  /* absolute minimum spawn interval (frames) */

#define DIFF_START_ENERGY_CASUAL   45u
#define DIFF_START_ENERGY_NORMAL  30u   /* matches START_ENERGY (iter-2) */
#define DIFF_START_ENERGY_VETERAN 28u

static inline uint8_t difficulty_enemy_hp(uint8_t enemy_type, uint8_t diff) {
    if (diff >= 3) diff = 1;            /* clamp to NORMAL on garbage */
    if (enemy_type >= 3) enemy_type = 0;
    return DIFF_ENEMY_HP[diff][enemy_type];
}

static inline uint8_t difficulty_scale_interval(uint8_t base, uint8_t diff) {
    uint8_t num =
        (diff == 0) ? DIFF_SPAWN_NUM_CASUAL :
        (diff == 2) ? DIFF_SPAWN_NUM_VETERAN :
                      DIFF_SPAWN_NUM_NORMAL;
    uint16_t scaled = ((uint16_t)base * num) >> 3;
    if (scaled < DIFF_SPAWN_FLOOR) scaled = DIFF_SPAWN_FLOOR;
    if (scaled > 255u) scaled = 255u;
    return (uint8_t)scaled;
}

static inline uint8_t difficulty_starting_energy(uint8_t diff) {
    if (diff == 0) return DIFF_START_ENERGY_CASUAL;
    if (diff == 2) return DIFF_START_ENERGY_VETERAN;
    return DIFF_START_ENERGY_NORMAL;
}

static inline uint8_t difficulty_cycle_left(uint8_t diff) {
    return (diff == 0) ? 2u : (uint8_t)(diff - 1u);
}
static inline uint8_t difficulty_cycle_right(uint8_t diff) {
    return (diff >= 2) ? 0u : (uint8_t)(diff + 1u);
}

/* Iter-3 #19: per-difficulty score multiplier numerator (denominator 8).
 *   CASUAL  = 8  (×1.0)
 *   NORMAL  = 12 (×1.5)
 *   VETERAN = 16 (×2.0)
 * Garbage difficulty falls back to NORMAL. Applied as
 * `score_apply_mult(base, num) = (base * num) >> 3` (see score_calc.h). */
static inline uint8_t difficulty_score_mult_num(uint8_t diff) {
    if (diff == 0u) return 8u;
    if (diff == 2u) return 16u;
    return 12u;
}

#endif /* GBTD_DIFFICULTY_CALC_H */
