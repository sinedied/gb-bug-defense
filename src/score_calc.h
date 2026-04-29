#ifndef GBTD_SCORE_CALC_H
#define GBTD_SCORE_CALC_H

/* Iter-3 #19 — pure helpers for run-score arithmetic.
 *
 * Header-only, `<stdint.h>`-only. NEVER include `gtypes.h` or any GBDK
 * header here so host tests can link without GBDK stubs. */

#include <stdint.h>

/* Per-kill base points (pre-multiplier). */
#define SCORE_KILL_BUG       10u
#define SCORE_KILL_ROBOT     25u
#define SCORE_KILL_ARMORED   50u

/* Win bonus base (pre-multiplier). */
#define SCORE_WIN_BONUS    5000u

/* Wave-clear base (pre-multiplier): 100 × wave_num. */

static inline uint16_t score_kill_base(uint8_t enemy_type) {
    switch (enemy_type) {
        case 0u: return SCORE_KILL_BUG;
        case 1u: return SCORE_KILL_ROBOT;
        case 2u: return SCORE_KILL_ARMORED;
        default: return 0u;
    }
}

static inline uint16_t score_wave_base(uint8_t wave_num) {
    return (uint16_t)(100u * (uint16_t)wave_num);
}

/* Difficulty multiplier: result = (base * mult_num) >> 3
 * where mult_num ∈ {CASUAL=8, NORMAL=12, VETERAN=16}. */
static inline uint16_t score_apply_mult(uint16_t base, uint8_t mult_num) {
    return (uint16_t)(((uint32_t)base * (uint32_t)mult_num) >> 3);
}

/* u16 saturating add — clamps at 0xFFFF, never wraps. */
static inline uint16_t score_add_clamped(uint16_t cur, uint16_t delta) {
    uint32_t s = (uint32_t)cur + (uint32_t)delta;
    return s > 0xFFFFu ? (uint16_t)0xFFFFu : (uint16_t)s;
}

#endif /* GBTD_SCORE_CALC_H */
