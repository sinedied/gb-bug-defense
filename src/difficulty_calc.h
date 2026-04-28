#ifndef GBTD_DIFFICULTY_CALC_H
#define GBTD_DIFFICULTY_CALC_H

/* Iter-3 #20 — Difficulty modes (EASY / NORMAL / HARD).
 *
 * Pure helper header — `<stdint.h>` only, no GBDK pull-in. Joins the
 * `tuning.h` / `game_modal.h` / `*_anim.h` family so host tests can
 * include this header directly without `gtypes.h` / `<gb/gb.h>`. The
 * difficulty enum lives here (NOT in `game.h`) so the tests can reach
 * it; `game.h` re-exports via `#include "difficulty_calc.h"`. */

#include <stdint.h>
#include "tuning.h"

enum { DIFF_EASY = 0, DIFF_NORMAL = 1, DIFF_HARD = 2, DIFF_COUNT = 3 };

/* Per-(difficulty, enemy-type) HP table — 6 bytes ROM. Indexed
 * [difficulty][enemy_type]; order matches DIFF_* and ENEMY_*. The NORMAL
 * row uses BUG_HP / ROBOT_HP symbolically so any future drift in
 * `tuning.h` produces a compile-time identity (T1 in test_difficulty
 * keeps it as defense in depth). */
static const uint8_t DIFF_ENEMY_HP[3][2] = {
    /* EASY   */ { 2, 4 },
    /* NORMAL */ { BUG_HP, ROBOT_HP },
    /* HARD   */ { 5, 9 },
};

/* Spawn-interval scaler: result = max(floor, (base * num) >> 3).
 * NORMAL is identity (num=8). 30-frame engine floor (supersedes the
 * iter-2 D12 50-frame floor — see memory/conventions.md iter-3 #20). */
#define DIFF_SPAWN_NUM_EASY    12u  /* x1.5 — slower */
#define DIFF_SPAWN_NUM_NORMAL   8u  /* x1.0 */
#define DIFF_SPAWN_NUM_HARD     6u  /* x0.75 — faster */
#define DIFF_SPAWN_FLOOR       30u  /* absolute minimum spawn interval (frames) */

#define DIFF_START_ENERGY_EASY    45u
#define DIFF_START_ENERGY_NORMAL  30u   /* matches START_ENERGY (iter-2) */
#define DIFF_START_ENERGY_HARD    24u

static inline uint8_t difficulty_enemy_hp(uint8_t enemy_type, uint8_t diff) {
    if (diff >= 3) diff = 1;            /* clamp to NORMAL on garbage */
    if (enemy_type >= 2) enemy_type = 0;
    return DIFF_ENEMY_HP[diff][enemy_type];
}

static inline uint8_t difficulty_scale_interval(uint8_t base, uint8_t diff) {
    uint8_t num =
        (diff == 0) ? DIFF_SPAWN_NUM_EASY :
        (diff == 2) ? DIFF_SPAWN_NUM_HARD :
                      DIFF_SPAWN_NUM_NORMAL;
    uint16_t scaled = ((uint16_t)base * num) >> 3;
    if (scaled < DIFF_SPAWN_FLOOR) scaled = DIFF_SPAWN_FLOOR;
    if (scaled > 255u) scaled = 255u;
    return (uint8_t)scaled;
}

static inline uint8_t difficulty_starting_energy(uint8_t diff) {
    if (diff == 0) return DIFF_START_ENERGY_EASY;
    if (diff == 2) return DIFF_START_ENERGY_HARD;
    return DIFF_START_ENERGY_NORMAL;
}

static inline uint8_t difficulty_cycle_left(uint8_t diff) {
    return (diff == 0) ? 2u : (uint8_t)(diff - 1u);
}
static inline uint8_t difficulty_cycle_right(uint8_t diff) {
    return (diff >= 2) ? 0u : (uint8_t)(diff + 1u);
}

#endif /* GBTD_DIFFICULTY_CALC_H */
